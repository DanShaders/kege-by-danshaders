import { requireAuth, userInfo } from "./common";
import { getTaskTypes } from "admin";
import katex from "katex";

import { showInternalErrorScreen, toggleLoadingScreen } from "utils/common";
import * as jsx from "utils/jsx";
import { EmptyPayload, requestU } from "utils/requests";
import { Page, Router } from "utils/router";

import { ContestantAnswer, ContestantKim, ParticipationEndRequest } from "proto/contestant_pb";
import { TaskType } from "proto/task-types_pb";
import { Task } from "proto/tasks_pb";

import { Component } from "components/component";

type TaskSettings = Task & {
  page: KimSolvePage;
  taskTypes: Map<number, TaskType.AsObject>;
  pos: number;
};

const textDecoder = new TextDecoder();
const textEncoder = new TextEncoder();

class TaskComponent extends Component<TaskSettings> {
  flushAnswers?: () => void;

  createElement(): HTMLElement {
    const textContainer = (<div class="text-container" ref />).asElement();
    textContainer.innerHTML =
      `<b>Задание ${this.settings.pos + 1}</b><br>` + this.settings.getText();

    for (const elem of textContainer.querySelectorAll("formula")) {
      katex.render((elem as HTMLElement).innerText, elem as HTMLElement, {
        throwOnError: false,
      });
    }

    const navLeft = (
      <div>
        <button
          class="solve-task-nav-btn"
          onclick={() => void this.settings.page.showTask(this.settings.page.currentPos - 1)}
        >
          <span>&#8592;</span>
        </button>
      </div>
    );
    const navRight = (
      <div>
        <button
          class="solve-task-nav-btn"
          onclick={() => void this.settings.page.showTask(this.settings.page.currentPos + 1)}
        >
          <span>&#8594;</span>
        </button>
      </div>
    );

    const attachments = (
      <div>
        {this.settings.getAttachmentsList().map((attachment) => (
          <>
            <a
              class="solve-attachment"
              download={attachment.getFilename()}
              href={`/api/attachment/${attachment.getHash()}`}
            >
              <img src="/file.png" />
              {attachment.getFilename()}
            </a>
          </>
        ))}
      </div>
    ).asElement();

    if (this.settings.getAnswerCols() == 1 && this.settings.getAnswerRows() == 1) {
      const [elem, answerInput] = (
        <div class="solve-task-wrap">
          <div class="solve-task-viewport">
            {navLeft}
            {textContainer}
            {navRight}
          </div>
          <div>
            {attachments}
            <form
              class="d-flex"
              onsubmit={async (e: Event): Promise<void> => {
                e.preventDefault();
                this.settings.setAnswer(textEncoder.encode(answerInput.value));
                await this.settings.page.sendAnswer(this.settings.getAnswer_asU8());
              }}
            >
              <input ref class="solve-simple-answer-text" type="text" />
              <button class="solve-simple-answer-btn" role="submit">
                Сохранить ответ
              </button>
            </form>
          </div>
        </div>
      ).create() as [HTMLDivElement, HTMLInputElement, HTMLButtonElement];

      this.flushAnswers = (): void => {
        answerInput.value = textDecoder.decode(this.settings.getAnswer_asU8());
      };
      this.flushAnswers();

      return elem;
    } else {
      const inputs: HTMLInputElement[] = [];

      const inputTable = document.createElement("tbody");

      const firstRow = document.createElement("tr");
      firstRow.appendChild(document.createElement("td"));
      for (let j = 0; j < this.settings.getAnswerCols(); ++j) {
        firstRow.appendChild((<td>{j + 1 + ""}</td>).asElement());
      }
      inputTable.appendChild(firstRow);

      for (let i = 0; i < this.settings.getAnswerRows(); ++i) {
        const row = document.createElement("tr");
        row.appendChild((<td>{i + 1 + ""}</td>).asElement());

        for (let j = 0; j < this.settings.getAnswerCols(); ++j) {
          const input = (<input type="text" />).asElement() as HTMLInputElement;
          inputs.push(input);
          row.appendChild((<td>{input}</td>).asElement());
        }
        inputTable.appendChild(row);
      }

      this.flushAnswers = (): void => {
        const currentAnswer = this.settings.getAnswer_asU8();
        for (let i = 0, idx = 0; i < currentAnswer.length; ++i, ++idx) {
          let j = i;
          for (; j < currentAnswer.length && currentAnswer[j]; ++j);
          if (idx >= inputs.length) {
            break;
          }
          inputs[idx].value = textDecoder.decode(currentAnswer.subarray(i, j));
          i = j;
        }
      };
      this.flushAnswers();

      const [elem] = (
        <div class="solve-task-wrap">
          <div class="solve-task-viewport">
            {navLeft}
            {textContainer}
            <div class="extended-answer">
              <div>Введите или скопируйте свой ответ в поля таблицы</div>
              <div>
                <table>{inputTable}</table>
              </div>
              <div class="answer-button-holder">
                <button
                  onclick={async (): Promise<void> => {
                    let length = 0;
                    for (const input of inputs) {
                      length += input.value.length + 1;
                    }
                    if (length == inputs.length) {
                      this.settings.setAnswer(new Uint8Array(0));
                    } else {
                      const result = new Uint8Array(length - 1);
                      length = 0;
                      for (const input of inputs) {
                        textEncoder.encodeInto(input.value, result.subarray(length));
                        length += input.value.length + 1;
                      }
                      this.settings.setAnswer(result);
                    }
                    await this.settings.page.sendAnswer(this.settings.getAnswer_asU8());
                  }}
                >
                  Сохранить ответ
                </button>
              </div>
            </div>
            {navRight}
          </div>
          <div>{attachments}</div>
        </div>
      ).create() as [HTMLDivElement];
      return elem;
    }
  }
}

class KimSolvePage extends Page {
  static URL = "solve" as const;

  kim?: ContestantKim;

  countdown = document.createElement("span");
  answerCountSpan = document.createElement("span");
  taskElem: HTMLElement = document.createElement("div");

  tasks: TaskComponent[] = [];
  buttons: HTMLButtonElement[] = [];

  hasAnswers: boolean[] = [];
  answerCount = 0;

  unloadable = true;
  intervalId?: number;

  private writeToken?: Uint8Array;

  currentPos = -1;

  showTask(taskPos: number): void {
    if (!this.kim || taskPos < 0 || taskPos >= this.kim.getTasksList().length) {
      return;
    }

    if (this.currentPos !== -1) {
      this.buttons[this.currentPos].classList.remove("solve-list-btn-filled");
    }
    this.currentPos = taskPos;
    this.buttons[this.currentPos].classList.add("solve-list-btn-filled");

    const flusher = this.tasks[this.currentPos].flushAnswers;
    if (flusher) {
      flusher();
    }
    const newTaskElem = this.tasks[this.currentPos].elem;
    this.taskElem.replaceWith(newTaskElem);
    this.taskElem = newTaskElem;
  }

  syncButtonWithAnswerPresence(pos: number): void {
    if (this.hasAnswers[pos]) {
      this.buttons[pos].classList.add("solve-list-btn-outline");
    } else {
      this.buttons[pos].classList.remove("solve-list-btn-outline");
    }
  }

  async sendAnswer(answer: Uint8Array): Promise<void> {
    toggleLoadingScreen(true, "sending");
    try {
      this.answerCount -= +this.hasAnswers[this.currentPos];
      this.hasAnswers[this.currentPos] = answer.length !== 0;
      this.answerCount += +this.hasAnswers[this.currentPos];
      this.answerCountSpan.innerText = this.answerCount.toString();
      this.syncButtonWithAnswerPresence(this.currentPos);

      const result = await requestU(
        EmptyPayload,
        "/api/contestant/answer",
        new ContestantAnswer()
          .setWriteToken(this.writeToken!)
          .setAnswer(answer)
          .setTaskId(this.kim!.getTasksList()[this.currentPos].getId())
      );
      if (result.bytes) {
        this.writeToken = result.bytes;
      }
    } catch (e) {
      showInternalErrorScreen(e);
    }
    toggleLoadingScreen(false);
  }

  override async mount(): Promise<void> {
    requireAuth();

    this.unloadable = !!userInfo!.perms;

    const taskTypes = await getTaskTypes();
    this.kim = await requestU(ContestantKim, "/api/contestant/tasks?id=" + this.params.get("id")!);
    this.writeToken = this.kim.getWriteToken_asU8();

    for (const [index, task] of this.kim.getTasksList().entries()) {
      this.tasks.push(
        new TaskComponent(
          Object.assign(task, {
            taskTypes: taskTypes,
            page: this,
            pos: index,
          }),
          null
        )
      );
      this.buttons.push(
        (
          <button class="solve-list-btn" onclick={() => void this.showTask(index)}>
            {taskTypes.get(task.getTaskType())?.shortName.toString()}
          </button>
        ).asElement() as HTMLButtonElement
      );

      const hasAnswer = task.getAnswer_asU8().length !== 0;
      this.answerCount += +hasAnswer;
      this.hasAnswers.push(hasAnswer);

      this.syncButtonWithAnswerPresence(index);
    }

    this.answerCountSpan.innerText = this.answerCount.toString();

    let taskScrollPos = 0;
    const moveTaskList = (pixels: number): void => {
      taskScrollPos += pixels;
      taskScrollPos = Math.max(
        0,
        Math.min(taskScrollPos, taskListViewport.clientHeight - taskListWrap.clientHeight)
      );
      taskListViewport.style.marginTop = -taskScrollPos + "px";
    };

    const updateCountdown = (): void => {
      const remainingTime = Math.floor((this.kim!.getEndTime() - Date.now()) / 1000 / 60);
      this.countdown.innerText =
        Math.floor(remainingTime / 60) + ":" + (remainingTime % 60).toString().padStart(2, "0");
      if (remainingTime < 0) {
        this.unloadable = true;
        Router.instance.redirect("");
      }
    };
    updateCountdown();
    this.intervalId = setInterval(updateCountdown, 200);

    const [taskListWrap, taskListViewport] = (
      <>
        <div class="solve-header fixed-top">
          <span class="solve-clock">
            <img src="watch.png" />
            {this.countdown}
          </span>
          <span>КИМ № {this.kim.getId().toString().padStart(8, "0")}</span>
          <span>БР № 0000000000000</span>
          <a
            class="solve-end"
            onclick={async (): Promise<void> => {
              if (!confirm("Вы уверены, что хотите завершить экзамен досрочно?")) {
                return;
              }
              try {
                await requestU(
                  EmptyPayload,
                  "/api/contestant/end",
                  new ParticipationEndRequest().setId(this.kim!.getId())
                );
                this.unloadable = true;
                Router.instance.goTo("");
              } catch (e) {
                showInternalErrorScreen(e);
              }
            }}
          >
            Завершить экзамен досрочно
          </a>
        </div>
        <div class="solve-list-wrap">
          <span>Дано ответов</span>
          <span>
            {this.answerCountSpan}/{this.kim.getTasksList().length.toString()}
          </span>
          <button
            class="solve-list-btn solve-list-btn-filled fs-5"
            onclick={() => void moveTaskList(-300)}
          >
            &#8593;
          </button>
          <div ref class="solve-list">
            <div ref class="solve-list-viewport">
              {this.buttons.map((elem) => (
                <>{elem}</>
              ))}
            </div>
          </div>
          <button
            class="solve-list-btn solve-list-btn-filled fs-5"
            onclick={() => void moveTaskList(300)}
          >
            &#8595;
          </button>
        </div>
        {this.taskElem}
      </>
    ).replaceContentsOf("main") as [HTMLSpanElement, HTMLDivElement, HTMLDivElement];

    this.showTask(0);
    toggleLoadingScreen(false);
  }

  override unload(): boolean {
    if (this.unloadable && this.intervalId !== undefined) {
      clearInterval(this.intervalId);
    }
    return this.unloadable;
  }
}

Router.instance.registerPage(KimSolvePage);

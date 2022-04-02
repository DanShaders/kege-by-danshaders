import { Component } from "components/component";
import { requireAuth } from "pages/common";
import { Task } from "proto/tasks_pb";
import { TaskDelta, DiffableTask } from "proto/tasks_pb_diff";
import { TaskTypeListResponse } from "proto/task-types_pb";
import { getTaskTypes } from "admin";
import { dbId } from "utils/common";
import { Router } from "utils/router";
import { requestU, EmptyPayload } from "utils/requests";
import { toggleLoadingScreen } from "utils/common";
import { ButtonIcon } from "components/button-icon";
import { FileSelect } from "components/file-select";
import { Checkbox } from "components/checkbox";
import { TextEditor } from "components/text-editor";

import * as jsx from "jsx";

type TaskSettings = {
  id: number;
  taskType: number;
  parent: number;
  text: string;
  answerRows: number;
  answerCols: number;
  answer: Uint8Array;
  taskTypes: TaskTypeListResponse.AsObject;
};

type AnswerSettings = {
  answerRows: number;
  answerCols: number;
  answer: Uint8Array;
};

const textDecoder = new TextDecoder(),
  textEncoder = new TextEncoder();

class AnswerTable extends Component<AnswerSettings> {
  parsedAnswer?: string[][];

  createElement(): HTMLElement {
    const rows: jsx.Fragment[] = [];
    this.parsedAnswer = [];
    let answerPos = 0;
    const answer = this.settings.answer;
    for (let i = 0; i < this.settings.answerRows; ++i) {
      const row = document.createElement("tr");
      this.parsedAnswer.push([]);
      for (let j = 0; j < this.settings.answerCols; ++j) {
        let h = answerPos;
        for (; h < answer.length && answer[h]; ++h);
        this.parsedAnswer[i].push(textDecoder.decode(answer.subarray(answerPos, h)));
        answerPos = Math.min(answer.length, h + 1);

        const cell = (
          <input ref type="text" class="textarea-line" value={this.parsedAnswer[i][j]} />
        ).asElement() as HTMLInputElement;
        row.appendChild((<td class="column-200px">{cell}</td>).asElement());
        cell.addEventListener("change", () => {
          this.parsedAnswer![i][j] = cell.value;
          this.updateComputedAnswer();
        });
      }
      rows.push(<>{row}</>);
    }

    return (
      <div class="flex-align-end">
        <div style="display: inline-block;">
          <table class="table-input table-bordered table-width-auto focusable">
            <tbody>{rows}</tbody>
          </table>
        </div>
        <ButtonIcon settings={{ title: "Изменить размер", icon: "icon-grid-resize", margins: [0, 0, 0, 5] }} />
      </div>
    ).asElement() as HTMLDivElement;
  }

  updateComputedAnswer(): void {
    let length = 0;
    for (let i = 0; i < this.settings.answerRows; ++i) {
      for (let j = 0; j < this.settings.answerCols; ++j) {
        length += this.parsedAnswer![i][j].length + 1;
      }
    }
    const result = new Uint8Array(length - 1);
    length = 0;
    for (let i = 0; i < this.settings.answerRows; ++i) {
      for (let j = 0; j < this.settings.answerCols; ++j) {
        textEncoder.encodeInto(this.parsedAnswer![i][j], result.subarray(length));
        length += this.parsedAnswer![i][j].length + 1;
      }
    }
    this.settings.answer = result;
  }
}

class TaskEditPage extends Component<TaskSettings> {
  createElement(): HTMLElement {
    let elems: Element[] = [];

    const answerTable = new AnswerTable(this.settings, this);

    const elem = (
      <div>
        <div class="group-15-row">
          <label class="group-15-label group-15-left">Тип задания</label>
          <div class="group-15-right">
            <div class="group-15-wrap-border focusable">
              <select ref class="select-line">
                {this.settings.taskTypes.typeList.map((elem) => (
                  <option value={elem.id} selectedIf={elem.id === this.settings.taskType}>
                    {elem.fullName}
                  </option>
                ))}
              </select>
            </div>
          </div>
        </div>

        <div class="group-15-row">
          <label class="group-15-label group-15-left">Текст</label>
          <div class="group-15-right">
            <div class="group-15-wrap-border focusable">
              <TextEditor settings={this.settings} />
            </div>
          </div>
        </div>

        <div class="group-15-row">
          <label class="group-15-label group-15-left">Ответ</label>
          <div class="group-15-right">
            <div class="group-15-wrap-no">{answerTable.elem}</div>
          </div>
        </div>
      </div>
    ).asElement(elems) as HTMLDivElement;

    const [select] = elems as [HTMLSelectElement];

    select.addEventListener("change", () => {
      this.settings.taskType = parseInt(select.value, 10);
    });
    return elem;
  }
}

async function showTaskListPage(params: URLSearchParams): Promise<void> {
  requireAuth(1);

  let handlerInProgress = false,
    updateWhileSave = false;
  const cbDeltaChange = async () => {
    updateWhileSave = true;
    if (handlerInProgress) {
      return;
    }
    handlerInProgress = true;
    while (updateWhileSave) {
      updateWhileSave = false;
      syncText.innerText = "Сохранение...";

      let syncWith = local.ctx!.delta;
      let backup = remote.applyDelta(syncWith);
      local.ctx!.delta = {};

      try {
        await requestU(EmptyPayload, "api/tasks/update", local.serializeDelta(syncWith));
        syncText.innerText = "";
      } catch (e) {
        syncText.innerText = "Не все изменения сохранены";
        console.error(e);

        remote.applyDelta(backup);
        DiffableTask.applyDelta(syncWith, local.ctx!.delta);
        local.ctx!.delta = syncWith;
      }
    }
    handlerInProgress = false;
  };

  const taskTypes = await getTaskTypes();

  let id = dbId();
  let local: DiffableTask,
    remote: DiffableTask,
    raw = new Task();
  if (!params.has("id")) {
    params.set("id", id.toString());
    Router.instance.setUrl(Router.instance.currentPage + "?" + params.toString());
  } else {
    raw = await requestU(Task, "api/tasks/get?id=" + params.get("id"));
  }
  if (raw.getId()) {
    remote = new DiffableTask(raw);
    local = remote.createLocal(cbDeltaChange);
  } else {
    remote = new DiffableTask(new Task().setId(id));
    local = remote.createLocal(() => {});
    local.answerRows = 1;
    local.answerCols = 1;
    local.ctx!.cbDeltaChange = cbDeltaChange;
  }

  const page = new TaskEditPage(Object.assign(local, { taskTypes: taskTypes }), null);
  const redirectBack = (): never => Router.instance.redirect(params.get("back") ?? "");
  const [syncText] = (
    <>
      <div class="flex-space-between">
        <h1 id="page-title">
          <ButtonIcon settings={{ title: "Назад", icon: "icon-back", margins: [5, 10, 0, 0], onClick: redirectBack }} />
          Редактирование задания
        </h1>
        <span ref class="flex-align-center"></span>
      </div>

      {page.elem}
      {/*<div class="group-15-wrap-no group-15-not-first">
            Размер:&nbsp;
            <input type="text" class="textarea-20px textarea-freestanding focusable" />
            &nbsp;&times;&nbsp;
            <input type="text" class="textarea-20px textarea-freestanding focusable" />
          </div>*/}
      <hr />

      <div class="group-15-row">
        <label class="group-15-label group-15-left">Файлы</label>
        <div class="group-15-right">
          <div class="group-15-wrap-border">
            <table class="table-first-col-left">
              <thead>
                <tr>
                  <td class="column-norm">Описание</td>
                  <td class="column-min"></td>
                  <td class="column-min">Ссылка</td>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>123</td>
                  <td>
                    <ButtonIcon settings={{ title: "Открыть", icon: "icon-open" }} />
                    <ButtonIcon settings={{ title: "Удалить", icon: "icon-delete", hoverColor: "red" }} />
                  </td>
                  <td class="clickable">f://fcdeba98</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>
      </div>
      <div class="group-15-row">
        <label class="group-15-label group-15-left">Загрузить</label>
        <div class="group-15-right">
          <div class="group-15-wrap-border focusable">
            <textarea class="textarea-line" rows="1" placeholder="Название файла" />
          </div>
          <div class="group-15-wrap-no group-15-not-first">
            <FileSelect settings={{ required: true }} />
            <button class="button-green" style="margin-left: 5px;">
              Загрузить
            </button>
          </div>
        </div>
      </div>
    </>
  ).replaceContentsOf("main");

  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/edit", showTaskListPage);

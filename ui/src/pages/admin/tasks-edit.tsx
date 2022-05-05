import { Component } from "components/component";
import { SetEntry, SetComponent, listProviderOf, factoryOf } from "components/lists";
import { requireAuth } from "pages/common";
import { Task } from "proto/tasks_pb";
import * as diff from "proto/tasks_pb_diff";
import { TaskTypeListResponse } from "proto/task-types_pb";
import { getTaskTypes } from "admin";
import { dbId } from "utils/common";
import { Router } from "utils/router";
import { SyncController } from "utils/sync-controller";
import { requestU, EmptyPayload } from "utils/requests";
import { toggleLoadingScreen } from "utils/common";
import { ButtonIcon } from "components/button-icon";
import { FileSelectComponent, FileSelect } from "components/file-select";
import { Checkbox } from "components/checkbox";
import { TextEditor } from "components/text-editor";
import { LengthChangeEvent } from "utils/events";

import * as jsx from "jsx";

type AnswerSettings = {
  answerRows: number;
  answerCols: number;
  answer: Uint8Array;
};

type PageSettings = diff.DiffableTask & {
  taskTypes: TaskTypeListResponse.AsObject;
};

type AttachmentSettings = {
  isBlob: boolean;
  realLink: string;
  fakeLink: string;
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
          <input type="text" class="textarea-line" value={this.parsedAnswer[i][j]} />
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

class Attachment extends SetEntry<diff.Task.DiffableAttachment, AttachmentSettings> {
  createElement(): HTMLElement {
    return (
      <tr>
        <td>{this.settings.filename}</td>
        <td>
          <ButtonIcon settings={{ title: "Открыть", icon: "icon-open", onClick: this.open.bind(this) }} />
          <ButtonIcon
            settings={{ title: "Удалить", icon: "icon-delete", hoverColor: "red", onClick: this.delete.bind(this) }}
          />
        </td>
        <td class="clickable">{this.settings.fakeLink}</td>
      </tr>
    ).asElement() as HTMLElement;
  }

  open() {
    window.open(this.settings.realLink);
  }

  delete() {
    delete this.settings.contents;
    if (this.settings.isBlob) {
      URL.revokeObjectURL(this.settings.realLink);
    }
    this.settings.deleted = true;
    this.parent.pop(this.i);
  }
}

class Page extends Component<PageSettings> {
  createElement(): HTMLElement {
    const elems: any[] = [];

    const answerTable = new AnswerTable(this.settings, this);
    let linkCnt = 1;
    const attachmentsSet = new SetComponent<diff.Task.DiffableAttachment, AttachmentSettings>(
      this.settings.attachments,
      this,
      listProviderOf("tbody"),
      factoryOf(Attachment),
      (obj) =>
        Object.assign(obj, {
          isBlob: false,
          realLink: "api/attachment?hash=" + obj.hash,
          fakeLink: `f://${linkCnt++}`,
        })
    );

    const uploadFile = async () => {
      const files = fileInput.getFiles() ?? [];
      if (files.length === 0) {
        fileInput.forceFocus();
        return;
      }
      const file = files[0];
      const contents = new Uint8Array(await file.arrayBuffer());
      const links = {
        isBlob: true,
        realLink: URL.createObjectURL(file),
        fakeLink: `f://${linkCnt++}`,
      };
      attachmentsSet.push(new diff.Task.DiffableAttachment(new Task.Attachment().setId(dbId())), links, (local) => {
        local.filename = filenameInput.value || file.name;
        local.contents = contents;
        local.mimeType = file.type;
      });
      fileInput.clear();
    };

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

        <hr />

        <div ref class="group-15-row" hiddenIf={!attachmentsSet.length}>
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
                {attachmentsSet.elem}
              </table>
            </div>
          </div>
        </div>
        <div class="group-15-row">
          <label class="group-15-label group-15-left">Загрузить</label>
          <div class="group-15-right">
            <div class="group-15-wrap-border focusable">
              <textarea ref class="textarea-line" rows="1" placeholder="Название файла" />
            </div>
            <div class="group-15-wrap-no group-15-not-first">
              <FileSelect ref settings={{ required: true }} />
              <button class="button-green" style="margin-left: 5px;" onclick={uploadFile}>
                Загрузить
              </button>
            </div>
          </div>
        </div>
      </div>
    ).asElement(elems) as HTMLDivElement;

    const [select, filesList, filenameInput, fileInput] = elems as [
      HTMLSelectElement,
      HTMLDivElement,
      HTMLTextAreaElement,
      FileSelectComponent
    ];

    attachmentsSet.addEventListener("lengthchange", (({ length, delta }: LengthChangeEvent) => {
      if (!length) {
        filesList.setAttribute("hidden", "");
      } else {
        filesList.removeAttribute("hidden");
      }
    }) as EventListener);

    select.addEventListener("change", () => {
      this.settings.taskType = parseInt(select.value, 10);
    });
    return elem;
  }
}

async function showTaskListPage(params: URLSearchParams): Promise<void> {
  requireAuth(1);

  let raw = new Task();
  if (!params.has("id")) {
    params.set("id", dbId().toString());
    Router.instance.setUrl(Router.instance.currentPage + "?" + params.toString());
  } else {
    raw = await requestU(Task, "api/tasks/get?id=" + params.get("id"));
  }

  const id = parseInt(params.get("id")!);
  raw.setId(id);

  const statusElem = document.createElement("span");
  const syncController = new SyncController({
    statusElem: statusElem,
    remote: new diff.DiffableTask(raw),
    saveURL: "api/tasks/update",
  });

  const settings = syncController.getLocal();
  settings.answerRows ||= 1;
  settings.answerCols ||= 1;
  syncController.supressSave = false;

  const page = new Page(Object.assign(settings, { taskTypes: await getTaskTypes() }), null);

  const redirectBack = (): never => Router.instance.redirect(params.get("back") ?? "");
  const [syncText] = (
    <>
      <div class="flex-space-between">
        <h1 id="page-title">
          <ButtonIcon settings={{ title: "Назад", icon: "icon-back", margins: [5, 10, 0, 0], onClick: redirectBack }} />
          Редактирование задания
        </h1>
        <span ref class="flex-align-center">
          {statusElem}
        </span>
      </div>

      {page.elem}
    </>
  ).replaceContentsOf("main");

  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/edit", showTaskListPage);

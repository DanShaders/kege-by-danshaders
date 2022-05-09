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
import { AnswerTable } from "components/answer-table";
import { TextEditor } from "components/text-editor";
import { LengthChangeEvent } from "utils/events";

import * as jsx from "jsx";

type PageSettings = diff.DiffableTask & {
  taskTypes: TaskTypeListResponse.AsObject;
};

type AttachmentSettings = {
  isBlob: boolean;
  realLink: string;
  fakeLink: string;
};

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
        <td role="button" class="user-select-all">
          {this.settings.fakeLink}
        </td>
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
          realLink: "api/attachment/" + obj.hash,
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

    const S_ROW = "row gy-3",
      S_LABEL = "col-md-2 text-md-end fw-600 gx-2 gx-lg-3",
      S_INPUT = "col-md-10 gy-0 gy-md-2 gx-2 gx-lg-3";

    const elem = (
      <div class="container-fluid">
        <div class={S_ROW}>
          <label class={S_LABEL}>Тип задания</label>
          <div class={S_INPUT}>
            <select ref class="form-select">
              {this.settings.taskTypes.typeList.map((elem) => (
                <option value={elem.id} selectedIf={elem.id === this.settings.taskType}>
                  {elem.fullName}
                </option>
              ))}
            </select>
          </div>

          <label class={S_LABEL}>Текст</label>
          <div class={S_INPUT}>
            <div class="border rounded focusable" style="background-color: white;">
              <TextEditor settings={this.settings} />
            </div>
          </div>

          <label class={S_LABEL}>Ответ</label>
          <div class={S_INPUT}>
            <div class="group-15-wrap-no">{answerTable.elem}</div>
          </div>

          <hr class="gy-4" />

          <div class="row gx-0 gy-2" ref hiddenIf={!attachmentsSet.length}>
            <label class={S_LABEL}>Файлы</label>
            <div class={S_INPUT}>
              <div class="border rounded">
                <table class="table table-first-col-left table-no-sep table-external-border mb-0">
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

          <label class={S_LABEL}>Загрузить</label>
          <div class={S_INPUT}>
            <textarea ref class="form-control" rows="1" placeholder="Название файла" />

            <div class="mt-2">
              <FileSelect ref settings={{ required: true }} />
              <button class="btn btn-success ms-2" onclick={uploadFile}>
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
    raw = await requestU(Task, "api/tasks/" + params.get("id"));
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
  (
    <>
      <div class="row align-items-end g-0 mb-0 mb-md-3">
        <h2 class="col-md mb-0">
          <ButtonIcon settings={{ title: "Назад", icon: "icon-back", margins: [0, 5, 2, 0], onClick: redirectBack }} />
          Редактирование задания
        </h2>
        <span class="col-md-auto text-end">&nbsp;{statusElem}</span>
      </div>

      {page.elem}
    </>
  ).replaceContentsOf("main");

  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/edit", showTaskListPage);

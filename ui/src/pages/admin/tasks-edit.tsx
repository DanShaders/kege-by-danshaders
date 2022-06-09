import { Component } from "components/component";
import { SetEntry, SetComponent, listProviderOf, factoryOf } from "components/lists";
import { requireAuth } from "pages/common";
import { Task } from "proto/tasks_pb";
import * as diff from "proto/tasks_pb_diff";
import { TaskTypeListResponse } from "proto/task-types_pb";
import { getTaskTypes } from "admin";
import { dbId } from "utils/common";
import BidirectionalMap from "utils/bidirectional-map";
import { Router } from "utils/router";
import { SyncController, SynchronizablePage } from "utils/sync-controller";
import { requestU, EmptyPayload } from "utils/requests";
import { toggleLoadingScreen } from "utils/common";
import { ButtonIcon } from "components/button-icon";
import { FileSelectComponent, FileSelect } from "components/file-select";
import { AnswerTable } from "components/answer-table";
import { TextEditor, TextEditorComponent } from "components/text-editor";
import { LengthChangeEvent } from "utils/events";

import * as jsx from "jsx";

type TaskEditSettings = diff.DiffableTask & {
  taskTypes: TaskTypeListResponse.AsObject;
  uploadImage: (file: File | Blob) => number;
  realMap: BidirectionalMap<number, string>;
  fakeMap: BidirectionalMap<number, string>;
};

type AttachmentSettings = {
  isBlob: boolean;
  realLink: string;
  fakeLink: string;
  realMap: BidirectionalMap<number, string>;
  fakeMap: BidirectionalMap<number, string>;
};

class Attachment extends SetEntry<diff.Task.DiffableAttachment, AttachmentSettings> {
  createElement(): HTMLElement {
    const setVisibility = (e: Event) => {
      this.settings.shownToUser = (e.target as HTMLInputElement).checked;
    };

    return (
      <tr>
        <td>
          <div class="text-truncate">{this.settings.filename}</div>
        </td>
        <td>
          <div class="form-check d-inline-block align-middle m-0" title="Показывать участнику">
            <input
              type="checkbox"
              class="form-check-input"
              onchange={setVisibility}
              checkedIf={this.settings.shownToUser}
            />
          </div>
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
    this.settings.realMap.delete(this.settings.id);
    this.settings.fakeMap.delete(this.settings.id);
    this.parent.pop(this.i);
  }
}

class TaskEditComponent extends Component<TaskEditSettings> {
  textEditor?: TextEditorComponent;

  createElement(): HTMLElement {
    const elems: any[] = [];

    const answerTable = new AnswerTable(this.settings, this);
    let linkCnt = 1;

    const getLink = (): string => {
      return `f://${linkCnt++}`;
    };

    const attachmentsSet = new SetComponent<diff.Task.DiffableAttachment, AttachmentSettings>(
      this.settings.attachments,
      this,
      listProviderOf("tbody"),
      factoryOf(Attachment),
      (obj): diff.Task.DiffableAttachment & AttachmentSettings => {
        const realLink = "/api/attachment/" + obj.hash,
          fakeLink = getLink();
        this.settings.realMap.add(obj.id, realLink);
        this.settings.fakeMap.add(obj.id, fakeLink);
        return Object.assign(obj, {
          isBlob: false,
          realLink: realLink,
          fakeLink: fakeLink,
          realMap: this.settings.realMap,
          fakeMap: this.settings.fakeMap,
        });
      }
    );

    const addFile = async (id: number, file: File | Blob, filename: string, shownToUser: boolean): Promise<void> => {
      const realLink = URL.createObjectURL(file),
        fakeLink = getLink();
      this.settings.realMap.add(id, realLink);
      this.settings.fakeMap.add(id, fakeLink);

      const contents = new Uint8Array(await file.arrayBuffer());
      const links = {
        isBlob: true,
        realLink: realLink,
        fakeLink: fakeLink,
        realMap: this.settings.realMap,
        fakeMap: this.settings.fakeMap,
      };
      const remote = new Task.Attachment().setId(id);
      attachmentsSet.push(new diff.Task.DiffableAttachment(remote), links, (local) => {
        local.filename = filename;
        local.shownToUser = shownToUser;
        local.contents = contents;
        local.mimeType = file.type;
      });
    };

    const uploadFile = async () => {
      const files = fileInput.getFiles() ?? [];
      if (files.length === 0) {
        fileInput.forceFocus();
        return;
      }
      const file = files[0];
      await addFile(dbId(), file, filenameInput.value || file.name, true);
      fileInput.clear();
    };

    this.settings.uploadImage = (file: File | Blob): number => {
      const id = dbId();
      (async () => {
        await addFile(id, file, file instanceof File ? file.name : "Image", false);
      })();
      return id;
    };

    const S_ROW = "row gy-2",
      S_LABEL = "col-md-2 text-md-end fw-600 gx-2 gx-lg-3 align-self-start",
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

          <label class={S_LABEL}>Описание</label>
          <div class={S_INPUT}>
            <textarea ref class="form-control" rows="1" />
          </div>

          <label class={S_LABEL}>Текст</label>
          <div class={S_INPUT}>
            <div class="border rounded focusable" style="background-color: white;">
              <TextEditor ref settings={this.settings} />
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
                <table class="table table-fixed table-first-col-left table-no-sep table-external-border mb-0">
                  <thead>
                    <tr>
                      <td class="column-norm">Описание</td>
                      <td class="column-100px"></td>
                      <td class="column-80px">Ссылка</td>
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

    const [select, tagInput, textEditor, filesList, filenameInput, fileInput] = elems as [
      HTMLSelectElement,
      HTMLTextAreaElement,
      TextEditorComponent,
      HTMLDivElement,
      HTMLTextAreaElement,
      FileSelectComponent
    ];
    this.textEditor = textEditor;

    attachmentsSet.addEventListener("lengthchange", (({ length, delta }: LengthChangeEvent) => {
      if (!length) {
        filesList.setAttribute("hidden", "");
      } else {
        filesList.removeAttribute("hidden");
      }
      if (delta < 0) {
        textEditor.onImageRemoval();
      }
    }) as EventListener);

    select.addEventListener("change", () => {
      this.settings.taskType = parseInt(select.value, 10);
    });
    tagInput.addEventListener("change", () => {
      this.settings.tag = tagInput.value;
    });
    return elem;
  }

  flush() {
    this.textEditor?.flush();
  }
}

class TaskEditPage extends SynchronizablePage<diff.DiffableTask> {
  static URL = "admin/tasks/edit";

  taskEdit?: TaskEditComponent;

  override async mount(): Promise<void> {
    requireAuth(1);
    
    let raw = new Task();
    if (!this.params.has("id")) {
      this.params.set("id", dbId().toString());
      Router.instance.updateUrl();
    } else {
      raw = await requestU(Task, "/api/tasks/" + this.params.get("id"));
    }

    const id = parseInt(this.params.get("id")!);
    raw.setId(id);

    const statusElem = document.createElement("span");
    this.syncController = new SyncController({
      statusElem: statusElem,
      remote: new diff.DiffableTask(raw),
      saveURL: "/api/tasks/update",
    });

    const settings = this.syncController.getLocal();
    settings.answerRows ||= 1;
    settings.answerCols ||= 1;
    this.syncController.supressSave = false;

    const pageSettings = Object.assign(settings, {
      taskTypes: await getTaskTypes(),
      uploadImage: () => 0,
      realMap: new BidirectionalMap<number, string>(),
      fakeMap: new BidirectionalMap<number, string>(),
    });
    this.taskEdit = new TaskEditComponent(pageSettings, null);

    const redirectBack = (): never => Router.instance.redirect(this.params.get("back") ?? "");
    (
      <>
        <div class="row align-items-end g-0 mb-0 mb-md-3">
          <h2 class="col-md mb-0">
            <ButtonIcon
              settings={{ title: "Назад", icon: "icon-back", margins: [0, 5, 2, 0], onClick: redirectBack }}
            />
            Редактирование задания
          </h2>
          <span class="col-md-auto text-end">&nbsp;{statusElem}</span>
        </div>

        {this.taskEdit.elem}
      </>
    ).replaceContentsOf("main");

    toggleLoadingScreen(false);
  }

  override flush(): void {
    this.taskEdit?.flush();
  }
}

Router.instance.registerPage(TaskEditPage);

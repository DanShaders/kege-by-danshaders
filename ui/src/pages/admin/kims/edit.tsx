import { getTaskTypes } from "admin";

import * as jsx from "jsx";

import { dbId, getPrintableParts, toggleLoadingScreen, showInternalErrorScreen } from "utils/common";
import { PositionUpdateEvent } from "utils/events";
import { requestU, EmptyPayload } from "utils/requests";
import { Router } from "utils/router";
import { SyncController, SynchronizablePage } from "utils/sync-controller";

import { Kim } from "proto/kims_pb";
import * as diff from "proto/kims_pb_diff";
import { TaskType } from "proto/task-types_pb";

import { ButtonIcon, ButtonIconComponent } from "components/button-icon";
import { Component } from "components/component";
import {
  factoryOf,
  listProviderOf,
  OrderedSetComponent,
  OrderedSetEntry,
  SetComponent,
  SetEntry,
} from "components/lists";

import { requireAuth } from "pages/common";

const timeZoneOffset = new Date().getTimezoneOffset() * 60 * 1000;

function asInputValue(timestamp: number): string {
  const date = new Date(timestamp - timeZoneOffset);
  return date.toISOString().split("Z")[0];
}

type TaskTypeRequired = {
  taskTypes: Map<number, TaskType.AsObject>;
};

class Task extends OrderedSetEntry<diff.Kim.DiffableTaskEntry, TaskTypeRequired> {
  buttonMoveUp = (
    <ButtonIcon
      settings={{
        title: "Переместить вверх",
        icon: "icon-up",
        onClick: () => void this.parent.swap(this.i, this.i - 1),
      }}
    />
  ).comp as ButtonIconComponent;

  buttonMoveDown = (
    <ButtonIcon
      settings={{
        title: "Переместить вниз",
        icon: "icon-down",
        onClick: () => void this.parent.swap(this.i, this.i + 1),
      }}
    />
  ).comp as ButtonIconComponent;

  createElement(): HTMLElement {
    const updateButtonsAvailability = (): void => {
      this.buttonMoveUp.setEnabled(!this.isFirst);
      this.buttonMoveDown.setEnabled(!this.isLast);
    };
    updateButtonsAvailability();
    this.addEventListener(PositionUpdateEvent.NAME, updateButtonsAvailability);

    return (
      <tr>
        <td>{this.settings.id.toString()}</td>
        <td>{this.settings.taskTypes.get(this.settings.taskType)!.shortName}</td>
        <td>
          <div class="d-flex justify-content-between">
            <span class="text-truncate">{this.settings.tag}</span>
            <span class="ms-1">
              {this.buttonMoveUp.elem}
              {this.buttonMoveDown.elem}
              <ButtonIcon
                settings={{
                  title: "Открыть для редактирования",
                  icon: "icon-open",
                  href:
                    `admin/tasks/edit?id=${this.settings.id}&back=` +
                    encodeURIComponent(Router.instance.currentURL),
                }}
              />
              <ButtonIcon
                settings={{
                  title: "Удалить из варианта",
                  icon: "icon-remove",
                  onClick: () => this.parent.pop(this.i),
                }}
              />
            </span>
          </div>
        </td>
      </tr>
    ).asElement() as HTMLElement;
  }
}

class GroupEntry extends SetEntry<diff.Kim.DiffableGroupEntry, {}> {
  createElement(): HTMLElement {
    const examFlag = this.settings.isExam ? "e" : "";
    const virtualFlag = this.settings.isVirtual ? "v" : "";
    let flags = ` [${examFlag}${virtualFlag}]`;
    if (flags === " []") {
      flags = "";
    }

    const startTime = new Date(this.settings.startTime);
    const endTime = new Date(this.settings.endTime);
    const [sy, sM, sd, sh, sm] = getPrintableParts(startTime);
    const [ey, eM, ed, eh, em] = getPrintableParts(endTime);

    let timeStr = "";
    if (sy == ey && sM == eM && sd == ed) {
      timeStr = `${sd}.${sM}.${sy} ${sh}:${sm} – ${eh}:${em}`;
    } else if (sy == ey) {
      timeStr = `${sd}.${sM} – ${ed}.${eM}.${ey}`;
    } else {
      timeStr = `${sd}.${sM}.${sy} – ${ed}.${eM}.${ey}`;
    }

    return (
      <tr>
        <td>{timeStr}</td>
        <td>{this.settings.id.toString()}</td>
        <td>
          <div class="d-flex justify-content-between">
            <span class="text-truncate">{flags}</span>
            <span class="ms-1">
              <ButtonIcon
                settings={{
                  title: "Редактировать",
                  icon: "icon-sliders",
                }}
              />
              <ButtonIcon
                settings={{
                  title: "Удалить из варианта",
                  icon: "icon-remove",
                }}
              />
            </span>
          </div>
        </td>
      </tr>
    ).asElement() as HTMLElement;
  }
}

class KimEditComponent extends Component<
  diff.DiffableKim & { syncController: SyncController<any> } & TaskTypeRequired
> {
  createElement(): HTMLElement {
    const S_ROW = "row gy-2";
    const S_LABEL = "col-md-2 text-md-end fw-600 gx-2 gx-lg-3 align-self-start";
    const S_INPUT = "col-md-10 gy-0 gy-md-2 gx-2 gx-lg-3";

    const taskList = new OrderedSetComponent<diff.Kim.DiffableTaskEntry, TaskTypeRequired>(
      this.settings.tasks,
      this,
      listProviderOf("tbody"),
      factoryOf(Task),
      (settings) => Object.assign(settings, { taskTypes: this.settings.taskTypes })
    );

    const groupSet = new SetComponent<diff.Kim.DiffableGroupEntry, {}>(
      this.settings.groups,
      this,
      listProviderOf("tbody"),
      factoryOf(GroupEntry),
      (settings) => settings
    );

    const [elem, nameTextarea] = (
      <div class="container-fluid">
        <div class={S_ROW}>
          <label class={S_LABEL}>Название</label>
          <div class={S_INPUT}>
            <textarea
              ref
              class="form-control"
              rows="1"
              onchange={() => void (this.settings.name = nameTextarea.value)}
            >
              {this.settings.name}
            </textarea>
          </div>

          <label class={S_LABEL}>Задания</label>
          <div class={S_INPUT}>
            <div class="border rounded">
              <table class="table table-fixed table-external-border table-third-col-left mb-0">
                <thead>
                  <tr>
                    <td class="column-80px">ID</td>
                    <td class="column-80px">Тип</td>
                    <td class="column-norm">
                      <div class="d-flex justify-content-center">
                        <span class="ms-auto">Комментарий</span>
                        <span class="ms-auto">
                          <ButtonIcon
                            settings={{
                              title: "Добавить задания",
                              icon: "add",
                              href:
                                "admin/tasks/list?back=" +
                                encodeURIComponent(Router.instance.currentURL) +
                                "&kimId=" +
                                this.settings.id,
                            }}
                          />
                          <ButtonIcon
                            settings={{
                              title: "Отсортировать",
                              icon: "icon-sort-down-alt",
                            }}
                          />
                          <ButtonIcon
                            settings={{
                              title: "Перемешать",
                              icon: "icon-shuffle",
                            }}
                          />
                        </span>
                      </div>
                    </td>
                  </tr>
                </thead>
                {taskList.elem}
              </table>
            </div>
          </div>

          <label class={S_LABEL}>Доступ</label>
          <div class={S_INPUT}>
            <div class="border rounded mb-2">
              <table class="table table-fixed table-external-border table-third-col-left mb-0">
                <thead>
                  <tr>
                    <td class="column-200px">Время</td>
                    <td class="column-80px">ID</td>
                    <td class="column-norm">
                      <div class="d-flex justify-content-center">
                        <span class="ms-auto">Группа</span>
                        <span class="ms-auto">
                          <ButtonIcon
                            settings={{
                              title: "Добавить",
                              icon: "add",
                            }}
                          />
                        </span>
                      </div>
                    </td>
                  </tr>
                </thead>
                {groupSet.elem}
              </table>
            </div>

            <button
              class="btn btn-outline-secondary me-2"
              onclick={async (event: Event): Promise<void> => {
                try {
                  toggleLoadingScreen(true);
                  await requestU(EmptyPayload, `/api/kim/${this.settings.id}/revoke-access-keys`);
                } catch (e) {
                  showInternalErrorScreen(e);
                } finally {
                  toggleLoadingScreen(false);
                }
                (event.target as HTMLButtonElement).blur();
              }}
            >
              Revoke access keys
            </button>

            <button class="btn btn-outline-secondary" disabled>
              Clear task cache
            </button>
          </div>
        </div>
      </div>
    ).create() as [HTMLElement, HTMLTextAreaElement];

    return elem;
  }
}

class KimEditPage extends SynchronizablePage<diff.DiffableKim> {
  static URL = "admin/kims/edit" as const;
  static CATEGORY = "kims" as const;

  override async mount(): Promise<void> {
    requireAuth(1);

    let raw = new Kim();
    if (!this.params.has("id")) {
      const id = await dbId();
      raw.setId(id);

      this.params.set("id", id.toString());
      Router.instance.updateUrl();
    } else {
      raw = await requestU(Kim, "/api/kim/" + this.params.get("id"));
    }

    const statusElem = document.createElement("span");
    this.syncController = new SyncController({
      statusElem: statusElem,
      remote: new diff.DiffableKim(raw),
      saveURL: "/api/kim/update",
    });

    const settings = this.syncController.getLocal();
    this.syncController.supressSave = false;

    const kimEdit = new KimEditComponent(
      Object.assign(settings, {
        syncController: this.syncController,
        taskTypes: await getTaskTypes(),
      }),
      null
    );

    (
      <>
        <div class="row align-items-end g-0 mb-0 mb-md-3">
          <h2 class="col-md mb-0">
            <ButtonIcon
              settings={{
                title: "Назад",
                icon: "icon-back",
                margins: [0, 5, 2, 0],
                href: this.params.get("back") ?? "",
              }}
            />
            Редактирование варианта
          </h2>
          <span class="col-md-auto text-end">&nbsp;{statusElem}</span>
        </div>

        {kimEdit.elem}
      </>
    ).replaceContentsOf("main");

    toggleLoadingScreen(false);
  }
}

Router.instance.registerPage(KimEditPage);

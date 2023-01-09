import { getTaskTypes, getGroups } from "admin";

import * as jsx from "jsx";

import {
  dbId,
  getPrintableParts,
  toggleLoadingScreen,
  showInternalErrorScreen,
} from "utils/common";
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

import { Modal } from "bootstrap";

import { requireAuth } from "pages/common";

const timeZoneOffset = new Date().getTimezoneOffset() * 60 * 1000;

function asInputValue(timestamp: number): string {
  const date = new Date(timestamp - timeZoneOffset);
  return date.toISOString().split("Z")[0];
}

type TaskTypeRequired = {
  taskTypes: Map<number, TaskType.AsObject>;
};

type GroupEntrySettings = {
  groupsInfo: Map<number, string>;
  settingsUI: GroupSettingsUI;
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

class GroupSettingsUI extends Component<{ syncController: SyncController<any> }> {
  modal?: Modal;
  currentEntry?: GroupEntry;
  refs?: [
    HTMLHeadingElement,
    HTMLInputElement,
    HTMLInputElement,
    HTMLInputElement,
    HTMLInputElement,
    HTMLInputElement
  ];

  createElement(): HTMLDivElement {
    const updateTimeRelatedSettings = (
      why: "startTime" | "endTime" | "isVirtual" | "duration" | "save"
    ): void => {
      const MILLIS_IN_MIN = 60 * 1000;

      let startDate = new Date(startTimeInput.value);
      let endDate = new Date(endTimeInput.value);

      if (
        startDate.getFullYear() < 2022 ||
        startDate.getFullYear() > 2100 ||
        endDate.getFullYear() < 2022 ||
        endDate.getFullYear() > 2100
      ) {
        if (why !== "save") {
          return;
        } else {
          startDate.setFullYear(2023);
          endDate.setFullYear(2023);
        }
      }

      let startTime = startDate.getTime();
      let endTime = endDate.getTime();

      if (Number.isNaN(startTime) || Number.isNaN(endTime)) {
        return;
      }

      if (endTime < startTime) {
        if (why === "endTime") {
          startTime = endTime;
        } else {
          endTime = startTime;
        }
      }

      let inferredDuration = endTime - startTime;

      // Round interval to minutes
      const spareMillis = inferredDuration % MILLIS_IN_MIN;
      if (spareMillis) {
        inferredDuration -= spareMillis;
        endTime -= spareMillis;
      }

      let actualDuration = Math.round(parseInt(durationInput.value, 10) * MILLIS_IN_MIN);
      const isVirtual = isVirtualCheckbox.checked;

      if (Number.isNaN(actualDuration)) {
        actualDuration = 0;
      }

      // Clap actualDuration to inferredDuration if needed
      actualDuration = Math.max(actualDuration, 0);

      if (inferredDuration < actualDuration || (!isVirtual && actualDuration < inferredDuration)) {
        if (why === "duration") {
          endTime += actualDuration - inferredDuration;
        } else {
          actualDuration = inferredDuration;
        }
      }

      if (why !== "save") {
        startTimeInput.value = asInputValue(startTime);
        endTimeInput.value = asInputValue(endTime);
        durationInput.value = (actualDuration / MILLIS_IN_MIN).toString();
      } else {
        this.settings.syncController.asAtomicChange(() => {
          if (this.currentEntry) {
            this.currentEntry.settings.startTime = startTime;
            this.currentEntry.settings.endTime = endTime;
            this.currentEntry.settings.duration = actualDuration;
            this.currentEntry.settings.isVirtual = isVirtual;
            this.currentEntry.settings.isExam = isExamCheckbox.checked;
            this.currentEntry.updateSummaryInfo();
          }
        });
      }
    };

    this.refs = [] as any;

    const elem = (
      <div class="modal" tabindex="-1">
        <div class="modal-dialog modal-dialog-centered">
          <div class="modal-content">
            <div class="modal-header">
              <h4 ref class="modal-title"></h4>
              <button
                type="button"
                class="btn-close btn-no-shadow"
                data-bs-dismiss="modal"
              ></button>
            </div>
            <div class="modal-body p-3">
              <div class="text-start">
                <div class="input-group">
                  <input
                    ref
                    type="datetime-local"
                    class="form-control"
                    onchange={() => void updateTimeRelatedSettings("startTime")}
                  />
                  <span class="input-group-text">&mdash;</span>
                  <input
                    ref
                    type="datetime-local"
                    class="form-control"
                    onchange={() => void updateTimeRelatedSettings("endTime")}
                  />
                </div>
                <div class="input-group mt-2" style="max-width: 200px;">
                  <input
                    ref
                    type="number"
                    class="form-control"
                    onchange={() => void updateTimeRelatedSettings("duration")}
                  />
                  <span class="input-group-text">мин.</span>
                </div>
                <div class="form-check mt-1">
                  <input
                    ref
                    class="form-check-input"
                    type="checkbox"
                    id="is-virtual-check"
                    onchange={() => void updateTimeRelatedSettings("isVirtual")}
                  />
                  <label class="form-check-label" for="is-virtual-check">
                    Виртуальное участие
                  </label>
                </div>
                <div class="form-check mt-1">
                  <input ref class="form-check-input" type="checkbox" id="is-exam-check" />
                  <label class="form-check-label" for="is-exam-check">
                    Экзамен
                  </label>
                </div>
              </div>
            </div>
            <div class="modal-footer">
              <button
                class="btn btn-primary"
                onclick={() => {
                  updateTimeRelatedSettings("save");
                  this.modal!.hide();
                }}
              >
                Сохранить
              </button>
            </div>
          </div>
        </div>
      </div>
    ).asElement(this.refs) as HTMLDivElement;

    const [
      heading,
      startTimeInput,
      endTimeInput,
      durationInput,
      isVirtualCheckbox,
      isExamCheckbox,
    ] = this.refs as HTMLInputElement[];

    this.modal = new Modal(elem);
    elem.addEventListener("bs.modal.hidden", () => {
      this.currentEntry = undefined;
    });
    return elem;
  }

  updateSettingsOf(entry: GroupEntry): void {
    this.currentEntry = entry;

    const [
      heading,
      startTimeInput,
      endTimeInput,
      durationInput,
      isVirtualCheckbox,
      isExamCheckbox,
    ] = this.refs as HTMLInputElement[];
    heading.innerText =
      entry.settings.groupsInfo.get(entry.settings.id) ?? "id=" + entry.settings.id.toString();
    startTimeInput.value = asInputValue(entry.settings.startTime);
    endTimeInput.value = asInputValue(entry.settings.endTime);
    durationInput.value = Math.round(entry.settings.duration / 60 / 1000).toString();
    isVirtualCheckbox.checked = entry.settings.isVirtual;
    isExamCheckbox.checked = entry.settings.isExam;

    this.modal!.show();
  }
}

class GroupEntry extends SetEntry<diff.Kim.DiffableGroupEntry, GroupEntrySettings> {
  columnTime = document.createElement("td");
  columnFlags = (<span class="text-truncate"></span>).asElement() as HTMLSpanElement;

  createElement(): HTMLElement {
    this.updateSummaryInfo();

    return (
      <tr>
        {this.columnTime}
        <td>{this.settings.id.toString()}</td>
        <td>
          <div class="d-flex justify-content-between">
            <span>
              {this.settings.groupsInfo.get(this.settings.id) ?? ""}
              {this.columnFlags}
            </span>
            <span class="ms-1">
              <ButtonIcon
                settings={{
                  title: "Редактировать",
                  icon: "icon-sliders",
                  onClick: () => {
                    this.settings.settingsUI.updateSettingsOf(this);
                  },
                }}
              />
              <ButtonIcon
                settings={{
                  title: "Удалить",
                  icon: "icon-remove",
                  onClick: () => {
                    this.settings.deleted = true;
                    this.parent.pop(this.i);
                  },
                }}
              />
            </span>
          </div>
        </td>
      </tr>
    ).asElement() as HTMLElement;
  }

  updateSummaryInfo(): void {
    const examFlag = this.settings.isExam ? "e" : "";
    const virtualFlag = this.settings.isVirtual ? "v" : "";
    let flags = ` [${examFlag}${virtualFlag}]`;
    if (flags === " []") {
      flags = "";
    }
    this.columnFlags.innerText = flags;

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
    this.columnTime.innerText = timeStr;
  }
}

class KimEditComponent extends Component<
  diff.DiffableKim & {
    syncController: SyncController<any>;
    groupsInfo: Map<number, string>;
  } & TaskTypeRequired
> {
  createElement(): HTMLElement {
    const S_ROW = "row gy-2";
    const S_LABEL = "col-md-2 text-md-end fw-600 gx-2 gx-lg-3 align-self-start";
    const S_INPUT = "col-md-10 gy-0 gy-md-2 gx-2 gx-lg-3";

    const groupSettingsUI = new GroupSettingsUI(this.settings, null);

    const taskList = new OrderedSetComponent<diff.Kim.DiffableTaskEntry, TaskTypeRequired>(
      this.settings.tasks,
      this,
      listProviderOf("tbody"),
      factoryOf(Task),
      (settings) => Object.assign(settings, { taskTypes: this.settings.taskTypes })
    );

    const staticGroupSettings = {
      settingsUI: groupSettingsUI,
      groupsInfo: this.settings.groupsInfo,
    };

    const groupSet = new SetComponent<diff.Kim.DiffableGroupEntry, GroupEntrySettings>(
      this.settings.groups,
      this,
      listProviderOf("tbody"),
      factoryOf(GroupEntry),
      (settings) => Object.assign(settings, staticGroupSettings)
    );

    const [elem, nameTextarea, groupAddModalElem, groupSelect] = (
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
                              onClick: () => {
                                groupSelect.innerHTML = "";

                                // FIXME: This is dumb, we are definitely maintaining this set somewhere.
                                const currentIds = new Set<number>();
                                for (const [_, groupElem] of groupSet.entries()) {
                                  currentIds.add(groupElem.settings.id);
                                }

                                for (const [id, name] of this.settings.groupsInfo.entries()) {
                                  if (!currentIds.has(id)) {
                                    groupSelect.appendChild(
                                      (<option value={id}>{name}</option>).asElement()
                                    );
                                  }
                                }
                                groupAddModal.show();
                              },
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

            <button
              class="btn btn-outline-secondary"
              onclick={async (event: Event): Promise<void> => {
                try {
                  toggleLoadingScreen(true);
                  await requestU(EmptyPayload, `/api/kim/${this.settings.id}/rejudge-submissions`);
                } catch (e) {
                  showInternalErrorScreen(e);
                } finally {
                  toggleLoadingScreen(false);
                }
                (event.target as HTMLButtonElement).blur();
              }}
            >
              Rejudge submissions
            </button>
          </div>
        </div>

        {groupSettingsUI.elem}

        <div ref class="modal" tabindex="-1">
          <div class="modal-dialog modal-dialog-centered">
            <div class="modal-content">
              <div class="modal-header">
                <h4 class="modal-title">Добавить группу</h4>
                <button
                  type="button"
                  class="btn-close btn-no-shadow"
                  data-bs-dismiss="modal"
                ></button>
              </div>
              <div class="modal-body p-3">
                <select ref class="form-select"></select>
              </div>
              <div class="modal-footer">
                <button
                  class="btn btn-primary"
                  onclick={() => {
                    groupAddModal.hide();

                    const groupId = parseInt(groupSelect.value);
                    const remote = new Kim.GroupEntry().setId(groupId);

                    const ROUNDING = 10 * 60 * 1000; // 10 mins
                    const goodDefaultTime =
                      Math.round((Date.now() + 24 * 3600 * 1000) / ROUNDING) * ROUNDING;

                    // FIXME: Consider user deleting an entry and then adding it back again. This
                    // will in some cases mess with SyncController.
                    groupSet.push(
                      new diff.Kim.DiffableGroupEntry(remote),
                      staticGroupSettings,
                      (local) => {
                        local.endTime = local.startTime = goodDefaultTime;
                      }
                    );
                  }}
                >
                  Добавить
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    ).create() as [HTMLElement, HTMLTextAreaElement, HTMLDivElement, HTMLSelectElement];

    const groupAddModal = new Modal(groupAddModalElem);

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
        groupsInfo: await getGroups(),
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

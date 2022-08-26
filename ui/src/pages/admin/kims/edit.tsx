import * as jsx from "jsx";

import { dbId, toggleLoadingScreen } from "utils/common";
import { PositionUpdateEvent } from "utils/events";
import { requestU } from "utils/requests";
import { Router } from "utils/router";
import { SyncController, SynchronizablePage } from "utils/sync-controller";

import { Kim } from "proto/kims_pb";
import * as diff from "proto/kims_pb_diff";

import { ButtonIcon, ButtonIconComponent } from "components/button-icon";
import { Component } from "components/component";
import { factoryOf, listProviderOf, OrderedSetComponent, OrderedSetEntry } from "components/lists";

import { requireAuth } from "pages/common";

const timeZoneOffset = new Date().getTimezoneOffset() * 60 * 1000;

function asInputValue(timestamp: number): string {
  const date = new Date(timestamp - timeZoneOffset);
  return date.toISOString().split("Z")[0];
}

class Task extends OrderedSetEntry<diff.Kim.DiffableTaskEntry, {}> {
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
        <td>{this.settings.taskType.toString()}</td>
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

class KimEditComponent extends Component<
  diff.DiffableKim & { syncController: SyncController<any> }
> {
  createElement(): HTMLElement {
    const S_ROW = "row gy-2";
    const S_LABEL = "col-md-2 text-md-end fw-600 gx-2 gx-lg-3 align-self-start";
    const S_INPUT = "col-md-10 gy-0 gy-md-2 gx-2 gx-lg-3";

    const taskList = new OrderedSetComponent<diff.Kim.DiffableTaskEntry, {}>(
      this.settings.tasks,
      this,
      listProviderOf("tbody"),
      factoryOf(Task),
      (e) => e
    );

    const updateTimeRelatedSettings = (
      why: "startTime" | "endTime" | "isVirtual" | "duration"
    ): void => {
      const MILLIS_IN_MIN = 60 * 1000;

      let startTime = new Date(startTimeInput.value).getTime();
      let endTime = new Date(endTimeInput.value).getTime();

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

      this.settings.syncController.asAtomicChange(() => {
        startTimeInput.value = asInputValue((this.settings.startTime = startTime));
        endTimeInput.value = asInputValue((this.settings.endTime = endTime));
        this.settings.isVirtual = isVirtual;
        this.settings.duration = actualDuration;
        durationInput.value = (this.settings.duration / MILLIS_IN_MIN).toString();
      });
    };

    const [
      elem,
      nameTextarea,
      isExamCheckbox,
      startTimeInput,
      endTimeInput,
      durationInput,
      isVirtualCheckbox,
    ] = (
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
            <div class="form-check mt-1">
              <input
                ref
                class="form-check-input"
                type="checkbox"
                checkedIf={this.settings.isExam}
                onchange={() => void (this.settings.isExam = isExamCheckbox.checked)}
              />
              <label class="form-check-label" for="flexCheckDefault">
                Экзамен
              </label>
            </div>
          </div>

          <label class={S_LABEL}>Время</label>
          <div class={S_INPUT}>
            <div class="input-group">
              <input
                ref
                type="datetime-local"
                class="form-control"
                value={asInputValue(this.settings.startTime)}
                onchange={() => void updateTimeRelatedSettings("startTime")}
              />
              <span class="input-group-text">&mdash;</span>
              <input
                ref
                type="datetime-local"
                class="form-control"
                value={asInputValue(this.settings.endTime)}
                onchange={() => void updateTimeRelatedSettings("endTime")}
              />
            </div>
            <div class="input-group mt-2" style="max-width: 200px;">
              <input
                ref
                type="number"
                class="form-control"
                value={Math.round(this.settings.duration / 60 / 1000).toString()}
                onchange={() => void updateTimeRelatedSettings("duration")}
              />
              <span class="input-group-text">мин.</span>
            </div>
            <div class="form-check mt-1">
              <input
                ref
                class="form-check-input"
                type="checkbox"
                checkedIf={this.settings.isVirtual}
                onchange={() => void updateTimeRelatedSettings("isVirtual")}
              />
              <label class="form-check-label" for="flexCheckDefault">
                Виртуальное участие
              </label>
            </div>
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
        </div>
      </div>
    ).create() as [
      HTMLElement,
      HTMLTextAreaElement,
      HTMLInputElement,
      HTMLInputElement,
      HTMLInputElement,
      HTMLInputElement,
      HTMLInputElement
    ];

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
      Object.assign(settings, { syncController: this.syncController }),
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

import { Modal } from "bootstrap";

import * as jsx from "jsx";

import { dbId, showInternalErrorScreen, toggleLoadingScreen } from "utils/common";
import { BulkSelectionChangeEvent } from "utils/events";
import { EmptyPayload, requestU } from "utils/requests";
import { Router } from "utils/router";

import { Kim } from "proto/kims_pb";

import { ButtonIcon } from "components/button-icon";
import { FileSelect } from "components/file-select";
import { TaskSelect, TaskSelectComponent } from "components/task-select";

import { requireAuth } from "pages/common";

async function showTaskListPage(params: URLSearchParams): Promise<void> {
  requireAuth(1);

  const setFilter = (): void => {
    taskSelect.setFilter(filterInput.value);
  };

  const [filterInput, createKIMButton, taskSelect, importModal] = (
    <>
      <h2 class="d-flex justify-content-between">
        Задания
        <span>
          <ButtonIcon
            settings={{
              title: "Новое задание",
              icon: "add",
              href: "admin/tasks/edit?back=" + encodeURIComponent(Router.instance.currentURL),
            }}
          />
          <ButtonIcon
            settings={{
              title: "Импортировать",
              icon: "icon-import",
              onClick: (): void => {
                importModalClass.show();
              },
            }}
          />
        </span>
      </h2>

      <div class="row mt-3">
        <div class="col-8">
          <div class="input-group rounded focusable filter-group">
            <input
              ref
              style="font-size: .9rem;"
              class="form-control no-glow font-monospace"
              type="text"
              placeholder="Фильтр"
              onchange={setFilter}
            />
            <button class="btn btn-outline-primary no-glow" onclick={setFilter}>
              <svg>
                <use xlink:href="#icon-search" />
              </svg>
              <div class="icon-hover-circle" />
            </button>
          </div>
        </div>
        <div class="col-4 text-end">
          <button
            ref
            class="btn btn-outline-secondary"
            disabled
            onclick={async (): Promise<void> => {
              let kimId = params.has("kimId") ? parseInt(params.get("kimId")!) : NaN;
              const isNew = Number.isNaN(kimId);
              if (isNew) {
                kimId = await dbId();
              }

              const tasks = [];
              for (const [i, id] of Array.from(
                taskSelect.selectionController.getSelection()
              ).entries()) {
                tasks.push(new Kim.TaskEntry().setId(id).setCurrPos(-1).setSwapPos(i));
              }

              const kim = new Kim().setId(kimId).setTasksList(tasks);

              try {
                toggleLoadingScreen(true);
                await requestU(EmptyPayload, "/api/kim/update", kim);
                Router.instance.redirect(
                  isNew ? "admin/kims/edit?id=" + kim.getId() : params.get("back")!
                );
              } catch (e) {
                toggleLoadingScreen(false);
                showInternalErrorScreen(e);
              }
            }}
          >
            {params.has("kimId") ? "Добавить в КИМ" : "Создать КИМ"}
          </button>
        </div>
      </div>

      <div class="border rounded mt-2">
        <TaskSelect ref settings={{}} />
      </div>

      <div ref class="modal" tabindex="-1">
        <div class="modal-dialog modal-dialog-centered">
          <div class="modal-content">
            <div class="modal-header">
              <h4 class="modal-title">Импортировать задания</h4>
              <button
                type="button"
                class="btn-close btn-no-shadow"
                data-bs-dismiss="modal"
              ></button>
            </div>
            <div class="modal-body p-0">
              <div class="accordion accordion-flush" id="import-accordion">
                <div class="accordion-item">
                  <h2 class="accordion-header">
                    <button
                      class="accordion-button"
                      type="button"
                      data-bs-toggle="collapse"
                      data-bs-target="#import-pdf"
                    >
                      PDF / изображение
                    </button>
                  </h2>
                  <div
                    id="import-pdf"
                    class="accordion-collapse collapse show"
                    data-bs-parent="#import-accordion"
                  >
                    <div class="accordion-body">
                      <FileSelect ref settings={{}} />
                      <div class="form-check mt-3">
                        <input
                          class="form-check-input"
                          type="checkbox"
                          value=""
                          id="import-pdf-smart"
                          checked
                        />
                        <label class="form-check-label" for="import-pdf-smart">
                          Использовать эвристику для разделения на задания
                        </label>
                      </div>
                    </div>
                  </div>
                </div>
                <div class="accordion-item">
                  <h2 class="accordion-header">
                    <button
                      class="accordion-button collapsed"
                      type="button"
                      data-bs-toggle="collapse"
                      data-bs-target="#import-kompege"
                    >
                      kompege.ru
                    </button>
                  </h2>
                  <div
                    id="import-kompege"
                    class="accordion-collapse collapse"
                    data-bs-parent="#import-accordion"
                  >
                    <div class="accordion-body">
                      <label class="form-label fw-600">ID заданий</label>
                      <input
                        type="text"
                        class="form-control"
                        placeholder="напр.: task:1-5, kim:25004839"
                      />
                      <label class="form-label mt-3 fw-600">Комментарий</label>
                      <input type="text" class="form-control" placeholder="" />
                    </div>
                  </div>
                </div>
              </div>
            </div>
            <div class="modal-footer">
              <button class="btn btn-primary">Далее</button>
            </div>
          </div>
        </div>
      </div>
    </>
  ).replaceContentsOf("main") as [
    HTMLInputElement,
    HTMLButtonElement,
    TaskSelectComponent,
    HTMLTableSectionElement
  ];

  taskSelect.setFilter("");

  const controller = taskSelect.selectionController;
  controller.addEventListener(BulkSelectionChangeEvent.NAME, () => {
    if (controller.getSelection().size) {
      createKIMButton.removeAttribute("disabled");
    } else {
      createKIMButton.setAttribute("disabled", "");
    }
  });

  const importModalClass = new Modal(importModal);
  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/list", showTaskListPage, "tasks");

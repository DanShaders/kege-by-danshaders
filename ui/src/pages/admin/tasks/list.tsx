import { Modal } from "bootstrap";

import * as jsx from "jsx";

import { toggleLoadingScreen } from "utils/common";
import { EmptyPayload, requestU } from "utils/requests";
import { Router } from "utils/router";

import { TaskBulkDeleteRequest, TaskListResponse } from "proto/tasks_pb";

import { ButtonIcon } from "components/button-icon";
import { AnyComponent } from "components/component";
import { FileSelect } from "components/file-select";
import { factoryOf, ListComponent, ListEntry, listProviderOf } from "components/lists";
import { TaskSelect, TaskSelectComponent } from "components/task-select";

import { requireAuth } from "pages/common";

class TaskEntry extends ListEntry<TaskListResponse.TaskEntry.AsObject> {
  createElement(): HTMLTableRowElement {
    const refs: AnyComponent[] = [];

    return (
      <tr>
        <td class="ps-3">
          <input class="form-check-input" type="checkbox" />
        </td>
        <td>{this.settings.id.toString()}</td>
        <td>{this.settings.taskType.toString()}</td>
        <td>{this.settings.tag}</td>
        <td class="tr-hover-visible">
          <ButtonIcon
            settings={{
              title: "Открыть",
              icon: "icon-open",
              href:
                `admin/tasks/edit?id=${this.settings.id}&back=` +
                encodeURIComponent(Router.instance.currentURL),
            }}
          />
          <ButtonIcon
            ref
            settings={{
              title: "Удалить",
              icon: "icon-delete",
              hoverColor: "red",
              onClick: async (): Promise<void> => {
                refs[0]!.unproxiedElem!.blur();
                await requestU(
                  EmptyPayload,
                  "/api/tasks/bulk-delete",
                  new TaskBulkDeleteRequest().addTasks(this.settings.id)
                );
                this.parent.pop(this.i);
              },
            }}
          />
        </td>
      </tr>
    ).asElement(refs) as HTMLTableRowElement;
  }
}

async function showTaskListPage(): Promise<void> {
  requireAuth(1);

  const [taskSelect, importModal] = (
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
            <input class="form-control no-glow" type="text" placeholder="Фильтр" />
            <button class="btn btn-outline-primary no-glow">
              <svg>
                <use xlink:href="#icon-search" />
              </svg>
              <div class="icon-hover-circle" />
            </button>
          </div>
        </div>
        <div class="col-4 text-end">
          <button class="btn btn-outline-secondary" disabled>
            Создать КИМ
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
  ).replaceContentsOf("main") as [TaskSelectComponent, HTMLTableSectionElement];

  taskSelect.setFilter("");

  const importModalClass = new Modal(importModal);
  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/list", showTaskListPage, "tasks");

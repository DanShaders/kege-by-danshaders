import { Modal } from "bootstrap";

import { ButtonIcon } from "components/button-icon";
import { FileSelect } from "components/file-select";
import { Router } from "utils/router";
import { toggleLoadingScreen } from "utils/common";
import { requireAuth } from "pages/common";
import * as jsx from "utils/jsx";

async function showTaskListPage(): Promise<void> {
  requireAuth(1);

  const [importModal] = (
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
          <ButtonIcon settings={{
            title: "Импортировать",
            icon: "icon-import",
            onClick: () => {
              importModalClass.show();
            },
          }} />
        </span>
      </h2>

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
                      <button class="accordion-button" type="button" data-bs-toggle="collapse" data-bs-target="#import-pdf">
                        PDF / изображение
                      </button>
                    </h2>
                    <div id="import-pdf" class="accordion-collapse collapse show" data-bs-parent="#import-accordion">
                      <div class="accordion-body">
                        <FileSelect ref settings={{}} />
                        <div class="form-check mt-3">
                          <input class="form-check-input" type="checkbox" value="" id="import-pdf-smart" checked />
                          <label class="form-check-label" for="import-pdf-smart">
                            Использовать эвристику для разделения на задания
                          </label>
                        </div>
                      </div>
                    </div>
                  </div>
                  <div class="accordion-item">
                    <h2 class="accordion-header">
                      <button class="accordion-button collapsed" type="button" data-bs-toggle="collapse" data-bs-target="#import-kompege">
                        kompege.ru
                      </button>
                    </h2>
                    <div id="import-kompege" class="accordion-collapse collapse" data-bs-parent="#import-accordion">
                      <div class="accordion-body">
                        <label class="form-label fw-600">ID заданий</label>
                        <input type="text" class="form-control" placeholder="напр.: task:1-5, kim:25004839" />
                        <label class="form-label mt-3 fw-600">Комментарий</label>
                        <input type="text" class="form-control" placeholder="" />
                      </div>
                    </div>
                  </div>
                </div>
              </div>
              <div class="modal-footer">
                <button class="btn btn-primary">
                  Далее
                </button>
              </div>
            </div>
          </div>
        </div>
    </>
  ).replaceContentsOf("main") as [HTMLDivElement];
  
  const importModalClass = new Modal(importModal);
  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/list", showTaskListPage, "tasks");

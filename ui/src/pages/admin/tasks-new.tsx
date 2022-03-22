import { Router } from "../../utils/router";
import { toggleLoadingScreen } from "../../utils/common";
import { ButtonIcon } from "../../components/button-icon";
import { TextEditor } from "../../components/text-editor";

import * as jsx from "../../utils/jsx";

async function showTaskListPage(params: URLSearchParams): Promise<void> {
  const redirectBack = (): never => Router.instance.redirect(params.get("back") ?? "");

  (
    <>
      <h1 id="page-title">
        <ButtonIcon settings={{ title: "Назад", icon: "icon-back", margins: [5, 10, 0, 0], onClick: redirectBack }} />
        Редактирование задания
      </h1>

      <div class="group-15-row">
        <label class="group-15-label group-15-left">Тип задания</label>
        <div class="group-15-right">
          <div class="group-15-wrap-border focusable">
            <textarea class="group-15-textarea-line" rows="1" placeholder="Название" />
          </div>
        </div>
      </div>

      <div class="group-15-row">
        <label class="group-15-label group-15-left">Текст</label>
        <div class="group-15-right">
          <div class="group-15-wrap-border focusable">
            <TextEditor settings={{}} />
          </div>
        </div>
      </div>
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
            <textarea class="group-15-textarea-line" rows="1" placeholder="Название файла" />
          </div>
          <div class="group-15-wrap-no group-15-not-first">
            <div autoregister class="file-input">
              <input type="file" />
              <span>Файл не выбран</span>
              <button class="button-blue">Выбрать</button>
            </div>
            <button class="button-green">Загрузить</button>
          </div>
        </div>
      </div>
    </>
  ).replaceContentsOf("main");

  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/new", showTaskListPage);

import { ButtonIcon } from "components/button-icon";
import { Router } from "utils/router";
import { toggleLoadingScreen } from "utils/common";
import { requireAuth } from "pages/common";
import * as jsx from "utils/jsx";

async function showTaskListPage(): Promise<void> {
  requireAuth(1);

  (
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
          <ButtonIcon settings={{ title: "Импортировать", icon: "icon-import" }} />
        </span>
      </h2>
    </>
  ).replaceContentsOf("main");
  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks/list", showTaskListPage, "tasks");

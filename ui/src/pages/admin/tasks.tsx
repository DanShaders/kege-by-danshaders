import { ButtonIcon } from "../../components/button-icon";
import { Router } from "../../utils/router";
import { toggleLoadingScreen } from "../../utils/common";
import { requireAuth } from "../common";
import * as jsx from "../../utils/jsx";

async function showTaskListPage(): Promise<void> {
  requireAuth(1);

  const newTaskHandler = (): never =>
    Router.instance.redirect("admin/tasks/edit?back=" + encodeURIComponent(Router.instance.currentURL));
  (
    <>
      <h1 id="page-title" class="flex-space-between">
        Задания
        <span>
          <ButtonIcon settings={{ title: "Новое задание", icon: "add", onClick: newTaskHandler }} />
          <ButtonIcon settings={{ title: "Импортировать", icon: "icon-import" }} />
        </span>
      </h1>
    </>
  ).replaceContentsOf("main");
  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/tasks", showTaskListPage);

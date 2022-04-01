import { Router } from "../utils/router";
import { requireAuth } from "./common";
import { toggleLoadingScreen } from "../utils/common";
import * as jsx from "../utils/jsx";

async function show404(params: URLSearchParams): Promise<void> {
  const [link] = (
    <>
      <h1 id="page-title">404 Not Found</h1>
      <p>
        Запрошенный URL (<pre style="display: inline;">{params.get("url") ?? ""}</pre>) не найден на сервере.&nbsp;
        <a href="#" ref>
          Вернуться на главную
        </a>
      </p>
    </>
  ).replaceContentsOf("main") as [HTMLAnchorElement];
  link.addEventListener("click", (e) => Router.instance.redirect(""));
  toggleLoadingScreen(false);
}

Router.instance.addRoute("404", show404, true);

import { Router, createLink } from "../utils/router";
import { toggleLoadingScreen } from "../utils/common";
import * as jsx from "../utils/jsx";

async function show404(params: URLSearchParams): Promise<void> {
  const [link] = (
    <>
      <h2>404 Not Found</h2>
      <p>
        Запрошенный URL (<code>{params.get("url") ?? ""}</code>) не найден на сервере.&nbsp;
        <a ref>Вернуться на главную</a>
      </p>
    </>
  ).replaceContentsOf("main") as [HTMLAnchorElement];
  createLink(link, "");
  toggleLoadingScreen(false);
}

Router.instance.addRoute("404", show404, true);

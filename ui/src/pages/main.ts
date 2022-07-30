import { toggleLoadingScreen } from "utils/common";
import { Router } from "utils/router";

import { requireAuth } from "pages/common";

async function showMainPage(): Promise<void> {
  requireAuth();
  document.getElementById("main")!.innerHTML = `Main page`;
  toggleLoadingScreen(false);
}

Router.instance.addRoute("", showMainPage);

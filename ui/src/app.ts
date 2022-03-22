import { Router, RedirectNotification, RouteNotFoundError } from "./utils/router";
import { formCallbacks } from "./utils/common";

import "./pages/common";
import "./pages/login";
import "./pages/main";

async function initApplication(): Promise<void> {
  await Router.instance.goTo("#init");
  try {
    await Router.instance.goTo(location.hash.substr(1), false);
  } catch (e) {
    if (!(e instanceof RedirectNotification)) {
      if (e instanceof RouteNotFoundError) {
        await Router.instance.goTo("");
      } else {
        formCallbacks.reportUnexpectedError(e);
      }
    }
  }
}

window.addEventListener("DOMContentLoaded", initApplication);

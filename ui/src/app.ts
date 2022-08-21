import { RouteNotFoundError, Router } from "utils/router";

import "pages/404";
import "pages/common";
import "pages/login";
import "pages/main";

import "pages/list";
import "pages/solve";
import "pages/watch";

async function initApplication(): Promise<void> {
  await Router.instance.goTo("#init");
  try {
    await Router.instance.goTo(location.pathname.substr(1) + location.search, false, true);
  } catch (e) {
    if (e instanceof RouteNotFoundError) {
      await Router.instance.goTo("");
    } else {
      throw e;
    }
  }
}

window.addEventListener("DOMContentLoaded", initApplication);

import { urlRelativeToRoot, RouteNotFoundError, Router } from "utils/router";
import { TaskType, TaskTypeListResponse } from "proto/task-types_pb";
import { requestU } from "utils/requests";
import {showInternalErrorScreen} from "utils/common";

import "pages/404";
import "pages/common";
import "pages/login";
import "pages/main";
import "pages/solve";

let cachedTaskTypes: Map<number, TaskType.AsObject>;

export async function getTaskTypes(): Promise<Map<number, TaskType.AsObject>> {
  if (!cachedTaskTypes) {
    cachedTaskTypes = new Map();
    try {
      const result = (await requestU(TaskTypeListResponse, "/api/task-types/list")).toObject();
      for (const type of result.typeList) {
        cachedTaskTypes.set(type.id, type);
      }
    } catch (e) {
      showInternalErrorScreen(e);
    }
  }
  return cachedTaskTypes;
}

async function initApplication(): Promise<void> {
  await Router.instance.goTo("#init");
  try {
    const url = urlRelativeToRoot(location.pathname);
    if (url === false) {
      throw Error(`Document root mismatch. Current URL "${location.url}" is not a child of "${window.documentRoot}"`);
    }
    await Router.instance.goTo(url + location.search, false, true);
  } catch (e) {
    if (e instanceof RouteNotFoundError) {
      await Router.instance.goTo("");
    } else {
      throw e;
    }
  }
}

window.addEventListener("DOMContentLoaded", initApplication);

import { showInternalErrorScreen } from "utils/common";
import { requestU } from "utils/requests";

import { TaskType, TaskTypeListResponse } from "proto/task-types_pb";
import { GroupListResponse } from "proto/groups_pb";

import "pages/admin/jobs/list";
import "pages/admin/kims/edit";
import "pages/admin/kims/list";
import "pages/admin/tasks/edit";
import "pages/admin/tasks/list";
import "pages/admin/standings";

export const headerSettings = {
  highlightedId: "",
  nav: {
    items: [
      {
        id: "kims",
        text: "Варианты",
        url: "admin/kims/list",
      },
      {
        id: "tasks",
        text: "Задания",
        url: "admin/tasks/list",
      },
      {
        id: "standings",
        text: "Результаты",
        url: "admin/standings",
      },
      {
        id: "control",
        text: "Управление",
        items: [
          {
            text: "Пользователи",
            url: "admin/users",
          },
          {
            text: "Группы",
            url: "admin/groups",
          },
          {
            text: "Сессии",
            url: "admin/sessions",
          },
          {
            text: "Задачи",
            url: "admin/jobs/list",
          },
        ],
      },
    ],
    moreText: "Ещё",
  },
};

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

let cachedGroups: Map<number, string>;

export async function getGroups(): Promise<Map<number, string>> {
  if (!cachedGroups) {
    cachedGroups = new Map();
    try {
      const result = (await requestU(GroupListResponse, "/api/groups/list")).toObject();
      for (const group of result.groupsList) {
        cachedGroups.set(group.id, group.name);
      }
    } catch (e) {
      showInternalErrorScreen(e);
    }
  }
  return cachedGroups;
}

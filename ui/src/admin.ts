import { requestU } from "utils/requests";

import { TaskTypeListResponse } from "proto/task-types_pb";

import "pages/admin/jobs/list";
import "pages/admin/kims/list";
import "pages/admin/tasks/edit";
import "pages/admin/tasks/list";

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

let cachedTaskTypes: TaskTypeListResponse.AsObject;

export async function getTaskTypes(): Promise<TaskTypeListResponse.AsObject> {
  if (!cachedTaskTypes) {
    cachedTaskTypes = (await requestU(TaskTypeListResponse, "/api/task-types/list")).toObject();
  }
  return cachedTaskTypes;
}

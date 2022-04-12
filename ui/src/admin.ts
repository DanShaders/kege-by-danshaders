import { requestU } from "./utils/requests";
import { TaskTypeListResponse } from "./proto/task-types_pb";

import "./pages/admin/tasks";
import "./pages/admin/tasks-edit";

export const headerSettings = {
  highlightedId: "",
  nav: {
    items: [
      {
        id: "kims",
        text: "Варианты",
        url: "admin/kims",
      },
      {
        id: "tasks",
        text: "Задания",
        url: "admin/tasks",
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
        ],
      },
    ],
    moreText: "Ещё",
  },
};

let cachedTaskTypes: TaskTypeListResponse.AsObject;

export async function getTaskTypes(): Promise<TaskTypeListResponse.AsObject> {
  if (!cachedTaskTypes) {
    cachedTaskTypes = (await requestU(TaskTypeListResponse, "api/task-types/list")).toObject();
  }
  return cachedTaskTypes;
}

import "./pages/admin/tasks";
import "./pages/admin/tasks-new";

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

import Papa from "papaparse";

import * as jsx from "jsx";

import { showInternalErrorScreen, toggleLoadingScreen } from "utils/common";
import { EmptyPayload, requestU } from "utils/requests";
import { Router } from "utils/router";

import { User, UserReplaceRequest } from "proto/users_pb";

import { requireAuth } from "pages/common";

async function showUsersPage(): Promise<void> {
  requireAuth(1);

  const [csvInput] = (
    <>
      <h2>Пользователи</h2>
      Колонки: логин, пароль, имя, группы. Например:
      <pre>
        danklishch,password?,"Клищ Данил Павлович",Долгопрудный,12-ая параллель{"\n"}
        mlxa,"qwerty,","""Будников"" Михаил Сергеевич",Бремен
      </pre>
      <b>Это удаляет текущих пользователей, включая информацию об их ответах!</b> (кроме admin и
      user)
      <br />
      <textarea ref></textarea>
      <button
        onclick={async (): Promise<void> => {
          try {
            const input = Papa.parse<string[]>(csvInput.value);
            const groups: Map<string, number> = new Map();
            let groupsCount = 0;
            const req: UserReplaceRequest = new UserReplaceRequest();

            for (const row of input.data) {
              if (row.length === 0 || (row.length === 1 && row[0] === "")) {
                continue;
              } else if (row.length < 3) {
                alert("Bad line: " + row);
                continue;
              }

              const user = new User();
              user.setUsername(row[0]).setPassword(row[1]).setDisplayName(row[2]);
              for (let i = 3; i < row.length; ++i) {
                if (row[i] === "") {
                  continue;
                }
                if (!groups.has(row[i])) {
                  groups.set(row[i], groupsCount++);
                  req.addGroups(row[i]);
                }
                user.addGroups(groups.get(row[i])!);
              }
              req.addUsers(user);
            }

            let text = "";
            text += "Группы:\n";
            for (const [name, id] of groups.entries()) {
              text += name + " -> " + id + "\n";
            }
            text += "\nПользователи:\n";
            for (const user of req.getUsersList()) {
              text += `${user.getUsername()}: ${user.getDisplayName()} (${user.getGroupsList()})\n`;
            }
            if (confirm(text) && prompt('Введите "да" без кавычек\n' + text) === "да") {
              await requestU(EmptyPayload, "/api/users/replace", req);
              alert("Заменено");
            } else {
              alert("Отменено");
            }
          } catch (e) {
            showInternalErrorScreen(e);
          }
        }}
      >
        Заменить
      </button>
      <br />
      <a href="/api/users/html-list" target="_blank">
        Текущий список пользователей
      </a>
    </>
  ).replaceContentsOf("main");
  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/users", showUsersPage, "control");

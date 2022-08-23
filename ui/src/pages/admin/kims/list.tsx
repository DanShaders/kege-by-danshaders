import * as jsx from "jsx";

import { toggleLoadingScreen } from "utils/common";
import { requestU } from "utils/requests";
import { Router } from "utils/router";

import { Kim, KimListResponse } from "proto/kims_pb";

import { ButtonIcon } from "components/button-icon";
import { factoryOf, ListComponent, ListEntry, listProviderOf } from "components/lists";

import { requireAuth } from "pages/common";

function getPrintableParts(
  date: Date
): [year: string, month: string, date: string, hours: string, minutes: string] {
  return [
    date.getFullYear().toString(),
    date.getMonth().toString().padStart(2, "0"),
    date.getDate().toString().padStart(2, "0"),
    date.getHours().toString().padStart(2, "0"),
    date.getMinutes().toString().padStart(2, "0"),
  ];
}

class KimEntry extends ListEntry<Kim.AsObject> {
  createElement(): HTMLTableRowElement {
    const examFlag = this.settings.isExam ? "e" : "";
    const virtualFlag = this.settings.isVirtual ? "v" : "";
    let flags = ` [${examFlag}${virtualFlag}]`;
    if (flags === " []") {
      flags = "";
    }

    const startTime = new Date(this.settings.startTime);
    const endTime = new Date(this.settings.endTime);
    const [sy, sM, sd, sh, sm] = getPrintableParts(startTime);
    const [ey, eM, ed, eh, em] = getPrintableParts(endTime);

    let timeStr = "";
    if (sy == ey && sM == eM && sd == ed) {
      timeStr = `${sd}.${sM}.${sy} ${sh}:${sm} – ${eh}:${em}`;
    } else if (sy == ey) {
      timeStr = `${sd}.${sM} – ${ed}.${eM}.${ey}`;
    } else {
      timeStr = `${sd}.${sM}.${sy} – ${ed}.${eM}.${ey}`;
    }

    return (
      <tr>
        <td>{this.settings.id.toString()}</td>
        <td>{timeStr}</td>
        <td>
          {this.settings.name}
          {flags}
        </td>
        <td class="tr-hover-visible">
          <ButtonIcon
            settings={{
              title: "Открыть",
              icon: "icon-open",
              href:
                "admin/kims/edit?back=" +
                encodeURIComponent(Router.instance.currentURL) +
                "&id=" +
                this.settings.id,
            }}
          />
          <ButtonIcon
            settings={{
              title: "",
              icon: "icon-delete",
              hoverColor: "red",
              onClick: async (): Promise<void> => {
                if (!confirm("Вы уверены, что хотите удалить вариант?")) {
                  return;
                }

                // TODO
              },
            }}
          />
        </td>
      </tr>
    ).asElement() as HTMLTableRowElement;
  }
}

async function showKimsListPage(): Promise<void> {
  requireAuth(1);

  const [kimListElem] = (
    <>
      <h2 class="d-flex justify-content-between">
        Варианты
        <span>
          <ButtonIcon
            settings={{
              title: "Новый КИМ",
              icon: "add",
              href: "admin/kims/edit?back=" + encodeURIComponent(Router.instance.currentURL),
            }}
          />
        </span>
      </h2>

      <div class="border rounded mt-2">
        <table
          class={"table table-fixed table-no-sep table-third-col-left table-external-border mb-0"}
        >
          <thead>
            <tr>
              <td class="column-80px">ID</td>
              <td class="column-200px">Время</td>
              <td class="column-norm">Название</td>
              <td class="column-80px"></td>
            </tr>
          </thead>
          <tbody ref class="no-hover-effects">
            <tr>
              <td colspan="4">
                <span
                  style="border-width: 2px;"
                  class="spinner-border spinner-border-sm m-2"
                ></span>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </>
  ).replaceContentsOf("main");

  toggleLoadingScreen(false);

  requestU(KimListResponse, "/api/kim/list").then(
    (kims) => {
      kimListElem.replaceWith(
        new ListComponent(
          kims.toObject().kimsList,
          null,
          listProviderOf("tbody"),
          factoryOf(KimEntry)
        ).elem
      );
    },
    (reason) => {
      kimListElem.replaceWith(
        (
          <tbody class="no-hover-effects">
            <tr>
              <td colspan="4" class="text-start">
                Произошла ошибка во время загрузки списка вариантов
                <pre class="mb-0">{(reason.stack ?? reason).toString()}</pre>
              </td>
            </tr>
          </tbody>
        ).asElement()
      );
    }
  );
}

Router.instance.addRoute("admin/kims/list", showKimsListPage, "kims");

import * as jsx from "jsx";

import { toggleLoadingScreen, showInternalErrorScreen } from "utils/common";
import { requestU, EmptyPayload } from "utils/requests";
import { Router } from "utils/router";

import { Kim, KimListResponse, KimDeleteRequest } from "proto/kims_pb";

import { ButtonIcon } from "components/button-icon";
import { factoryOf, ListComponent, ListEntry, listProviderOf } from "components/lists";

import { requireAuth } from "pages/common";

class KimEntry extends ListEntry<Kim.AsObject> {
  createElement(): HTMLTableRowElement {
    return (
      <tr>
        <td>{this.settings.id.toString()}</td>
        <td>
          <div class="d-flex justify-content-between">
            <span class="text-truncate">{this.settings.name}</span>
            <span class="ms-1 tr-hover-visible">
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
                    try {
                      toggleLoadingScreen(true);
                      await requestU(EmptyPayload, "/api/kim/delete", new KimDeleteRequest().setId(this.settings.id));
                      this.parent.pop(this.i);
                    } catch (e) {
                      showInternalErrorScreen(e);
                    } finally {
                      toggleLoadingScreen(false);
                    }
                  },
                }}
              />
            </span>
          </div>
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
          class={"table table-fixed table-no-sep table-second-col-left table-external-border mb-0"}
        >
          <thead>
            <tr>
              <td class="column-80px">ID</td>
              <td class="column-norm">Название</td>
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

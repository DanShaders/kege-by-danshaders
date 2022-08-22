import { toggleLoadingScreen } from "utils/common";
import * as jsx from "utils/jsx";
import { EmptyPayload, requestU } from "utils/requests";
import { Page, Router } from "utils/router";

import { Kim, UserKimListResponse } from "proto/kims_pb";

import { Component } from "components/component";

import { requireAuth } from "pages/common";

type UnsolvedKim = {
  id: string;
  name: string;
  endTime: string;
  duration: string;
  comment: string;
  action: {
    text: string;
    url: string;
  };
};

export class UnsolvedKimTable extends Component<Array<UnsolvedKim>> {
  constructor(settings: Array<Kim.AsObject>) {
    super([], null);
    console.log(settings);
    for (const item of settings) {
      let unsolvedKim: UnsolvedKim;
      unsolvedKim = {
        id: item.id.toString(),
        name: item.name,
        endTime: "2020.20.20",
        duration: item.duration.toString(),
        comment: item.comment,
        action: {
          text: item.state === 0 ? "Решать" : "Посмотреть",
          url: "kim/" + (item.state === 0 ? "solve" : "watch") + "?id=" + item.id,
        },
      };
      this.settings.push(unsolvedKim);
    }
  }

  createElement(): HTMLElement {
    const rows: jsx.Fragment[] = [];

    const firstRow = document.createElement("tr");
    firstRow.innerHTML = `
      <td class='column-200px'>КИМ</td>
      <td class='column-200px'>Название</td>
      <td class='column-200px'>Продолжительность</td>
      <td class='column-200px'>Время окончания</td>
      <td class='column-200px'>Комментарий</td>
      <td class='column-200px'>Действие</td>`;
    rows.push(<>{firstRow}</>);

    for (const kim of this.settings) {
      const row = document.createElement("tr");
      const columnTexts = [
        kim.id,
        kim.name,
        kim.duration,
        kim.endTime,
        kim.comment,
        kim.action.text,
      ];
      const columnTypes = ["id", "name", "duration", "endTime", "comment", "url"];
      const elements: Array<Element> = [];
      for (let i = 0; i < columnTypes.length; ++i) {
        const type = columnTypes[i];
        const text = columnTexts[i];
        const cell = document.createElement("span");
        cell.innerHTML = text;
        const element: Element = (
          <td class="column-200px" id={type}>
            {cell}
          </td>
        ).asElement();
        if (type == "url") {
          element.classList.add("kim-table-action");
          element.addEventListener("click", async () => {
            await requestU(EmptyPayload, "/api/kim/update/state/" + kim.id.toString() + "/1");
            await Router.instance.redirect(kim.action.url);
          });
        }
        row.appendChild(element);
      }
      rows.push(<>{row}</>);
    }

    const refs: Element[] = [];
    const elem = (
      <table class="table answer-table table-external-border mb-0">
        <tbody>{rows}</tbody>
      </table>
    ).asElement(refs) as HTMLDivElement;

    return elem;
  }
}

class KimListPage extends Page {
  static URL = "kim/list";

  async getUserKims(): Promise<UserKimListResponse.AsObject> {
    let userKims: UserKimListResponse.AsObject;
    userKims = (await requestU(UserKimListResponse, "/api/kim/list")).toObject();
    return userKims;
  }

  override async mount() {
    requireAuth(0);
    const kimList = (await this.getUserKims()).kimList;
    const unsolvedKimList: Array<Kim.AsObject> = [];
    const solvedKimList: Array<Kim.AsObject> = [];
    for (const kim of kimList) {
      if (kim.state === 0) {
        unsolvedKimList.push(kim);
      } else if (kim.state == 2) {
        solvedKimList.push(kim);
      }
    }
    const unsolvedKimTable = new UnsolvedKimTable(unsolvedKimList);
    (
      <>
        <div class="unsolved-kims">
          <h2>Доступные для решения варианты</h2>
          {unsolvedKimTable.elem}
        </div>
      </>
    ).replaceContentsOf("main");
    toggleLoadingScreen(false);
  }
}

Router.instance.registerPage(KimListPage);

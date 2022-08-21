import { Router, Page } from "utils/router";
import { toggleLoadingScreen } from "utils/common";
import { requireAuth } from "pages/common";
import { requestU, EmptyPayload } from "utils/requests";
import { UserKimListResponse, Kim } from "proto/kims_pb";
import { Component } from "components/component";
import * as jsx from "utils/jsx";

type UnsolvedKim = {
  id: string;
  name: string;
  endTime: string;
  duration: string;
  comment: string;
  action: {
    text: string;
    url: string;
  }
};

export class UnsolvedKimTable extends Component<Array<UnsolvedKim>> {
  constructor(settings: Array<Kim.AsObject>) {
    super([], null);
    console.log(settings);
    for (let item of settings) {
      let unsolvedKim : UnsolvedKim;
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

    let firstRow = document.createElement("tr");
    firstRow.innerHTML = `
      <td class='column-200px'>КИМ</td>
      <td class='column-200px'>Название</td>
      <td class='column-200px'>Продолжительность</td>
      <td class='column-200px'>Время окончания</td>
      <td class='column-200px'>Комментарий</td>
      <td class='column-200px'>Действие</td>`;
    rows.push(<>{firstRow}</>)

    for (const kim of this.settings) {
      let row = document.createElement("tr");
      let columnTexts = [kim.id, kim.name, kim.duration, kim.endTime, kim.comment, kim.action.text];
      let columnTypes = ["id", "name", "duration", "endTime", "comment", "url"];
      let elements : Array<Element> = [];
      for (let i = 0; i < columnTypes.length; ++i) {
        const type = columnTypes[i], text = columnTexts[i];
        let cell = document.createElement("span");
        cell.innerHTML = text;
        let element : Element = (<td class="column-200px" id={type}>{cell}</td>).asElement();
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
    let kimList = (await this.getUserKims()).kimList;
    let unsolvedKimList : Array<Kim.AsObject> = [];
    let solvedKimList : Array<Kim.AsObject> = [];
    for (const kim of kimList) {
      if (kim.state === 0) {
        unsolvedKimList.push(kim);
      } else if (kim.state == 2) {
        solvedKimList.push(kim);
      }
    }
    let unsolvedKimTable = new UnsolvedKimTable(unsolvedKimList);
    (
      <>
      <div class='unsolved-kims'>
        <h2>Доступные для решения варианты</h2>
        {unsolvedKimTable.elem}
      </div>
      </>
    ).replaceContentsOf("main");
    toggleLoadingScreen(false);
  }
}

Router.instance.registerPage(KimListPage);

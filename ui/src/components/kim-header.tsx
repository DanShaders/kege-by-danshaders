import { Router } from "../utils/router";
import { Component } from "./component";

export type KimHeaderSettings = {
  clock: string;
  kimId: string;
  finishExam: {
    text: string;
    url: string;
  };
};

export class KimHeaderComponent extends Component<KimHeaderSettings> {
  override createElement(): HTMLElement {
    const elem = document.createElement("header");
    elem.innerHTML = `
      <div class='kim-header-clock'></div>
      <div class='kim-header-number'></div>
      <div class='kim-header-finish'></div>
    `;

    const [clock, kimNumber, finishExam] = elem.children;

    clock.innerHTML = this.settings.clock;

    kimNumber.innerHTML = this.settings.kimId;

    finishExam.innerHTML = this.settings.finishExam.text;
    finishExam.addEventListener("click", async () => {
      if (confirm("Вы уверены, что хотите завершить экзамен досрочно?")) {
        await Router.instance.redirect(this.settings.finishExam.url);
      }
    });

    return elem;
  }
}
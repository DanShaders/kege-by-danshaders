import { Component, createComponentFactory } from "./component";
import * as jsx from "../utils/jsx";

export type NoticeSettings = {
  shown: boolean;
  message: string;
  type: "error" | "info";
};

export class NoticeComponent extends Component<NoticeSettings> {
  private version: number = 1;
  messageSpan?: HTMLSpanElement;

  createElement(): HTMLElement {
    const elem = document.createElement("div");
    const [messageSpan, closeButton] = (
      <>
        <span>
          <svg hiddenIf={this.settings.type !== "error"}>
            <use xlink:href={"#notice-" + this.settings.type}></use>
          </svg>
          <span ref>{this.settings.message}</span>
        </span>
        <button ref class='wrapper'>
          <svg><use xlink:href='#close'></use></svg>
        </button>
      </>
    ).insertInto(elem) as [HTMLSpanElement, HTMLButtonElement];
    this.messageSpan = messageSpan;
    
    closeButton.addEventListener("click", () => {
      this.setVisibility(false);
    });
    if (!this.settings.shown) {
      elem.setAttribute("hidden", "");
    }
    elem.classList.add("notice", "notice-" + this.settings.type);
    return elem;
  }

  private doSetVisibility(elem: HTMLElement, shown: boolean): void {
    ++this.version;
    if (shown) {
      elem.classList.remove("notice-fade-out");
      elem.removeAttribute("hidden");
    } else {
      elem.classList.add("notice-fade-out");
      const savedVersion = this.version;
      setTimeout(() => {
        if (savedVersion === this.version) {
          elem.setAttribute("hidden", "");
          elem.classList.remove("notice-fade-out");
        }
      }, 300);
    }
  }

  setVisibility(shown: boolean): void {
    this.settings.shown = shown;
    if (this.unproxiedElem) {
      this.doSetVisibility(this.unproxiedElem, shown);
    }
  }

  setMessage(message: string): void {
    this.settings.message = message;
    if (this.messageSpan) {
      this.messageSpan.innerText = message;
    }
  }

  setType(type: typeof this.settings.type): void {
    if (type !== this.settings.type) {
      this.settings.type = type;
      if (this.unproxiedElem) {
        this.rerenderMe();
      }
    }
  }
}

export const Notice = createComponentFactory<NoticeSettings>(NoticeComponent);

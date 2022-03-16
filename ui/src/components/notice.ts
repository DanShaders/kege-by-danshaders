import { Component } from "./component";

type NoticeSettings = {
  shown: boolean;
  message: string;
  type: "error" | "info";
};

export class NoticeComponent extends Component<NoticeSettings> {
  private version: number = 1;
  messageSpan?: HTMLSpanElement;

  createElement(): HTMLElement {
    const elem = document.createElement("div");
    elem.innerHTML = `
      <span>
        <svg ${this.settings.type === "error" ? "" : "hidden"}>
          <use xlink:href='#notice-${this.settings.type}'></use>
        </svg>
        <span></span>
      </span>
      <button class='wrapper'>
        <svg><use xlink:href='#close'></use></svg>
      </button>`;
    const [messageWrap, closeButton] = elem.children;
    closeButton.addEventListener("click", () => {
      this.setVisibility(false);
    });
    this.messageSpan = messageWrap.children[1] as HTMLSpanElement;
    this.messageSpan.innerHTML = this.settings.message;
    if (!this.settings.shown) {
      elem.setAttribute("hidden", "");
    }
    this.doSetVisibility(elem, this.settings.shown);
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

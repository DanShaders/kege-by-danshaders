import * as jsx from "jsx";

import { Component, createComponentFactory } from "components/component";

export type NoticeSettings = {
  shown: boolean;
  message: string;
  type: "error" | "info";
};

const NOTICE_CLASSES = {
  info: "alert-primary",
  error: "alert-danger",
};

export class NoticeComponent extends Component<NoticeSettings> {
  private version: number = 1;
  messageSpan?: HTMLSpanElement;

  createElement(): HTMLElement {
    const messageSpanRef: Element[] = [];
    const elem = (
      <div
        class={"alert alert-dismissible fade show " + NOTICE_CLASSES[this.settings.type]}
        hiddenIf={!this.settings.shown}
      >
        <svg class="me-sm-3 me-2" width="26" height="26" hiddenIf={this.settings.type !== "error"}>
          <use xlink:href="#notice-error" />
        </svg>
        <span ref>{this.settings.message}</span>
        <button
          type="button"
          class="btn-close btn-no-shadow"
          onclick={(): void => {
            this.setVisibility(false);
          }}
        />
      </div>
    ).asElement(messageSpanRef) as HTMLDivElement;
    this.messageSpan = messageSpanRef[0] as HTMLSpanElement;
    return elem;
  }

  private doSetVisibility(elem: HTMLElement, shown: boolean): void {
    ++this.version;
    if (shown) {
      elem.classList.add("show");
      elem.removeAttribute("hidden");
    } else {
      elem.classList.remove("show");
      const savedVersion = this.version;
      setTimeout(() => {
        if (savedVersion === this.version) {
          elem.setAttribute("hidden", "");
        }
      }, 200);
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

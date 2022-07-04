import { createLink } from "../utils/router";
import { Component, createComponentFactory } from "./component";
import * as jsx from "../utils/jsx";

type Settings = {
  title: string;
  icon: string;
  enabled?: boolean;
  href?: string;
  onClick?: () => void;
  hoverColor?: string;
  margins?: [number, number, number, number];
};

class ButtonIconComponent extends Component<Settings> {
  createElement(): HTMLElement {
    this.settings.enabled ??= this.settings.onClick !== undefined || this.settings.href !== undefined;

    const button = (
      <a class="button-icon" title={this.settings.title} disabledIf={!this.settings.enabled}>
        <svg>
          <use xlink:href={"#" + this.settings.icon}></use>
        </svg>
      </a>
    ).asElement() as HTMLAnchorElement;
    if (this.settings.hoverColor) {
      button.style.setProperty("--button-icon-hover-fill", this.settings.hoverColor);
    }
    if (this.settings.margins) {
      button.style.margin = this.settings.margins.join("px ") + "px";
    }

    if (this.settings.enabled) {
      const href = this.settings.href;
      if (typeof href === "string") {
        createLink(button, href);
      } else if (this.settings.onClick) {
        button.addEventListener("click", this.settings.onClick);
      }
    }
    return button;
  }
}

export const ButtonIcon = createComponentFactory(ButtonIconComponent);

import * as jsx from "jsx";

import { createLink } from "utils/router";

import { Component, createComponentFactory } from "components/component";

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
    this.settings.enabled ??=
      this.settings.onClick !== undefined || this.settings.href !== undefined;

    const icon = (
      <svg>
        <use xlink:href={"#" + this.settings.icon}></use>
      </svg>
    );
    let button: HTMLAnchorElement | HTMLButtonElement;

    if (typeof this.settings.href === "string") {
      button = (
        <a class="button-icon" title={this.settings.title}>
          {icon}
        </a>
      ).asElement() as HTMLAnchorElement;
      createLink(button, this.settings.href);
    } else {
      button = (
        <button class="button-icon" title={this.settings.title} disabledIf={!this.settings.enabled}>
          {icon}
        </button>
      ).asElement() as HTMLButtonElement;
      if (this.settings.onClick) {
        button.addEventListener("click", this.settings.onClick);
      }
    }

    if (this.settings.hoverColor) {
      button.style.setProperty("--button-icon-hover-fill", this.settings.hoverColor);
    }
    if (this.settings.margins) {
      button.style.margin = this.settings.margins.join("px ") + "px";
    }
    return button;
  }
}

export const ButtonIcon = createComponentFactory(ButtonIconComponent);

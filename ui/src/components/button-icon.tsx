import { Component, createComponentFactory } from "./component";
import * as jsx from "../utils/jsx";

type Settings = {
  title: string;
  icon: string;
  onClick?: () => void;
  hoverColor?: string;
  margins?: [number, number, number, number];
};

class ButtonIconComponent extends Component<Settings> {
  createElement(): HTMLElement {
    const button = (
      <button class="button-icon" title={this.settings.title}>
        <svg>
          <use xlink:href={"#" + this.settings.icon}></use>
        </svg>
      </button>
    ).asElement() as HTMLButtonElement;
    if (this.settings.hoverColor) {
      button.style.setProperty("--button-icon-hover-fill", this.settings.hoverColor);
    }
    if (this.settings.margins) {
      button.style.margin = this.settings.margins.join("px ") + "px";
    }
    if (this.settings.onClick) {
      button.addEventListener("click", this.settings.onClick);
    }
    return button;
  }
}

export const ButtonIcon = createComponentFactory(ButtonIconComponent);

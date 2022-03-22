import { Component, createComponentFactory } from "./component";
import { uniqueId } from "../utils/common";
import * as jsx from "../utils/jsx";

type Settings = {
  text: string;
};

class CheckboxComponent extends Component<Settings> {
  createElement(): HTMLElement {
    const checkboxId = uniqueId();
    const elem = (
      <div class="checkbox-wrap">
        <input type="checkbox" id={checkboxId} />
        <label for={checkboxId}>{this.settings.text}</label>
      </div>
    ).asElement() as HTMLDivElement;
    return elem;
  }
}

export const Checkbox = createComponentFactory(CheckboxComponent);
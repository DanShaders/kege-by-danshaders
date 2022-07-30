import { Component, createComponentFactory } from "./component";
import * as jsx from "../utils/jsx";

export type TabSelectSettings = {
  selectedTab: number;
  tabNames: string[];
  onTabSwitch?: (index: number, oldIndex: number) => void;
};

export class TabSelectComponent extends Component<TabSelectSettings> {
  private controls: Element[] = [];
  private length: number = 0;

  createElement(): HTMLElement {
    const elem = (
      <div class="tab-wrap">
        <nav>
          {this.settings.tabNames.map((name, index) => (
            <button ref disabledIf={index === this.settings.selectedTab} class="tab-button">
              {name}
            </button>
          ))}
        </nav>
        {this.children.map((frag, index) => (
          <div ref class={index === this.settings.selectedTab ? "tab-active" : "tab-inactive"}>
            {frag}
          </div>
        ))}
      </div>
    ).asElement(this.controls) as HTMLElement;

    this.length = this.settings.tabNames.length;
    for (let i = 0; i < this.length; ++i) {
      this.controls[i].addEventListener("click", () => this.selectTab(i));
    }
    return elem;
  }

  selectTab(index: number): void {
    const oldIndex = this.settings.selectedTab;

    this.controls[oldIndex].removeAttribute("disabled");
    const cl1 = this.controls[oldIndex + this.length].classList;
    cl1.remove("tab-active");
    cl1.add("tab-inactive");

    this.settings.selectedTab = index;
    this.controls[index].setAttribute("disabled", "");
    const cl2 = this.controls[index + this.length].classList;
    cl2.add("tab-active");
    cl2.remove("tab-inactive");

    if (this.settings.onTabSwitch) {
      this.settings.onTabSwitch(index, oldIndex);
    }
  }
}

export const TabSelect = createComponentFactory(TabSelectComponent);

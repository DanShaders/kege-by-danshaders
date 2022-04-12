import { Component, createComponentFactory } from "./component";
import * as jsx from "../utils/jsx";

type Settings = {
  multiple?: boolean;
  required?: boolean;
  buttonClass?: string;
};

export class FileSelectComponent extends Component<Settings> {
  private inputElem?: HTMLInputElement;
  private spanElem?: HTMLSpanElement;
  private buttonElem?: HTMLButtonElement;

  private getTextForEmpty() {
    return this.settings.multiple ? "Файлы не выбраны" : "Файл не выбран";
  }

  createElement(): HTMLDivElement {
    let refs: HTMLElement[] = [];
    const elem = (
      <div class="file-input">
        <input ref type="file" requiredIf={this.settings.required} multipleIf={this.settings.multiple} />
        <span ref>{this.getTextForEmpty()}</span>
        <button ref class={this.settings.buttonClass ?? "button-blue"}>
          Выбрать
        </button>
      </div>
    ).asElement(refs) as HTMLDivElement;
    [this.inputElem, this.spanElem, this.buttonElem] = refs as [HTMLInputElement, HTMLSpanElement, HTMLButtonElement];

    this.inputElem.addEventListener("change", () => {
      this.onChange();
    });
    elem.addEventListener("click", () => {
      this.elem.classList.remove("file-input-focus");
      this.inputElem!.click();
    });
    return elem;
  }

  onChange() {
    let filename = "";
    let totalSize = 0;
    for (const file of this.inputElem!.files ?? []) {
      filename += ", " + file.name;
      totalSize += file.size;
    }
    if (totalSize >= 20 << 20) {
      alert("Максимальный размер файла \u2014 20Мб");
      window.requestAnimationFrame(() => this.clear());
    }
    filename = filename.substr(2);
    if (filename === "") {
      filename = this.getTextForEmpty();
    }
    if (filename === ", ") filename = "?";
    this.spanElem!.innerText = filename;
  }

  getFiles(): FileList | null {
    return this.inputElem!.files;
  }

  clear() {
    this.inputElem!.value = "";
    this.onChange();
  }

  forceFocus(): void {
    this.elem.classList.add("file-input-focus");
  }
}

export const FileSelect = createComponentFactory(FileSelectComponent);

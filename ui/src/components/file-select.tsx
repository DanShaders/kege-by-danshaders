import { Component, createComponentFactory } from "./component";
import * as jsx from "../utils/jsx";

type Settings = {
  multiple?: boolean;
  required?: boolean;
};

export class FileSelectComponent extends Component<Settings> {
  private inputElem?: HTMLInputElement;
  private spanElem?: HTMLSpanElement;
  private buttonElem?: HTMLButtonElement;

  private getTextForEmpty(): string {
    return this.settings.multiple ? "Файлы не выбраны" : "Файл не выбран";
  }

  createElement(): HTMLDivElement {
    const refs: HTMLElement[] = [];
    const elem = (
      <div class="input-group d-inline-flex file-select focusable" role="button">
        <span class="input-group-text">
          <span ref class="text-truncate text-start">
            {this.getTextForEmpty()}
          </span>
        </span>
        <input ref type="file" requiredIf={this.settings.required} multipleIf={this.settings.multiple} hidden />
        <button ref type="button" class="btn btn-primary">
          Выбрать
        </button>
      </div>
    ).asElement(refs) as HTMLDivElement;
    [this.spanElem, this.inputElem, this.buttonElem] = refs as [HTMLSpanElement, HTMLInputElement, HTMLButtonElement];

    this.inputElem.addEventListener("change", () => {
      this.onChange();
    });

    elem.addEventListener("mousedown", () => {
      window.requestAnimationFrame(() => {
        this.buttonElem!.focus();
      });
    });
    elem.addEventListener("click", () => {
      this.buttonElem!.focus();
      this.inputElem!.click();
    });
    return elem;
  }

  onChange(): void {
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

  clear(): void {
    this.inputElem!.value = "";
    this.onChange();
  }

  forceFocus(): void {
    this.buttonElem!.focus();
  }
}

export const FileSelect = createComponentFactory(FileSelectComponent);

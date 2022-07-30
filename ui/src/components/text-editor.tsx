import katex from "katex";
import Quill, { RangeStatic } from "quill";
import Delta from "quill-delta";
import Op from "quill-delta/dist/Op";

import * as jsx from "jsx";

import BidirectionalMap from "utils/bidirectional-map";
import * as transforms from "utils/editor-transforms";

import { Component, createComponentFactory } from "components/component";
import { TabSelect, TabSelectSettings } from "components/tab-select";

enum EditorTabs {
  QUILL = 0,
  CODE = 1,
  PREVIEW = 2,
}

type TextEditorSettings = {
  text: string;
  uploadImage: (file: File) => number;
  realMap: BidirectionalMap<number, string>;
  fakeMap: BidirectionalMap<number, string>;
};

const ATTRIBUTES_WHITELIST = new Set([
  "bold",
  "italic",
  "underline",
  "strike",
  "align",
  "size",
  "color",
  "script",
  "link",
  "codeblock",
]);

const EMBED_WHITELIST = new Set(["formula", "image"]);

const QUILL_COLORS = [
  "#000000",
  "#e60000",
  "#ff9900",
  "#ffff00",
  "#008a00",
  "#0066cc",
  "#9933ff",
  "#ffffff",
  "#facccc",
  "#ffebcc",
  "#ffffcc",
  "#cce8cc",
  "#cce0f5",
  "#ebd6ff",
  "#bbbbbb",
  "#f06666",
  "#ffc266",
  "#ffff66",
  "#66b966",
  "#66a3e0",
  "#c285ff",
  "#888888",
  "#a10000",
  "#b26b00",
  "#b2b200",
  "#006100",
  "#0047b2",
  "#6b24b2",
  "#444444",
  "#5c0000",
  "#663d00",
  "#666600",
  "#003700",
  "#002966",
  "#3d1466",
];

function sanitizeQuillOp(op: Op): Op | null {
  if (op.insert !== undefined) {
    let attributes = op.attributes;
    if (op.attributes) {
      attributes = {};
      for (const [key, value] of Object.entries(op.attributes)) {
        if (ATTRIBUTES_WHITELIST.has(key)) {
          attributes[key] = value;
        }
      }
    }
    if (typeof op.insert !== "string") {
      for (const [key, value] of Object.entries(op.insert)) {
        if (EMBED_WHITELIST.has(key)) {
          return { insert: { [key]: value }, attributes: attributes };
        }
      }
      return {};
    } else {
      return { insert: op.insert, attributes: attributes };
    }
  }
  return op;
}

export class TextEditorComponent extends Component<TextEditorSettings> {
  private quill: Quill;
  private quillWrap: HTMLDivElement;
  private previewContainer: HTMLDivElement;
  private codeContainer: HTMLTextAreaElement;
  private textVersion = 0;
  private domVersions = [0, 0, 0];
  private textHtml = ["", "", ""];
  private ctx: transforms.TextEditorContext;
  private tabsSettings: TabSelectSettings;

  constructor(settings: TextEditorSettings, parent: Component<unknown> | null, children?: jsx.Fragment[]) {
    super(settings, parent, children);

    const fileInput = (<input type="file" hidden accept="image/*" />).asElement() as HTMLInputElement;

    this.ctx = {
      color: [],
      size: [],
      uploadImage: this.settings.uploadImage,
      realMap: this.settings.realMap,
      fakeMap: this.settings.fakeMap,
    };
    this.tabsSettings = {
      selectedTab: 0,
      tabNames: ["Редактор", "Код", "Предпросмотр"],
      onTabSwitch: this.onTabSwitch.bind(this),
    };

    this.quillWrap = document.createElement("div");
    this.quillWrap.classList.add("ql-wrap");
    this.quillWrap.appendChild(document.createElement("div"));
    this.quillWrap.appendChild(fileInput);

    this.previewContainer = document.createElement("div");
    this.previewContainer.classList.add("preview-container");
    this.codeContainer = (
      <textarea class="code-container monospace" style="font-size: 14px;" />
    ).asElement() as HTMLTextAreaElement;
    this.codeContainer.addEventListener("focusout", () => {
      this.onTabSwitch(-1, EditorTabs.CODE);
    });
    this.quillWrap.children[0].addEventListener("focusout", () => {
      this.onTabSwitch(-1, EditorTabs.QUILL);
    });
    this.quill = new Quill(this.quillWrap.children[0], {
      modules: {
        syntax: false,
        toolbar: {
          container: [
            ["bold", "italic", "underline", "strike"],
            [
              { align: [false, "center", "right", "justify"] },
              { size: ["xsmall", "small", false, "large", "xlarge", "xxlarge"] },
            ],
            [{ color: QUILL_COLORS }],
            [{ script: "sub" }, { script: "super" }, "code-block"],
            ["image", "link", "formula"],
            ["clean"],
          ],
        },
        uploader: {
          handler: (range: RangeStatic, files: File[]): void => {
            const delta = new Delta().retain(range.index).delete(range.length);
            for (const file of files) {
              const id = this.settings.uploadImage(file);
              delta.insert({ image: this.settings.realMap?.getSecondary(id) });
            }
            this.quill.updateContents(delta, "user");
            this.quill.setSelection(range.index + files.length, 0, "silent");
          },
        },
        clipboard: {
          onConvert: (delta: Delta): Delta => {
            return new Delta(delta.map(sanitizeQuillOp).filter((value): value is Op => value !== null));
          },
        },
        keyboard: {
          bindings: {
            formula: {
              key: "=",
              altKey: true,
              handler: (): void => {
                (this.quill as any).theme.tooltip.edit("formula");
              },
            },
          },
        },
        history: {
          userOnly: true,
        },
      },
      placeholder: "",
      theme: "snow",
    });

    fileInput.addEventListener("change", async () => {
      if (fileInput.files?.length !== 1) {
        return;
      }
      const file = fileInput.files[0];
      fileInput.value = "";

      const id = await this.settings.uploadImage(file);
      const range = this.quill.getSelection(true);
      const update = new Delta()
        .retain(range.index)
        .delete(range.length)
        .insert({
          image: this.ctx.realMap?.getSecondary(id),
        });
      this.quill.updateContents(update, "user");
      this.quill.setSelection(range.index + 1, 0, "silent");
    });

    if (settings.text !== "") {
      this.textVersion = 1;
      this.textHtml[EditorTabs.PREVIEW] = settings.text;
      this.textHtml[EditorTabs.CODE] = transforms.previewToCode(settings.text, this.ctx);
      this.textHtml[EditorTabs.QUILL] = transforms.previewToQuill(settings.text, this.ctx);
      this.onTabSwitch(0, -1);
    }
  }

  onTabSwitch(index: number, oldIndex: number): void {
    if (oldIndex === EditorTabs.CODE) {
      const code = this.codeContainer.value;
      if (this.textHtml[oldIndex] !== code) {
        this.textHtml[oldIndex] = code;
        ++this.textVersion;
        const preview = (this.textHtml[EditorTabs.PREVIEW] = transforms.codeToPreview(code, this.ctx));
        this.settings.text = preview;
        this.textHtml[EditorTabs.QUILL] = transforms.previewToQuill(preview, this.ctx);
      }
    } else if (oldIndex === EditorTabs.QUILL) {
      let quill = (this.quill as any).getSemanticHTML() as string;
      if (quill.startsWith('<div class="ql-editor" contenteditable="true">') && quill.endsWith("</div>")) {
        quill = quill.substr(46, quill.length - 52);
      }
      if (this.textHtml[oldIndex] !== quill) {
        this.textHtml[oldIndex] = quill;
        ++this.textVersion;
        const code = (this.textHtml[EditorTabs.CODE] = transforms.quillToCode(quill, this.ctx).trimEnd());
        this.settings.text = this.textHtml[EditorTabs.PREVIEW] = transforms.codeToPreview(code, this.ctx);
      }
    }
    this.domVersions[oldIndex] = this.textVersion;

    if (this.domVersions[index] !== this.textVersion) {
      this.domVersions[index] = this.textVersion;
      if (index === EditorTabs.CODE) {
        this.codeContainer.value = this.textHtml[index];
      } else if (index === EditorTabs.PREVIEW) {
        this.previewContainer.innerHTML = this.textHtml[index];
        for (const elem of this.previewContainer.querySelectorAll("formula")) {
          katex.render((elem as HTMLElement).innerText, elem as HTMLElement, { throwOnError: false });
        }
      } else if (index === EditorTabs.QUILL) {
        this.quill.setContents(this.quill.clipboard.convert({ html: this.textHtml[index], text: "\n" }));
      }
    }

    if (index !== -1 && oldIndex !== -1 && oldIndex !== index) {
      if (index === EditorTabs.QUILL) {
        this.quill.focus();
      } else if (index === EditorTabs.CODE) {
        this.codeContainer.focus();
      }
    }
  }

  onImageRemoval(): void {
    this.onTabSwitch(-1, this.tabsSettings.selectedTab);

    const curr = transforms.updateImages(this.textHtml[EditorTabs.PREVIEW], this.ctx);
    if (this.textHtml[EditorTabs.PREVIEW] !== curr) {
      ++this.textVersion;
      this.textHtml[EditorTabs.PREVIEW] = this.settings.text = curr;
      this.textHtml[EditorTabs.QUILL] = transforms.previewToQuill(curr, this.ctx);
      this.textHtml[EditorTabs.CODE] = transforms.previewToCode(curr, this.ctx);
    }

    this.onTabSwitch(this.tabsSettings.selectedTab, -1);
  }

  createElement(): HTMLElement {
    return (
      <TabSelect settings={this.tabsSettings}>
        {this.quillWrap}
        {this.codeContainer}
        {this.previewContainer}
      </TabSelect>
    ).asElement() as HTMLElement;
  }

  flush(): void {
    this.onTabSwitch(this.tabsSettings.selectedTab, this.tabsSettings.selectedTab);
  }
}

export const TextEditor = createComponentFactory(TextEditorComponent);

import Quill from "quill";
import katex from "katex";

import { Component, createComponentFactory } from "./component";
import { TabSelect } from "./tab-select";
import * as jsx from "../utils/jsx";

enum EditorTabs {
  QUILL = 0,
  CODE = 1,
  PREVIEW = 2,
}

type TraverseContext = { color: string[]; size: string[] };
const defaultContext: TraverseContext = { color: [], size: [] };
type TraverseElement = HTMLElement | Text;

type TraverseInHandler = (elem: HTMLElement, ctx: TraverseContext) => boolean | void;
type TraverseOutHandler = (elem: HTMLElement, children: TraverseElement[], ctx: TraverseContext) => TraverseElement[];
type TraverseHandler = [TraverseInHandler, TraverseOutHandler];

type TraverseTextInHandler = (elem: Text, ctx: TraverseContext) => boolean | void;
type TraverseTextOutHandler = (elem: Text, children: TraverseElement[], ctx: TraverseContext) => TraverseElement[];
type TraverseTextHandler = [TraverseTextInHandler, TraverseTextOutHandler];

type TraverseRule = [string, TraverseHandler] | [string, TraverseHandler, boolean | number] | [0, TraverseTextHandler];
type TraverseRules = TraverseRule[];

const ALLOWED_TAGS = ["a", "b", "i", "s", "u", "div", "img", "font", "sub", "sup", "br", "formula", "pre"];
const ALLOWED_CLOSE_TAGS = new Set(ALLOWED_TAGS.map((elem) => "/" + elem));
ALLOWED_CLOSE_TAGS.delete("/img");
const ALLOWED_COLORS = new Set(["black", "red", "yellow", "green", "blue", "gray", "white"]);

const CODE_TO_PREVIEW_RULES: TraverseRules = [
  [0, useText()], // 0 is a selector for text nodes
  ["br", useBrInPreview()],
  ["b, i, u, s, sub, sup, pre", useTagWithoutAttributes()],
  ["a", useTagWithAttributes(["href"])],
  ["font", useFontInPreview()],
  ["img", useImageSanitized()],
  ["div[align=center], div[align=left], div[align=right], div[align=justify]", useTagWithAttributes(["align"])],
  ["formula", useTagWithoutAttributes()],
];

const PREVIEW_TO_CODE_RULES: TraverseRules = [
  [0, useText()],
  ["br", insertNewLine()],
  ["div, pre", useDivInCode()],
  ["*", useTag()],
];

const PREVIEW_TO_QUILL_RULES: TraverseRules = [
  [0, useTextInQuill()],
  ["br", useTagWithoutAttributes()],
  ["b, i, u, s, sub, sup, pre", useTagWithoutAttributes()],
  ["a", useTagWithAttributes(["href"])],
  ["img", useTagWithAttributes(["src"])],
  ["font", useFontInQuill()],
  ["div[align]", useAlignInQuill()],
  ["formula", useFormulaInQuill()],
];

const PREVIEW_TO_QUILL_SIZE = new Map([
  ["1", "ql-size-xsmall"],
  ["2", "ql-size-small"],
  ["4", "ql-size-large"],
  ["5", "ql-size-xlarge"],
  ["6", "ql-size-xxlarge"],
]);

const PREVIEW_TO_QUILL_ALIGN = new Map([
  ["center", "ql-align-center"],
  ["right", "ql-align-right"],
  ["justify", "ql-align-justify"],
]);

const QUILL_TO_CODE_RULES: TraverseRules = [
  [0, useText()],
  [".ql-size-xsmall", wrapInto("font", [["size", "1"]]), 1], // last `1` means that we want to go on applying rules
  [".ql-size-small", wrapInto("font", [["size", "2"]]), 1],
  [".ql-size-large", wrapInto("font", [["size", "4"]]), 1],
  [".ql-size-xlarge", wrapInto("font", [["size", "5"]]), 1],
  [".ql-size-xxlarge", wrapInto("font", [["size", "6"]]), 1],
  [".ql-align-center", wrapInto("div", [["align", "center"]]), 1],
  [".ql-align-right", wrapInto("div", [["align", "right"]]), 1],
  [".ql-align-justify", wrapInto("div", [["align", "justify"]]), 1],
  ["[style]", useStyleInCode(), 1], // finished with inline styles
  ["u, s, sup, sub", useTagWithoutAttributes()],
  ["strong", wrapInto("b", [])],
  ["em", wrapInto("i", [])],
  ["a", useTagWithAttributes(["href"])],
  ["img", useTagWithAttributes(["src"])],
  ["div.ql-code-block", wrapInto("pre", [])],
  ["pre", useTagWithoutAttributes()],
  ["p", insertNewLine()],
  ["formula", useFormulaInCode()],
];

function cloneTag(elem: HTMLElement, children: TraverseElement[]): HTMLElement {
  let res = document.createElement(elem.tagName);
  for (let child of children) res.appendChild(child);
  return res;
}

function useTag(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): [HTMLElement] => {
      let res = cloneTag(elem, children);
      for (let attr of elem.attributes) res.setAttribute(attr.name, attr.value);
      return [res];
    },
  ];
}

function useTagWithoutAttributes(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): [HTMLElement] => {
      return [cloneTag(elem, children)];
    },
  ];
}

function useTagWithAttributes(attrs: string[]): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): [HTMLElement] => {
      let res = cloneTag(elem, children);
      for (let attr of attrs) {
        const value = elem.getAttribute(attr);
        if (value !== null) {
          res.setAttribute(attr, value);
        }
      }
      return [res];
    },
  ];
}

function isElementCreatingBr(elem: any): boolean {
  if (!(elem instanceof HTMLDivElement || elem instanceof HTMLPreElement)) {
    return false;
  }
  if (elem.classList.length) {
    return true;
  }
  for (let c of elem.innerText) {
    if (c !== " ") {
      return true;
    }
  }
  return false;
}

function useBrInPreview(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): HTMLElement[] => {
      if (isElementCreatingBr(elem.previousSibling)) return []; // don't want <br> tag in situations like "</div> <ENTER> 123"
      return [document.createElement("br")];
    },
  ];
}

function useDivInCode(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): TraverseElement[] => {
      let res = cloneTag(elem, children);
      for (let attr of elem.attributes) res.setAttribute(attr.name, attr.value);
      if (isElementCreatingBr(elem)) return [res, new Text("\n")];
      return [res];
    },
  ];
}

function useImageSanitized(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): HTMLElement[] => {
      let src = elem.getAttribute("src");
      if (src !== null && /^f:\/\/[0-9a-f]{8,8}$/.test(src)) {
        let res = document.createElement("img");
        res.setAttribute("src", src);
        return [res];
      }
      return []; // throwing away images with bad links
    },
  ];
}

function useImageOnDelete(url: string): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): HTMLElement[] => {
      let src = elem.getAttribute("src");
      if (src !== null && src !== url) {
        let res = document.createElement("img");
        res.setAttribute("src", src);
        return [res];
      }
      return [];
    },
  ];
}

function useFontInPreview(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): HTMLElement[] => {
      let res = cloneTag(elem, children);
      let color = elem.getAttribute("color");
      if (color && ALLOWED_COLORS.has(color)) res.setAttribute("color", color);
      let size = elem.getAttribute("size");
      if (size && PREVIEW_TO_QUILL_SIZE.has(size)) res.setAttribute("size", size);
      return [res];
    },
  ];
}

function dropTag(): TraverseHandler {
  return [() => {}, (elem, children, ctx): TraverseElement[] => children];
}

function useText(): TraverseTextHandler {
  return [() => {}, (elem, children, ctx): [Text] => [elem.cloneNode(true) as Text]];
}

function useFontInQuill(): TraverseHandler {
  return [
    (elem, ctx) => {
      let ccolor = ctx.color[ctx.color.length - 1],
        csize = ctx.size[ctx.size.length - 1];
      const ncolor = elem.getAttribute("color"),
        nsize = elem.getAttribute("size");
      if (ncolor) {
        ccolor = ncolor;
      }
      if (nsize) {
        csize = nsize;
      }
      ctx.color.push(ccolor);
      ctx.size.push(csize);
    },
    (elem, children, ctx) => {
      ctx.color.pop();
      ctx.size.pop();
      return children;
    },
  ];
}

function useTextInQuill(): TraverseTextHandler {
  return [
    () => {},
    (elem, children, ctx): [TraverseElement] => {
      let ccolor = ctx.color[ctx.color.length - 1],
        csize = ctx.size[ctx.size.length - 1];
      if (ccolor !== "black" || csize !== "3") {
        let res = document.createElement("span");
        if (ccolor !== "black") res.setAttribute("style", "color: " + ccolor + ";");
        if (csize !== "3") res.classList.add(PREVIEW_TO_QUILL_SIZE.get(csize)!);
        res.innerText = elem.data;
        return [res];
      }
      return [elem.cloneNode(true) as Text];
    },
  ];
}

function useAlignInQuill(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): [HTMLElement] => {
      let res = document.createElement("div");
      let calign = elem.getAttribute("align")!;
      const nalign = PREVIEW_TO_QUILL_ALIGN.get(calign);
      if (nalign) {
        res.setAttribute("class", nalign);
      }
      for (let child of children) res.appendChild(child);
      return [res];
    },
  ];
}

function wrapInto(tag: string, attrs: [string, string][]): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): TraverseElement[] => {
      let res = document.createElement(tag);
      for (let child of children) res.appendChild(child);
      for (let attr of attrs) res.setAttribute(attr[0], attr[1]);
      if (tag !== "div" || elem.tagName === "P") return [res];
      else return [res, new Text("\n")];
    },
  ];
}

function useStyleInCode(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): TraverseElement[] => {
      let cstyle = elem.getAttribute("style")!;
      let match,
        color = "black";
      if ((match = cstyle.match(/color: ([a-z]+);/))) color = match[1];
      if (color !== "black") {
        let res = document.createElement("font");
        res.setAttribute("color", color);
        for (let child of children) res.appendChild(child);
        return [res];
      } else {
        return children;
      }
    },
  ];
}

function insertNewLine(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): TraverseElement[] => {
      children.push(new Text("\n"));
      return children;
    },
  ];
}

const FORMULA_MAGIC = "H16YohxG="; // our Quill fork identifies formulas by this prefix

function useFormulaInQuill(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): [HTMLElement] => {
      let res = document.createElement("formula");
      res.setAttribute("data-value", FORMULA_MAGIC + elem.innerText);
      return [res];
    },
  ];
}

function useFormulaInCode(): TraverseHandler {
  return [
    () => {},
    (elem, children, ctx): [HTMLElement] => {
      let res = document.createElement("formula");
      res.innerText = elem.getAttribute("data-value")!.substr(FORMULA_MAGIC.length);
      return [res];
    },
  ];
}

function traverse(
  elem: DocumentFragment | TraverseElement,
  rules: TraverseRules,
  ctx: TraverseContext
): TraverseElement[] {
  if (elem instanceof DocumentFragment) {
    let node = document.createElement("div");
    for (let child of elem.childNodes) {
      for (let curr of traverse(child as HTMLElement, rules, ctx)) {
        node.appendChild(curr);
      }
    }
    return [node];
  }
  for (let rule of rules) {
    if (elem instanceof Text) {
      if (rule[0] === 0 && !rule[1][0](elem, ctx)) {
        break;
      }
    } else if (rule[0] !== 0 && elem.matches(rule[0])) {
      if (!rule[1][0](elem, ctx)) {
        break;
      }
    }
  }
  let children = [];
  for (let child of elem.childNodes) {
    for (let res of traverse(child as TraverseElement, rules, ctx)) {
      children.push(res);
    }
  }
  for (let rule of rules) {
    let res: TraverseElement[] | undefined = undefined;
    if (elem instanceof Text) {
      if (rule[0] === 0) {
        res = rule[1][1](elem, children, ctx);
      }
    } else if (rule[0] !== 0 && elem.matches(rule[0])) {
      res = rule[1][1](elem, children, ctx);
    }
    if (res) {
      if (rule.length > 2 && rule[2]) {
        children = res;
      } else {
        return res;
      }
    }
  }
  return children;
}

function codeToPreview(data: string): string {
  // Dealing with possibly unmatched '<' & '>', checking tag names
  let lastOpen = -1;
  let tagBuffer = [];
  let sanitizedData = [];

  for (let i = 0; i <= data.length; ++i) {
    let c = data[i];
    if (i == data.length) c = "<";

    if (c == "<") {
      let tag = tagBuffer.join("");
      if (lastOpen != -1) {
        sanitizedData.push("&lt;");
        sanitizedData.push(tag);
        tagBuffer = [];
      }
      lastOpen = i;
    } else if (c == ">") {
      let tag = tagBuffer.join("");
      if (lastOpen != -1) {
        if (ALLOWED_CLOSE_TAGS.has(tag)) {
          sanitizedData.push(`<${tag}>`);
        } else {
          let flag = false;
          for (const allowed_tag of ALLOWED_TAGS) {
            if (tag.startsWith(allowed_tag + " ") || tag == allowed_tag) {
              flag = true;
              break;
            }
          }
          if (!flag) {
            sanitizedData.push(`&lt;${tag}&gt;`);
          } else {
            sanitizedData.push(`<${tag}>`);
          }
        }
        tagBuffer = [];
      } else {
        sanitizedData.push("&gt;");
      }
      lastOpen = -1;
    } else {
      if (lastOpen == -1) {
        sanitizedData.push(c);
      } else {
        tagBuffer.push(c);
      }
    }
  }
  let sanitizedDataStr = sanitizedData.join("").trim();

  // Ensuring XML/HTML structure by creating template element (note it won't cause JS listeners to be run)
  let template = document.createElement("template");
  template.innerHTML = sanitizedDataStr.replaceAll("\n", "<br>");

  // Removing any strange attributes
  return (traverse(template.content, CODE_TO_PREVIEW_RULES, defaultContext)[0] as HTMLElement).innerHTML;
}

function previewToQuill(data: string): string {
  let template = document.createElement("template");
  template.innerHTML = data;

  // Actually, we are not creating 100% compatible HTML here but hope that Quill is smart enough to handle it
  return (traverse(template.content, PREVIEW_TO_QUILL_RULES, { size: ["3"], color: ["black"] })[0] as HTMLElement)
    .innerHTML;
}

function quillToCode(data: string) {
  let template = document.createElement("template");
  template.innerHTML = data;

  return (traverse(template.content, QUILL_TO_CODE_RULES, defaultContext)[0] as HTMLElement).innerHTML;
}

function previewToCode(data: string) {
  let template = document.createElement("template");
  template.innerHTML = data;

  return (traverse(template.content, PREVIEW_TO_CODE_RULES, defaultContext)[0] as HTMLElement).innerHTML;
}

// function deleteImageOccurrences(url) {
//   const RULES = [
//     [0, useText()],
//     ["img", useImageOnDelete(url)],
//     ["*", useTag()]
//   ];

//   ON_HIDE[currentLayout]();

//   let template = document.createElement("template");
//   template.innerHTML = htmlPreview;
//   template = traverse(template.content, RULES, {})[0];
//   let curr = template.innerHTML;
//   if (htmlPreview !== curr) {
//     ++htmlVersion;
//     htmlPreview = curr;
//     htmlQuill = previewToQuill(htmlPreview);
//     htmlCode = previewToCode(htmlPreview);
//   }

//   ON_SHOW[currentLayout]();
// }

export class TextEditorComponent extends Component<{text: string}> {
  private quill: Quill;
  private quillWrap: HTMLDivElement;
  private previewContainer: HTMLDivElement;
  private codeContainer: HTMLTextAreaElement;
  private textVersion = 0;
  private domVersions = [0, 0, 0];
  private textHtml = ["", "", ""];

  constructor(settings: {text: string}, parent: Component<unknown> | null, children?: jsx.Fragment[]) {
    super(settings, parent, children);

    this.quillWrap = document.createElement("div");
    this.quillWrap.classList.add("ql-wrap");
    this.quillWrap.innerHTML = `<div></div>`;

    this.previewContainer = document.createElement("div");
    this.previewContainer.classList.add("preview-container");
    this.codeContainer = (
      <textarea id="code-container" class="tab-textarea monospace" style="font-size: 14px;" />
    ).asElement() as HTMLTextAreaElement;
    this.codeContainer.addEventListener("focusout", () => {
      this.onTabSwitch(-1, EditorTabs.CODE);
    });
    this.quillWrap.children[0].addEventListener("focusout", () => {
      this.onTabSwitch(-1, EditorTabs.QUILL);
    })
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
            [{ color: Array.from(ALLOWED_COLORS) }],
            [{ script: "sub" }, { script: "super" }, "code-block"],
            ["image", "link", "formula"],
            ["clean"],
          ],
          handlers: {
            image: async (): Promise<void> => {
              // QI.insertEmbed(QI.getSelection().index, "image", url);
            },
          },
        },
        clipboard: {
          onPostPaste: (e: ClipboardEvent): void => {
            if (!e.clipboardData) {
              return;
            }
            // if we pasted content from quill itself, do nothing
            if (e.clipboardData.getData("meta/origin") === "quill") {
              return;
            }
            let selection = this.quill.getSelection();
            let html = previewToQuill(codeToPreview(quillToCode((this.quill as any).getSemanticHTML())));
            this.quill.setContents(this.quill.clipboard.convert({ html: html, text: "\n" }));
            // FIXME selection will change unexpectedly if pasted text contains forbidden tags
            if (selection) {
              this.quill.setSelection(selection);
            }
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
        }
      },
      placeholder: "",
      theme: "snow",
    });

    if (settings.text !== "") {
      this.textVersion = 1;
      this.textHtml[EditorTabs.PREVIEW] = settings.text;
      this.textHtml[EditorTabs.CODE] = previewToCode(settings.text);
      this.textHtml[EditorTabs.QUILL] = previewToQuill(settings.text);
      this.onTabSwitch(0, -1);
    }
  }

  onTabSwitch(index: number, oldIndex: number) {
    if (oldIndex === EditorTabs.CODE) {
      const code = this.codeContainer.value;
      if (this.textHtml[oldIndex] !== code) {
        this.textHtml[oldIndex] = code;
        ++this.textVersion;
        const preview = (this.textHtml[EditorTabs.PREVIEW] = codeToPreview(code));
        this.settings.text = preview;
        this.textHtml[EditorTabs.QUILL] = previewToQuill(preview);
      }
    } else if (oldIndex === EditorTabs.QUILL) {
      let quill = (this.quill as any).getSemanticHTML() as string;
      if (quill.startsWith('<div class="ql-editor" contenteditable="true">') && quill.endsWith("</div>")) {
        quill = quill.substr(46, quill.length - 52);
      }
      if (this.textHtml[oldIndex] !== quill) {
        this.textHtml[oldIndex] = quill;
        ++this.textVersion;
        const code = (this.textHtml[EditorTabs.CODE] = quillToCode(quill).trimEnd());
        this.settings.text = this.textHtml[EditorTabs.PREVIEW] = codeToPreview(code);
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

    if (index !== -1 && oldIndex !== -1) {
      if (index === EditorTabs.QUILL) {
        this.quill.focus();
      } else if (index === EditorTabs.CODE) {
        this.codeContainer.focus();
      }
    }
  }

  createElement(): HTMLElement {
    return (
      <TabSelect
        settings={{
          selectedTab: 0,
          tabNames: ["Редактор", "Код", "Предпросмотр"],
          onTabSwitch: this.onTabSwitch.bind(this),
        }}
      >
        {this.quillWrap}
        {this.codeContainer}
        {this.previewContainer}
      </TabSelect>
    ).asElement() as HTMLElement;
  }
}

export const TextEditor = createComponentFactory(TextEditorComponent);

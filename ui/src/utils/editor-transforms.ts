/* eslint-disable @typescript-eslint/explicit-function-return-type */
import BidirectionalMap from "utils/bidirectional-map";
import {
  traverse,
  TraverseElement,
  TraverseHandler,
  TraverseRules,
  TraverseTextHandler,
} from "utils/traverse";

export type TextEditorContext = {
  color: string[];
  size: string[];
  uploadImage?: (file: File) => Promise<number>;
  realMap?: BidirectionalMap<number, string>;
  fakeMap?: BidirectionalMap<number, string>;
};

type EditorRules = TraverseRules<TextEditorContext>;
type EditorHandler = TraverseHandler<TextEditorContext>;
type EditorTextHandler = TraverseTextHandler<TextEditorContext>;

const ALLOWED_TAGS = [
  "a",
  "b",
  "i",
  "s",
  "u",
  "div",
  "img",
  "font",
  "sub",
  "sup",
  "br",
  "formula",
  "pre",
];
const ALLOWED_CLOSE_TAGS = new Set(ALLOWED_TAGS.map((elem) => "/" + elem));
ALLOWED_CLOSE_TAGS.delete("/img");

const CODE_TO_PREVIEW_RULES: EditorRules = [
  [0, useText()], // 0 is a selector for text nodes
  ["br", useBrInPreview()],
  ["b, i, u, s, sub, sup, pre", useTagWithoutAttributes()],
  ["a", useTagWithAttributes(["href"])],
  ["font", useFontInPreview()],
  ["img", useImageInPreview()],
  [
    "div[align=center], div[align=left], div[align=right], div[align=justify]",
    useTagWithAttributes(["align"]),
  ],
  ["formula", useTagWithoutAttributes()],
];

const PREVIEW_TO_CODE_RULES: EditorRules = [
  [0, useText()],
  ["br", insertNewLine()],
  ["img", useImageInCode()],
  ["div, pre", useDivInCode()],
  ["*", useTag()],
];

const PREVIEW_TO_QUILL_RULES: EditorRules = [
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

const QUILL_TO_CODE_RULES: EditorRules = [
  [0, useText()],
  // last `1` means that we want to go on applying rules
  [".ql-size-xsmall", wrapInto("font", [["size", "1"]]), 1],
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
  ["img", useImageInCode()],
  ["div.ql-code-block", wrapInto("pre", [])],
  ["pre", useTagWithoutAttributes()],
  ["p", insertNewLine()],
  ["formula", useFormulaInCode()],
];

const IMAGE_UPDATE_RULES: EditorRules = [
  [0, useText()],
  ["img", useImageInPreview()],
  ["*", useTag()],
];

function cloneTag(elem: HTMLElement, children: TraverseElement[]): HTMLElement {
  const res = document.createElement(elem.tagName);
  for (const child of children) res.appendChild(child);
  return res;
}

function useTag(): EditorHandler {
  return [
    () => {},
    (elem, children): [HTMLElement] => {
      const res = cloneTag(elem, children);
      for (const attr of elem.attributes) res.setAttribute(attr.name, attr.value);
      return [res];
    },
  ];
}

function useTagWithoutAttributes(): EditorHandler {
  return [
    () => {},
    (elem, children): [HTMLElement] => {
      return [cloneTag(elem, children)];
    },
  ];
}

function useTagWithAttributes(attrs: string[]): EditorHandler {
  return [
    () => {},
    (elem, children): [HTMLElement] => {
      const res = cloneTag(elem, children);
      for (const attr of attrs) {
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
  for (const c of elem.innerText) {
    if (c !== " ") {
      return true;
    }
  }
  return false;
}

function useBrInPreview(): EditorHandler {
  return [
    () => {},
    (elem): HTMLElement[] => {
      // don't want <br> tag in situations like "</div> <ENTER> 123"
      if (isElementCreatingBr(elem.previousSibling)) return [];
      return [document.createElement("br")];
    },
  ];
}

function useDivInCode(): EditorHandler {
  return [
    () => {},
    (elem, children): TraverseElement[] => {
      const res = cloneTag(elem, children);
      for (const attr of elem.attributes) res.setAttribute(attr.name, attr.value);
      if (isElementCreatingBr(elem)) return [res, new Text("\n")];
      return [res];
    },
  ];
}

function useImageInPreview(): EditorHandler {
  return [
    () => {},
    (elem, children, ctx): HTMLElement[] => {
      const src = elem.getAttribute("src") ?? "";
      const id = ctx.realMap?.getPrimary(src) ?? ctx.fakeMap?.getPrimary(src);
      if (id === undefined) {
        return []; // throwing away images with bad links
      }

      const res = document.createElement("img");
      res.src = ctx.realMap?.getSecondary(id) ?? "";
      res.dataset.id = id.toString();
      return [res];
    },
  ];
}

function useImageInCode(): EditorHandler {
  return [
    () => {},
    (elem, children, ctx): HTMLElement[] => {
      const src = elem.getAttribute("src") ?? "";
      const id = ctx.realMap?.getPrimary(src) ?? ctx.fakeMap?.getPrimary(src);
      if (id === undefined) {
        return [];
      }

      const res = document.createElement("template");
      res.innerHTML = `<img src="${ctx.fakeMap?.getSecondary(id) ?? ""}">`;
      const img = res.content.children[0].cloneNode(true) as HTMLImageElement;
      return [img];
    },
  ];
}

function useFontInPreview(): EditorHandler {
  return [
    () => {},
    (elem, children): HTMLElement[] => {
      const res = cloneTag(elem, children);
      const color = elem.getAttribute("color");
      if (color) res.setAttribute("color", color);
      const size = elem.getAttribute("size");
      if (size && PREVIEW_TO_QUILL_SIZE.has(size)) res.setAttribute("size", size);
      return [res];
    },
  ];
}

function useText(): EditorTextHandler {
  return [() => {}, (elem): [Text] => [elem.cloneNode(true) as Text]];
}

function useFontInQuill(): EditorHandler {
  return [
    (elem, ctx) => {
      let ccolor = ctx.color[ctx.color.length - 1];
      let csize = ctx.size[ctx.size.length - 1];
      const ncolor = elem.getAttribute("color");
      const nsize = elem.getAttribute("size");
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

function useTextInQuill(): EditorTextHandler {
  return [
    () => {},
    (elem, children, ctx): [TraverseElement] => {
      const ccolor = ctx.color[ctx.color.length - 1];
      const csize = ctx.size[ctx.size.length - 1];
      if (ccolor !== "black" || csize !== "3") {
        const res = document.createElement("span");
        if (ccolor !== "black") res.setAttribute("style", "color: " + ccolor + ";");
        if (csize !== "3") res.classList.add(PREVIEW_TO_QUILL_SIZE.get(csize)!);
        res.innerText = elem.data;
        return [res];
      }
      return [elem.cloneNode(true) as Text];
    },
  ];
}

function useAlignInQuill(): EditorHandler {
  return [
    () => {},
    (elem, children): [HTMLElement] => {
      const res = document.createElement("div");
      const calign = elem.getAttribute("align")!;
      const nalign = PREVIEW_TO_QUILL_ALIGN.get(calign);
      if (nalign) {
        res.setAttribute("class", nalign);
      }
      for (const child of children) res.appendChild(child);
      return [res];
    },
  ];
}

function wrapInto(tag: string, attrs: [string, string][]): EditorHandler {
  return [
    () => {},
    (elem, children): TraverseElement[] => {
      const res = document.createElement(tag);
      for (const child of children) res.appendChild(child);
      for (const attr of attrs) res.setAttribute(attr[0], attr[1]);
      if (tag !== "div" || elem.tagName === "P") return [res];
      else return [res, new Text("\n")];
    },
  ];
}

function getColorFromStyle(style: string): string | null {
  let match;
  if (!(match = style.match(/color: ([^;]+);/))) {
    return null;
  }
  let color = match[1];
  if (!color.startsWith("rgb(")) {
    return color;
  }
  color = color.replace(/^[^\d]+/, "").replace(/[^\d]+$/, "");
  const hex = color
    .split(",")
    .map((component) => `00${parseInt(component, 10).toString(16)}`.slice(-2))
    .join("");
  return `#${hex}`;
}

function useStyleInCode(): EditorHandler {
  return [
    () => {},
    (elem, children): TraverseElement[] => {
      const color = getColorFromStyle(elem.getAttribute("style")!);
      if (color && color !== "black" && color !== "#000000") {
        const res = document.createElement("font");
        res.setAttribute("color", color);
        for (const child of children) res.appendChild(child);
        return [res];
      } else {
        return children;
      }
    },
  ];
}

function insertNewLine(): EditorHandler {
  return [
    () => {},
    (elem, children): TraverseElement[] => {
      children.push(new Text("\n"));
      return children;
    },
  ];
}

const FORMULA_MAGIC = "H16YohxG="; // our Quill fork identifies formulas by this prefix

function useFormulaInQuill(): EditorHandler {
  return [
    () => {},
    (elem): [HTMLElement] => {
      const res = document.createElement("formula");
      res.setAttribute("data-value", FORMULA_MAGIC + elem.innerText);
      return [res];
    },
  ];
}

function useFormulaInCode(): EditorHandler {
  return [
    () => {},
    (elem): [HTMLElement] => {
      const res = document.createElement("formula");
      res.innerText = elem.getAttribute("data-value")!.substr(FORMULA_MAGIC.length);
      return [res];
    },
  ];
}

export function codeToPreview(data: string, ctx: TextEditorContext): string {
  // Dealing with possibly unmatched '<' & '>', checking tag names
  let lastOpen = -1;
  let tagBuffer = [];
  const sanitizedData = [];

  for (let i = 0; i <= data.length; ++i) {
    let c = data[i];
    if (i == data.length) c = "<";

    if (c == "<") {
      const tag = tagBuffer.join("");
      if (lastOpen != -1) {
        sanitizedData.push("&lt;");
        sanitizedData.push(tag);
        tagBuffer = [];
      }
      lastOpen = i;
    } else if (c == ">") {
      const tag = tagBuffer.join("");
      if (lastOpen != -1) {
        if (ALLOWED_CLOSE_TAGS.has(tag)) {
          sanitizedData.push(`<${tag}>`);
        } else {
          let flag = false;
          for (const allowedTag of ALLOWED_TAGS) {
            if (tag.startsWith(allowedTag + " ") || tag == allowedTag) {
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
  const sanitizedDataStr = sanitizedData.join("").trim();

  // Ensuring XML/HTML structure by creating template element
  // (note it won't cause JS listeners to be run)
  const template = document.createElement("template");
  template.innerHTML = sanitizedDataStr.replaceAll("\n", "<br>");

  // Removing any strange attributes
  return traverse(template.content, CODE_TO_PREVIEW_RULES, ctx)[0].innerHTML;
}

export function previewToQuill(data: string, ctx: TextEditorContext): string {
  const template = document.createElement("template");
  template.innerHTML = data;

  ctx = Object.assign(ctx, { size: ["3"], color: ["black"] });

  // Actually, we are not creating 100% compatible HTML here
  // but hope that Quill is smart enough to handle it
  return traverse(template.content, PREVIEW_TO_QUILL_RULES, ctx)[0].innerHTML;
}

export function quillToCode(data: string, ctx: TextEditorContext) {
  const template = document.createElement("template");
  template.innerHTML = data;

  return traverse(template.content, QUILL_TO_CODE_RULES, ctx)[0].innerHTML;
}

export function previewToCode(data: string, ctx: TextEditorContext) {
  const template = document.createElement("template");
  template.innerHTML = data;

  return traverse(template.content, PREVIEW_TO_CODE_RULES, ctx)[0].innerHTML;
}

export function updateImages(data: string, ctx: TextEditorContext) {
  const template = document.createElement("template");
  template.innerHTML = data;

  return traverse(template.content, IMAGE_UPDATE_RULES, ctx)[0].innerHTML;
}

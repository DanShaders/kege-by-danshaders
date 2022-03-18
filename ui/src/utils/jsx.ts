import { Component, ComponentFactory } from "../components/component";
import { nonNull } from "./assert";

export declare namespace JSX {
  type Element = Fragment;
  interface IntrinsicElements extends IntrinsicElementMap {}

  type IntrinsicElementMap = {
    [K in keyof ElementTagNameMap]: {
      [k: string]: any;
    };
  };

  type Tag = keyof JSX.IntrinsicElements;
}

type Attributes = { [key: string]: string | boolean };

const XML_NS_XHTML = "http://www.w3.org/1999/xhtml";
const XML_NS_SVG = "http://www.w3.org/2000/svg";
const XML_NS_PREFIXES: Map<string, string> = new Map([
  ["xlink", "http://www.w3.org/1999/xlink"],
]);

export class Fragment {
  private tag: JSX.Tag | null;
  private attributes: Attributes;
  private children: Node[];
  private comp?: Component<unknown>;
  private isRef: boolean;

  constructor(tag: JSX.Tag | null, attributes: Attributes, children: Node[], comp?: Component<unknown>) {
    this.tag = tag;
    this.attributes = attributes;
    this.children = children;
    this.comp = comp;
    this.isRef = attributes["ref"] !== undefined;
    delete attributes["ref"];
  }

  static fromComponent(attrs: Attributes, comp: Component<unknown>) {
    return new Fragment(null, attrs, [], comp);
  }

  insertInto(elem: Element, inSvg: boolean = false, refs: any[] = []): any[] {
    if (this.comp) {
      elem.appendChild(this.comp.elem);
      if (this.isRef) {
        refs.push(this.comp);
      }
      return refs;
    }

    let place: Element = elem;
    if (this.tag) {
      if (this.tag === "svg") {
        inSvg = true;
      }
      place = document.createElementNS(inSvg ? XML_NS_SVG : XML_NS_XHTML, this.tag);
      if (this.isRef) {
        refs.push(place);
      }
      elem.appendChild(place);
      for (const [prop, value] of Object.entries(this.attributes ?? {})) {
        let namespace: string | null = null, propName = prop;
        if (prop.indexOf(":") !== -1) {
          [namespace, propName] = prop.split(":", 2);
          namespace = XML_NS_PREFIXES.get(namespace) ?? null;
        }
        if (typeof value === "boolean" && propName.endsWith("If")) {
            if (value) {
                place.setAttributeNS(namespace, propName.substr(0, propName.length - 2), "");
            }
        } else {
            place.setAttributeNS(namespace, propName, value.toString());
        }
      }
    }
    for (const child of this.children) {
        if (typeof child === "string") {
          place.appendChild(new Text(child));
        } else if (child instanceof Fragment) {
          child.insertInto(place, inSvg, refs);
        } else {
          throw Error("Unknown children type");
        }
      }
    return refs;
  }

  replaceContentsOf(elem: Element | string): any[] {
    if (typeof elem === "string") {
      elem = nonNull(document.getElementById(elem));
    }
    elem.innerHTML = "";
    return this.insertInto(elem);
  }
}

export function jsx(
  tag: JSX.Tag | typeof Fragment | ComponentFactory<unknown>,
  attributes: Attributes | null,
  ...children: Node[]
): Fragment {
  if (tag === Fragment) {
    return new Fragment(null, {}, children);
  } else if (typeof tag === "function") {
    return (tag as ComponentFactory<unknown>)((attributes ?? {}) as any);
  } else {
    return new Fragment(tag, attributes ?? {}, children);
  }
}

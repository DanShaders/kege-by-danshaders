import { assert, nonNull } from "utils/assert";

import { Component, ComponentFactory } from "components/component";

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

type Attributes = { [key: string]: string | boolean | ((e: Event) => void) };

const XML_NS_XHTML = "http://www.w3.org/1999/xhtml";
const XML_NS_SVG = "http://www.w3.org/2000/svg";
const XML_NS_PREFIXES: Map<string, string> = new Map([["xlink", "http://www.w3.org/1999/xlink"]]);

export class Fragment {
  private tag: JSX.Tag | null;
  private attributes: Attributes;
  private children: (Fragment | Node | Fragment[])[];
  comp?: Component<unknown>;
  private isRef: boolean;
  private elem?: Element;

  constructor(
    tag: JSX.Tag | null,
    attributes: Attributes,
    children: (Fragment | Node | Fragment[])[],
    comp?: Component<unknown>
  ) {
    this.tag = tag;
    this.attributes = attributes;
    this.children = children;
    this.comp = comp;
    this.isRef = attributes["ref"] !== undefined;
    delete attributes["ref"];
  }

  static fromComponent(attrs: Attributes, comp: Component<unknown>): Fragment {
    return new Fragment(null, attrs, [], comp);
  }

  insertInto(elem: Element | null, inSvg: boolean = false, refs: any[] = []): any[] {
    if (this.attributes["removedIf"] === true) {
      return refs;
    }
    if (this.comp) {
      if (this.isRef) {
        refs.push(this.comp);
      }
      if (elem) {
        elem.appendChild(this.comp.elem);
      } else {
        this.elem = this.comp.elem;
      }
      return refs;
    }

    let place: Element | null = elem;
    if (this.tag) {
      if (this.tag === "svg") {
        inSvg = true;
      }
      place = document.createElementNS(inSvg ? XML_NS_SVG : XML_NS_XHTML, this.tag);
      this.elem = place;
      if (this.isRef) {
        refs.push(place);
      }
      elem?.appendChild(place);
      for (const [prop, value] of Object.entries(this.attributes ?? {})) {
        let namespace: string | null = null;
        let propName = prop;
        if (prop.indexOf(":") !== -1) {
          [namespace, propName] = prop.split(":", 2);
          namespace = XML_NS_PREFIXES.get(namespace) ?? null;
        }
        if (propName.startsWith("on") && value instanceof Function) {
          place.addEventListener(propName.substr(2), value);
        } else if (propName.endsWith("If")) {
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
        place!.appendChild(new Text(child));
      } else if (child instanceof Fragment) {
        child.insertInto(place, inSvg, refs);
      } else if (Array.isArray(child)) {
        for (const grandchild of child) {
          grandchild.insertInto(place, inSvg, refs);
        }
      } else if (child instanceof Node) {
        place!.appendChild(child);
      } else {
        console.log(child);
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

  asElement(refs?: any[]): Element {
    this.insertInto(null, false, refs);
    return nonNull(this.elem);
  }

  create(): [HTMLElement, ...any] {
    const refs: any[] = [];
    const elem = this.asElement(refs) as HTMLElement;
    return [elem, ...refs];
  }
}

export function jsx(
  tag: JSX.Tag | typeof Fragment | ComponentFactory<unknown>,
  attributes: Attributes | null,
  ...children: (Node | Fragment | Fragment[])[]
): Fragment {
  if (tag === Fragment) {
    return new Fragment(null, {}, children);
  } else if (typeof tag === "function") {
    const wrappedChildren = children.map((child) => {
      if (child instanceof Node) {
        return new Fragment(null, {}, [child]);
      }
      assert(child instanceof Fragment);
      return child;
    });
    return (tag as ComponentFactory<unknown>)((attributes ?? {}) as any, wrappedChildren);
  } else {
    return new Fragment(tag, attributes ?? {}, children);
  }
}

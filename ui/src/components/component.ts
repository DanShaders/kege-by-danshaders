import { assert } from "../utils/assert";

interface EventSource<A, B> {
  addEventListener(a: A, b: B): any;
  removeEventListener(a: A, b: B): any;
}

export abstract class Component<T> {
  settings: T;
  parent: Component<any> | null;
  unproxiedElem?: HTMLElement;
  unmountHooks?: (() => void)[];

  constructor(settings: T, parent: Component<any> | null) {
    this.settings = settings;
    this.parent = parent;
    this.unproxiedElem = undefined;
  }

  get elem(): HTMLElement {
    if (!this.unproxiedElem) {
      this.unproxiedElem = this.createElement();
    }
    return this.unproxiedElem;
  }

  rerenderMe(): void {
    assert(!!this.unproxiedElem, "Component was requested to be rerendered before creation");
    this.unmount();
    const oldElem = this.unproxiedElem;
    this.unproxiedElem = this.createElement();
    oldElem.replaceWith(this.elem);
  }

  firstRender(): Component<T> {
    assert(!this.unproxiedElem, "Component was requested to be rerendered as if it was first render");
    this.unproxiedElem = this.createElement();
    return this;
  }

  registerExternalListener<T extends EventSource<A, B>, A, B>(source: T, event: A, handler: B): void {
    source.addEventListener(event, handler);
    this.unmountHooks ??= [];
    this.unmountHooks.push(() => {
      source.removeEventListener(event, handler);
    });
  }

  unmount(): void {
    if (this.unmountHooks) {
      for (const hook of this.unmountHooks) {
        hook();
      }
      this.unmountHooks = undefined;
    }
  }

  apply(settings: T): void {
    if (settings === this.settings) {
      return;
    }
    this.settings = settings;
    if (this.unproxiedElem) {
      this.rerenderMe();
    }
  }

  abstract createElement(): HTMLElement;
}

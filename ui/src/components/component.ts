import { Fragment } from "jsx";

import { assert } from "utils/assert";

interface EventSource<A, B> {
  addEventListener(a: A, b: B): any;
  removeEventListener(a: A, b: B): any;
}

export type AnyComponent = Component<unknown> | null;

export abstract class Component<
  Settings,
  Parent extends AnyComponent = AnyComponent
> extends EventTarget {
  settings: Settings;
  parent: Parent;
  unproxiedElem?: HTMLElement;
  unmountHooks?: (() => void)[];
  children: Fragment[];

  constructor(settings: Settings, parent: Parent, children?: Fragment[]) {
    super();
    this.settings = settings;
    this.parent = parent;
    this.unproxiedElem = undefined;
    this.children = children ?? [];
    for (const child of this.children) {
      if (child.comp instanceof Component) {
        child.comp.parent = this;
      }
    }
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

  firstRender(): this {
    assert(
      !this.unproxiedElem,
      "Component was requested to be rerendered as if it was first render"
    );
    this.unproxiedElem = this.createElement();
    return this;
  }

  registerExternalListener<T extends EventSource<A, B>, A, B>(
    source: T,
    event: A,
    handler: B
  ): void {
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

  apply(settings: Settings): void {
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

type ComponentConstructor<Settings, Parent extends AnyComponent> = new (
  a: Settings,
  b: Parent,
  c?: Fragment[]
) => Component<Settings, Parent>;

export type ComponentFactory<Settings> = (
  a: { settings: Settings; parent?: AnyComponent; ref?: boolean },
  b: Fragment[]
) => Fragment;

export function createComponentFactory<T>(
  constructor: ComponentConstructor<T, AnyComponent>
): ComponentFactory<T> {
  return (
    { settings, parent, ref }: { settings: T; parent?: AnyComponent; ref?: boolean },
    children: Fragment[]
  ): Fragment => {
    return Fragment.fromComponent(
      ref ? { ref: true } : {},
      new constructor(settings, parent ?? null, children)
    );
  };
}

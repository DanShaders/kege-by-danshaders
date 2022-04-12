import { Component } from "./component";
import { IDiffableOf, DiffableSetOf } from "utils/diff";

export interface ListProvider {
  elem: HTMLElement;
  length: number;

  get(i: number): HTMLElement;
  push(elem: HTMLElement, belongs?: boolean): void;
  pop(i: number): void;
}

export abstract class EnumerableComponent<T, U, Enumerator extends Component<unknown>> extends Component<T & U> {
  i: number;
  isFirst: boolean;
  isLast: boolean;
  declare parent: Enumerator;

  constructor(settings: T & U, parent: Enumerator, i: number, isFirst: boolean, isLast: boolean) {
    super(settings, parent);
    this.i = i;
    this.isFirst = isFirst;
    this.isLast = isLast;
  }

  onPositionUpdate(i: number, isFirst: boolean, isLast: boolean) {
    this.i = i;
    this.isFirst = isFirst;
    this.isLast = isLast;
  }
}

export class BasicListProvider implements ListProvider {
  elem: HTMLElement;
  length: number;
  offset: number;
  elemFactory: () => HTMLElement;

  constructor(offset: number, elemFactory: () => HTMLElement) {
    this.offset = offset;
    this.elemFactory = elemFactory;
    this.elem = elemFactory();
    this.length = 0;
  }

  get(i: number): HTMLElement {
    return this.elem.children[i + this.offset] as HTMLElement;
  }

  push(elem: HTMLElement, belongs: boolean = true): void {
    if (!belongs) {
      this.elem.appendChild(elem);
      return;
    }
    const bc = this.elem.children;
    if (bc.length > this.length + this.offset) {
      this.elem.insertBefore(elem, bc[this.length + this.offset]);
    } else {
      this.elem.appendChild(elem);
    }
    ++this.length;
  }

  pop(i: number): void {
    this.elem.removeChild(this.get(i));
    --this.length;
  }
}

export type ComponentFactory<T, U, Set extends Component<unknown>> = (
  s: T & U,
  p: Set,
  i: number,
  f: boolean,
  l: boolean
) => EnumerableComponent<T, U, Set>;

export class SetComponent<T extends IDiffableOf<T>, U> extends Component<DiffableSetOf<T>> {
  protected provider;
  protected factory;
  protected comps;

  constructor(
    settings: DiffableSetOf<T>,
    parent: Component<unknown> | null,
    provider: ListProvider,
    factory: ComponentFactory<T, U, SetComponent<T, U>>,
    settingsMap: (obj: T) => T & U
  ) {
    super(settings, parent);
    this.provider = provider;
    this.factory = factory;
    this.comps = Array.from(this.settings.entries()).map(([id, obj], index) =>
      factory(settingsMap(obj), this, index, index === 0, index + 1 === this.settings.length)
    );
  }

  createElement(): HTMLElement {
    for (const [i, compSettings] of this.settings.entries()) {
      this.provider.push(this.comps[i].elem);
    }
    return this.provider.elem;
  }

  push(remote: T, additional: U, initializeLocal: (local: T) => void): void {
    const local = remote.clone();
    this.settings.add(local, remote, initializeLocal);

    const i = this.comps.length;
    this.comps.push(this.factory(Object.assign(local, additional), this, i, i === 0, true));
    if (i) {
      this.comps[i - 1].onPositionUpdate(i - 1, i - 1 === 0, false);
    }
    this.provider.push(this.comps[i].elem);
  }

  pop(i: number): void {
    this.provider.pop(i);
    this.comps.splice(i, 1);
    for (let j = Math.max(0, i - 1); j < this.comps.length; ++j) {
      this.comps[j].onPositionUpdate(j, j === 0, j + 1 === this.comps.length);
    }
  }
}

export abstract class SetEntry<T extends IDiffableOf<T>, U> extends EnumerableComponent<T, U, SetComponent<T, U>> {}

export function listProviderOf(tag: string, classList: string[] = []): ListProvider {
  return new BasicListProvider(0, () => {
    const x = document.createElement(tag);
    if (classList.length) {
      x.classList.add(...classList);
    }
    return x;
  });
}

export function factoryOf<T, U, Set extends Component<unknown>>(
  obj: new (s: T & U, p: Set, i: number, f: boolean, l: boolean) => EnumerableComponent<T, U, Set>
): ComponentFactory<T, U, Set> {
  return (s, p, i, f, l) => {
    return new obj(s, p, i, f, l);
  };
}

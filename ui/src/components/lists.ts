import { Component } from "./component";
import { IDiffableOf, DiffableSetOf } from "utils/diff";
import { LengthChangeEvent, ListPosition, PositionUpdateEvent } from "utils/events";

export interface ListProvider {
  elem: HTMLElement;
  length: number;
  eventTarget?: EventTarget;

  get(i: number): HTMLElement;
  push(elem: HTMLElement, belongs?: boolean): void;
  pop(i: number): void;
}

export abstract class EnumerableComponent<T, U, Enumerator extends Component<unknown>> extends Component<T & U> {
  declare parent: Enumerator;

  private pos: ListPosition;

  constructor(settings: T & U, parent: Enumerator, pos: ListPosition) {
    super(settings, parent);
    this.pos = pos;
    this.addEventListener("positionupdate", this.onPositionUpdate.bind(this) as EventListener);
  }

  get i(): number {
    return this.pos.i;
  }

  get isFirst(): boolean {
    return this.pos.isFirst;
  }

  get isLast(): boolean {
    return this.pos.isLast;
  }

  onPositionUpdate({ pos }: PositionUpdateEvent): void {
    this.pos = pos;
  }
}

export class BasicListProvider implements ListProvider {
  elem: HTMLElement;
  length: number;
  eventTarget?: EventTarget;

  offset: number;

  constructor(elem: HTMLElement, offset: number) {
    this.offset = offset;
    this.elem = elem;
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
    this.eventTarget?.dispatchEvent(new LengthChangeEvent(this.length, 1));
  }

  pop(i: number): void {
    this.elem.removeChild(this.get(i));
    --this.length;
    this.eventTarget?.dispatchEvent(new LengthChangeEvent(this.length, -1));
  }
}

export type ComponentFactory<T, U, Set extends Component<unknown>> = (
  s: T & U,
  p: Set,
  pos: ListPosition
) => EnumerableComponent<T, U, Set>;

export class SetComponent<T extends IDiffableOf<T>, U> extends Component<DiffableSetOf<T>> {
  protected provider;
  protected factory;
  protected comps;

  protected positionOf(i: number, length: number): ListPosition {
    return { i: i, isFirst: i === 0, isLast: i === length - 1 };
  }

  protected dispatchPositionUpdate(i: number): void {
    this.comps[i].dispatchEvent(new PositionUpdateEvent(this.positionOf(i, this.comps.length)));
  }

  constructor(
    settings: DiffableSetOf<T>,
    parent: Component<unknown> | null,
    provider: ListProvider,
    factory: ComponentFactory<T, U, SetComponent<T, U>>,
    settingsMap: (obj: T) => T & U
  ) {
    super(settings, parent);
    this.provider = provider;
    this.provider.eventTarget = this;
    this.factory = factory;
    this.comps = Array.from(this.settings.entries()).map(([id, obj], index) =>
      factory(settingsMap(obj), this, this.positionOf(index, this.settings.length))
    );
    for (const comp of this.comps) {
      this.provider.push(comp.elem);
    }
  }

  createElement(): HTMLElement {
    return this.provider.elem;
  }

  push(remote: T, additional: U, initializeLocal: (local: T) => void): void {
    const local = remote.clone();
    this.settings.add(local, remote, initializeLocal);

    const i = this.comps.length;
    this.comps.push(this.factory(Object.assign(local, additional), this, this.positionOf(i, i + 1)));
    if (i) {
      this.dispatchPositionUpdate(i - 1);
    }
    this.provider.push(this.comps[i].elem);
  }

  pop(i: number): void {
    this.provider.pop(i);
    this.comps.splice(i, 1);
    for (let j = Math.max(0, i - 1); j < this.comps.length; ++j) {
      this.dispatchPositionUpdate(j);
    }
  }

  get length(): number {
    return this.provider.length;
  }
}

export abstract class SetEntry<T extends IDiffableOf<T>, U> extends EnumerableComponent<T, U, SetComponent<T, U>> {}

export function listProviderOf(tag: string, classList: string[] = []): ListProvider {
  const elem = document.createElement(tag);
  if (classList.length) {
    elem.classList.add(...classList);
  }
  return new BasicListProvider(elem, 0);
}

export function factoryOf<T, U, Set extends Component<unknown>>(
  Obj: new (s: T & U, p: Set, pos: ListPosition) => EnumerableComponent<T, U, Set>
): ComponentFactory<T, U, Set> {
  return (s, p, pos) => {
    return new Obj(s, p, pos);
  };
}

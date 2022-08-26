import { DiffableSetOf, IDiffableOf } from "utils/diff";
import { LengthChangeEvent, ListPosition, PositionUpdateEvent } from "utils/events";

import { AnyComponent, Component } from "components/component";

interface ListProvider {
  elem: HTMLElement;
  length: number;
  eventTarget?: EventTarget;

  get(i: number): HTMLElement;
  push(elem: HTMLElement, belongs?: boolean): void;
  pop(i: number): void;
}

export abstract class EnumerableComponent<
  DiffableSettings,
  StaticSettings,
  Enumerator extends AnyComponent
> extends Component<DiffableSettings & StaticSettings, Enumerator> {
  private pos: ListPosition;

  constructor(settings: DiffableSettings & StaticSettings, parent: Enumerator, pos: ListPosition) {
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

interface ArrayLike<T> {
  length: number;
  entries(): IterableIterator<[number, T]>;
}

type EnumerableConstructor<
  DiffableSettings,
  StaticSettings,
  Enumerator extends AnyComponent
> = new (
  s: DiffableSettings & StaticSettings,
  p: Enumerator,
  pos: ListPosition
) => EnumerableComponent<DiffableSettings, StaticSettings, Enumerator>;

type ComponentFactory<DiffableSettings, StaticSettings, Enumerator extends AnyComponent> = (
  s: DiffableSettings & StaticSettings,
  p: Enumerator,
  pos: ListPosition
) => EnumerableComponent<DiffableSettings, StaticSettings, Enumerator>;

export class BasicSetComponent<
  DiffableSettings,
  StaticSettings,
  Settings extends ArrayLike<DiffableSettings>,
  Parent extends AnyComponent = AnyComponent
> extends Component<Settings, Parent> {
  protected provider;
  protected factory: ComponentFactory<DiffableSettings, StaticSettings, this>;
  protected comps: EnumerableComponent<DiffableSettings, StaticSettings, this>[];

  protected positionOf(i: number, length: number): ListPosition {
    return { i: i, isFirst: i === 0, isLast: i === length - 1 };
  }

  protected dispatchPositionUpdate(i: number): void {
    this.comps[i].dispatchEvent(new PositionUpdateEvent(this.positionOf(i, this.comps.length)));
  }

  constructor(
    settings: Settings,
    parent: Parent,
    provider: ListProvider,
    factory: ComponentFactory<
      DiffableSettings,
      StaticSettings,
      BasicSetComponent<DiffableSettings, StaticSettings, Settings, Parent>
    >,
    settingsMap: (obj: DiffableSettings) => DiffableSettings & StaticSettings
  ) {
    super(settings, parent);
    this.provider = provider;
    this.provider.eventTarget = this;
    this.factory = factory as any;
    this.comps = Array.from(this.settings.entries()).map(([, obj], index) =>
      this.factory(settingsMap(obj), this, this.positionOf(index, this.settings.length))
    );
    for (const comp of this.comps) {
      this.provider.push(comp.elem);
    }
  }

  createElement(): HTMLElement {
    return this.provider.elem;
  }

  entries(): IterableIterator<[number, typeof this.comps[0]]> {
    return this.comps.entries();
  }

  protected pushComponent(settings: DiffableSettings & StaticSettings): void {
    const i = this.comps.length;
    this.comps.push(this.factory(settings, this, this.positionOf(i, i + 1)));
    if (i) {
      this.dispatchPositionUpdate(i - 1);
    }
    this.provider.push(this.comps[i].elem);
  }

  protected popComponent(i: number): void {
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

export class SetComponent<
  Diffable extends IDiffableOf<Diffable>,
  StaticSettings,
  Parent extends AnyComponent = AnyComponent
> extends BasicSetComponent<Diffable, StaticSettings, DiffableSetOf<Diffable>, Parent> {
  constructor(
    settings: DiffableSetOf<Diffable>,
    parent: Parent,
    provider: ListProvider,
    factory: ComponentFactory<
      Diffable,
      StaticSettings,
      SetComponent<Diffable, StaticSettings, Parent>
    >,
    settingsMap: (obj: Diffable) => Diffable & StaticSettings
  ) {
    super(settings, parent, provider, factory as any, settingsMap);
  }

  push(
    remote: Diffable,
    additional: StaticSettings,
    initializeLocal: (local: Diffable) => void
  ): void {
    // [Dan Klishch, literally 2 months after writing this code]
    // Genially, no idea why using clone() here, why passing initializeLocal to DiffableSet.add
    // Absence of tests and documentation only exacerbates the situation.
    // It works and I'm not intending on fixing/rewriting this shit.
    const local = remote.clone();
    this.settings.add(local, remote, initializeLocal);
    this.pushComponent(Object.assign(local, additional));
  }

  pop(i: number): void {
    this.popComponent(i);
  }
}

export class OrderedSetComponent<
  Diffable extends IDiffableOf<Diffable> & { currPos: number },
  StaticSettings,
  Parent extends AnyComponent = AnyComponent
> extends SetComponent<Diffable, StaticSettings, Parent> {
  private nextFreePos = 0;

  constructor(
    settings: DiffableSetOf<Diffable>,
    parent: Parent,
    provider: ListProvider,
    factory: ComponentFactory<
      Diffable,
      StaticSettings,
      OrderedSetComponent<Diffable, StaticSettings, Parent>
    >,
    settingsMap: (obj: Diffable) => Diffable & StaticSettings
  ) {
    super(settings, parent, provider, factory as any, settingsMap);
    this.nextFreePos = this.comps.length;
  }

  push(
    remote: Diffable,
    additional: StaticSettings,
    initializeLocal: (local: Diffable) => void
  ): void {
    const local = remote.clone();
    // Next 2 lines are kinda initializeLocal, see SetComponent::push
    remote.currPos = -1;
    // Cannot use this.comps.length because of holes from this::pop.
    local.currPos = this.nextFreePos++;

    this.settings.add(local, remote, initializeLocal);
    this.pushComponent(Object.assign(local, additional));
  }

  pop(i: number): void {
    // This creates a hole in the indexation but I never promised anybody that currPos will be
    // sequential.
    this.comps[i].settings.currPos = -1;
    this.popComponent(i);
  }

  swap(i: number, j: number): void {
    // First of all, deal with DOM
    const a = this.provider.get(i);
    const b = this.provider.get(j);
    const afterB = b.nextElementSibling;
    const parent = b.parentNode!;
    if (afterB === a) {
      parent.insertBefore(a, b);
    } else {
      a.replaceWith(b);
      parent.insertBefore(a, afterB);
    }

    // Then with this.comps and components' positions
    [this.comps[i], this.comps[j]] = [this.comps[j], this.comps[i]];
    this.settings.asAtomicChange(() => {
      const sa = this.comps[i].settings;
      const sb = this.comps[j].settings;
      [sa.currPos, sb.currPos] = [sb.currPos, sa.currPos];
    });

    // Issue position updates to them
    this.dispatchPositionUpdate(i);
    this.dispatchPositionUpdate(j);
  }
}

export class ListComponent<
  Settings,
  Parent extends AnyComponent = AnyComponent
> extends BasicSetComponent<Settings, {}, Settings[], Parent> {
  constructor(
    settings: Settings[],
    parent: Parent,
    provider: ListProvider,
    factory: ComponentFactory<Settings, {}, ListComponent<Settings, Parent>>
  ) {
    super(settings, parent, provider, factory as any, (e) => e);
  }

  push(comp: Settings): void {
    this.settings.push(comp);
    this.pushComponent(comp);
  }

  pop(i: number): void {
    this.settings.splice(i, 1);
    this.popComponent(i);
  }
}

export abstract class SetEntry<
  Diffable extends IDiffableOf<Diffable>,
  StaticSettings
> extends EnumerableComponent<Diffable, StaticSettings, SetComponent<Diffable, StaticSettings>> {}

export abstract class OrderedSetEntry<
  Diffable extends IDiffableOf<Diffable> & { currPos: number },
  StaticSettings
> extends EnumerableComponent<
  Diffable,
  StaticSettings,
  OrderedSetComponent<Diffable, StaticSettings>
> {}

export abstract class ListEntry<Settings> extends EnumerableComponent<
  Settings,
  {},
  ListComponent<Settings>
> {}

export function listProviderOf(tag: string, classList: string[] = []): ListProvider {
  const elem = document.createElement(tag);
  if (classList.length) {
    elem.classList.add(...classList);
  }
  return new BasicListProvider(elem, 0);
}

export function factoryOf<DiffableSettings, StaticSettings, Enumerator extends AnyComponent>(
  Obj: EnumerableConstructor<DiffableSettings, StaticSettings, Enumerator>
): ComponentFactory<DiffableSettings, StaticSettings, Enumerator> {
  return (s, p, pos) => {
    return new Obj(s, p, pos);
  };
}

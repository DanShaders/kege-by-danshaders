import { Component } from "./component";

export interface ListProvider {
  elem: HTMLElement;
  length: number;

  get(i: number): HTMLElement;
  push(elem: HTMLElement, belongs?: boolean): void;
  pop(i: number): void;
}

export abstract class EnumerableComponent<T> extends Component<T> {
  i: number;
  isFirst: boolean;
  isLast: boolean;

  constructor(settings: T, parent: Component<any> | null, i: number, isFirst: boolean, isLast: boolean) {
    super(settings, parent);
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
    if (bc.length > this.length + this.offset) this.elem.insertBefore(elem, bc[this.length + this.offset]);
    else this.elem.appendChild(elem);
    ++this.length;
  }

  pop(i: number): void {
    this.elem.removeChild(this.get(i));
    --this.length;
  }
}

export class TableColumnListProvider extends BasicListProvider {
  columnIndex: number;
  columnLengths: number[];

  constructor(columnIndex: number, columnLengths: number[], offset: number, elem: HTMLElement) {
    super(offset, () => elem);
    this.columnIndex = columnIndex;
    this.columnLengths = columnLengths;
  }

  getContainer(i: number): HTMLElement {
    return super.get(i).children[this.columnIndex] as HTMLElement;
  }

  override get(i: number): HTMLElement {
    return this.getContainer(i).children[0] as HTMLElement;
  }

  override push(elem: HTMLElement, belongs: boolean = true): void {
    if (Math.max(...this.columnLengths) === this.columnLengths[this.columnIndex]) {
      const newRow = document.createElement("tr");
      for (let i = 0; i < this.columnLengths.length; ++i) newRow.appendChild(document.createElement("td"));
      this.elem.appendChild(newRow);
    }
    ++this.columnLengths[this.columnIndex];
    if (belongs) {
      ++this.length;
      const i = this.length - 1;
      for (let j = this.columnLengths[this.columnIndex] - 1; j > i; --j)
        this.getContainer(j).appendChild(this.get(j - 1));
      this.getContainer(i).appendChild(elem);
    } else {
      this.getContainer(this.columnLengths[this.columnIndex] - 1).appendChild(elem);
    }
  }

  override pop(i: number): void {
    this.getContainer(i).removeChild(this.get(i));
    for (let j = i; j < this.columnLengths[this.columnIndex] - 1; ++j)
      this.getContainer(j).appendChild(this.get(j + 1));
    --this.length;
    --this.columnLengths[this.columnIndex];
    if (Math.max(...this.columnLengths) === this.columnLengths[this.columnIndex])
      this.elem.removeChild(super.get(this.columnLengths[this.columnIndex]));
  }
}

export type ComponentFactory<T> = (s: T, p: Component<T[]>, i: number, f: boolean, l: boolean) => Component<T>;

export class ListComponent<T> extends Component<T[]> {
  listProvider: ListProvider;
  compFactory: ComponentFactory<T>;
  comps: Component<T>[];

  constructor(
    settings: T[],
    parent: Component<any> | null,
    listProvider: ListProvider,
    compFactory: ComponentFactory<T>
  ) {
    super(settings, parent);
    this.listProvider = listProvider;
    this.compFactory = compFactory;
    this.comps = Array(settings.length);
  }

  createElement(): HTMLElement {
    for (const [i, compSettings] of this.settings.entries()) {
      this.comps[i] = this.compFactory(compSettings, this, i, i === 0, i === this.settings.length - 1);
      this.listProvider.push(this.comps[i].elem);
    }
    return this.listProvider.elem;
  }

  swap(i: number, j: number): void {
    [this.settings[i], this.settings[j]] = [this.settings[j], this.settings[i]];
    this.rerender(i);
    this.rerender(j);
    this.onChange();
  }

  push(current: T): void {
    const i = this.settings.length;
    this.settings.push(current);
    this.comps.push(this.compFactory(current, this, i, i === 0, true));
    if (i > 0) this.rerender(i - 1);
    this.listProvider.push(this.comps[i].elem);
    this.onLengthChange();
    this.onChange();
  }

  pop(i: number): void {
    this.settings.splice(i, 1);
    this.comps.splice(i, 1);
    this.listProvider.pop(i);
    for (let j = Math.max(0, i - 1); j < this.settings.length; ++j) this.rerender(j);
    this.onLengthChange();
    this.onChange();
  }

  rerender(i: number): void {
    this.comps[i] = this.compFactory(this.settings[i], this, i, i === 0, i === this.settings.length - 1);
    this.listProvider.get(i).replaceWith(this.comps[i].elem);
  }

  onLengthChange(): void {}
  onChange(): void {}
}

export function listProviderOf(tag: string, classList: string[] = []): ListProvider {
  return new BasicListProvider(0, () => {
    const x = document.createElement(tag);
    if (classList.length) x.classList.add(...classList);
    return x;
  });
}

import { assert } from "utils/assert";
import { BulkSelectionChangeEvent } from "utils/events";

export class BulkSelectionController extends EventTarget {
  private selection = new Set<number>();
  private elements: Set<number>;
  private bulkSelectCheckbox: HTMLInputElement;

  private verifyConsistency(): void {
    for (const id of this.selection) {
      assert(this.elements.has(id));
    }
  }

  private sendSelectionUpdate(isDOMReflected: boolean): void {
    this.verifyConsistency();
    this.dispatchEvent(new BulkSelectionChangeEvent(isDOMReflected));

    const sup = this.elements.size;
    const sub = this.selection.size;
    this.bulkSelectCheckbox.indeterminate = sub !== 0 && sub !== sup;
    this.bulkSelectCheckbox.checked = sup !== 0 && sub === sup;
    this.bulkSelectCheckbox.disabled = sup === 0;
  }

  constructor(elements: Set<number>, bulkSelectCheckbox: HTMLInputElement) {
    super();
    this.elements = elements;
    this.bulkSelectCheckbox = bulkSelectCheckbox;
    this.sendSelectionUpdate(true);

    bulkSelectCheckbox.addEventListener("change", () => {
      if (bulkSelectCheckbox.checked) {
        this.selectAll(false);
      } else {
        this.clearSelection(false);
      }
    });
  }

  setSelection(selection: Set<number>, isDOMReflected: boolean = true): void {
    this.selection = selection;
    this.sendSelectionUpdate(isDOMReflected);
  }

  clearSelection(isDOMReflected: boolean = true): void {
    this.selection.clear();
    this.sendSelectionUpdate(isDOMReflected);
  }

  getSelection(): Set<number> {
    return this.selection;
  }

  notifySingleSelectionChange(id: number, state: boolean, isDOMReflected: boolean = true): void {
    if (state) {
      this.selection.add(id);
    } else {
      this.selection.delete(id);
    }
    this.sendSelectionUpdate(isDOMReflected);
  }

  selectAll(isDOMReflected: boolean = true): void {
    this.selection = new Set(this.elements);
    this.sendSelectionUpdate(isDOMReflected);
  }

  setElements(elements: Set<number>, isDOMReflected: boolean = true): void {
    this.elements = elements;
    this.sendSelectionUpdate(isDOMReflected);
  }

  getElements(): Set<number> {
    return this.elements;
  }

  push(id: number, isDOMReflected: boolean = true): void {
    this.elements.add(id);
    this.sendSelectionUpdate(isDOMReflected);
  }

  pop(id: number, isDOMReflected: boolean = true): void {
    this.elements.delete(id);
    this.selection.delete(id);
    this.sendSelectionUpdate(isDOMReflected);
  }
}

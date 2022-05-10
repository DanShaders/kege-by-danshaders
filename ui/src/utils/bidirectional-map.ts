import { nonNull } from "./assert";

export default class BidirectionalMap<T, U> {
  private forward: Map<T, U>;
  private backward: Map<U, T>;

  constructor() {
    this.forward = new Map();
    this.backward = new Map();
  }

  add(primary: T, secondary: U): void {
    this.forward.set(primary, secondary);
    this.backward.set(secondary, primary);
  }

  delete(primary: T): void {
    this.backward.delete(nonNull(this.forward.get(primary)));
    this.forward.delete(primary);
  }

  getSecondary(primary: T): U | undefined {
    return this.forward.get(primary);
  }

  getPrimary(secondary: U): T | undefined {
    return this.backward.get(secondary);
  }
}

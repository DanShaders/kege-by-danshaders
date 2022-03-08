export class Queue<T> {
  private s1: T[] = [];
  private s2: T[] = [];

  constructor() {}

  push(t: T): void {
    this.s2.push(t);
  }

  pop(): T | undefined {
    if (!this.s1.length) {
      while (this.s2.length) {
        this.s1.push(this.s2.pop()!);
      }
    }
    return this.s1.pop();
  }

  get length(): number {
    return this.s1.length + this.s2.length;
  }

  clear(): void {
    this.s1 = [];
    this.s2 = [];
  }
}

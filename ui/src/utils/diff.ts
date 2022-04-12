import { nonNull } from "./assert";

export type DeltaChangeCallback = (fields: number, fieldsDelta: number) => void;

export interface IContext<Diffable, Delta> {
  remote: Diffable;
  delta: Delta;
  onDeltaChange: DeltaChangeCallback;
}

export interface IDiffable<Diffable, Delta, Transport> {
  get id(): number;

  clone(): Diffable;
  commit(commitDelta: Delta): void;
  createLocal(onDeltaChange: DeltaChangeCallback): Diffable;
  mount(ctx: IContext<Diffable, Delta>): void;
  serialize(): Transport;
  synchronize(): [syncObj: Transport, commitDelta: Delta] | undefined;
}

type SetDelta<Delta> = Map<number, Delta>;
type SetContext<Set> = Set extends DiffableSet<any, infer Delta, any> ? IContext<Set, SetDelta<Delta>> : never;

export class DiffableSet<Diffable extends IDiffable<Diffable, Delta, Transport>, Delta, Transport> {
  private factory: (msg: Transport) => Diffable;
  private elements: Map<number, Diffable>;
  private ctx?: SetContext<this>;
  private fields = 0;
  private supressPropagation = false;

  constructor(msgs: Iterable<Transport>, factory: (msg: Transport) => Diffable) {
    this.factory = factory;
    this.elements = new Map();
    for (const msg of msgs) {
      const element = factory(msg);
      this.elements.set(element.id, element);
    }
  }

  get length(): number {
    return this.elements.size;
  }

  get(id: number): Diffable | undefined {
    return this.elements.get(id);
  }

  entries(): IterableIterator<[number, Diffable]> {
    return this.elements.entries();
  }

  add(local: Diffable, remote: Diffable, initializeLocal: (local: Diffable) => void): void {
    const saveFields = this.fields;
    this.supressPropagation = true;

    this.ctx!.remote.elements.set(remote.id, remote);
    this.elements.set(local.id, local);
    this.mountSingle(local.id, local);
    initializeLocal(local);

    this.supressPropagation = false;
    if (this.fields !== saveFields) {
      this.ctx!.onDeltaChange(this.fields, this.fields - saveFields);
    }
  }

  private mountSingle(id: number, elem: Diffable): void {
    const delta = {} as Delta;
    const ctx = nonNull(this.ctx);
    elem.mount({
      remote: nonNull(ctx.remote.elements.get(id)!),
      delta: delta,
      onDeltaChange: (fields: number, fieldsDelta: number): void => {
        if (!fields && fieldsDelta < 0) {
          ctx.delta.delete(id);
          --this.fields;
          this.supressPropagation || ctx.onDeltaChange(this.fields, -1);
        } else if (fields === fieldsDelta) {
          ctx.delta.set(id, delta);
          ++this.fields;
          this.supressPropagation || ctx.onDeltaChange(this.fields, 1);
        } else {
          this.supressPropagation || ctx.onDeltaChange(this.fields, 0);
        }
      },
    });
  }

  mount(ctx: SetContext<this>): void {
    this.ctx = ctx;
    for (const [id, elem] of this.elements) {
      this.mountSingle(id, elem);
    }
  }

  commit(commitDelta: SetDelta<Delta>): void {
    const saveFields = this.fields;
    this.supressPropagation = true;
    for (const [id, delta] of commitDelta) {
      this.elements.get(id)!.commit(delta);
    }
    this.supressPropagation = false;
    if (this.fields !== saveFields) {
      this.ctx!.onDeltaChange(this.fields, this.fields - saveFields);
    }
  }

  serialize(adder: (msg: Transport) => void): void {
    for (const id of this.ctx!.delta.keys()) {
      adder(this.elements.get(id)!.serialize());
    }
  }
}

export type IDiffableOf<T> = IDiffable<T, any, any>;
export type DiffableSetOf<T extends IDiffableOf<T>> = DiffableSet<T, any, any>;

export function areBuffersEqual(a: ArrayBuffer, b: ArrayBuffer): boolean {
  if (a.byteLength !== b.byteLength) {
    return false;
  }
  const iters = Math.floor(a.byteLength / 4);
  const a32 = new Uint32Array(a, 0, iters * 4);
  const b32 = new Uint32Array(b, 0, iters * 4);
  const a8 = new Uint8Array(a, iters * 4);
  const b8 = new Uint8Array(b, iters * 4);
  for (let i = 0; i < iters; ++i) {
    if (a32[i] !== b32[i]) {
      return false;
    }
  }
  for (let i = 0; i < a8.length; ++i) {
    if (a8[i] !== b8[i]) {
      return false;
    }
  }
  return true;
}

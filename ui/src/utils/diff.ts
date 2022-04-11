import { assert, nonNull } from "./assert";

export type DeltaChangeCallback = (fields: number, fieldsDelta: number) => void;

export interface IContext<Diffable, Delta> {
  remote: Diffable;
  delta: Delta;
  onDeltaChange: DeltaChangeCallback;
}

export interface IDiffable<Diffable, Delta, Transport> {
  commit(commitDelta: Delta): void;
  createLocal(onDeltaChange: DeltaChangeCallback): Diffable;
  mount(ctx: IContext<Diffable, Delta>): void;
  synchronize(): [syncObj: Transport, commitDelta: Delta] | undefined;
}

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

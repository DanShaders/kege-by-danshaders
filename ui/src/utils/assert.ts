class AssertionError extends Error {}

export function assert(condition: boolean, msg: string = ""): asserts condition {
  if (!condition) {
    throw new AssertionError(msg);
  }
}

export function nonNull<T>(obj: T | null | undefined, msg: string = ""): T {
  assert(obj !== null && obj !== undefined, msg);
  return obj;
}

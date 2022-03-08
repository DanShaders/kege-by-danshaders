/* eslint-disable */
enum FutureStatus {
  CLEAR,
  REJECTED,
  RESOLVED,
  AWAITING,
  FULFILLED,
}
/* eslint-enable */

export class FutureError extends Error {}

export class Future<T> {
  private status: FutureStatus = FutureStatus.CLEAR;
  private resolveValue?: T;
  private rejectValue?: Error;
  private resolver?: (value: T) => void;
  private rejecter?: (reason?: any) => void;

  async wait(): Promise<T> {
    if (this.status === FutureStatus.FULFILLED || this.status === FutureStatus.AWAITING) {
      throw new FutureError("Future is not awaitable");
    }
    return new Promise<T>((resolve, reject) => {
      if (this.status === FutureStatus.CLEAR) {
        this.status = FutureStatus.AWAITING;
        this.resolver = resolve;
        this.rejecter = reject;
      } else if (this.status === FutureStatus.REJECTED) {
        this.status = FutureStatus.FULFILLED;
        // eslint-disable-next-line prefer-promise-reject-errors
        reject(this.rejectValue!);
      } else if (this.status === FutureStatus.RESOLVED) {
        this.status = FutureStatus.FULFILLED;
        resolve(this.resolveValue!);
      }
    });
  }

  resolveWith(value: T): void {
    if (this.status === FutureStatus.AWAITING) {
      this.resolver!(value);
      this.status = FutureStatus.FULFILLED;
    } else if (this.status === FutureStatus.CLEAR) {
      this.resolveValue = value;
      this.status = FutureStatus.RESOLVED;
    } else {
      throw new FutureError("Future is not clear or awaiting");
    }
  }

  rejectWith(reason?: Error): void {
    if (this.status === FutureStatus.AWAITING) {
      this.rejecter!(reason);
      this.status = FutureStatus.FULFILLED;
    } else if (this.status === FutureStatus.CLEAR) {
      this.rejectValue = reason;
      this.status = FutureStatus.REJECTED;
    } else {
      throw new FutureError("Future is not clear or awaiting");
    }
  }

  clear(): void {
    if (this.status === FutureStatus.AWAITING) {
      throw new FutureError("Cannot clear awaiting future");
    }
    this.status = FutureStatus.CLEAR;
  }

  isAwaiting(): boolean {
    return this.status === FutureStatus.AWAITING;
  }
}

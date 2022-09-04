import { toggleLoadingScreen } from "utils/common";
import { IDiffable } from "utils/diff";
import { Future } from "utils/future";
import { EmptyPayload, requestU } from "utils/requests";
import { BinarySerializable } from "utils/requests";
import { Page } from "utils/router";

type SyncControllerParams<Diffable> = {
  statusElem: HTMLSpanElement;
  remote: Diffable;
  saveURL: string;
};

export class SyncController<Diffable extends IDiffable<Diffable, unknown, BinarySerializable>> {
  private params: SyncControllerParams<Diffable>;
  private local: Diffable;
  private handlerInProgress = false;
  private updateWhileSave = false;
  private updateNotifier = new Future<void>();
  private lastFields = 0;
  private catchAllLastFields = 0;
  private deltaCalled = 0;
  supressSave = true;

  constructor(params: SyncControllerParams<Diffable>) {
    this.params = params;
    this.local = params.remote.createLocal(this.onDeltaChange.bind(this));
  }

  asAtomicChange(fn: () => void): void {
    if (this.supressSave) {
      fn();
      return;
    }

    const deltaEpoch = this.deltaCalled;
    this.supressSave = true;
    fn();
    this.supressSave = false;
    if (deltaEpoch !== this.deltaCalled) {
      this.onDeltaChange(this.catchAllLastFields);
    }
  }

  async onDeltaChange(fields: number): Promise<void> {
    ++this.deltaCalled;
    this.catchAllLastFields = fields;
    this.updateWhileSave = true;
    if (this.supressSave) {
      return;
    }
    this.lastFields = fields;
    if (this.handlerInProgress) {
      return;
    }
    this.updateNotifier.clear();
    this.lastFields = fields;
    this.handlerInProgress = true;
    while (this.updateWhileSave) {
      this.updateWhileSave = false;

      const syncResult = this.local.synchronize();
      if (syncResult === undefined) {
        this.params.statusElem.innerText = "";
        break;
      }

      this.params.statusElem.innerText = "Сохранение...";
      const [syncObj, commitDelta] = syncResult;
      try {
        await requestU(EmptyPayload, this.params.saveURL, syncObj);
        if (!this.updateWhileSave) {
          this.lastFields = 0;
          this.updateNotifier.resolveWith();
        }
        this.local.commit(commitDelta);
        this.params.statusElem.innerText = "";
      } catch (e) {
        this.params.statusElem.innerText = "Не все изменения сохранены";
        if (!this.updateWhileSave) {
          this.updateNotifier.rejectWith();
        }
        console.error(e);
      }
    }
    this.handlerInProgress = false;
  }

  getLocal(): Diffable {
    return this.local;
  }

  isSynchronized(): boolean {
    return !this.lastFields;
  }

  async synchronize(): Promise<boolean> {
    if (!this.isSynchronized()) {
      try {
        await this.updateNotifier.wait();
      } catch {
        return false;
      }
    }
    return true;
  }
}

export abstract class SynchronizablePage<
  Diffable extends IDiffable<Diffable, unknown, BinarySerializable>
> extends Page {
  syncController?: SyncController<Diffable>;

  flush(): void {}

  override unload(): boolean {
    this.flush();
    return this.syncController?.isSynchronized() ?? true;
  }

  override async unmount(): Promise<boolean> {
    this.flush();
    if (!((await this.syncController?.synchronize()) ?? true)) {
      toggleLoadingScreen(false);
      return confirm("Не удалось сохранить все изменения.\nПокинуть страницу?");
    }
    return true;
  }
}

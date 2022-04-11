import { IDiffable } from "./diff";
import { requestU, EmptyPayload } from "./requests";
import { BinarySerializable } from "./requests";

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

  supressSave = true;

  constructor(params: SyncControllerParams<Diffable>) {
    this.params = params;
    this.local = params.remote.createLocal(this.onDeltaChange.bind(this));
  }

  async onDeltaChange(): Promise<void> {
    this.updateWhileSave = true;
    if (this.handlerInProgress || this.supressSave) {
      return;
    }
    this.handlerInProgress = true;
    while (this.updateWhileSave) {
      this.updateWhileSave = false;
      this.params.statusElem.innerText = "Сохранение...";

      const syncResult = this.local.synchronize();
      if (syncResult === undefined) {
        break;
      }

      const [syncObj, rollbackDelta] = syncResult;
      try {
        await requestU(EmptyPayload, this.params.saveURL, syncObj);
        this.params.statusElem.innerText = "";
      } catch (e) {
        this.params.statusElem.innerText = "Не все изменения сохранены";
        console.error(e);
        this.local.rollback(rollbackDelta);
      }
    }
    this.handlerInProgress = false;
  }

  getLocal(): Diffable {
    return this.local;
  }
}

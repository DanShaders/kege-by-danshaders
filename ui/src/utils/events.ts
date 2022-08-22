export type ListPosition = {
  i: number;
  isFirst: boolean;
  isLast: boolean;
};

export class PositionUpdateEvent extends Event {
  static NAME = "positionupdate";

  pos: ListPosition;

  constructor(pos: ListPosition) {
    super(PositionUpdateEvent.NAME);
    this.pos = pos;
  }
}

export class LengthChangeEvent extends Event {
  static NAME = "lengthchange";

  length: number;
  delta: number;

  constructor(length: number, delta: number) {
    super(LengthChangeEvent.NAME);
    this.length = length;
    this.delta = delta;
  }
}

export class PageCategoryUpdateEvent extends Event {
  static NAME = "pagecategoryupdate";

  constructor() {
    super(PageCategoryUpdateEvent.NAME);
  }
}

export class BulkSelectionChangeEvent extends Event {
  static NAME = "bulkselectionchangeevent";

  isDOMReflected: boolean;

  constructor(isDOMReflected: boolean) {
    super(BulkSelectionChangeEvent.NAME);
    this.isDOMReflected = isDOMReflected;
  }
}

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

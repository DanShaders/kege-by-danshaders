export type ListPosition = {
  i: number;
  isFirst: boolean;
  isLast: boolean;
};

export class PositionUpdateEvent extends Event {
  pos: ListPosition;

  constructor(pos: ListPosition) {
    super("positionupdate");
    this.pos = pos;
  }
}

export class LengthChangeEvent extends Event {
  length: number;
  delta: number;

  constructor(length: number, delta: number) {
    super("lengthchange");
    this.length = length;
    this.delta = delta;
  }
}

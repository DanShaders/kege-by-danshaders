#include "fields.h"

void PosFieldCodeGenerator::generate_serialize() {
  print(R"(if (delta.currPos !== undefined) {
  obj.setSwapPos(delta.currPos);
  obj.setCurrPos(this.ctx!.remote.currPos);
}
)");
}

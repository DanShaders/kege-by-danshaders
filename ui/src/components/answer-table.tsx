import { Component } from "components/component";
import { ButtonIcon } from "components/button-icon";

import * as jsx from "jsx";

type AnswerSettings = {
  answerRows: number;
  answerCols: number;
  answer: Uint8Array;
};

const textDecoder = new TextDecoder(),
  textEncoder = new TextEncoder();

export class AnswerTable extends Component<AnswerSettings> {
  parsedAnswer?: string[][];

  createElement(): HTMLElement {
    const rows: jsx.Fragment[] = [];
    this.parsedAnswer = [];
    let answerPos = 0;
    const answer = this.settings.answer;
    for (let i = 0; i < this.settings.answerRows; ++i) {
      const row = document.createElement("tr");
      this.parsedAnswer.push([]);
      for (let j = 0; j < this.settings.answerCols; ++j) {
        let h = answerPos;
        for (; h < answer.length && answer[h]; ++h);
        this.parsedAnswer[i].push(textDecoder.decode(answer.subarray(answerPos, h)));
        answerPos = Math.min(answer.length, h + 1);

        const cell = (
          <input type="text" class="form-control btn-no-shadow" value={this.parsedAnswer[i][j]} />
        ).asElement() as HTMLInputElement;
        row.appendChild((<td class="column-200px">{cell}</td>).asElement());
        cell.addEventListener("change", () => {
          this.parsedAnswer![i][j] = cell.value;
          this.updateComputedAnswer();
        });
      }
      rows.push(<>{row}</>);
    }

    return (
      <div class="d-flex align-items-end">
        <div class="d-inplace-block border rounded focusable">
          <table class="table answer-table table-external-border mb-0">
            <tbody>{rows}</tbody>
          </table>
        </div>
        <ButtonIcon settings={{ title: "Изменить размер", icon: "icon-grid-resize", margins: [0, 0, 0, 5] }} />
      </div>
    ).asElement() as HTMLDivElement;
  }

  updateComputedAnswer(): void {
    let length = 0;
    for (let i = 0; i < this.settings.answerRows; ++i) {
      for (let j = 0; j < this.settings.answerCols; ++j) {
        length += this.parsedAnswer![i][j].length + 1;
      }
    }
    const result = new Uint8Array(length - 1);
    length = 0;
    for (let i = 0; i < this.settings.answerRows; ++i) {
      for (let j = 0; j < this.settings.answerCols; ++j) {
        textEncoder.encodeInto(this.parsedAnswer![i][j], result.subarray(length));
        length += this.parsedAnswer![i][j].length + 1;
      }
    }
    this.settings.answer = result;
  }
}

import { Modal } from "bootstrap";

import { Component } from "components/component";
import { ButtonIcon } from "components/button-icon";

import * as jsx from "jsx";

type AnswerSettings = {
  answerRows: number;
  answerCols: number;
  answer: Uint8Array;
};

const textDecoder = new TextDecoder();
const textEncoder = new TextEncoder();

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
    // Fix answer string if needed
    this.updateComputedAnswer();

    const refs: Element[] = [];
    const elem = (
      <div class="d-flex align-items-end">
        <div ref class="modal" tabindex="-1">
          <div class="modal-dialog modal-dialog-centered">
            <div class="modal-content">
              <div class="modal-header">
                <h5 class="modal-title">Размер ответа</h5>
                <button
                  type="button"
                  class="btn-close btn-no-shadow"
                  data-bs-dismiss="modal"
                  aria-label="Close"
                ></button>
              </div>
              <div class="modal-body">
                <form ref novalidate>
                  <div class="input-group has-validation mb-2">
                    <input ref class="form-control" type="number" min="1" max="10" step="1" required />
                    <span class="input-group-text">&times;</span>
                    <input ref class="form-control" type="number" min="1" max="10" step="1" required />
                    <button type="submit" class="btn btn-primary">
                      Изменить
                    </button>
                    <div class="invalid-feedback">Размерность должна быть натуральным числом, не превосходящим 10.</div>
                  </div>
                </form>
              </div>
            </div>
          </div>
        </div>
        <div class="d-inplace-block border rounded focusable">
          <table class="table answer-table table-external-border mb-0">
            <tbody>{rows}</tbody>
          </table>
        </div>
        <ButtonIcon
          settings={{
            title: "Изменить размер",
            icon: "icon-grid-resize",
            onClick: (): void => {
              rowInput.value = this.settings.answerRows.toString();
              colInput.value = this.settings.answerCols.toString();
              dimsModal.show();
            },
            margins: [0, 0, 0, 5],
          }}
        />
      </div>
    ).asElement(refs) as HTMLDivElement;

    const [modal, form, rowInput, colInput] = refs as [
      HTMLDivElement,
      HTMLFormElement,
      HTMLInputElement,
      HTMLInputElement
    ];

    const dimsModal = new Modal(modal);
    modal.addEventListener("shown.bs.modal", () => {
      rowInput.focus();
    });
    modal.addEventListener("hidden.bs.modal", () => {
      form.classList.remove("was-validated");
    });

    form.addEventListener("submit", (e) => {
      if (form.checkValidity()) {
        this.settings.answerRows = parseInt(rowInput.value, 10);
        this.settings.answerCols = parseInt(colInput.value, 10);
        dimsModal.hide();
        this.rerenderMe();
      } else {
        form.classList.add("was-validated");
      }
      e.preventDefault();
      e.stopPropagation();
    });
    return elem;
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

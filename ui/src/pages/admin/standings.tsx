import * as jsx from "jsx";

import { assert } from "utils/assert";

import { toggleLoadingScreen, showInternalErrorScreen } from "utils/common";
import { requestU } from "utils/requests";
import { Router, Page } from "utils/router";

import { Jobs } from "proto/jobs_pb";
import { KimListResponse } from "proto/kims_pb";
import { getGroups } from "admin";
import { StandingsRequest, StandingsResponse, SubmissionSummaryRequest, SubmissionSummaryResponse } from "proto/standings_pb";

import { ButtonIcon } from "components/button-icon";
import { factoryOf, ListComponent, ListEntry, listProviderOf } from "components/lists";

import { requireAuth } from "pages/common";

class StandingsPage extends Page {
  static URL = "admin/standings" as const;
  static CATEGORY = "standings" as const;

  updateTimer?: number;
  updateStandings?: number;

  override async mount(): Promise<void> {
    requireAuth(1);

    const addCell = (gridRow: number, centerLeft = false, isHeader = false) => {
      const elem = document.createElement("div");
      if (centerLeft) {
        elem.classList.add("text-start");
      }
      if (isHeader) {
        elem.classList.add("header");
      }
      elem.style.gridRow = gridRow.toString();
      standingsElem.appendChild(elem);
      return elem;
    };

    let syncTag = 0;
    let lastUserUpdateTime = 0;

    const updateTime = () => {
      if (standingsUpdateLock) {
        return;
      }

      if (lastUserUpdateTime === 0) {
        updateTimeSpan.innerText = "N/A";
      } else {
        const timeSinceLastUpdate = Math.floor((Date.now() - lastUserUpdateTime) / 10000) * 10;
        if (timeSinceLastUpdate > 0) {
          updateTimeSpan.innerText = timeSinceLastUpdate + " секунд назад";
        } else {
          updateTimeSpan.innerText = "сейчас";
        }
      }
    };

    let problemIds: number[] = [];
    let problems: Map<number, [string, number]> = new Map;
    let participants: Map<number, {
      position: number,
      name: string,
      elements: HTMLDivElement[],
      total: number,
      scores: (number | undefined)[],
      hasSubmissions: boolean
    }> = new Map;
    const reallyUpdateStandings = async (fromChange: boolean = true): Promise<void> => {
      if (fromChange) {
        syncTag = 0;
      }
      if (groupSelect.value === "" || kimSelect.value === "") {
        tableWrap.setAttribute("hidden", "");
        syncTag = 0;
        lastUserUpdateTime = 0;
      } else {
        tableWrap.removeAttribute("hidden");

        try {
          const request = new StandingsRequest().setGroupId(parseInt(groupSelect.value)).setKimId(parseInt(kimSelect.value)).setSyncTag(syncTag);

          const savedSyncTag = syncTag;
          syncTag = 1;
          const response = (await requestU(StandingsResponse, "/api/admin/standings", request)).toObject();
          if (!syncTag) {
            assert(requestedWhileBlocking);
            return;
          }
          syncTag = savedSyncTag;

          if (!syncTag) {
            standingsElem.innerHTML = "";
            addCell(1, false, true);
            {
              const cell = addCell(1, false, true);
              cell.style.minWidth = "300px";
              cell.innerText = "Участник";
            }
            addCell(1, false, true).innerHTML = "&Sigma;";

            problemIds = [];
            problems = new Map;
            for (const [pos, {id, name}] of response.tasksList.entries()) {
              problemIds.push(id);
              problems.set(id, [name, pos]);
              addCell(1, false, true).innerText = name;
            }

            participants = new Map;
            for (const {id, name} of response.usersList) {
              const scores: undefined[] = [];
              scores.length = problemIds.length;
              participants.set(id, {position: -1, name: name, elements: [], total: 0, hasSubmissions: false, scores: scores});
            }
          }

          for (const {score, taskId, userId} of response.submissionsList) {
            const problemPos = problems.get(taskId)![1];
            const participant = participants.get(userId)!;
            const scoresArray = participant.scores;

            participant.total -= scoresArray[problemPos] ?? 0;
            scoresArray[problemPos] = score;
            participant.total += score;
            participant.hasSubmissions = true;
          }

          const positions: [number, number, number][] = [];
          for (const [id, participant] of participants) {
            if (participant.hasSubmissions) {
              positions.push([participant.total, id, participant.position]);
            }
          }
          positions.sort((a, b) => b[0] - a[0]);

          for (const [new_position, [total, id, old_position]] of positions.entries()) {
            const participant = participants.get(id)!;
            const elems = participant.elements;

            if (new_position !== old_position) {
              if (elems.length) {
                for (const elem of elems) {
                  elem.style.gridRow = (new_position + 2).toString();
                }
              } else {
                elems.push(addCell(new_position + 2));
                elems.push(addCell(new_position + 2, true));
                elems.push(addCell(new_position + 2));
                for (const [i, problem] of problemIds.entries()) {
                  elems.push(addCell(new_position + 2));
                }
                elems[1].innerText = participant.name;
              }

              elems[0].innerText = (new_position + 1).toString();
              participant.position = new_position;
            }
            elems[2].innerText = total.toString();
          }

          for (const {score, taskId, userId} of response.submissionsList) {
            const problemPos = problems.get(taskId)![1];
            const participant = participants.get(userId)!;
            participant.elements[problemPos + 3].innerText = score.toString();
          }

          syncTag = response.syncTag;
          lastUserUpdateTime = Date.now();
        } catch (e) {
          showInternalErrorScreen(e);
        }
      }
    };

    let standingsUpdateLock = false;
    let requestedWhileBlocking = false;
    const updateStandings = async (fromChange: boolean = true): Promise<void> => {
      this.params.set("gid", groupSelect.value);
      this.params.set("id", kimSelect.value);
      Router.instance.updateUrl();

      if (standingsUpdateLock) {
        requestedWhileBlocking = true;
        return;
      }
      standingsUpdateLock = true;

      requestedWhileBlocking = true;
      while (requestedWhileBlocking) {
        requestedWhileBlocking = false;
        reallyUpdateStandings(fromChange);
      }
      standingsUpdateLock = false;
      updateTime();
    };

    const [kimSelect, groupSelect, updateTimeSpan, tableWrap, standingsElem] = (
      <>
        <h2>Результаты</h2>

        <div class="row">
          <div class="col-md-5">
            <select ref class="form-select col-md-5" onchange={() => updateStandings(true)}>
              <option value=""></option>
            </select>
          </div>
          <div class="col-md-5">
            <select ref class="form-select col-md-5" onchange={() => updateStandings(true)}>
              <option value=""></option>
            </select>
          </div>
          <div class="col-md-2 text-end">
            <ButtonIcon
              settings={{
                title: "Обновить",
                icon: "icon-refresh",
                onClick: () => {
                  syncTag = 0;
                  updateStandings(true);
                }
              }}
            />
          </div>
        </div>
        <div class="row">
          <small>Обновлено: <span ref></span> <span style="color: rgba(173, 181, 189, 0.75);">(автоматические обновления не поддерживают перепроверку или изменение списка задач)</span></small>
        </div>

        <div ref class="row mt-3">
          <div class="col">
            <div class="border rounded-top overflow-auto">
              <div ref class="standings-table" />
            </div>
          </div>
        </div>
      </>
    ).replaceContentsOf("main") as [HTMLSelectElement, HTMLSelectElement, HTMLSpanElement, HTMLDivElement, HTMLDivElement];

    const kimPromise = requestU(KimListResponse, "/api/kim/list");
    const groupsPromise = getGroups();

    for (const kim of (await kimPromise).getKimsList()) {
      kimSelect.appendChild((<option value={kim.getId()}>{kim.getName()}</option>).asElement());
    }
    for (const [id, name] of await groupsPromise) {
      groupSelect.appendChild((<option value={id}>{name}</option>).asElement());
    }

    kimSelect.value = this.params.get("id") ?? "";
    groupSelect.value = this.params.get("gid") ?? "";

    await updateStandings();
    this.updateTimer = setInterval(updateTime, 300);
    this.updateStandings = setInterval(() => void updateStandings(false), 60000);

    toggleLoadingScreen(false);
  }

  override async unmount(): Promise<boolean> {
    if (this.updateTimer) {
      clearInterval(this.updateTimer);
    }
    if (this.updateStandings) {
      clearInterval(this.updateStandings);
    }
    return true;
  }
}

Router.instance.registerPage(StandingsPage);

import * as jsx from "jsx";

import { getPrintableParts, showInternalErrorScreen, toggleLoadingScreen } from "utils/common";
import { requestU } from "utils/requests";
import { createLink, Page, Router } from "utils/router";

import { StatusResponse } from "proto/api_pb";
import { ContestantKim, ContestantKimList, ParticipationStatus } from "proto/contestant_pb";

import { Component } from "components/component";

import { requireAuth } from "pages/common";

class KimEntry extends Component<ContestantKim.AsObject & { timeOffset: number }> {
  createElement(): HTMLElement {
    const formatDuration = (duration: number): string => {
      duration = Math.floor(duration / 1000);
      return (
        Math.floor(duration / 3600).toString() +
        ":" +
        (Math.floor(duration / 60) % 60).toString().padStart(2, "0") +
        ":" +
        (duration % 60).toString().padStart(2, "0")
      );
    };

    let lastStatus = 1;
    const updateCountdown = (): void => {
      const current = Date.now() + this.settings.timeOffset;
      if (this.settings.startTime <= current && current < this.settings.endTime) {
        if (this.settings.isVirtual && this.settings.status !== ParticipationStatus.IN_PROGRESS) {
          status.innerText = "Доступно";
        } else {
          status.innerText = "В процессе";
        }
        enterLink.removeAttribute("hidden");
        countdown.innerText = formatDuration(this.settings.endTime - Date.now());
        if (lastStatus === 0 && !this.settings.isVirtual) {
          Router.instance.goTo("solve?id=" + this.settings.id);
        }
        lastStatus = 1;
      } else if (current < this.settings.startTime) {
        status.innerText = "До начала";
        enterLink.setAttribute("hidden", "");
        countdown.innerText = formatDuration(this.settings.startTime - Date.now());
        lastStatus = 0;
      } else {
        status.innerText = "Завершено";
        enterLink.setAttribute("hidden", "");
        countdown.innerText = "";
        lastStatus = 2;
      }
    };

    const [year, month, date, hour, minute, second] = getPrintableParts(
      new Date(this.settings.startTime)
    );

    const status = document.createElement("span");
    const countdown = (<span class="text-secondary"></span>).asElement() as HTMLSpanElement;
    const enterLink = (
      <a hidden class="ms-3" style="font-size: .9rem;">
        {this.settings.isVirtual && this.settings.status !== ParticipationStatus.IN_PROGRESS
          ? "Виртуальное участие"
          : "Войти"}{" "}
        »
      </a>
    ).asElement() as HTMLAnchorElement;
    createLink(enterLink, "solve?id=" + this.settings.id);

    updateCountdown();

    const intervalId = setInterval(updateCountdown, 200);
    this.unmountHooks ??= [];
    this.unmountHooks.push(() => void clearInterval(intervalId));

    return (
      <tr>
        <td>
          {this.settings.name}
          <br />
          {enterLink}
        </td>
        <td>
          {`${date}.${month}.${year}`}
          <br />
          {`${hour}:${minute}:${second}`}
        </td>
        <td>
          {formatDuration(
            Math.min(this.settings.duration, this.settings.endTime - this.settings.startTime)
          )}
        </td>
        <td>
          {status}
          <br />
          {countdown}
        </td>
      </tr>
    ).asElement() as HTMLElement;
  }
}

class MainPage extends Page {
  static URL = "" as const;

  comps?: KimEntry[];

  override async mount(): Promise<void> {
    requireAuth();

    try {
      const serverTime = (await requestU(StatusResponse, "/api/status")).getServerTime();
      const timeOffset = serverTime - Date.now();

      const list = await requestU(ContestantKimList, "/api/contestant/available-kims");
      this.comps = list
        .toObject()
        .kimsList.map((kim) => new KimEntry(Object.assign(kim, { timeOffset: timeOffset }), null));

      (
        <div class="border rounded">
          <table
            class={"table table-fixed table-no-sep table-first-col-left table-external-border mb-0"}
          >
            <thead>
              <tr>
                <td class="column-norm">Название</td>
                <td class="column-120px">Время</td>
                <td class="column-120px">Длит.</td>
                <td class="column-120px"></td>
              </tr>
            </thead>
            <tbody>
              {this.comps.map((comp) => (
                <>{comp.elem}</>
              ))}
            </tbody>
          </table>
        </div>
      ).replaceContentsOf("main");
    } catch (e) {
      showInternalErrorScreen(e);
    }

    toggleLoadingScreen(false);
  }

  override async unmount(): Promise<boolean> {
    if (this.comps) {
      for (const comp of this.comps) {
        comp.unmount();
      }
    }
    return true;
  }
}

Router.instance.registerPage(MainPage);

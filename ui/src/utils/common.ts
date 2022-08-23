import { Mutex } from "async-mutex";

import { assert } from "utils/assert";
import { requestU } from "utils/requests";

import { IdRangeResponse } from "proto/user_pb";

export class ExpectedError extends Error {}

let loadingScreenShown: boolean = false;
let loadingScreenElem: HTMLDivElement | null = null;
let loadingScreenText: HTMLSpanElement | null = null;

const LOADING_REASONS = {
  loading: "Загрузка",
  executing: "Выполняется",
  login: "Выполняется вход",
  reservingIDs: "Reserving IDs",
};

export type LoadingReason = keyof typeof LOADING_REASONS;

export function toggleLoadingScreen(
  shown: boolean = true,
  reason: LoadingReason = "loading"
): void {
  if (!loadingScreenElem || !loadingScreenText) {
    loadingScreenElem = document.getElementById("loadingScreen") as HTMLDivElement;
    loadingScreenText = document.getElementById("loadingScreenText") as HTMLDivElement;
  }
  if (shown !== loadingScreenShown) {
    if (!shown) {
      loadingScreenElem.setAttribute("hidden", "");
    } else {
      loadingScreenElem.removeAttribute("hidden");
    }
    loadingScreenShown = shown;
  }
  if (shown) {
    loadingScreenText.innerText = LOADING_REASONS[reason];
  }
}

export function showInternalErrorScreen(e: any): void {
  if (e.prototype === ExpectedError) {
    return;
  }
  alert(`Произошла проблема, решить которую мы не в силах :(
Возможно, требуется перезагрузить страницу.

Техническая информация:
${e.stack ?? e}
`);
}

let idRangeStart = 0;
let idRangeEnd = 0;
const dbIdMutex = new Mutex();

export async function dbId(): Promise<number> {
  if (idRangeStart !== idRangeEnd) {
    return idRangeStart++;
  }

  const shouldOpenLoadingScreen = !loadingScreenShown;
  if (shouldOpenLoadingScreen) {
    toggleLoadingScreen(true, "reservingIDs");
  }
  await dbIdMutex.runExclusive(async (): Promise<void> => {
    try {
      ({ start: idRangeStart, end: idRangeEnd } = (
        await requestU(IdRangeResponse, "/api/user/request-id-range")
      ).toObject());
    } catch (e) {
      showInternalErrorScreen(e);
    }
  });
  if (shouldOpenLoadingScreen) {
    toggleLoadingScreen(false, "reservingIDs");
  }

  assert(idRangeStart < idRangeEnd);
  return await dbId();
}

export function clearDbIdCache(): void {
  (idRangeStart = 0), (idRangeEnd = 0);
}

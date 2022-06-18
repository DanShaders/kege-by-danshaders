export class ExpectedError extends Error {}

let loadingScreenShown: boolean = false;
let loadingScreenElem: HTMLDivElement | null = null;
let loadingScreenText: HTMLSpanElement | null = null;

const LOADING_REASONS = {
  loading: "Загрузка",
  executing: "Выполняется",
  login: "Выполняется вход",
};

export type LoadingReason = keyof typeof LOADING_REASONS;

export function toggleLoadingScreen(shown: boolean = true, reason: LoadingReason = "loading"): void {
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

export function dbId(): number {
  return Math.round(new Date().getTime() * 1000 + Math.random() * 1_000_000);
}

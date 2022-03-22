import { RedirectNotification } from "./router";

export class ExpectedError extends Error {}

let loadingScreenShown: boolean = false;
let loadingScreenElem: HTMLDivElement | null = null;

let handlerInProgress = false;
export const toggleLoadingScreen = (shown: boolean = true): void => {
  if (shown !== loadingScreenShown) {
    if (!loadingScreenElem) {
      loadingScreenElem = document.createElement("div");
      loadingScreenElem.classList.add("loading-screen");
      document.body.appendChild(loadingScreenElem);
    }
    if (!shown) {
      loadingScreenElem.classList.add("hidden");
    } else {
      loadingScreenElem.classList.remove("hidden");
    }
    loadingScreenShown = shown;
  }
};

type Callback = (event: Event) => void;
type NullableCallback = Callback | null | undefined;
type HandlerResult = void | boolean | Promise<void | boolean>;

type FormCallbacks = {
  reportUnexpectedError: (error: any) => void;
  defaultSuccessfulSubmitCallback: Callback | undefined;
};

export const formCallbacks: FormCallbacks = {
  reportUnexpectedError: (e: any) => {
    if (e.prototype === ExpectedError || e instanceof RedirectNotification) {
      return;
    }
    try {
      throw Error("Not implemented");
    } catch (e2) {
      alert(`Houston, we have a problem.
Мы пытались отобразить красивое сообщение об ошибке, но у нас не получилось.
Возможно, требуется перезагрузить страницу.

Техническая информация:
Rendering the following error message:
${e.stack}
caused UI to produce yet another exception:
${e2}
`);
      console.error(e);
      console.error(e2);
    }
  },
  defaultSuccessfulSubmitCallback: undefined,
};

export async function setupFormEventHandler(
  event: Event,
  elem: HTMLElement,
  submitListener: (e: Event) => HandlerResult,
  hideLoadingOnFinish: boolean = false,
  successfulSubmitCallback: NullableCallback
): Promise<void> {
  if (handlerInProgress) throw new Error("Another event handler is in progress");
  handlerInProgress = true;
  toggleLoadingScreen(true);
  let isRedirect = false;
  try {
    let result = submitListener(event);
    if (result instanceof Promise) {
      result = await result;
    }
    if (result !== false && elem instanceof HTMLFormElement) {
      elem.reset();
    }
    if (successfulSubmitCallback !== null) {
      let callback = formCallbacks.defaultSuccessfulSubmitCallback;
      if (successfulSubmitCallback !== undefined) callback = successfulSubmitCallback;
      if (callback !== undefined) await callback(event);
    }
  } catch (e) {
    if (e instanceof RedirectNotification) {
      isRedirect = true;
    } else if (!(e instanceof ExpectedError)) {
      console.error(e);
      formCallbacks.reportUnexpectedError(e);
    }
  } finally {
    handlerInProgress = false;
    if (hideLoadingOnFinish && !isRedirect) {
      toggleLoadingScreen(false);
    }
  }
}

export function setupForm(
  elem: HTMLElement,
  submitListener: (e: Event) => HandlerResult,
  hideLoadingOnFinish: boolean = true,
  successfulSubmitCallback?: NullableCallback
): void {
  elem.classList.remove("hidden");
  elem.setAttribute("autocomplete", "off");
  let eventName = "submit";
  if (elem instanceof HTMLButtonElement) eventName = "click";
  if (elem instanceof HTMLFormElement)
    elem.onsubmit = (): boolean => {
      return false;
    };
  if (elem instanceof HTMLInputElement) eventName = "change";
  elem.addEventListener(eventName, async (event): Promise<void> => {
    await setupFormEventHandler(event, elem, submitListener, hideLoadingOnFinish, successfulSubmitCallback);
  });
}

window.addEventListener("unhandledrejection", (e): void => {
  e = e.reason;
  formCallbacks.reportUnexpectedError(e);
});

window.addEventListener("error", (e): void => {
  e = e.error;
  formCallbacks.reportUnexpectedError(e);
});

export const byId = (id: string): HTMLElement | null => document.getElementById(id);

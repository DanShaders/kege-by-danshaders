import { toggleLoadingScreen, LoadingReason } from "./common";
import { nonNull } from "./assert";

type Handler = (params: URLSearchParams) => Promise<void>;

export class RedirectNotification {}
export class RouteNotFoundError extends Error {}

export abstract class Page {
  params: URLSearchParams;

  constructor(params: URLSearchParams) {
    this.params = params;
  }

  abstract mount(): Promise<void>;

  unload(): boolean {
    return true;
  }

  async unmount(): Promise<boolean> {
    return this.unload();
  }
}

type PageClass = {
  URL: string;
  new (params: URLSearchParams): Page;
};

export class Router {
  private static _instance: Router;
  private publicRoutes: Map<string, Handler>;
  private privateRoutes: Map<string, Handler>;

  currentURL: string = "";
  currentPage: string = "";

  pageInstance?: Page;

  private static locationChangeListener(e: Event): void {
    e.preventDefault();
    Router.instance.redirect(location.pathname.substr(1) + location.search, false, true);
  }

  registerListener(): void {
    window.addEventListener("popstate", Router.locationChangeListener);
    window.addEventListener("beforeunload", (e): boolean => {
      if (this.pageInstance) {
        if (!this.pageInstance.unload()) {
          e.preventDefault();
          return (e.returnValue = true);
        }
      }
      return false;
    });
  }

  private constructor() {
    this.publicRoutes = new Map();
    this.privateRoutes = new Map();
  }

  static get instance(): Router {
    return this._instance || (this._instance = new Router());
  }

  addRoute(page: string, handler: Handler, isPrivate: boolean = false): void {
    if (isPrivate) {
      this.privateRoutes.set(page, handler);
    } else {
      this.publicRoutes.set(page, handler);
    }
  }

  setUrl(url: string): void {
    history.replaceState(undefined, "", "/" + url);
    this.currentURL = url;
    this.currentPage = url.split("?", 2)[0];
  }

  async goTo(
    url: string,
    isPrivate: boolean = true,
    historyUpdated = false,
    reason: LoadingReason = "loading"
  ): Promise<void> {
    toggleLoadingScreen(true, reason);

    if (this.pageInstance) {
      if (!(await this.pageInstance.unmount())) {
        toggleLoadingScreen(false);
        location.hash = this.currentURL;
        return;
      }
      this.pageInstance = undefined;
    }

    const [page, params] = url.split("?", 2);
    let handler = this.publicRoutes.get(page);

    if (handler && !historyUpdated) {
      history.pushState(undefined, "", "/" + url);
    }
    if (isPrivate) {
      handler ??= this.privateRoutes.get(page);
    }
    if (handler) {
      this.currentURL = url;
      this.currentPage = page;
      await handler(new URLSearchParams(params));
    } else {
      if (page === "404") {
        throw new RouteNotFoundError(`Route for '${url}' is not found`);
      } else {
        history.pushState(undefined, "", "/" + url);
        this.goTo("404?url=" + encodeURIComponent(url));
      }
    }
  }

  redirect(url: string, isPrivate: boolean = true, historyUpdated = false, reason: LoadingReason = "loading"): never {
    setTimeout(async () => {
      await this.goTo(url, isPrivate, historyUpdated, reason);
    }, 0);
    throw new RedirectNotification();
  }

  updateUrl(): void {
    this.setUrl(this.currentPage + "?" + nonNull(this.pageInstance).params.toString());
  }

  registerPage<T extends PageClass>(PageClass: T): void {
    this.addRoute(PageClass.URL, async (params): Promise<void> => {
      this.pageInstance = new PageClass(params);
      await this.pageInstance.mount();
    });
  }
}

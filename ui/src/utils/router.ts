import { toggleLoadingScreen } from "../utils/common";

type Handler = (params: URLSearchParams) => Promise<void>;

export class RedirectNotification {}
export class RouteNotFoundError extends Error {}

export class Router {
  private static _instance: Router;
  private publicRoutes: Map<string, Handler>;
  private privateRoutes: Map<string, Handler>;

  currentURL: string = "";
  currentPage: string = "";

  private static hashChangeListener() {
    Router.instance.redirect(location.hash.substr(1), false, true);
  }

  registerListener() {
    window.addEventListener("hashchange", Router.hashChangeListener);
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
    history.replaceState(undefined, "", "#" + url);
    this.currentURL = url;
    this.currentPage = url.split("?", 2)[0];
  }

  async goTo(url: string, isPrivate: boolean = true, historyUpdated = false): Promise<void> {
    toggleLoadingScreen(true);
    const [page, params] = url.split("?", 2);
    let handler = this.publicRoutes.get(page);

    if (handler && !historyUpdated) {
      history.pushState(undefined, "", "#" + url);
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
        this.goTo("404?url=" + encodeURIComponent(url));
      }
    }
  }

  redirect(url: string, isPrivate: boolean = true, historyUpdated = false): never {
    setTimeout(async () => {
      await this.goTo(url, isPrivate, historyUpdated);
    }, 0);
    throw new RedirectNotification();
  }
}

import { toggleLoadingScreen } from "../utils/common";

type Handler = (params: URLSearchParams) => Promise<void>;

export class RedirectNotification {}
export class RouteNotFoundError extends Error {}

export class Router {
  private static _instance: Router;
  private public_routes: Map<string, Handler>;
  private private_routes: Map<string, Handler>;

  currentURL: string = "";

  private constructor() {
    this.public_routes = new Map();
    this.private_routes = new Map();
  }

  static get instance(): Router {
    return this._instance || (this._instance = new Router());
  }

  addRoute(page: string, handler: Handler, isPrivate: boolean = false): void {
    if (isPrivate) {
      this.private_routes.set(page, handler);
    } else {
      this.public_routes.set(page, handler);
    }
  }

  setUrl(url: string): void {
    location.hash = url;
  }

  async goTo(url: string, isPrivate: boolean = true): Promise<void> {
    toggleLoadingScreen(true);
    const [page, params] = url.split("?", 2);
    let handler = this.public_routes.get(page);
    if (handler) {
      location.hash = url;
    }
    if (isPrivate) {
      handler ??= this.private_routes.get(page);
    }
    if (handler) {
      Router.instance.currentURL = url;
      await handler(new URLSearchParams(params));
    } else {
      throw new RouteNotFoundError(`Route for '${url}' is not found`);
    }
  }

  redirect(url: string, isPrivate: boolean = true): never {
    setTimeout(async () => {
      await this.goTo(url, isPrivate);
    }, 0);
    throw new RedirectNotification();
  }
}

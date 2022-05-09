import { Router } from "utils/router";
import { HeaderComponent, HeaderSettings } from "components/header";
import { UserInfo, ErrorCode } from "proto/api_pb";
import { request } from "utils/requests";
import { toggleLoadingScreen } from "utils/common";

export let userInfo: UserInfo.AsObject | undefined | null;

export function setGlobalUserInfo(userInfo_: typeof userInfo): void {
  userInfo = userInfo_;
}

export function requireAuth(perms: number = 0): void {
  if (!userInfo) {
    Router.instance.redirect("login");
  } else {
    if ((userInfo.perms & perms) !== perms) {
      throw new Error("Access denied");
    }
  }
}

let headerWrap: HTMLElement;
export let header: HeaderComponent | undefined;

const headerSettingsLoggedOut = {
  highlightedId: "",
  nav: {
    items: [],
    moreText: "",
  },
  profile: {
    show: false,
  },
};

async function updateHeader(): Promise<void> {
  if (!header) {
    header = new HeaderComponent(headerSettingsLoggedOut, null);
  }
  if (userInfo) {
    let headerSettings: HeaderSettings = {
      ...headerSettingsLoggedOut,
      profile: {
        show: true,
        name: userInfo.displayName,
        username: userInfo.username,
      },
    };
    if (userInfo.perms & 1) {
      toggleLoadingScreen(true, "loading");
      const adminSpecific = await import("../admin");
      headerSettings = {
        ...headerSettings,
        ...adminSpecific.headerSettings,
      };
    }
    header.apply(headerSettings);
  } else {
    header.apply(headerSettingsLoggedOut);
  }
  headerWrap.appendChild(header.elem);
}

async function init(): Promise<void> {
  if (userInfo === undefined) {
    const [code, result] = await request(UserInfo, "/api/user/info");
    if (code === ErrorCode.ACCESS_DENIED) {
      userInfo = null;
    } else if (code === ErrorCode.OK && result) {
      userInfo = result.toObject();
    }
  }
  headerWrap = document.getElementById("page-header-wrap")!;
  await Router.instance.goTo("#update-header");
  Router.instance.registerListener();
}

Router.instance.addRoute("#init", init, true);
Router.instance.addRoute("#update-header", updateHeader, true);

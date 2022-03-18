import { Router } from "../utils/router";
import { setupForm, toggleLoadingScreen, byId } from "../utils/common";
import { request, requestU, EmptyPayload } from "../utils/requests";
import { userInfo, setGlobalUserInfo } from "./common";
import { Notice, NoticeComponent, NoticeSettings } from "../components/notice";
import { ErrorCode, UserInfo } from "../proto/api_pb";
import { LoginRequest, LogoutRequest } from "../proto/user_pb";
import * as jsx from "../utils/jsx";

async function showLoginPage(params: URLSearchParams): Promise<void> {
  if (userInfo) {
    Router.instance.redirect("");
  }

  let noticeSettings: NoticeSettings = {shown: false, message: "", type: "error"};
  if (params.has("logged_out")) {
    noticeSettings = { shown: true, message: "Выход успешно осуществлен", type: "info" };
    params.delete("logged_out");
    Router.instance.setUrl("login?" + params.toString());
  }

  const [notice, loginForm, loginField, passwordField] = (
    <>
      <div id="notice-wrap" style="padding-top: 20px;">
        <Notice ref settings={noticeSettings} parent={null}></Notice>
      </div>
      <h1 id="page-title">kege-by-danshaders</h1>
      <div id="login-container">
        <form ref>
          <label for="login-field">Логин</label>
          <input ref required class="focusable" id="login-field" type="text" name="login" spellcheck="false" />
          <label for="password-field">Пароль</label>
          <input ref required class="focusable" id="password-field" type="password" name="password" />
          <button class="button-blue" id="button-login" type="submit" name="button_login" value="1">
            Войти
          </button>
        </form>
      </div>
    </>
  ).replaceContentsOf("main") as [NoticeComponent, HTMLFormElement, HTMLInputElement, HTMLInputElement];

  setupForm(loginForm, async (): Promise<boolean> => {
    const data = new LoginRequest()
      .setUsername(loginField.value)
      .setPassword(passwordField.value);
    const [code, result] = await request(UserInfo, "/api/user/login", data);

    if (code !== ErrorCode.OK || !result) {
      if (code === ErrorCode.INVALID_CREDENTIALS) {
        notice.setMessage("Неверный логин или пароль");
      } else {
        notice.setMessage(`Ошибка при обращении к API (code: ${code})`);
      }
      notice.setType("error");
      notice.setVisibility(true);
    } else {
      setGlobalUserInfo(result.toObject());
      await Router.instance.goTo("#update-header");
      Router.instance.redirect("");
    }
    return false;
  });

  toggleLoadingScreen(false);
}

async function logout(): Promise<void> {
  if (!userInfo) {
    throw new Error("No userInfo");
  }
  const data = new LogoutRequest().setUserId(userInfo.userId);
  await requestU(EmptyPayload, "/api/user/logout", data);
  setGlobalUserInfo(null);
  await Router.instance.goTo("#update-header");
  Router.instance.redirect("login?logged_out");
}

Router.instance.addRoute("login", showLoginPage);
Router.instance.addRoute("#logout", logout, true);

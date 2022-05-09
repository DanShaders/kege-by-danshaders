import { Router } from "../utils/router";
import { setupForm, toggleLoadingScreen } from "../utils/common";
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

  let noticeSettings: NoticeSettings = { shown: false, message: "", type: "error" };
  if (params.has("logged_out")) {
    noticeSettings = { shown: true, message: "Выход успешно осуществлен", type: "info" };
    params.delete("logged_out");
    Router.instance.setUrl("login?" + params.toString());
  }

  const [notice, loginForm, loginField, passwordField] = (
    <>
      <div id="notice-wrap" class="mt-5">
        <Notice ref settings={noticeSettings}></Notice>
      </div>
      <div class="row">
        <div class="col-sm-auto mb-4">
          <h2 class="mb-0">Станция КЕГЭ</h2>
          <span class="ms-3" style="color: var(--bs-gray);">
            ФМЛ №31 г. Челябинска
          </span>
        </div>
        <div class="col-sm justify-self-end">
          <div id="signin" class="border rounded p-3 ms-auto" style="max-width: 330px;">
            <form ref>
              <div class="form-floating">
                <input
                  ref
                  required
                  class="form-control"
                  id="loginField"
                  type="text"
                  spellcheck="false"
                  placeholder="&nbsp;"
                />
                <label for="loginField">Логин</label>
              </div>

              <div class="form-floating mb-2">
                <input ref required class="form-control" id="passwordField" type="password" placeholder="&nbsp;" />
                <label for="passwordField">Пароль</label>
              </div>

              <input class="w-100 btn btn-primary btn-floating p-1" id="button-login" type="submit" value="Войти" />
            </form>
          </div>
        </div>
      </div>
    </>
  ).replaceContentsOf("main") as [NoticeComponent, HTMLFormElement, HTMLInputElement, HTMLInputElement];

  setupForm(loginForm, async (): Promise<boolean> => {
    const data = new LoginRequest().setUsername(loginField.value).setPassword(passwordField.value);
    const [code, result] = await request(UserInfo, "api/user/login", data);

    if (code !== ErrorCode.OK || !result) {
      if (code === ErrorCode.INVALID_CREDENTIALS) {
        notice.setMessage("Неверный логин или пароль");
      } else {
        notice.setMessage(`Ошибка при обращении к серверу (${code})`);
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
  await requestU(EmptyPayload, "api/user/logout", data);
  setGlobalUserInfo(null);
  await Router.instance.goTo("#update-header");
  Router.instance.redirect("login?logged_out");
}

Router.instance.addRoute("login", showLoginPage);
Router.instance.addRoute("#logout", logout, true);

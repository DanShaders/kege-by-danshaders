import { Router } from "../utils/router";
import { setupForm, toggleLoadingScreen, byId } from "../utils/common";
import { request, requestU, EmptyPayload } from "../utils/requests";
import { userInfo, setGlobalUserInfo } from "./common";
import { NoticeComponent } from "../components/notice";
import { ErrorCode, UserInfo } from "../proto/api_pb";
import { LoginRequest, LogoutRequest } from "../proto/user_pb";

async function showLoginPage(params: URLSearchParams): Promise<void> {
  if (userInfo) {
    Router.instance.redirect("");
  }
  byId("main")!.innerHTML = `
    <div id='notice-wrap' style='padding-top: 20px;'></div>
	  <h1 id='page-title'>kege-by-danshaders</h1>
    <div id='login-container'>
      <form id='login-form'>
        <label for='login-field'>Логин</label>
        <input required class='focusable' id='login-field' type="text" name="login">
        <label for='password-field'>Пароль</label>
        <input required class='focusable' id='password-field' type="password" name="password">
        <button class='button-blue' id='button-login' type="submit" name="button_login" value="1">Войти</button>
      </form>
    </div>`;

  const notice = new NoticeComponent({ shown: false, message: "", type: "error" }, null);
  if (params.has("logged_out")) {
    notice.apply({ shown: true, message: "Выход успешно осуществлен", type: "info" });
    params.delete("logged_out");
    Router.instance.setUrl("login?" + params.toString());
  }
  byId("notice-wrap")!.appendChild(notice.elem);

  setupForm(byId("login-form")!, async (): Promise<boolean> => {
    const data = new LoginRequest()
      .setUsername((byId("login-field") as HTMLInputElement).value)
      .setPassword((byId("password-field") as HTMLInputElement).value);
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

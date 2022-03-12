import {requestU} from './utils/requests';
import {UserInfo, LoginRequest} from './proto/api_pb';

declare global {
  // eslint-disable-next-line no-unused-vars
  interface Window {
    auth: any;
  }
}

window.auth = async (username: string, password: string) => {
  const resp = await requestU(UserInfo, "/api/login", new LoginRequest().setUsername(username).setPassword(password));
  console.log(resp.toObject());
};

async function initApplication(): Promise<void> {
  console.log("initApplication");
}

window.addEventListener("DOMContentLoaded", initApplication);

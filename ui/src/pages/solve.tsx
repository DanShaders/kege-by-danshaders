import { requireAuth } from "./common";

import { toggleLoadingScreen } from "utils/common";
import * as jsx from "utils/jsx";
import { requestU } from "utils/requests";
import { Page, Router } from "utils/router";
import { SyncController, SynchronizablePage } from "utils/sync-controller";

import { Kim } from "proto/kims_pb";
import { Task } from "proto/tasks_pb";
// import * as diffKim from "proto/kims_pb_diff";
import * as diffTask from "proto/tasks_pb_diff";

import { ButtonIcon } from "components/button-icon";
import { Component } from "components/component";
import { KimHeaderComponent, KimHeaderSettings } from "components/kim-header";

class KimSolvePage extends Page {
  static URL = "kim/solve";

  override async mount(): Promise<void> {
    requireAuth(0);
    let headerSettings: KimHeaderSettings;
    headerSettings = {
      clock: "2020.20.20",
      kimId: "КИМ №10",
      finishExam: {
        text: "Завершить экзамен досрочно",
        url: "kim/finish?id=",
      },
    };
    const header = new KimHeaderComponent(headerSettings, null);
    console.log(header);
    (<>{header.elem}</>).replaceContentsOf("page-header-wrap");
    (<></>).replaceContentsOf("main");

    toggleLoadingScreen(false);
  }
}

Router.instance.registerPage(KimSolvePage);

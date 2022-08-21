import { Component } from "components/component";
import { Router, Page } from "utils/router";
import { toggleLoadingScreen } from "utils/common";
import { requireAuth } from "./common";
import { Kim } from "proto/kims_pb";
import { Task } from "proto/tasks_pb";
import { requestU } from "utils/requests";
// import * as diffKim from "proto/kims_pb_diff";
import * as diffTask from "proto/tasks_pb_diff";
import { ButtonIcon } from "components/button-icon";
import { SyncController, SynchronizablePage } from "utils/sync-controller";
import { KimHeaderSettings, KimHeaderComponent } from "components/kim-header";

import * as jsx from "utils/jsx";

class KimSolvePage extends Page {
	static URL = "kim/solve";

	override async mount(): Promise<void> {
		requireAuth(0);
		let headerSettings : KimHeaderSettings;
		headerSettings = {
			clock: "2020.20.20",
			kimId: "КИМ №10",
			finishExam: {
				text: "Завершить экзамен досрочно",
				url: "kim/finish?id=",
			},
		};
		let header = new KimHeaderComponent(headerSettings, null);
		console.log(header);
		(
			<>
			{header.elem}
			</>
    ).replaceContentsOf("page-header-wrap");
    (
			<>
			</>
    ).replaceContentsOf("main")

	  toggleLoadingScreen(false);
	}
}

Router.instance.registerPage(KimSolvePage);
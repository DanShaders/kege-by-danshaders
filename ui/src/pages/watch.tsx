import { Component } from "components/component";
import { Router } from "utils/router";
import { toggleLoadingScreen } from "utils/common";
import { requireAuth } from "./common";
import { Kim } from "proto/kims_pb";
import { Task } from "proto/tasks_pb";
import { requestU } from "utils/requests";
// import * as diffKim from "proto/kims_pb_diff";
import * as diffTask from "proto/tasks_pb_diff";
import { ButtonIcon } from "components/button-icon";
import { SyncController, SynchronizablePage } from "utils/sync-controller";

import * as jsx from "jsx";

class KimWatchPage extends SynchronizablePage<diffKim.DiffableKim> {
	static URL = "kim/watch";

	override async mount(): Promise<void> {
		requireAuth(0);
		(
			<>
			Watch
			</>
    ).replaceContentsOf("main");

	 	toggleLoadingScreen(false);
	}
}

Router.instance.registerPage(KimWatchPage);
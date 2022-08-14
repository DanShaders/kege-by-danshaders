import * as jsx from "jsx";

import { toggleLoadingScreen } from "utils/common";
import { requestU } from "utils/requests";
import { Router } from "utils/router";

import { Jobs } from "proto/jobs_pb";

import { ButtonIcon } from "components/button-icon";
import { factoryOf, ListComponent, ListEntry, listProviderOf } from "components/lists";

import { requireAuth } from "pages/common";

class Job extends ListEntry<Jobs.Desc.AsObject> {
  createElement(): HTMLTableRowElement {
    return (
      <tr>
        <td>{this.settings.id.toString()}</td>
        <td>{this.settings.desc}</td>
        <td>{this.settings.status}</td>
        <td>
          <ButtonIcon
            settings={{
              title: "Открыть",
              icon: "icon-open",
              href: this.settings.openLink,
              enabled: this.settings.openLink !== "",
            }}
          />
          <ButtonIcon
            settings={{
              title: "Удалить",
              icon: "icon-delete",
              hoverColor: "red",
              href: this.settings.deleteLink,
              enabled: this.settings.deleteLink !== "",
            }}
          />
        </td>
      </tr>
    ).asElement() as HTMLTableRowElement;
  }
}

async function showTaskListPage(): Promise<void> {
  requireAuth(1);

  const jobs = (await requestU(Jobs, "/api/jobs/list")).toObject().jobsList;

  const jobsSet = new ListComponent(jobs, null, listProviderOf("tbody"), factoryOf(Job));

  (
    <>
      <h2>Фоновые задачи</h2>

      <div class="border rounded">
        <table
          class="table table-fixed table-no-sep table-second-col-left table-external-border
          mb-0"
        >
          <thead>
            <tr>
              <td class="column-160px">ID</td>
              <td class="column-norm">Описание</td>
              <td class="column-100px">Статус</td>
              <td class="column-80px"></td>
            </tr>
          </thead>
          {jobsSet.elem}
        </table>
      </div>
    </>
  ).replaceContentsOf("main");

  toggleLoadingScreen(false);
}

Router.instance.addRoute("admin/jobs/list", showTaskListPage, "control");

import * as jsx from "jsx";

import { BulkSelectionChangeEvent } from "utils/events";
import { EmptyPayload, request, requestU } from "utils/requests";
import { Router } from "utils/router";
import { BulkSelectionController } from "utils/selection-controller";

import { TaskBulkDeleteRequest, TaskListRequest, TaskListResponse } from "proto/tasks_pb";

import { ButtonIcon, ButtonIconComponent } from "components/button-icon";
import { Component, createComponentFactory } from "components/component";
import { EnumerableComponent, factoryOf, ListComponent, listProviderOf } from "components/lists";

type TaskSettings = TaskListResponse.TaskEntry.AsObject;

class TaskEntry extends EnumerableComponent<
  TaskSettings,
  {},
  ListComponent<TaskSettings, TaskSelectComponent>
> {
  checkbox = (
    <input ref class="form-check-input" type="checkbox" />
  ).asElement() as HTMLInputElement;

  createElement(): HTMLTableRowElement {
    const controller = this.parent.parent.selectionController;

    if (controller.getSelection().has(this.settings.id)) {
      this.checkbox.checked = true;
    }
    this.checkbox.addEventListener("change", () => {
      controller.notifySingleSelectionChange(this.settings.id, this.checkbox.checked);
    });

    return (
      <tr>
        <td class="ps-3">{this.checkbox}</td>
        <td>{this.settings.id.toString()}</td>
        <td>{this.settings.taskType.toString()}</td>
        <td>{this.settings.tag}</td>
        <td class="tr-hover-visible">
          <ButtonIcon
            settings={{
              title: "Открыть",
              icon: "icon-open",
              href:
                `admin/tasks/edit?id=${this.settings.id}&back=` +
                encodeURIComponent(Router.instance.currentURL),
            }}
          />
          <ButtonIcon
            settings={{
              title: "Удалить",
              icon: "icon-delete",
              hoverColor: "red",
              onClick: async (): Promise<void> => {
                if (!confirm("Вы уверены, что хотите удалить задание?")) {
                  return;
                }

                this.parent.pop(this.i);
                controller.pop(this.settings.id);
                await requestU(
                  EmptyPayload,
                  "/api/tasks/bulk-delete",
                  new TaskBulkDeleteRequest().addTasks(this.settings.id)
                );
              },
            }}
          />
        </td>
      </tr>
    ).asElement() as HTMLTableRowElement;
  }
}

export class TaskSelectComponent extends Component<{}> {
  private bulkSelectCheckbox = (
    <input class="form-check-input" type="checkbox" />
  ).asElement() as HTMLInputElement;

  selectionController = new BulkSelectionController(new Set(), this.bulkSelectCheckbox);

  private loadingIndicator = (
    <tbody class="no-hover-effects">
      <tr>
        <td colspan="5">
          <span style="border-width: 2px;" class="spinner-border spinner-border-sm m-2"></span>
        </td>
      </tr>
    </tbody>
  ).asElement() as HTMLTableSectionElement;
  private loadingEpoch = 1;

  private bulkDeleteButton = (
    <ButtonIcon
      settings={{
        title: "Удалить выбранные",
        icon: "icon-delete",
        enabled: false,
        onClick: async (): Promise<void> => {
          const selection = Array.from(this.selectionController.getSelection());
          if (!confirm(`Вы уверены, что хотите удалить ${selection.length} заданий?`)) {
            return;
          }

          for (const id of selection) {
            this.selectionController.pop(id);
          }
          this.selectionController.clearSelection(false);
          await requestU(
            EmptyPayload,
            "/api/tasks/bulk-delete",
            new TaskBulkDeleteRequest().setTasksList(selection)
          );
        },
      }}
    />
  ).comp as ButtonIconComponent;

  private get tableBody(): HTMLElement {
    return this.elem.children[1] as HTMLElement;
  }

  private visibleTasks?: ListComponent<TaskSettings, TaskSelectComponent>;

  createElement(): HTMLElement {
    this.selectionController.addEventListener(BulkSelectionChangeEvent.NAME, (e) => {
      const selection = this.selectionController.getSelection();
      const elements = this.selectionController.getElements();

      if (selection.size) {
        this.bulkDeleteButton.setEnabled(true);
      } else {
        this.bulkDeleteButton.setEnabled(false);
      }

      const { isDOMReflected } = e as BulkSelectionChangeEvent;
      if (!isDOMReflected && this.visibleTasks) {
        for (const [pos, task] of Array.from(this.visibleTasks.entries()).reverse()) {
          if (elements.has(task.settings.id)) {
            (task as TaskEntry).checkbox.checked = selection.has(task.settings.id);
          } else {
            this.visibleTasks.pop(pos);
          }
        }
      }
    });

    return (
      <table class="table table-fixed table-no-sep table-forth-col-left table-external-border mb-0">
        <thead>
          <tr>
            <td class="column-40px ps-3">{this.bulkSelectCheckbox}</td>
            <td class="column-80px">ID</td>
            <td class="column-80px">Тип</td>
            <td class="column-norm">Комментарий</td>
            <td class="column-80px">
              <div style="width: 31px; display: inline-block;"></div>
              {this.bulkDeleteButton.elem}
            </td>
            <td style="width: 0px; padding: 0;" />
          </tr>
        </thead>
        {this.loadingIndicator}
      </table>
    ).asElement() as HTMLTableElement;
  }

  async setFilter(filter: string): Promise<void> {
    if (!this.loadingEpoch) {
      this.selectionController.clearSelection();
      this.selectionController.setElements(new Set());
      this.tableBody.replaceWith(this.loadingIndicator);
      this.visibleTasks = undefined;
    }

    const currentEpoch = ++this.loadingEpoch;
    const [code, rawTasks] = await request(
      TaskListResponse,
      "/api/tasks/list",
      new TaskListRequest().setFilter(filter || "1")
    );
    if (this.loadingEpoch == currentEpoch) {
      if (rawTasks instanceof TaskListResponse) {
        const tasks = rawTasks.toObject().tasksList;
        this.loadingEpoch = 0;
        this.selectionController.setElements(new Set(tasks.map((task) => task.id)));
        this.visibleTasks = new ListComponent(
          tasks,
          this,
          listProviderOf("tbody"),
          factoryOf(TaskEntry)
        );
        this.tableBody.replaceWith(this.visibleTasks.elem);
      } else {
        const errorText = rawTasks
          ? new TextDecoder().decode(rawTasks)
          : "no additional information available";
        const msg =
          "Ваш фильтр не скомпилировался или произошла другая ошибка при выполнении " +
          `запроса (code=${code}):\n${errorText}`;
        this.loadingEpoch = 0;
        this.tableBody.replaceWith(
          (
            <tbody class="no-hover-effects">
              <tr>
                <td colspan="5">
                  <pre class="mb-0" style="text-align: left;">
                    {msg}
                  </pre>
                </td>
              </tr>
            </tbody>
          ).asElement() as HTMLTableSectionElement
        );
      }
    }
  }
}

export const TaskSelect = createComponentFactory(TaskSelectComponent);

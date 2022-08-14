import * as jsx from "jsx";

import { PageCategoryUpdateEvent } from "utils/events";
import { createLink, Router } from "utils/router";

import { Component } from "components/component";

type HeaderTooltipEntries = {
  text: string;
  url: string;
}[];

type HeaderListItem = {
  id: string;
  text: string;
  items: HeaderTooltipEntries;
};

type HeaderPlainItem = {
  id: string;
  text: string;
  url: string;
};

export type HeaderSettings = {
  highlightedId: string;
  nav: {
    items: (HeaderListItem | HeaderPlainItem)[];
    moreText: string;
    moreItems?: HeaderTooltipEntries;
  };
  profile: {
    show: boolean;
    name?: string;
    username?: string;
  };
};

type TooltipFiller = (event: Event) => void;

export class HeaderComponent extends Component<HeaderSettings> {
  override createElement(): HTMLElement {
    const forceHighlightOn = (id: string): void => {
      const hlElem = elemById.get(id);
      if (hlElem === undefined) return;
      hlElem.classList.add("page-header-focused");
      hlElem.classList.remove("page-header-clickable");
    };

    const forceHighlightOff = (id: string | null): void => {
      if (id && id !== this.settings.highlightedId) {
        const hlElem = elemById.get(id);
        if (!hlElem) {
          return;
        }
        hlElem.classList.add("page-header-clickable");
        hlElem.classList.remove("page-header-focused");
      }
    };

    const defaultTooltipFillFunc = (
      items: HeaderTooltipEntries,
      clearTooltip: boolean = true
    ): TooltipFiller => {
      return () => {
        if (clearTooltip) tooltip.innerHTML = "";
        for (const item of items) {
          if (item.text !== "") {
            const elem = document.createElement("a");
            elem.classList.add("page-tooltip-link");
            createLink(elem, item.url);
            elem.innerHTML = item.text;
            tooltip.appendChild(elem);
          } else {
            const elem = document.createElement("hr");
            tooltip.appendChild(elem);
          }
        }
      };
    };

    const attachTooltip = (elem: HTMLElement, id: string, tooltipFillFunc: TooltipFiller): void => {
      if (id === this.settings.highlightedId) {
        elem.classList.remove("page-header-clickable");
        elem.classList.add("page-header-focused");
      }
      elemById.set(id, elem);
      elem.addEventListener("click", (event) => {
        const currentId = tooltip.getAttribute("last");
        const shown = tooltip.getAttribute("shown") === "true";
        if (!shown || currentId !== id) {
          forceHighlightOff(currentId);
          forceHighlightOn(id);
          headerLastClickTimestamp = event.timeStamp;
          tooltipFillFunc(event);
          tooltip.setAttribute("shown", "true");
          const xc = Math.min(
            elem.offsetLeft,
            document.documentElement.clientWidth - tooltip.clientWidth - 15
          );
          tooltip.setAttribute("style", `left: ${xc.toFixed(2)}px;`);
          tooltip.setAttribute("last", id);
          tooltip.removeAttribute("tabindex");
        }
      });
    };

    const refs: HTMLElement[] = [];
    const elem = (
      <header class="sticky-top">
        <a ref class="page-header-logo page-header-clickable"></a>
        <nav ref class="page-header-links"></nav>
        <div ref class="page-header-avatar-wrap"></div>
        <div ref class="page-tooltip" shown="false"></div>
      </header>
    ).asElement(refs) as HTMLElement;

    // Main code starts here
    const [headerLogo, headerLinks, avatarWrap, tooltip] = refs;
    createLink(headerLogo as HTMLAnchorElement, "");

    const elemById = new Map<string, HTMLElement>();
    let headerLastClickTimestamp = -1;

    this.registerExternalListener(document, "click", (event: MouseEvent) => {
      // Close tooltip on click
      if (
        tooltip.getAttribute("shown") === "true" &&
        headerLastClickTimestamp !== event.timeStamp
      ) {
        forceHighlightOff(tooltip.getAttribute("last"));
        tooltip.setAttribute("last", "none");
        tooltip.setAttribute("shown", "false");
      }
    });

    this.registerExternalListener(window, "resize", () => {
      // Move tooltip on resize
      const currentId = tooltip.getAttribute("last");
      const shown = tooltip.getAttribute("shown") === "true";
      if (currentId && shown) {
        const xc = Math.min(
          elemById.get(currentId)!.offsetLeft,
          document.documentElement.clientWidth - tooltip.clientWidth - 15
        );
        tooltip.setAttribute("style", `left: ${xc.toFixed(2)}px;`);
      }
    });

    // Navigation links
    for (const link of this.settings.nav.items) {
      const but = document.createElement("items" in link ? "button" : "a");
      but.classList.add("page-header-link");
      but.classList.add("page-header-clickable");
      let buttonHTML = link.text;
      if ("items" in link) {
        buttonHTML += `<svg class='page-header-icon'><use xlink:href='#angle-down'></use></svg>`;
        attachTooltip(but, link.id, defaultTooltipFillFunc(link.items));
      } else {
        elemById.set(link.id, but);
        createLink(but as HTMLAnchorElement, link.url);
      }
      but.innerHTML = buttonHTML;
      headerLinks.appendChild(but);
    }

    // "More" button
    if (this.settings.nav.moreText && this.settings.nav.moreItems) {
      const moreButton = document.createElement("button");
      moreButton.classList.add("page-header-link");
      moreButton.classList.add("page-header-clickable");
      attachTooltip(moreButton, "more", defaultTooltipFillFunc(this.settings.nav.moreItems));
      moreButton.innerHTML = `${this.settings.nav.moreText}
        <svg class='page-header-icon-big'><use xlink:href='#angle-down'></use></svg>`;
      headerLinks.appendChild(moreButton);
    }

    // "Avatar" button
    if (this.settings.profile.show) {
      avatarWrap.innerHTML = `
        <button class='page-header-avatar-but page-header-clickable'></button>
      `;
      const avatarElem = avatarWrap.children[0] as HTMLElement;
      avatarElem.innerHTML = `
				<svg class='page-header-avatar-icon'><use xlink:href='#avatar-icon'></use></svg>
				<svg class='page-header-avatar-arrow page-header-icon-big'>
          <use xlink:href='#angle-down'></use>
        </svg>`;
      attachTooltip(avatarElem, "profile", () => {
        tooltip.innerHTML = `<div class='page-header-pr-info'>
						<b>${this.settings.profile.name}</b><br>
						@${this.settings.profile.username}
					</div>
					<hr>
					<button class='page-tooltip-link'>Выйти</button>`;
        tooltip.children[2].addEventListener("click", () => {
          Router.instance.redirect("#logout", true, false, "executing");
        });
      });
    }

    this.registerExternalListener(Router.instance, PageCategoryUpdateEvent.NAME, () => {
      const prev = this.settings.highlightedId;
      this.settings.highlightedId = Router.instance.currentPageCategory;
      forceHighlightOff(prev);
      forceHighlightOn(this.settings.highlightedId);
    });
    return elem;
  }
}

import { Component } from "./component";
import { Router } from "../utils/router";

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
    const goTo = (url: string) => {
      return () => {
        Router.instance.goTo(url);
      };
    };

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

    const defaultTooltipFillFunc = (items: HeaderTooltipEntries, clearTooltip: boolean = true): TooltipFiller => {
      return () => {
        if (clearTooltip) tooltip.innerHTML = "";
        for (const item of items) {
          if (item.text !== "") {
            const elem = document.createElement("button");
            elem.classList.add("page-tooltip-link");
            elem.addEventListener("click", goTo(item.url));
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
          const xc = Math.min(elem.offsetLeft, document.documentElement.clientWidth - tooltip.clientWidth - 15);
          tooltip.setAttribute("style", `left: ${xc.toFixed(2)}px;`);
          tooltip.setAttribute("shown", "true");
          tooltip.setAttribute("last", id);
        }
      });
    };

    const elem = document.createElement("header");
    elem.innerHTML = `
			<a class='page-header-logo page-header-clickable'>
				<svg class='page-header-logo-image' width='75.12' height='24'><use xlink:href='#logo-small'></use></svg>
			</a>
			<nav class='page-header-links'></nav>
			<div class='page-header-avatar-wrap'></div>
			<div class='page-tooltip' style='left: -1000px;'></div>`;

    // Main code starts here
    const [headerLogo, headerLinks, avatarWrap, tooltip] = elem.children;
    headerLogo.addEventListener("click", goTo(""));

    const elemById = new Map<string, HTMLElement>();
    let headerLastClickTimestamp = -1;

    this.registerExternalListener(document, "click", (event: MouseEvent) => {
      // Close tooltip on click
      if (tooltip.getAttribute("shown") === "true" && headerLastClickTimestamp !== event.timeStamp) {
        forceHighlightOff(tooltip.getAttribute("last"));
        tooltip.setAttribute("style", "left: -1000px;");
        tooltip.setAttribute("last", "none");
        tooltip.setAttribute("shown", "false");
      }
    });

    this.registerExternalListener(window, "resize", (event: UIEvent) => {
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
      const but = document.createElement("button");
      but.classList.add("page-header-link");
      but.classList.add("page-header-clickable");
      let buttonHTML = link.text;
      if ("items" in link) {
        buttonHTML += `<svg class='page-header-icon'><use xlink:href='#angle-down'></use></svg>`;
        attachTooltip(but, link.id, defaultTooltipFillFunc(link.items));
      } else {
        elemById.set(link.id, but);
        but.addEventListener("click", goTo(link.url));
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
      avatarWrap.innerHTML = `<button class='page-header-avatar-but page-header-clickable'></button>`;
      const avatarElem = avatarWrap.children[0] as HTMLElement;
      avatarElem.innerHTML = `
				<svg class='page-header-avatar-icon'><use xlink:href='#avatar-icon'></use></svg>
				<svg class='page-header-avatar-arrow page-header-icon-big'><use xlink:href='#angle-down'></use></svg>`;
      attachTooltip(avatarElem, "profile", (event) => {
        tooltip.innerHTML = `<div class='page-header-pr-info'>
						<b>${this.settings.profile.name}</b><br>
						@${this.settings.profile.username}
					</div>
					<hr>
					<button class='page-tooltip-link'>Выйти</button>`;
        tooltip.children[2].addEventListener("click", goTo("#logout"));
      });
    }

    forceHighlightOn(this.settings.highlightedId);
    return elem;
  }
}

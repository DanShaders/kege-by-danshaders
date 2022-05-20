export type TraverseElement = HTMLElement | Text;

type TraverseInHandler<Ctx> = (elem: HTMLElement, ctx: Ctx) => boolean | void;
type TraverseOutHandler<Ctx> = (elem: HTMLElement, children: TraverseElement[], ctx: Ctx) => TraverseElement[];
export type TraverseHandler<Ctx> = [TraverseInHandler<Ctx>, TraverseOutHandler<Ctx>];

type TraverseTextInHandler<Ctx> = (elem: Text, ctx: Ctx) => boolean | void;
type TraverseTextOutHandler<Ctx> = (elem: Text, children: TraverseElement[], ctx: Ctx) => TraverseElement[];
export type TraverseTextHandler<Ctx> = [TraverseTextInHandler<Ctx>, TraverseTextOutHandler<Ctx>];

type TraverseRule<Ctx> =
  | [string, TraverseHandler<Ctx>]
  | [string, TraverseHandler<Ctx>, boolean | number]
  | [0, TraverseTextHandler<Ctx>];
export type TraverseRules<Ctx> = TraverseRule<Ctx>[];

export function traverse<Ctx>(elem: DocumentFragment, rules: TraverseRules<Ctx>, ctx: Ctx): [HTMLTemplateElement];
export function traverse<Ctx>(elem: TraverseElement, rules: TraverseRules<Ctx>, ctx: Ctx): TraverseElement[];
export function traverse<Ctx>(
  elem: TraverseElement | DocumentFragment,
  rules: TraverseRules<Ctx>,
  ctx: Ctx
): TraverseElement[] | [HTMLTemplateElement] {
  if (elem instanceof DocumentFragment) {
    const node = document.createElement("template");
    for (const child of elem.childNodes) {
      for (const curr of traverse(child as HTMLElement, rules, ctx)) {
        node.content.appendChild(curr);
      }
    }
    return [node];
  }
  for (const rule of rules) {
    if (elem instanceof Text) {
      if (rule[0] === 0 && !rule[1][0](elem, ctx)) {
        break;
      }
    } else if (rule[0] !== 0 && elem.matches(rule[0])) {
      if (!rule[1][0](elem, ctx)) {
        break;
      }
    }
  }
  let children = [];
  for (const child of elem.childNodes) {
    for (const res of traverse(child as TraverseElement, rules, ctx)) {
      children.push(res);
    }
  }
  for (const rule of rules) {
    let res: TraverseElement[] | undefined = undefined;
    if (elem instanceof Text) {
      if (rule[0] === 0) {
        res = rule[1][1](elem, children, ctx);
      }
    } else if (rule[0] !== 0 && elem.matches(rule[0])) {
      res = rule[1][1](elem, children, ctx);
    }
    if (res) {
      if (rule.length > 2 && rule[2]) {
        children = res;
      } else {
        return res;
      }
    }
  }
  return children;
}

import { allLessons } from "../content/loader";
import { buildBankCards, buildDemoCards, parseBank } from "./cards";
import type { Card } from "./cards";

// The warm-up deck: general CS questions from content/warmups/*.yaml, plus one set of cards for every gotcha demo.
const files = import.meta.glob("../../../content/warmups/*.yaml", { query: "?raw", import: "default", eager: true }) as Record<string, string>;

const bank = Object.keys(files)
  .sort()
  .flatMap((p) => parseBank(files[p] as string));

export const allCards: Card[] = [...buildDemoCards(allLessons), ...buildBankCards(bank)];
export const cardById = new Map(allCards.map((c) => [c.id, c]));

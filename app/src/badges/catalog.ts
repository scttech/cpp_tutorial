import { parse } from "yaml";
import { allLessons } from "../content/loader";
import type { BadgeDef, Catalog } from "./engine";

// Badge definitions are data: content/badges/*.yaml. Adding a badge needs no code change.
const files = import.meta.glob("../../../content/badges/*.yaml", { query: "?raw", import: "default", eager: true }) as Record<string, string>;

export const badgeDefs: BadgeDef[] = Object.keys(files)
  .sort()
  .flatMap((path) => parse(files[path]!) as BadgeDef[]);

export const badgeById = (id: string) => badgeDefs.find((b) => b.id === id);

/** What the engine needs to know about the course content. */
export const catalog: Catalog = {
  lessons: allLessons.map((l) => ({ id: l.meta.id, module: l.meta.module, type: l.meta.type, tiers: l.meta.tiers })),
};

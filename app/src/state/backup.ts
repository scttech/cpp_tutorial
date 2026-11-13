import { addDays } from "../warmup/schedule";
import type { Progress } from "./model";
import { localDate } from "./streak";

// A reminder to export your progress. Progress lives in this browser only, so an export is the one backup there is.
// The reminder state is kept apart from the progress (an import replaces progress; it must not reset the reminder).

export interface BackupState {
  /** Local date of the last export, or null if there has been none. */
  lastExport: string | null;
  /** Do not remind before this local date. */
  snoozedUntil: string | null;
}

export const BACKUP_KEY = "zth.backup.v1";
/** Days after an export (or the first day of activity) before the reminder appears, and the length of a snooze. */
export const REMIND_AFTER_DAYS = 7;

const DATE = /^\d{4}-\d{2}-\d{2}$/;
export const sanitizeBackup = (raw: unknown): BackupState => {
  const o = (typeof raw === "object" && raw !== null ? raw : {}) as Record<string, unknown>;
  return { lastExport: typeof o.lastExport === "string" && DATE.test(o.lastExport) ? o.lastExport : null, snoozedUntil: typeof o.snoozedUntil === "string" && DATE.test(o.snoozedUntil) ? o.snoozedUntil : null };
};

export function loadBackup(): BackupState {
  try {
    const text = localStorage.getItem(BACKUP_KEY);
    return sanitizeBackup(text ? JSON.parse(text) : null);
  } catch {
    return sanitizeBackup(null);
  }
}

export function saveBackup(s: BackupState): void {
  try {
    localStorage.setItem(BACKUP_KEY, JSON.stringify(s));
  } catch {
    /* storage unavailable: the reminder just comes back next visit */
  }
}

/** The first and the last day with any recorded activity, or null for a new learner. */
function activitySpan(p: Progress): { first: string; last: string } | null {
  const days = Object.keys(p.activity).sort();
  return days.length ? { first: days[0] as string, last: days[days.length - 1] as string } : null;
}

export interface BackupStatus {
  /** Show the reminder now. */
  due: boolean;
  /** Days since the last export, or null if never exported. */
  daysSince: number | null;
}

/**
 * The reminder is due when there is progress worth protecting (a lesson completed), at least REMIND_AFTER_DAYS have passed since the last
 * export (or since the first activity, if there was never one), something was done since the last export, and it was not snoozed.
 */
export function backupStatus(p: Progress, b: BackupState, today: string): BackupStatus {
  const span = activitySpan(p);
  const completed = Object.values(p.lessons).some((l) => l.status === "complete");
  const daysSince = b.lastExport ? Math.max(0, Math.round((Date.parse(`${today}T00:00:00Z`) - Date.parse(`${b.lastExport}T00:00:00Z`)) / 86_400_000)) : null;
  if (!span || !completed) return { due: false, daysSince };
  if (b.snoozedUntil && today < b.snoozedUntil) return { due: false, daysSince };
  const since = b.lastExport ?? span.first;
  if (today < addDays(since, REMIND_AFTER_DAYS)) return { due: false, daysSince };
  if (b.lastExport && span.last <= b.lastExport) return { due: false, daysSince }; // nothing new to protect
  return { due: true, daysSince };
}

export const afterExport = (_b: BackupState, today: string): BackupState => ({ lastExport: today, snoozedUntil: null });
export const afterSnooze = (b: BackupState, today: string): BackupState => ({ ...b, snoozedUntil: addDays(today, REMIND_AFTER_DAYS) });

/** Download the progress file and remember the export. */
export function downloadProgress(json: string, now = new Date()): void {
  const blob = new Blob([json], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `zth-progress-${now.toISOString().slice(0, 10)}.json`;
  a.click();
  URL.revokeObjectURL(url);
  saveBackup(afterExport(loadBackup(), localDate(now)));
}

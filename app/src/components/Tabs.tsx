import { useRef } from "react";
import type { ReactNode } from "react";

export interface TabDef<T extends string> {
  id: T;
  label: ReactNode;
}

export const tabId = (prefix: string, id: string): string => `${prefix}-tab-${id}`;

/** Props for the single panel that follows a tab list: it is labelled by whichever tab is selected. */
export const panelProps = (prefix: string, active: string) => ({ role: "tabpanel" as const, id: `${prefix}-panel`, "aria-labelledby": tabId(prefix, active) });

interface Props<T extends string> {
  prefix: string;
  label: string;
  tabs: TabDef<T>[];
  value: T;
  onChange(id: T): void;
  /** Extra content shown at the end of the tab strip (a timing label, a spinner). */
  children?: ReactNode;
}

/**
 * An accessible tab list (WAI-ARIA tabs pattern): one tab stop, arrow keys, Home and End move between the tabs and select them.
 * The panel it controls is `panelProps(prefix, value)` on the element that shows the content.
 */
export function Tabs<T extends string>({ prefix, label, tabs, value, onChange, children }: Props<T>) {
  const refs = useRef<Record<string, HTMLButtonElement | null>>({});
  const onKey = (e: React.KeyboardEvent) => {
    const i = tabs.findIndex((t) => t.id === value);
    let n = i;
    if (e.key === "ArrowRight") n = (i + 1) % tabs.length;
    else if (e.key === "ArrowLeft") n = (i - 1 + tabs.length) % tabs.length;
    else if (e.key === "Home") n = 0;
    else if (e.key === "End") n = tabs.length - 1;
    else return;
    e.preventDefault();
    onChange(tabs[n]!.id);
    refs.current[tabs[n]!.id]?.focus();
  };
  return (
    <div className="tabs" role="tablist" aria-label={label} onKeyDown={onKey}>
      {tabs.map((t) => (
        <button
          key={t.id}
          ref={(el) => {
            refs.current[t.id] = el;
          }}
          id={tabId(prefix, t.id)}
          role="tab"
          type="button"
          aria-selected={t.id === value}
          aria-controls={`${prefix}-panel`}
          tabIndex={t.id === value ? 0 : -1}
          className={t.id === value ? "tab on" : "tab"}
          onClick={() => onChange(t.id)}
        >
          {t.label}
        </button>
      ))}
      {children}
    </div>
  );
}

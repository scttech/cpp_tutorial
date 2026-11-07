import { useSyncExternalStore } from "react";

export interface Toast {
  id: number;
  kind: "badge" | "info";
  title: string;
  body?: string;
  icon?: string;
  badgeId?: string;
}

let toasts: Toast[] = [];
let nextId = 1;
const listeners = new Set<() => void>();
const emit = () => listeners.forEach((l) => l());

export function pushToast(t: Omit<Toast, "id">): number {
  const id = nextId++;
  toasts = [...toasts, { ...t, id }];
  emit();
  return id;
}

export function dismissToast(id: number) {
  toasts = toasts.filter((t) => t.id !== id);
  emit();
}

export const useToasts = () =>
  useSyncExternalStore(
    (l) => {
      listeners.add(l);
      return () => listeners.delete(l);
    },
    () => toasts,
  );

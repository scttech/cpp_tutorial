import { useEffect } from "react";
import { Link } from "react-router-dom";
import { dismissToast, useToasts } from "../state/toasts";
import type { Toast } from "../state/toasts";

function ToastItem({ t }: { t: Toast }) {
  useEffect(() => {
    const id = setTimeout(() => dismissToast(t.id), 9000);
    return () => clearTimeout(id);
  }, [t.id]);
  return (
    <div className={`toast ${t.kind}`} role="status">
      {t.icon && <span className="toast-icon" aria-hidden="true">{t.icon}</span>}
      <div className="toast-text">
        <b>{t.title}</b>
        {t.body && <span>{t.body}</span>}
        {t.kind === "badge" && (
          <Link to="/profile" onClick={() => dismissToast(t.id)}>
            See your badges
          </Link>
        )}
      </div>
      <button className="toast-close" aria-label="Dismiss" onClick={() => dismissToast(t.id)}>
        ×
      </button>
    </div>
  );
}

/** Fixed, screen-reader friendly (role=status) stack of notifications. */
export function ToastHost() {
  const toasts = useToasts();
  return (
    <div className="toasts" aria-live="polite">
      {toasts.map((t) => (
        <ToastItem key={t.id} t={t} />
      ))}
    </div>
  );
}

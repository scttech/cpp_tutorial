// Shared by the web app and tools/validate-content so both explain sanitizer reports identically.
// Plain JavaScript (typed by runtime-core.d.mts) so Node can run it without a build step.

/** Compile raw YAML rules. An invalid regex throws with the rule id, so a typo fails loudly in tests. */
export function compileRuntimeRules(raw) {
  return raw.map((r) => {
    try {
      return { id: r.id, re: new RegExp(r.match), title: r.title, friendly: r.friendly, lesson: r.lesson ?? null };
    } catch (e) {
      throw new Error(`runtime rule "${r.id}": invalid regex: ${e.message}`);
    }
  });
}

const place = (f) => (f ? `${f.file}:${f.line}` : null);
/** "READ of size 4" -> "read of 4 bytes" */
const accessPhrase = (a) => {
  const m = /^(READ|WRITE) of size (\d+)/.exec(a ?? "");
  return m ? `${m[1].toLowerCase()} of ${m[2]} bytes` : null;
};

/** Replace {where}, {freed}, {allocated}, {access} with what the report says (or a neutral phrase when it does not). */
export function fillPlaceholders(text, report) {
  const values = {
    where: place(report.location) ?? "your code",
    freed: place(report.freedAt) ?? "an earlier point",
    allocated: place(report.allocatedAt) ?? "an earlier point",
    access: accessPhrase(report.access) ?? "memory access",
  };
  return text.replace(/\{(where|freed|allocated|access)\}/g, (_, k) => values[k]);
}

/** The first rule that matches this report: { ruleId, title, text, lesson }, or null. */
export function describeReport(rules, report) {
  for (const rule of rules) {
    if (!rule.re.test(report.summary)) continue;
    return { ruleId: rule.id, title: rule.title, text: fillPlaceholders(rule.friendly, report), lesson: rule.lesson };
  }
  return null;
}

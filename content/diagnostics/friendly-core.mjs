// Shared by the web app and tools/validate-content so both apply rules identically.
// Plain JavaScript (typed by friendly-core.d.mts) so Node can run it without a build step.

/**
 * Compile raw YAML rule objects into matchers. Invalid regexes throw with the rule id, so a typo in
 * friendly.yaml fails loudly in tests instead of silently disabling a rule.
 */
export function compileRules(raw) {
  return raw.map((r) => {
    try {
      return {
        id: r.id,
        re: new RegExp(r.match),
        fileRe: r.fileMatch ? new RegExp(r.fileMatch) : null,
        friendly: r.friendly,
        lesson: r.lesson ?? null,
      };
    } catch (e) {
      throw new Error(`friendly rule "${r.id}": invalid regex: ${e.message}`);
    }
  });
}

/** First rule that matches this diagnostic, with $1.. substituted, or null. */
export function matchRule(rules, diag) {
  for (const rule of rules) {
    if (rule.fileRe && !rule.fileRe.test(diag.file ?? "")) continue;
    const m = rule.re.exec(diag.message);
    if (!m) continue;
    const text = rule.friendly.replace(/\$(\d)/g, (_, n) => m[Number(n)] ?? "");
    return { ruleId: rule.id, text, lesson: rule.lesson };
  }
  return null;
}

/** Returns copies of the diagnostics with `friendly`, `friendlyLesson` and `ruleId` filled in where a rule matches. */
export function applyFriendly(rules, diags) {
  return diags.map((d) => {
    const hit = matchRule(rules, d);
    if (!hit) return d;
    return { ...d, friendly: hit.text, friendlyLesson: hit.lesson ?? undefined, ruleId: hit.ruleId };
  });
}

// Tiny client for the runner API shared by the validation scripts. It waits and retries when the
// runner pushes back (429 rate limit, 503 busy), so bulk validation works against a rate-limited runner.
const RUNNER = process.env.RUNNER_URL ?? "http://localhost:8080";
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export async function runJob(body, { retries = 40 } = {}) {
  for (let attempt = 0; ; attempt++) {
    const res = await fetch(`${RUNNER}/run`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    });
    if (res.ok) return res.json();
    if ((res.status === 429 || res.status === 503) && attempt < retries) {
      const wait = Number(res.headers.get("retry-after")) || 3;
      await sleep(Math.min(wait, 10) * 1000);
      continue;
    }
    throw new Error(`runner HTTP ${res.status}: ${await res.text()}`);
  }
}

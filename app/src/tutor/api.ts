// A minimal streaming client for the Anthropic Messages API, called straight from the browser with the
// learner's own key. The key is only ever sent to API_URL.

export const API_URL = "https://api.anthropic.com/v1/messages";
export const API_VERSION = "2023-06-01";

export interface ChatMessage {
  role: "user" | "assistant";
  content: string;
}

export interface Usage {
  input: number;
  output: number;
}

export interface CompleteOptions {
  apiKey: string;
  model: string;
  maxTokens: number;
  system: string;
  messages: ChatMessage[];
  signal?: AbortSignal;
  /** Called with each new piece of text as it streams in. */
  onText?: (delta: string) => void;
  fetchImpl?: typeof fetch;
}

export interface Completion {
  text: string;
  usage: Usage;
  stopReason: string | null;
}

export class TutorApiError extends Error {
  readonly status: number;
  constructor(message: string, status: number) {
    super(message);
    this.status = status;
  }
}

/** Plain-language text for an API failure. */
export function explainFailure(status: number, apiMessage: string): string {
  if (status === 401) return "The API key was rejected. Check it in Settings.";
  if (status === 403) return "This API key is not allowed to use that model or feature.";
  if (status === 404) return `The model was not found. Pick another model in Settings. (${apiMessage})`;
  if (status === 429) return "Rate limit or spending limit reached. Wait a moment, or check the limit on your key.";
  if (status === 529 || status >= 500) return "The API is overloaded or down right now. Try again in a minute.";
  return apiMessage || `The API returned HTTP ${status}.`;
}

/** Parse a Server-Sent-Events stream into {event, data} records. */
export async function* readSse(body: ReadableStream<Uint8Array>): AsyncGenerator<{ event: string; data: string }> {
  const reader = body.getReader();
  const dec = new TextDecoder();
  let buf = "";
  try {
    for (;;) {
      const { done, value } = await reader.read();
      buf += dec.decode(value ?? new Uint8Array(), { stream: !done });
      buf = buf.replace(/\r\n?/g, "\n");
      let i: number;
      while ((i = buf.indexOf("\n\n")) >= 0) {
        const raw = buf.slice(0, i);
        buf = buf.slice(i + 2);
        let event = "message";
        const data: string[] = [];
        for (const line of raw.split("\n")) {
          if (line.startsWith("event:")) event = line.slice(6).trim();
          else if (line.startsWith("data:")) data.push(line.slice(5).replace(/^ /, ""));
        }
        if (data.length) yield { event, data: data.join("\n") };
      }
      if (done) break;
    }
  } finally {
    reader.releaseLock();
  }
}

/** Send one request and collect the streamed reply. */
export async function complete(o: CompleteOptions): Promise<Completion> {
  const doFetch = o.fetchImpl ?? fetch;
  let res: Response;
  try {
    res = await doFetch(API_URL, {
      method: "POST",
      signal: o.signal,
      headers: {
        "content-type": "application/json",
        "x-api-key": o.apiKey,
        "anthropic-version": API_VERSION,
        // Required for calls straight from a browser (the key is the learner's own).
        "anthropic-dangerous-direct-browser-access": "true",
      },
      body: JSON.stringify({ model: o.model, max_tokens: o.maxTokens, system: o.system, messages: o.messages, stream: true }),
    });
  } catch (e) {
    if (e instanceof DOMException && e.name === "AbortError") throw e;
    throw new TutorApiError("Could not reach api.anthropic.com. Check your connection (or an ad blocker).", 0);
  }
  if (!res.ok || !res.body) {
    let msg = "";
    try {
      const j = (await res.json()) as { error?: { message?: string } };
      msg = j.error?.message ?? "";
    } catch {
      /* not JSON */
    }
    throw new TutorApiError(explainFailure(res.status, msg), res.status);
  }
  let text = "";
  const usage: Usage = { input: 0, output: 0 };
  let stopReason: string | null = null;
  for await (const ev of readSse(res.body)) {
    let d: Record<string, any>;
    try {
      d = JSON.parse(ev.data) as Record<string, any>;
    } catch {
      continue;
    }
    switch (d.type) {
      case "message_start":
        usage.input = Number(d.message?.usage?.input_tokens ?? 0);
        usage.output = Number(d.message?.usage?.output_tokens ?? 0);
        break;
      case "content_block_delta":
        if (d.delta?.type === "text_delta" && typeof d.delta.text === "string") {
          text += d.delta.text;
          o.onText?.(d.delta.text);
        }
        break;
      case "message_delta":
        if (typeof d.usage?.output_tokens === "number") usage.output = d.usage.output_tokens;
        if (typeof d.delta?.stop_reason === "string") stopReason = d.delta.stop_reason;
        break;
      case "error":
        throw new TutorApiError(explainFailure(d.error?.type === "overloaded_error" ? 529 : 500, String(d.error?.message ?? "")), 500);
    }
  }
  return { text, usage, stopReason };
}

import { complete } from "./api";
import type { ChatMessage, CompleteOptions, Usage } from "./api";
import { FALLBACK_MESSAGE, checkReply } from "./guardrail";
import type { ReferenceSet, Verdict } from "./guardrail";
import { modeInfo } from "./modes";
import type { TutorMode } from "./modes";
import { RETRY_NOTICE, buildSystem } from "./prompt";
import type { TutorContext } from "./prompt";

export interface AskInput {
  apiKey: string;
  model: string;
  maxTokens: number;
  mode: TutorMode;
  question: string;
  /** Earlier turns of this lesson's conversation (plain text; nothing from other lessons). */
  history: ChatMessage[];
  context: TutorContext;
  /** Local-only comparison data for the output guardrail. Never sent to the API. */
  reference: ReferenceSet | null;
  signal?: AbortSignal;
  /** Streams text as it arrives; used only for unguarded modes, because guarded replies are checked first. */
  onText?: (delta: string) => void;
  /** Called for each reply the guardrail caught. */
  onLeak?: (v: Extract<Verdict, { ok: false }>) => void;
  send?: (o: CompleteOptions) => ReturnType<typeof complete>;
}

export interface AskResult {
  text: string;
  usage: Usage;
  /** The guardrail rejected the reply (twice), so `text` is the safe fallback. */
  fellBack: boolean;
}

const MAX_HISTORY = 16;

/**
 * One tutor turn. In guarded modes the reply is held back until it passes the guardrail; a reply that fails
 * is asked for once more with a stricter instruction, and if that fails too the learner gets a safe message.
 */
export async function ask(i: AskInput): Promise<AskResult> {
  const send = i.send ?? complete;
  const info = modeInfo(i.mode);
  const system = buildSystem(i.mode, i.context);
  const messages: ChatMessage[] = [...i.history.slice(-MAX_HISTORY), { role: "user", content: i.question }];
  const base = { apiKey: i.apiKey, model: i.model, maxTokens: i.maxTokens, system, signal: i.signal };
  const usage: Usage = { input: 0, output: 0 };
  const add = (u: Usage) => {
    usage.input += u.input;
    usage.output += u.output;
  };

  const first = await send({ ...base, messages, onText: info.guarded ? undefined : i.onText });
  add(first.usage);
  let verdict = checkReply(first.text, i.mode, i.reference);
  if (verdict.ok) return { text: first.text, usage, fellBack: false };

  i.onLeak?.(verdict);
  const second = await send({
    ...base,
    messages: [...messages, { role: "assistant", content: first.text }, { role: "user", content: RETRY_NOTICE }],
  });
  add(second.usage);
  verdict = checkReply(second.text, i.mode, i.reference);
  if (verdict.ok) return { text: second.text, usage, fellBack: false };
  i.onLeak?.(verdict);
  return { text: FALLBACK_MESSAGE, usage, fellBack: true };
}

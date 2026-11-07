// Bundle Monaco locally (no CDN). Language definitions load lazily, so only C++ is fetched.
// Only the generic editor worker is wired up: we use no JSON/CSS/TS language services.
import { loader } from "@monaco-editor/react";
import * as monaco from "monaco-editor";
import EditorWorker from "monaco-editor/editor/editor.worker.js?worker";

self.MonacoEnvironment = { getWorker: () => new EditorWorker() };
loader.config({ monaco });

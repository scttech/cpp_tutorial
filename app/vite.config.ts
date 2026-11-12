import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// BASE_PATH is set by the GitHub Pages workflow (e.g. "/cpp-zero-to-hero/").
export default defineConfig({
  base: process.env.BASE_PATH ?? '/',
  plugins: [react()],
  resolve: {
    alias: [
      // monaco-vim ships a UMD build for the "browser" condition and deep-imports "monaco-editor/esm/vs/...", which
      // monaco-editor 0.57's export map no longer allows. Use its ES build and map those imports onto the real files.
      // monaco-emacs is CommonJS and require()s "monaco-editor", which would resolve to the AMD build (min/vs) and share nothing with the editor.
      { find: /^monaco-editor$/, replacement: fileURLToPath(new URL('./node_modules/monaco-editor/esm/vs/index.js', import.meta.url)) },
      { find: /^monaco-vim$/, replacement: fileURLToPath(new URL('./node_modules/monaco-vim/dist/index.mjs', import.meta.url)) },
      { find: /^monaco-editor\/esm\/vs\/(.*)$/, replacement: fileURLToPath(new URL('./node_modules/monaco-editor/esm/vs/', import.meta.url)) + '$1.js' },
    ],
  },
  // The in-browser compiler (see /wasm-runner) runs in module Workers and loads its .wasm files by URL: Vite must
  // not pre-bundle these packages (that would break the URLs).
  worker: { format: 'es' },
  optimizeDeps: { exclude: ['browsercc', '@bjorn3/browser_wasi_shim'] },
  server: {
    port: 5173,
    // lesson content lives outside app/, in the repo-root /content directory
    fs: { allow: [fileURLToPath(new URL('..', import.meta.url))] },
  },
})

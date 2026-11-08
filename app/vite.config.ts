import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// BASE_PATH is set by the GitHub Pages workflow (e.g. "/cpp-zero-to-hero/").
export default defineConfig({
  base: process.env.BASE_PATH ?? '/',
  plugins: [react()],
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

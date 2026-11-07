import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// BASE_PATH is set by the GitHub Pages workflow (e.g. "/cpp-zero-to-hero/").
export default defineConfig({
  base: process.env.BASE_PATH ?? '/',
  plugins: [react()],
  server: {
    port: 5173,
    // lesson content lives outside app/, in the repo-root /content directory
    fs: { allow: [fileURLToPath(new URL('..', import.meta.url))] },
  },
})

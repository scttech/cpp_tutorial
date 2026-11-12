import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { App } from './App'
import './index.css'
import { initPrefs } from './prefs/prefs'
import { initStore } from './state/store'

// Theme and text size apply before anything renders, so there is no flash of the wrong theme.
initPrefs()

// Load saved progress (IndexedDB) before the first render so the UI never flashes empty state.
initStore()
  .catch((e) => console.error('Could not load saved progress; starting empty.', e))
  .finally(() => {
    createRoot(document.getElementById('root')!).render(
      <StrictMode>
        <App />
      </StrictMode>,
    )
  })

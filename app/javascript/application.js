// Entry point for the build script in your package.json
import "@hotwired/turbo-rails"
import "./controllers"
import "./bootstrap"
import "./jquery"

// Mount React app
import React from 'react'
import { createRoot } from 'react-dom/client'
import App from './components/App'

document.addEventListener('turbo:load', () => {
  const rootEl = document.getElementById('react-root')
  if (rootEl) {
    const root = createRoot(rootEl)
    root.render(React.createElement(App))
  }
})

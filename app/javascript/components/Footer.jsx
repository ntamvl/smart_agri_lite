import React from 'react'
import app from '../../../package.json'

export const Footer = () => {
  return (
    <footer className="footer mt-auto py-3 bg-body-tertiary">
      <div className="container-fluid text-left">
        <span className="text-body-secondary tw:text-[12px]">@<a href="https://github.com/ntamvl" target="_blank" rel="noopener noreferrer">Tam Nguyen</a></span>
        <span className="text-body-secondary tw:text-[12px] tw:font-bold"> • {app.displayName}</span>
        <span className="text-body-secondary tw:text-[12px]"> Version {app.version}</span>
      </div>
    </footer>
  )
}

export default Footer

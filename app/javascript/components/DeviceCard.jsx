import React from 'react';
import GpioPinControl from './GpioPinControl';

export default function DeviceCard({ device, onEdit, onDelete, onPinUpdate, onManagePins }) {
  const handlePinUpdate = (updatedPin) => {
    onPinUpdate(device.id, updatedPin);
  };

  const digitalPins = (device.gpio_pins || []).filter(p => p.pin_type === 'digital');
  const analogPins = (device.gpio_pins || []).filter(p => p.pin_type === 'analog');

  return (
    <div className="card device-card shadow-sm mb-4">
      <div className="card-header d-flex justify-content-between align-items-center">
        <div>
          <h5 className="card-title mb-0">
            <i className="bi bi-cpu me-2"></i>
            {device.name}
          </h5>
          <small className="text-muted">Code: {device.code}</small>
        </div>
        <div className="dropdown">
          <button className="btn btn-sm btn-outline-secondary dropdown-toggle" type="button" data-bs-toggle="dropdown">
            <i className="bi bi-gear"></i>
          </button>
          <ul className="dropdown-menu dropdown-menu-end">
            <li><button className="dropdown-item" onClick={() => onEdit(device)}><i className="bi bi-pencil me-2"></i>Edit Device</button></li>
            <li><button className="dropdown-item" onClick={() => onManagePins(device)}><i className="bi bi-pin-map me-2"></i>Manage Pins</button></li>
            <li><hr className="dropdown-divider" /></li>
            <li><button className="dropdown-item text-danger" onClick={() => onDelete(device)}><i className="bi bi-trash me-2"></i>Delete Device</button></li>
          </ul>
        </div>
      </div>
      <div className="card-body">
        {device.description && (
          <p className="card-text text-muted small mb-3">{device.description}</p>
        )}
        <div className="device-meta d-flex gap-3 mb-3">
          <span className="badge bg-secondary">
            <i className="bi bi-hdd-network me-1"></i>
            {device.ip || 'No IP'}
          </span>
          <span className="badge bg-primary">
            {(device.gpio_pins || []).length} pins
          </span>
          <span className="badge bg-info">
            {(device.updated_at || device.created_at) && (
              <span>{new Date(device.updated_at || device.created_at).toLocaleString()}</span>
            )}
          </span>
        </div>

        {digitalPins.length > 0 && (
          <div className="pin-section mb-2">
            <h6 className="text-uppercase small fw-bold text-muted mb-2">
              <i className="bi bi-toggle-on me-1"></i> Digital Pins
            </h6>
            <div className="list-group list-group-flush">
              {digitalPins.map(pin => (
                <div key={pin.id} className="list-group-item px-0">
                  <GpioPinControl
                    pin={pin}
                    deviceId={device.id}
                    onUpdate={handlePinUpdate}
                  />
                </div>
              ))}
            </div>
          </div>
        )}

        {analogPins.length > 0 && (
          <div className="pin-section">
            <h6 className="text-uppercase small fw-bold text-muted mb-2">
              <i className="bi bi-sliders me-1"></i> Analog Pins
            </h6>
            <div className="list-group list-group-flush">
              {analogPins.map(pin => (
                <div key={pin.id} className="list-group-item px-0">
                  <GpioPinControl
                    pin={pin}
                    deviceId={device.id}
                    onUpdate={handlePinUpdate}
                  />
                </div>
              ))}
            </div>
          </div>
        )}

        {(device.gpio_pins || []).length === 0 && (
          <div className="text-center text-muted py-3">
            <i className="bi bi-pin-map fs-3 d-block mb-2"></i>
            No GPIO pins configured
          </div>
        )}
      </div>
    </div>
  );
}

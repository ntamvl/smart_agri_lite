import React, { useState } from 'react';
import { api } from './api';

export default function GpioPinControl({ pin, deviceId, onUpdate }) {
  const [loading, setLoading] = useState(false);

  const handleDigitalControl = async (newValue) => {
    setLoading(true);
    try {
      const updated = await api.controlPin(deviceId, pin.id, newValue);
      onUpdate(updated);
    } catch (err) {
      alert('Control failed: ' + err.message);
    } finally {
      setLoading(false);
    }
  };

  const handleAnalogControl = async (e) => {
    const newValue = parseInt(e.target.value, 10);
    setLoading(true);
    try {
      const updated = await api.controlPin(deviceId, pin.id, newValue);
      onUpdate(updated);
    } catch (err) {
      alert('Control failed: ' + err.message);
    } finally {
      setLoading(false);
    }
  };

  if (pin.pin_type === 'digital') {
    return (
      <div className="gpio-pin-control d-flex align-items-center justify-content-between py-2">
        <div className="pin-info">
          <span className="pin-label fw-semibold">{pin.label || `GPIO ${pin.pin_number}`}</span>
          <small className="text-muted ms-2">Pin {pin.pin_number}</small>
        </div>
        <div className="btn-group" role="group">
          <button
            type="button"
            className={`btn btn-sm ${pin.value === 0 ? 'btn-danger' : 'btn-outline-secondary'}`}
            onClick={() => handleDigitalControl(0)}
            disabled={loading}
          >
            <i className="bi bi-power"></i> OFF
          </button>
          <button
            type="button"
            className={`btn btn-sm ${pin.value === 1 ? 'btn-success' : 'btn-outline-secondary'}`}
            onClick={() => handleDigitalControl(1)}
            disabled={loading}
          >
            <i className="bi bi-power"></i> ON
          </button>
        </div>
      </div>
    );
  }

  // Analog control
  return (
    <div className="gpio-pin-control py-2">
      <div className="d-flex align-items-center justify-content-between mb-1">
        <div className="pin-info">
          <span className="pin-label fw-semibold">{pin.label || `GPIO ${pin.pin_number}`}</span>
          <small className="text-muted ms-2">Pin {pin.pin_number}</small>
        </div>
        <span className="badge bg-info">{pin.value}</span>
      </div>
      <input
        type="range"
        className="form-range"
        min="0"
        max="255"
        value={pin.value}
        onChange={handleAnalogControl}
        disabled={loading}
      />
      <div className="d-flex justify-content-between">
        <small className="text-muted">0</small>
        <small className="text-muted">255</small>
      </div>
    </div>
  );
}

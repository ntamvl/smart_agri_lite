import React, { useState, useEffect } from 'react';
import { api } from './api';

export default function GpioPinForm({ device, onClose, onUpdate }) {
  const [pins, setPins] = useState([]);
  const [loading, setLoading] = useState(true);
  const [newPin, setNewPin] = useState({ pin_number: '', label: '', pin_type: 'digital' });
  const [editingPin, setEditingPin] = useState(null);
  const [editForm, setEditForm] = useState({ label: '', pin_type: 'digital' });
  const [errors, setErrors] = useState('');

  useEffect(() => {
    loadPins();
  }, [device.id]);

  const loadPins = async () => {
    try {
      const data = await api.getGpioPins(device.id);
      setPins(data);
    } catch (err) {
      setErrors(err.message);
    } finally {
      setLoading(false);
    }
  };

  const handleAddPin = async (e) => {
    e.preventDefault();
    setErrors('');
    try {
      const created = await api.createGpioPin(device.id, {
        ...newPin,
        pin_number: parseInt(newPin.pin_number, 10),
      });
      setPins([...pins, created]);
      setNewPin({ pin_number: '', label: '', pin_type: 'digital' });
      onUpdate();
    } catch (err) {
      setErrors(err.message);
    }
  };

  const handleUpdatePin = async (pinId) => {
    setErrors('');
    try {
      const updated = await api.updateGpioPin(device.id, pinId, editForm);
      setPins(pins.map(p => p.id === pinId ? updated : p));
      setEditingPin(null);
      onUpdate();
    } catch (err) {
      setErrors(err.message);
    }
  };

  const handleDeletePin = async (pinId) => {
    if (!confirm('Are you sure you want to delete this pin?')) return;
    try {
      await api.deleteGpioPin(device.id, pinId);
      setPins(pins.filter(p => p.id !== pinId));
      onUpdate();
    } catch (err) {
      setErrors(err.message);
    }
  };

  const startEditing = (pin) => {
    setEditingPin(pin.id);
    setEditForm({ label: pin.label || '', pin_type: pin.pin_type });
  };

  return (
    <div className="modal show d-block" tabIndex="-1" style={{ backgroundColor: 'rgba(0,0,0,0.5)' }}>
      <div className="modal-dialog modal-lg modal-dialog-centered modal-dialog-scrollable">
        <div className="modal-content">
          <div className="modal-header">
            <h5 className="modal-title">
              <i className="bi bi-pin-map me-2"></i>
              Manage GPIO Pins — {device.name}
            </h5>
            <button type="button" className="btn-close" onClick={onClose}></button>
          </div>
          <div className="modal-body">
            {errors && <div className="alert alert-danger">{errors}</div>}

            {loading ? (
              <div className="text-center py-4">
                <div className="spinner-border"></div>
              </div>
            ) : (
              <>
                {/* Existing pins */}
                <div className="table-responsive">
                  <table className="table table-hover align-middle">
                    <thead>
                      <tr>
                        <th>Pin #</th>
                        <th>Label</th>
                        <th>Type</th>
                        <th>Value</th>
                        <th className="text-end">Actions</th>
                      </tr>
                    </thead>
                    <tbody>
                      {pins.sort((a, b) => a.pin_number - b.pin_number).map(pin => (
                        <tr key={pin.id}>
                          <td><span className="badge bg-dark">GPIO {pin.pin_number}</span></td>
                          <td>
                            {editingPin === pin.id ? (
                              <input
                                type="text"
                                className="form-control form-control-sm"
                                value={editForm.label}
                                onChange={(e) => setEditForm({ ...editForm, label: e.target.value })}
                              />
                            ) : (
                              pin.label || `GPIO ${pin.pin_number}`
                            )}
                          </td>
                          <td>
                            {editingPin === pin.id ? (
                              <select
                                className="form-select form-select-sm"
                                value={editForm.pin_type}
                                onChange={(e) => setEditForm({ ...editForm, pin_type: e.target.value })}
                              >
                                <option value="digital">Digital</option>
                                <option value="analog">Analog</option>
                              </select>
                            ) : (
                              <span className={`badge ${pin.pin_type === 'digital' ? 'bg-primary' : 'bg-info'}`}>
                                {pin.pin_type}
                              </span>
                            )}
                          </td>
                          <td>{pin.value}</td>
                          <td className="text-end">
                            {editingPin === pin.id ? (
                              <>
                                <button className="btn btn-sm btn-success me-1" onClick={() => handleUpdatePin(pin.id)}>
                                  <i className="bi bi-check"></i>
                                </button>
                                <button className="btn btn-sm btn-secondary" onClick={() => setEditingPin(null)}>
                                  <i className="bi bi-x"></i>
                                </button>
                              </>
                            ) : (
                              <>
                                <button className="btn btn-sm btn-outline-primary me-1" onClick={() => startEditing(pin)}>
                                  <i className="bi bi-pencil"></i>
                                </button>
                                <button className="btn btn-sm btn-outline-danger" onClick={() => handleDeletePin(pin.id)}>
                                  <i className="bi bi-trash"></i>
                                </button>
                              </>
                            )}
                          </td>
                        </tr>
                      ))}
                      {pins.length === 0 && (
                        <tr>
                          <td colSpan="5" className="text-center text-muted py-3">
                            No GPIO pins configured
                          </td>
                        </tr>
                      )}
                    </tbody>
                  </table>
                </div>

                {/* Add new pin */}
                <hr />
                <h6 className="mb-3"><i className="bi bi-plus-circle me-2"></i>Add New Pin</h6>
                <form onSubmit={handleAddPin} className="row g-2 align-items-end">
                  <div className="col-auto">
                    <label className="form-label small">Pin Number</label>
                    <input
                      type="number"
                      className="form-control form-control-sm"
                      value={newPin.pin_number}
                      onChange={(e) => setNewPin({ ...newPin, pin_number: e.target.value })}
                      required
                      min="0"
                      max="16"
                      placeholder="e.g. 2"
                    />
                  </div>
                  <div className="col">
                    <label className="form-label small">Label</label>
                    <input
                      type="text"
                      className="form-control form-control-sm"
                      value={newPin.label}
                      onChange={(e) => setNewPin({ ...newPin, label: e.target.value })}
                      placeholder="e.g. Relay 1"
                    />
                  </div>
                  <div className="col-auto">
                    <label className="form-label small">Type</label>
                    <select
                      className="form-select form-select-sm"
                      value={newPin.pin_type}
                      onChange={(e) => setNewPin({ ...newPin, pin_type: e.target.value })}
                    >
                      <option value="digital">Digital</option>
                      <option value="analog">Analog</option>
                    </select>
                  </div>
                  <div className="col-auto">
                    <button type="submit" className="btn btn-sm btn-primary">
                      <i className="bi bi-plus-lg me-1"></i>Add
                    </button>
                  </div>
                </form>
              </>
            )}
          </div>
          <div className="modal-footer">
            <button type="button" className="btn btn-secondary" onClick={onClose}>Close</button>
          </div>
        </div>
      </div>
    </div>
  );
}

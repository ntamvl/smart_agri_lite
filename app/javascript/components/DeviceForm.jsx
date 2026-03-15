import React, { useState, useEffect } from 'react';
import { api } from './api';

export default function DeviceForm({ device, onSave, onClose }) {
  const isEditing = !!device;
  const [form, setForm] = useState({
    name: '',
    description: '',
    code: '',
    ip: '',
  });
  const [errors, setErrors] = useState('');
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    if (device) {
      setForm({
        name: device.name || '',
        description: device.description || '',
        code: device.code || '',
        ip: device.ip || '',
      });
    }
  }, [device]);

  const handleChange = (e) => {
    setForm({ ...form, [e.target.name]: e.target.value });
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setSaving(true);
    setErrors('');

    try {
      let result;
      if (isEditing) {
        result = await api.updateDevice(device.id, form);
      } else {
        result = await api.createDevice(form);
      }
      onSave(result);
    } catch (err) {
      setErrors(err.message);
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className="modal show d-block" tabIndex="-1" style={{ backgroundColor: 'rgba(0,0,0,0.5)' }}>
      <div className="modal-dialog modal-dialog-centered">
        <div className="modal-content">
          <div className="modal-header">
            <h5 className="modal-title">
              <i className={`bi ${isEditing ? 'bi-pencil' : 'bi-plus-circle'} me-2`}></i>
              {isEditing ? 'Edit Device' : 'Add New Device'}
            </h5>
            <button type="button" className="btn-close" onClick={onClose}></button>
          </div>
          <form onSubmit={handleSubmit}>
            <div className="modal-body">
              {errors && (
                <div className="alert alert-danger">{errors}</div>
              )}
              <div className="mb-3">
                <label className="form-label">Device Name *</label>
                <input
                  type="text"
                  className="form-control"
                  name="name"
                  value={form.name}
                  onChange={handleChange}
                  required
                  placeholder="e.g. ESP8266 Garden"
                />
              </div>
              <div className="mb-3">
                <label className="form-label">Device Code *</label>
                <input
                  type="text"
                  className="form-control"
                  name="code"
                  value={form.code}
                  onChange={handleChange}
                  required
                  disabled={isEditing}
                  placeholder="e.g. esp8266_garden (unique identifier)"
                />
                {isEditing && <small className="text-muted">Code cannot be changed after creation</small>}
              </div>
              <div className="mb-3">
                <label className="form-label">IP Address</label>
                <input
                  type="text"
                  className="form-control"
                  name="ip"
                  value={form.ip}
                  onChange={handleChange}
                  placeholder="e.g. 192.168.1.100"
                />
              </div>
              <div className="mb-3">
                <label className="form-label">Description</label>
                <textarea
                  className="form-control"
                  name="description"
                  value={form.description}
                  onChange={handleChange}
                  rows="3"
                  placeholder="Optional device description"
                />
              </div>
            </div>
            <div className="modal-footer">
              <button type="button" className="btn btn-secondary" onClick={onClose}>Cancel</button>
              <button type="submit" className="btn btn-primary" disabled={saving}>
                {saving ? (
                  <>
                    <span className="spinner-border spinner-border-sm me-2"></span>
                    Saving...
                  </>
                ) : (
                  <><i className="bi bi-check-lg me-1"></i> {isEditing ? 'Update' : 'Create'}</>
                )}
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
  );
}

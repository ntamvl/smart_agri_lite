import React, { useState, useEffect, useCallback } from 'react';
import { api } from './api';
import { subscribeToDeviceStatus } from './cable';
import DeviceCard from './DeviceCard';
import DeviceForm from './DeviceForm';
import GpioPinForm from './GpioPinForm';
import Footer from './Footer';

export default function App() {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [showDeviceForm, setShowDeviceForm] = useState(false);
  const [editingDevice, setEditingDevice] = useState(null);
  const [managingPinsDevice, setManagingPinsDevice] = useState(null);
  const [activeTab, setActiveTab] = useState('dashboard');

  // Load devices
  const loadDevices = useCallback(async () => {
    try {
      const data = await api.getDevices();
      setDevices(data);
    } catch (err) {
      console.error('Failed to load devices:', err);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    loadDevices();
  }, [loadDevices]);

  // Subscribe to ActionCable for real-time updates
  useEffect(() => {
    const subscription = subscribeToDeviceStatus((data) => {
      if (data.type === 'device_update' && data.device) {
        // Full device update from MQTT status
        setDevices(prev =>
          prev.map(d => d.id === data.device.id ? data.device : d)
        );
      } else if (data.pin_id) {
        // Single pin update from control action
        setDevices(prev =>
          prev.map(d => {
            if (d.id === data.device_id) {
              return {
                ...d,
                gpio_pins: d.gpio_pins.map(p =>
                  p.id === data.pin_id ? { ...p, value: data.value } : p
                ),
              };
            }
            return d;
          })
        );
      }
    });

    return () => subscription.unsubscribe();
  }, []);

  const handleDeviceSaved = (savedDevice) => {
    setDevices(prev => {
      const exists = prev.find(d => d.id === savedDevice.id);
      if (exists) {
        return prev.map(d => d.id === savedDevice.id ? savedDevice : d);
      }
      return [...prev, savedDevice];
    });
    setShowDeviceForm(false);
    setEditingDevice(null);
  };

  const handleDeleteDevice = async (device) => {
    if (!confirm(`Delete device "${device.name}"? This will also delete all its GPIO pins.`)) return;
    try {
      await api.deleteDevice(device.id);
      setDevices(prev => prev.filter(d => d.id !== device.id));
    } catch (err) {
      alert('Delete failed: ' + err.message);
    }
  };

  const handleEditDevice = (device) => {
    setEditingDevice(device);
    setShowDeviceForm(true);
  };

  const handlePinUpdate = (deviceId, updatedPin) => {
    setDevices(prev =>
      prev.map(d => {
        if (d.id === deviceId) {
          return {
            ...d,
            gpio_pins: d.gpio_pins.map(p =>
              p.id === updatedPin.id ? updatedPin : p
            ),
          };
        }
        return d;
      })
    );
  };

  const handleManagePins = (device) => {
    setManagingPinsDevice(device);
  };

  // Stats
  const totalPins = devices.reduce((sum, d) => sum + (d.gpio_pins || []).length, 0);
  const activePins = devices.reduce((sum, d) =>
    sum + (d.gpio_pins || []).filter(p => p.value > 0).length, 0
  );

  return (
    <div className="app-wrapper">
      {/* Navbar */}
      <nav className="navbar navbar-expand-lg navbar-dark bg-dark shadow-sm">
        <div className="container-fluid">
          <a className="navbar-brand d-flex align-items-center" href="/">
            <i className="bi bi-cpu-fill me-2 text-success fs-4"></i>
            <span className="fw-bold">Smart Agri</span>
            <span className="badge bg-success ms-2 small">IoT</span>
          </a>
          <div className="d-flex align-items-center gap-2">
            <button
              className="btn btn-success btn-sm"
              onClick={() => { setEditingDevice(null); setShowDeviceForm(true); }}
            >
              <i className="bi bi-plus-lg me-1"></i> Add Device
            </button>
          </div>
        </div>
      </nav>

      <div className="container-fluid py-4 tw:min-h-[calc(100vh_-_120px)]">
        {/* Stats bar */}
        <div className="row g-3 mb-4">
          <div className="col-md-3 col-6">
            <div className="card bg-primary text-white shadow-sm">
              <div className="card-body py-3">
                <div className="d-flex justify-content-between align-items-center">
                  <div>
                    <div className="small text-white-50">Devices</div>
                    <div className="fs-4 fw-bold">{devices.length}</div>
                  </div>
                  <i className="bi bi-hdd-network fs-2 opacity-50"></i>
                </div>
              </div>
            </div>
          </div>
          <div className="col-md-3 col-6">
            <div className="card bg-info text-white shadow-sm">
              <div className="card-body py-3">
                <div className="d-flex justify-content-between align-items-center">
                  <div>
                    <div className="small text-white-50">Total Pins</div>
                    <div className="fs-4 fw-bold">{totalPins}</div>
                  </div>
                  <i className="bi bi-pin-map fs-2 opacity-50"></i>
                </div>
              </div>
            </div>
          </div>
          <div className="col-md-3 col-6">
            <div className="card bg-success text-white shadow-sm">
              <div className="card-body py-3">
                <div className="d-flex justify-content-between align-items-center">
                  <div>
                    <div className="small text-white-50">Active Pins</div>
                    <div className="fs-4 fw-bold">{activePins}</div>
                  </div>
                  <i className="bi bi-lightning fs-2 opacity-50"></i>
                </div>
              </div>
            </div>
          </div>
          <div className="col-md-3 col-6">
            <div className="card bg-warning text-dark shadow-sm">
              <div className="card-body py-3">
                <div className="d-flex justify-content-between align-items-center">
                  <div>
                    <div className="small opacity-75">Inactive Pins</div>
                    <div className="fs-4 fw-bold">{totalPins - activePins}</div>
                  </div>
                  <i className="bi bi-moon fs-2 opacity-50"></i>
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* Device list */}
        {loading ? (
          <div className="text-center py-5">
            <div className="spinner-border text-primary" role="status">
              <span className="visually-hidden">Loading...</span>
            </div>
            <p className="mt-2 text-muted">Loading devices...</p>
          </div>
        ) : devices.length === 0 ? (
          <div className="text-center py-5">
            <i className="bi bi-hdd-network fs-1 text-muted d-block mb-3"></i>
            <h5 className="text-muted">No devices found</h5>
            <p className="text-muted">Click "Add Device" to get started</p>
            <button
              className="btn btn-primary"
              onClick={() => { setEditingDevice(null); setShowDeviceForm(true); }}
            >
              <i className="bi bi-plus-lg me-1"></i> Add Your First Device
            </button>
          </div>
        ) : (
          <div className="row g-4">
            {devices.map(device => (
              <div key={device.id} className="col-lg-6 col-xl-4">
                <DeviceCard
                  device={device}
                  onEdit={handleEditDevice}
                  onDelete={handleDeleteDevice}
                  onPinUpdate={handlePinUpdate}
                  onManagePins={handleManagePins}
                />
              </div>
            ))}
          </div>
        )}
      </div>

      <Footer />

      {/* Modals */}
      {showDeviceForm && (
        <DeviceForm
          device={editingDevice}
          onSave={handleDeviceSaved}
          onClose={() => { setShowDeviceForm(false); setEditingDevice(null); }}
        />
      )}

      {managingPinsDevice && (
        <GpioPinForm
          device={managingPinsDevice}
          onClose={() => setManagingPinsDevice(null)}
          onUpdate={loadDevices}
        />
      )}
    </div>
  );
}

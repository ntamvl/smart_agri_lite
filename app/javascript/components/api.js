// API helper for making requests to the Rails API
const API_BASE = '/api';

function getCSRFToken() {
  const meta = document.querySelector('meta[name="csrf-token"]');
  return meta ? meta.getAttribute('content') : '';
}

async function request(method, path, body = null) {
  const options = {
    method,
    headers: {
      'Content-Type': 'application/json',
      'X-CSRF-Token': getCSRFToken(),
    },
  };

  if (body) {
    options.body = JSON.stringify(body);
  }

  const response = await fetch(`${API_BASE}${path}`, options);

  if (response.status === 204) return null;

  const data = await response.json();

  if (!response.ok) {
    throw new Error(data.errors ? data.errors.join(', ') : 'Request failed');
  }

  return data;
}

export const api = {
  // Devices
  getDevices: () => request('GET', '/devices'),
  getDevice: (id) => request('GET', `/devices/${id}`),
  createDevice: (data) => request('POST', '/devices', data),
  updateDevice: (id, data) => request('PUT', `/devices/${id}`, data),
  deleteDevice: (id) => request('DELETE', `/devices/${id}`),

  // GPIO Pins
  getGpioPins: (deviceId) => request('GET', `/devices/${deviceId}/gpio_pins`),
  createGpioPin: (deviceId, data) => request('POST', `/devices/${deviceId}/gpio_pins`, data),
  updateGpioPin: (deviceId, pinId, data) => request('PUT', `/devices/${deviceId}/gpio_pins/${pinId}`, data),
  deleteGpioPin: (deviceId, pinId) => request('DELETE', `/devices/${deviceId}/gpio_pins/${pinId}`),
  controlPin: (deviceId, pinId, value) => request('PUT', `/devices/${deviceId}/gpio_pins/${pinId}/control`, { value }),
};

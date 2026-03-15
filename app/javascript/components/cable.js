import { createConsumer } from '@rails/actioncable';

const consumer = createConsumer();

export function subscribeToDeviceStatus(onReceived) {
  return consumer.subscriptions.create('DeviceStatusChannel', {
    received(data) {
      onReceived(data);
    },
    connected() {
      console.log('[ActionCable] Connected to DeviceStatusChannel');
    },
    disconnected() {
      console.log('[ActionCable] Disconnected from DeviceStatusChannel');
    },
  });
}

export default consumer;

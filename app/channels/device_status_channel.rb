class DeviceStatusChannel < ApplicationCable::Channel
  def subscribed
    stream_from "device_status"
  end

  def unsubscribed
    # Cleanup when channel is unsubscribed
  end
end

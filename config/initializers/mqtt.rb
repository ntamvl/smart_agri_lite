# MQTT Subscriber — runs in a background thread on server boot
# Subscribes to device status updates and broadcasts them via ActionCable

if defined?(Rails::Server) || defined?(Puma)
  Rails.application.config.after_initialize do
    Thread.new do
      Rails.logger.info "[MQTT] Starting status subscriber..."
      sleep 3 # Wait for full app initialization
      begin
        MqttService.subscribe_status
      rescue => e
        Rails.logger.error "[MQTT] Subscriber crashed: #{e.message}"
        sleep 5
        retry
      end
    end
  end
end

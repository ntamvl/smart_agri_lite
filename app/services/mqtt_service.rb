class MqttService
  config = YAML.load_file(Rails.root.join("config/mqtt.yml")).deep_symbolize_keys

  MQTT_CONFIG = {
    host: config[:host],
    port: config[:port],
    username: config[:username],
    password: config[:password],
    ssl: config[:ssl],
    client_id: config[:client_id] || "smart_agri_web_#{SecureRandom.hex(4)}",
  }.freeze

  # vinhlong/tamvinhlong/01/esp8266
  # smart_agri/device
  TOPIC_PREFIX = "vinhlong/tamvinhlong/01/esp8266".freeze

  class << self
    def publish_control(device_code, pin_number, value)
      topic = "#{TOPIC_PREFIX}/#{device_code}/pin"
      payload = { pin: pin_number, value: value }.to_json

      Rails.logger.info "[MQTT] Publishing to #{topic}: #{payload}"

      client = create_client
      client.publish(topic, payload, retain: false, qos: 1)
      client.disconnect
    rescue => e
      Rails.logger.error "[MQTT] Publish error: #{e.message}"
    end

    def subscribe_status
      client = create_client
      topic = "#{TOPIC_PREFIX}/+/status_pin"

      Rails.logger.info "[MQTT] Subscribing to #{topic}"

      client.get(topic) do |topic_name, message|
        Rails.logger.info "[MQTT] Received on #{topic_name}: #{message}"
        process_status_message(topic_name, message)
      end
    rescue => e
      Rails.logger.error "[MQTT] Subscribe error: #{e.message}"
      sleep 5
      retry
    end

    private

    def create_client
      MQTT::Client.connect(MQTT_CONFIG)
    end

    def process_status_message(topic_name, message)
      # Extract device code from topic: vinhlong/tamvinhlong/01/esp8266/{code}/status_pin
      device_code = topic_name.split("/").last(2)[0]
      return unless device_code

      data = JSON.parse(message)
      device = Device.find_by(code: device_code)
      return unless device

      # Update device IP if present
      device.update(ip: data["ip"]) if data["ip"].present?
      device.update(updated_at: Time.current) if data["status"] == "ok"

      # Update pin values
      if data["pins"].is_a?(Array)
        data["pins"].each do |pin_data|
          gpio_pin = device.gpio_pins.find_by(pin_number: pin_data["pin"])
          gpio_pin&.update(value: pin_data["value"]) if pin_data["value"].present?
        end
      end

      if data["pins"].is_a?(Array)
        # TODO: Update all pins at once for better performance
        # Reset all pins to zero if not present in the message
        device.gpio_pins.each do |gpio_pin|
          pin_data = data["pins"].find { |p| p["pin"] == gpio_pin.pin_number }
          if pin_data
            gpio_pin.update(value: pin_data["value"]) if pin_data["value"].present?
          else
            gpio_pin.update(value: 0) # Reset to zero if not in message
          end
        end
      end

      # Broadcast full device status via ActionCable
      ActionCable.server.broadcast("device_status", {
        type: "device_update",
        device: device.as_json(include: :gpio_pins),
      })
    rescue JSON::ParserError => e
      Rails.logger.error "[MQTT] JSON parse error: #{e.message}"
    end
  end
end

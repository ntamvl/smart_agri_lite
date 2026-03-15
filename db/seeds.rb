# Create sample devices with default GPIO pins
puts "Seeding devices..."

device1 = Device.find_or_create_by!(code: "TAM_VINHLONG_01_ESP8266_PIN") do |d|
  d.name = "TAM_VINHLONG_01"
  d.description = "Demo ESP8266 device for testing GPIO controls"
  d.ip = "192.168.1.100"
end

# Update default pin labels for the demo device
{ 2 => "Relay 1", 12 => "Relay 2", 13 => "Relay 3", 14 => "Relay 4", 15 => "Relay 5" }.each do |pin_number, label|
  pin = device1.gpio_pins.find_by(pin_number: pin_number)
  pin&.update!(label: label)
end

device2 = Device.find_or_create_by!(code: "esp8266_garden") do |d|
  d.name = "ESP8266 Garden"
  d.description = "Garden irrigation controller"
  d.ip = "192.168.1.101"
end

{ 2 => "Water Pump", 12 => "Valve 1", 13 => "Valve 2", 14 => "Light", 15 => "Fan" }.each do |pin_number, label|
  pin = device2.gpio_pins.find_by(pin_number: pin_number)
  pin&.update!(label: label)
end

puts "Seeded #{Device.count} devices with #{GpioPin.count} GPIO pins."

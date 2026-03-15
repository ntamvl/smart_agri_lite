module Api
  class GpioPinsController < ApplicationController
    skip_before_action :verify_authenticity_token
    before_action :set_device
    before_action :set_gpio_pin, only: [ :show, :update, :destroy, :control ]

    # GET /api/devices/:device_id/gpio_pins
    def index
      @gpio_pins = @device.gpio_pins.order(:pin_number)
      render json: @gpio_pins
    end

    # GET /api/devices/:device_id/gpio_pins/:id
    def show
      render json: @gpio_pin
    end

    # POST /api/devices/:device_id/gpio_pins
    def create
      @gpio_pin = @device.gpio_pins.build(gpio_pin_params)
      if @gpio_pin.save
        render json: @gpio_pin, status: :created
      else
        render json: { errors: @gpio_pin.errors.full_messages }, status: :unprocessable_entity
      end
    end

    # PUT /api/devices/:device_id/gpio_pins/:id
    def update
      if @gpio_pin.update(gpio_pin_params)
        render json: @gpio_pin
      else
        render json: { errors: @gpio_pin.errors.full_messages }, status: :unprocessable_entity
      end
    end

    # DELETE /api/devices/:device_id/gpio_pins/:id
    def destroy
      @gpio_pin.destroy
      head :no_content
    end

    # PUT /api/devices/:device_id/gpio_pins/:id/control
    def control
      new_value = params[:value].to_i
      if @gpio_pin.update(value: new_value)
        # Publish MQTT control message
        MqttService.publish_control(@device.code, @gpio_pin.pin_number, new_value)

        # Broadcast update via ActionCable
        ActionCable.server.broadcast("device_status", {
          device_id: @device.id,
          device_code: @device.code,
          pin_id: @gpio_pin.id,
          pin_number: @gpio_pin.pin_number,
          value: new_value
        })

        render json: @gpio_pin
      else
        render json: { errors: @gpio_pin.errors.full_messages }, status: :unprocessable_entity
      end
    end

    private

    def set_device
      @device = Device.find(params[:device_id])
    end

    def set_gpio_pin
      @gpio_pin = @device.gpio_pins.find(params[:id])
    end

    def gpio_pin_params
      params.permit(:pin_number, :label, :pin_type, :value)
    end
  end
end

module Api
  class DevicesController < ApplicationController
    skip_before_action :verify_authenticity_token
    before_action :set_device, only: [ :show, :update, :destroy ]

    # GET /api/devices
    def index
      @devices = Device.includes(:gpio_pins).order(:name)
      render json: @devices.as_json(include: :gpio_pins)
    end

    # GET /api/devices/:id
    def show
      render json: @device.as_json(include: :gpio_pins)
    end

    # POST /api/devices
    def create
      @device = Device.new(device_params)
      if @device.save
        render json: @device.as_json(include: :gpio_pins), status: :created
      else
        render json: { errors: @device.errors.full_messages }, status: :unprocessable_entity
      end
    end

    # PUT /api/devices/:id
    def update
      if @device.update(device_params)
        render json: @device.as_json(include: :gpio_pins)
      else
        render json: { errors: @device.errors.full_messages }, status: :unprocessable_entity
      end
    end

    # DELETE /api/devices/:id
    def destroy
      @device.destroy
      head :no_content
    end

    private

    def set_device
      @device = Device.find(params[:id])
    end

    def device_params
      params.permit(:name, :description, :code, :ip)
    end
  end
end

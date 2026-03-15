class Device < ApplicationRecord
  has_many :gpio_pins, dependent: :destroy
  accepts_nested_attributes_for :gpio_pins, allow_destroy: true

  validates :name, presence: true
  validates :code, presence: true, uniqueness: true

  DEFAULT_PINS = [ 2, 12, 13, 14, 15 ].freeze

  after_create :create_default_gpio_pins

  private

  def create_default_gpio_pins
    DEFAULT_PINS.each do |pin_number|
      gpio_pins.create!(
        pin_number: pin_number,
        label: "GPIO #{pin_number}",
        pin_type: "digital",
        value: 0
      )
    end
  end
end

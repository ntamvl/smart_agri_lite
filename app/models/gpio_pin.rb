class GpioPin < ApplicationRecord
  belongs_to :device

  validates :pin_number, presence: true,
    uniqueness: { scope: :device_id, message: "already exists for this device" }
  validates :pin_type, inclusion: { in: %w[digital analog] }
  validates :value, numericality: { only_integer: true, greater_than_or_equal_to: 0 }

  validate :value_within_range

  private

  def value_within_range
    if pin_type == "digital" && value.present? && !value.between?(0, 1)
      errors.add(:value, "must be 0 or 1 for digital pins")
    elsif pin_type == "analog" && value.present? && !value.between?(0, 255)
      errors.add(:value, "must be between 0 and 255 for analog pins")
    end
  end
end

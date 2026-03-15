class CreateGpioPins < ActiveRecord::Migration[8.1]
  def change
    create_table :gpio_pins do |t|
      t.references :device, null: false, foreign_key: true
      t.integer :pin_number, null: false
      t.string :label
      t.string :pin_type, default: "digital"
      t.integer :value, default: 0

      t.timestamps
    end

    add_index :gpio_pins, [ :device_id, :pin_number ], unique: true
  end
end

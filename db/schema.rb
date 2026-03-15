# This file is auto-generated from the current state of the database. Instead
# of editing this file, please use the migrations feature of Active Record to
# incrementally modify your database, and then regenerate this schema definition.
#
# This file is the source Rails uses to define your schema when running `bin/rails
# db:schema:load`. When creating a new database, `bin/rails db:schema:load` tends to
# be faster and is potentially less error prone than running all of your
# migrations from scratch. Old migrations may fail to apply correctly if those
# migrations use external dependencies or application code.
#
# It's strongly recommended that you check this file into your version control system.

ActiveRecord::Schema[8.1].define(version: 2026_03_11_170001) do
  create_table "devices", force: :cascade do |t|
    t.string "code", null: false
    t.datetime "created_at", null: false
    t.text "description"
    t.string "ip"
    t.string "name", null: false
    t.datetime "updated_at", null: false
    t.index ["code"], name: "index_devices_on_code", unique: true
  end

  create_table "gpio_pins", force: :cascade do |t|
    t.datetime "created_at", null: false
    t.integer "device_id", null: false
    t.string "label"
    t.integer "pin_number", null: false
    t.string "pin_type", default: "digital"
    t.datetime "updated_at", null: false
    t.integer "value", default: 0
    t.index ["device_id", "pin_number"], name: "index_gpio_pins_on_device_id_and_pin_number", unique: true
    t.index ["device_id"], name: "index_gpio_pins_on_device_id"
  end

  add_foreign_key "gpio_pins", "devices"
end

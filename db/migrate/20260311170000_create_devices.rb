class CreateDevices < ActiveRecord::Migration[8.1]
  def change
    create_table :devices do |t|
      t.string :name, null: false
      t.text :description
      t.string :code, null: false
      t.string :ip

      t.timestamps
    end

    add_index :devices, :code, unique: true
  end
end

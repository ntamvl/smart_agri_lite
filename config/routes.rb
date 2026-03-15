Rails.application.routes.draw do
  # Dashboard — React SPA mount point
  root "dashboard#index"

  # API endpoints
  namespace :api do
    resources :devices do
      resources :gpio_pins do
        member do
          put :control
        end
      end
    end
  end

  # Health check
  get "up" => "rails/health#show", as: :rails_health_check
end

# Smart Agri Web App
Ứng dụngr  điều khiển ESP8266 thông qua giao diện web

## Đăng ký tài khoản HiveMQ (miễn phí)
https://console.hivemq.cloud/

## Run Smart Agri Web App

![Smart Agri Web App](./screenshot.png)

**Run with docker**
```bash
./start_dev_docker.sh
```

hoặc

```bash
docker-compose -f docker-compose.dev.yml up
```

## Seed data

**Test Code**
```
Name: TAM_VINHLONG_01
Code: TAM_VINHLONG_01_ESP8266_PIN
```

Command to run seed data
```bash
rails db:seed
```

## Arduino Code
Xem thêm tại thư mục `arduino/README.md`

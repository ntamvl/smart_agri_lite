# RF 433MHz RX480 → ESP8266
Dưới đây là bảng kết nối chi tiết để tiện tra cứu khi lắp mạch:

## RF 433MHz RX480 → ESP8266

| RX480	| ESP8266 |	Mô tả |
| --- | --- | --- |
| VCC	| 3V3	| Nguồn 3.3V |
| GND	| GND	| Nối đất |
| D0 out	| D0 · GPIO16	| Kênh RF 1 (input) |
| D1 out	| D1 · GPIO5	| Kênh RF 2 (input) |
| D2 out	| D2 · GPIO4	| Kênh RF 3 (input) |

## ESP8266 → Relay Board

| ESP8266	| Relay Board	| Điều khiển |
| --- | --- | --- |
| VIN	| VCC	| Nguồn 5V (từ USB) |
| GND	| GND	| Nối đất |
| D5 · GPIO14	| IN1	| Relay 1 |
| D6 · GPIO12	| IN2	| Relay 2 |
| D7 · GPIO13	| IN3	| Relay 3|
| —	| IN4	| Chưa sử dụng |

**Lưu ý khi lắp mạch:**

- GPIO16 (D0) trên NodeMCU không hỗ trợ interrupt — chỉ dùng được  với digitalRead() trong loop(), đúng với cách syncRFToRelay() đang - hoạt động.
- Relay board thường dùng logic Active LOW (IN = LOW → relay bật). Nếu relay hoạt động ngược, đổi value 0/1 trong code hoặc dùng - !value khi digitalWrite.
- Nên dùng nguồn riêng cho relay board nếu tải điện lớn, tránh sụt - áp ảnh hưởng ESP8266.


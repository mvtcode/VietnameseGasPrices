# VietnameseGasPrices

Thư viện ESP32 / Arduino cập nhật **Giá gas bán lẻ (LPG)** tại các tỉnh/thành phố Việt Nam (**Hải Phòng**, **Hà Nội**, **Đà Nẵng**, **TP. Hồ Chí Minh**, **Cần Thơ**) từ nguồn chính thức của **Petrolimex Gas**.

---

## ✨ Tính năng nổi bật

- 🔥 **Độ phủ chi tiết**: Cập nhật giá gas bình dân dụng 12kg và bình thương mại 48kg cho đầy đủ các khu vực trọng điểm: Hải Phòng, Hà Nội, Đà Nẵng, TP.HCM, Cần Thơ.
- ⚡ **Non-blocking FreeRTOS Task**: Hỗ trợ fetch dữ liệu ngầm bằng FreeRTOS task riêng biệt, không gây giật lag màn hình LED Matrix / LVGL / OLED.
- 🛡️ **An toàn bộ nhớ**: Tối ưu hóa bộ nhớ RAM, payload JSON siêu nhẹ (~450 bytes), sử dụng ArduinoJson v7 an toàn và ổn định.
- ⏱️ **Tùy biến linh hoạt**: Tùy chọn khu vực mục tiêu (mặc định Hải Phòng), chu kỳ cập nhật và URL endpoint backend.
- 🔄 **Hỗ trợ Fallback thông minh**: Tự động ánh xạ khu vực và fallback an toàn khi mất mạng hoặc dữ liệu chưa sẵn sàng.

---

## 📦 Cài đặt

Thêm vào `platformio.ini`:
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0.3
    https://github.com/mvtcode/VietnameseGasPrices.git
```

Hoặc sử dụng thư viện cục bộ (nếu để cùng thư mục projects):
```ini
lib_extra_dirs = 
    d:/projects
```

---

## 🚀 Hướng dẫn sử dụng nhanh

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <VietnameseGasPrices.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
    Serial.begin(115200);
    delay(1000);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");

    // Khởi tạo service với thành phố mặc định là "Hải Phòng"
    VietnameseGasPrices::init("Hải Phòng");
    VietnameseGasPrices::setUpdateInterval(60); // 60 phút

    // Fetch ngay lần đầu (đồng bộ)
    if (VietnameseGasPrices::fetchNow()) {
        GasPriceInfo gas = VietnameseGasPrices::getGas();
        Serial.printf("=== GIÁ GAS THÁNG %s ===\n", gas.period);
        Serial.printf("Hải Phòng (12kg): %d đ (%d k)\n", gas.hp_12kg_price, gas.hp_12kg_k);
        Serial.printf("Hà Nội (12kg):    %d đ (%d k)\n", gas.hn_12kg_price, gas.hn_12kg_k);
        Serial.printf("Đà Nẵng (12kg):   %d đ (%d k)\n", gas.dn_12kg_price, gas.dn_12kg_k);
        Serial.printf("TP.HCM (12kg):    %d đ (%d k)\n", gas.hcm_12kg_price, gas.hcm_12kg_k);
        Serial.printf("Cần Thơ (12kg):   %d đ (%d k)\n", gas.ct_12kg_price, gas.ct_12kg_k);
    }
}

void loop() {
    // Tự động kiểm tra và cập nhật ngầm trong FreeRTOS background task
    VietnameseGasPrices::update(WiFi.status() == WL_CONNECTED);

    // Lấy giá hiện tại của thành phố đã chọn (ví dụ: Hải Phòng)
    GasCityPrice cur = VietnameseGasPrices::getCurrentCityPrice();
    if (cur.price_12kg > 0) {
        // Hiển thị ra LED Matrix: "GAS: 598k"
        // Serial.printf("%s: %d k\n", cur.city, cur.display_12kg_k);
    }

    delay(1000);
}
```

---

## 📄 License
MIT License - Developed by [mvtcode](https://github.com/mvtcode).

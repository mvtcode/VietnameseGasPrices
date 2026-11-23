#include <Arduino.h>
#include <WiFi.h>
#include <VietnameseGasPrices.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\nDang ket noi WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");

    // 1. Khoi tao service voi thanh pho mac dinh la Hai Phong
    VietnameseGasPrices::init("Hải Phòng");

    // 2. Fetch truc tiep (blocking) lan dau
    Serial.println("Dang tai gia gas Viet Nam...");
    if (VietnameseGasPrices::fetchNow()) {
        GasPriceInfo gas = VietnameseGasPrices::getGas();
        Serial.println("\n=== BANG GIA GAS BAN LE PETROLIMEX ===");
        Serial.printf("Ky dieu hanh: Thang %s\n", gas.period);
        Serial.printf("Che do cache: %s\n\n", gas.cache_mode);

        for (uint8_t i = 0; i < gas.city_count; i++) {
            Serial.printf("- %-15s: 12kg = %7d d (%3d k) | 48kg = %d d\n",
                gas.cities[i].city,
                gas.cities[i].price_12kg,
                gas.cities[i].display_12kg_k,
                gas.cities[i].price_48kg
            );
        }

        // Lay nhanh gia cua thanh pho muc tieu (Hai Phong)
        GasCityPrice cur = VietnameseGasPrices::getCurrentCityPrice();
        Serial.printf("\n=> Thanh pho dang chon (%s): %d k (Gia: %d d)\n",
            cur.city, cur.display_12kg_k, cur.price_12kg);
    } else {
        Serial.println("Lay du lieu that bai!");
    }
}

void loop() {
    // Non-blocking update dinh ky trong FreeRTOS task
    VietnameseGasPrices::update(WiFi.status() == WL_CONNECTED);

    // Doc gia hien tai ma khong block chuong trinh
    GasCityPrice cur = VietnameseGasPrices::getCurrentCityPrice();
    if (cur.price_12kg > 0) {
        // Serial.printf("GAS: %d k\n", cur.display_12kg_k);
    }

    delay(2000);
}

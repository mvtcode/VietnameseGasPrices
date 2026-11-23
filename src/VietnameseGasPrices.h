#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define MAX_GAS_CITIES 8

struct GasCityPrice {
    char city[24] = "";
    int price_12kg = 0;
    int price_48kg = 0;
    int display_12kg_k = 0;
};

struct GasPriceInfo {
    char period[16] = "";             // Kỳ báo giá, vd "11/2026"
    char cache_mode[24] = "";         // "normal_2h", "transition_15m"
    GasCityPrice cities[MAX_GAS_CITIES];
    uint8_t city_count = 0;

    // Các thành phố phổ biến hỗ trợ truy cập nhanh
    int hp_12kg_price = 0;            // Hải Phòng 12kg (vd: 598104)
    int hp_12kg_k = 0;                // Hải Phòng 12kg rút gọn k (vd: 598)
    int hn_12kg_price = 0;            // Hà Nội 12kg (vd: 598752)
    int hn_12kg_k = 0;                // Hà Nội 12kg rút gọn k (vd: 599)
    int dn_12kg_price = 0;            // Đà Nẵng 12kg
    int dn_12kg_k = 0;
    int hcm_12kg_price = 0;           // TP. Hồ Chí Minh 12kg
    int hcm_12kg_k = 0;
    int ct_12kg_price = 0;            // Cần Thơ 12kg
    int ct_12kg_k = 0;

    bool is_valid = false;
    uint32_t last_update_time = 0;
};

class VietnameseGasPrices {
private:
    static GasPriceInfo current_gas;
    static SemaphoreHandle_t gasMutex;
    static uint32_t last_fetch_time;
    static bool is_fetching;
    static uint32_t update_interval_ms;
    static char target_city[32];
    static char api_url[128];

    static void fetchGasTask(void *param);

public:
    static void init(const char *defaultCity = "Hải Phòng", const char *endpoint = "http://gas-prices.zaitan.workers.dev/");
    static void setTargetCity(const char *cityName);
    static void setApiUrl(const char *url);
    static void setUpdateInterval(uint32_t intervalMinutes);
    static void update(bool wifiConnected, bool force = false, uint32_t intervalMinutes = 0);
    static bool fetchNow();
    static GasPriceInfo getGas();
    
    // Tìm kiếm giá theo tên thành phố (hỗ trợ "Hải Phòng", "Hà Nội", "Đà Nẵng", "TP. Hồ Chí Minh", "Cần Thơ",...)
    static GasCityPrice getPriceForCity(const char *cityName);
    
    // Lấy nhanh giá của target_city đã cài đặt
    static GasCityPrice getCurrentCityPrice();

    static const char* getTargetCity() { return target_city; }
    static bool isFetching() { return is_fetching; }
};

// Alias để tương thích và tiện dụng
using GasService = VietnameseGasPrices;

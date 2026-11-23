#include "VietnameseGasPrices.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#if defined(DEBUG_VIETNAMESE_GAS) || defined(ENABLE_SERIAL_LOG)
#define VGP_LOG_I(fmt, ...) Serial.printf("[GasPrices][I] " fmt "\n", ##__VA_ARGS__)
#define VGP_LOG_W(fmt, ...) Serial.printf("[GasPrices][W] " fmt "\n", ##__VA_ARGS__)
#define VGP_LOG_E(fmt, ...) Serial.printf("[GasPrices][E] " fmt "\n", ##__VA_ARGS__)
#else
#define VGP_LOG_I(...)
#define VGP_LOG_W(...)
#define VGP_LOG_E(...)
#endif

GasPriceInfo VietnameseGasPrices::current_gas;
SemaphoreHandle_t VietnameseGasPrices::gasMutex = NULL;
uint32_t VietnameseGasPrices::last_fetch_time = 0;
bool VietnameseGasPrices::is_fetching = false;
uint32_t VietnameseGasPrices::update_interval_ms = 60 * 60 * 1000; // Mặc định 60 phút
char VietnameseGasPrices::target_city[32] = "Hải Phòng";
char VietnameseGasPrices::api_url[128] = "http://gas-prices.zaitan.workers.dev/";

static bool parseGasPayload(const String &payload, GasPriceInfo &outInfo) {
    if (payload.length() == 0) return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        VGP_LOG_E("deserializeJson() failed: %s", error.c_str());
        return false;
    }

    const char *p = doc["period"] | "";
    strncpy(outInfo.period, p, sizeof(outInfo.period) - 1);

    const char *cm = doc["cache_mode"] | "";
    strncpy(outInfo.cache_mode, cm, sizeof(outInfo.cache_mode) - 1);

    JsonArray dataArray = doc["data"].as<JsonArray>();
    outInfo.city_count = 0;

    for (JsonObject item : dataArray) {
        if (outInfo.city_count >= MAX_GAS_CITIES) break;

        const char *city = item["city"] | "";
        int p12 = item["price_12kg"] | 0;
        int p48 = item["price_48kg"] | 0;
        int dispK = item["display_12kg_k"] | 0;

        if (strlen(city) > 0 && p12 > 0) {
            GasCityPrice &g = outInfo.cities[outInfo.city_count];
            strncpy(g.city, city, sizeof(g.city) - 1);
            g.price_12kg = p12;
            g.price_48kg = p48;
            g.display_12kg_k = (dispK > 0) ? dispK : (int)((p12 + 500) / 1000);

            // Gán nhanh các thành phố chính
            if (strstr(city, "Hải Phòng") != NULL || strstr(city, "Hai Phong") != NULL) {
                outInfo.hp_12kg_price = p12;
                outInfo.hp_12kg_k = g.display_12kg_k;
            } else if (strstr(city, "Hà Nội") != NULL || strstr(city, "Ha Noi") != NULL) {
                outInfo.hn_12kg_price = p12;
                outInfo.hn_12kg_k = g.display_12kg_k;
            } else if (strstr(city, "Đà Nẵng") != NULL || strstr(city, "Da Nang") != NULL) {
                outInfo.dn_12kg_price = p12;
                outInfo.dn_12kg_k = g.display_12kg_k;
            } else if (strstr(city, "Hồ Chí Minh") != NULL || strstr(city, "Ho Chi Minh") != NULL || strstr(city, "Sài Gòn") != NULL) {
                outInfo.hcm_12kg_price = p12;
                outInfo.hcm_12kg_k = g.display_12kg_k;
            } else if (strstr(city, "Cần Thơ") != NULL || strstr(city, "Can Tho") != NULL) {
                outInfo.ct_12kg_price = p12;
                outInfo.ct_12kg_k = g.display_12kg_k;
            }

            outInfo.city_count++;
        }
    }

    return (outInfo.city_count > 0);
}

static bool executeFetch(const char *url, GasPriceInfo &resultInfo) {
    if (!url || strlen(url) == 0) return false;

    String payload = "";
    bool fetchSuccess = false;
    bool isHttps = (strncmp(url, "https://", 8) == 0);

    HTTPClient http;
    http.setTimeout(20000);
    http.setConnectTimeout(20000);
    http.setUserAgent("ESP32-VietnameseGasPrices/1.0");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (isHttps) {
        uint32_t maxAlloc = ESP.getMaxAllocHeap();
        if (maxAlloc < 15 * 1024) {
            VGP_LOG_W("Heap too fragmented for TLS: maxAlloc=%d", maxAlloc);
            return false;
        }

        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(15);
        static const char *alpn[] = {"http/1.1", NULL};
        client.setAlpnProtocols(alpn);

        VGP_LOG_I("Fetching gas prices (HTTPS) from: %s", url);
        if (http.begin(client, url)) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                payload = http.getString();
                fetchSuccess = true;
                VGP_LOG_I("Gas fetch OK, size: %d bytes", payload.length());
            } else {
                char errBuf[128] = {0};
                client.lastError(errBuf, sizeof(errBuf));
                VGP_LOG_W("HTTP Error: %d, TLS error: %s", httpCode, errBuf);
            }
            http.end();
        }
        client.stop();
    } else {
        WiFiClient client;
        client.setTimeout(15);

        VGP_LOG_I("Fetching gas prices (HTTP) from: %s", url);
        if (http.begin(client, url)) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                payload = http.getString();
                fetchSuccess = true;
                VGP_LOG_I("Gas fetch OK, size: %d bytes", payload.length());
            } else {
                VGP_LOG_W("HTTP Error: %d", httpCode);
            }
            http.end();
        }
        client.stop();
    }

    if (fetchSuccess && payload.length() > 0) {
        return parseGasPayload(payload, resultInfo);
    }
    return false;
}

void VietnameseGasPrices::init(const char *defaultCity, const char *endpoint) {
    if (gasMutex == NULL) {
        gasMutex = xSemaphoreCreateMutex();
    }
    if (defaultCity && strlen(defaultCity) > 0) {
        strncpy(target_city, defaultCity, sizeof(target_city) - 1);
    }
    if (endpoint && strlen(endpoint) > 0) {
        strncpy(api_url, endpoint, sizeof(api_url) - 1);
    }
    last_fetch_time = 0;
    is_fetching = false;
    VGP_LOG_I("Init VietnameseGasPrices, targetCity=%s", target_city);
}

void VietnameseGasPrices::setTargetCity(const char *cityName) {
    if (cityName && strlen(cityName) > 0) {
        if (gasMutex != NULL && xSemaphoreTake(gasMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            strncpy(target_city, cityName, sizeof(target_city) - 1);
            xSemaphoreGive(gasMutex);
        } else {
            strncpy(target_city, cityName, sizeof(target_city) - 1);
        }
    }
}

void VietnameseGasPrices::setApiUrl(const char *url) {
    if (url && strlen(url) > 0) {
        strncpy(api_url, url, sizeof(api_url) - 1);
    }
}

void VietnameseGasPrices::setUpdateInterval(uint32_t intervalMinutes) {
    if (intervalMinutes < 5) intervalMinutes = 5;
    update_interval_ms = intervalMinutes * 60 * 1000;
}

void VietnameseGasPrices::fetchGasTask(void *param) {
    GasPriceInfo info;
    if (executeFetch(api_url, info)) {
        if (gasMutex != NULL && xSemaphoreTake(gasMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            info.is_valid = true;
            info.last_update_time = millis();
            current_gas = info;
            xSemaphoreGive(gasMutex);
            VGP_LOG_I("Gas prices updated successfully!");
        }
    } else {
        VGP_LOG_W("Fetch gas prices failed");
    }
    is_fetching = false;
    vTaskDelete(NULL);
}

void VietnameseGasPrices::update(bool wifiConnected, bool force, uint32_t intervalMinutes) {
    if (!wifiConnected) return;

    uint32_t interval = (intervalMinutes > 0) ? (intervalMinutes * 60 * 1000) : update_interval_ms;
    if (!current_gas.is_valid) {
        interval = 20 * 1000; // Retry nhanh sau 20s neu chua co du lieu
    }

    uint32_t now = millis();
    if (!is_fetching && (force || last_fetch_time == 0 || (now - last_fetch_time >= interval))) {
        last_fetch_time = now;
        is_fetching = true;

        BaseType_t res = xTaskCreatePinnedToCore(
            fetchGasTask,
            "VnGasTask",
            6 * 1024,
            NULL,
            1,
            NULL,
            0
        );
        if (res != pdPASS) {
            VGP_LOG_E("Task VnGasTask creation failed");
            is_fetching = false;
        }
    }
}

bool VietnameseGasPrices::fetchNow() {
    GasPriceInfo info;
    if (executeFetch(api_url, info)) {
        if (gasMutex != NULL && xSemaphoreTake(gasMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            info.is_valid = true;
            info.last_update_time = millis();
            current_gas = info;
            xSemaphoreGive(gasMutex);
            return true;
        }
    }
    return false;
}

GasPriceInfo VietnameseGasPrices::getGas() {
    GasPriceInfo copy;
    if (gasMutex != NULL && xSemaphoreTake(gasMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = current_gas;
        xSemaphoreGive(gasMutex);
    }
    return copy;
}

GasCityPrice VietnameseGasPrices::getPriceForCity(const char *cityName) {
    GasCityPrice res;
    if (!cityName || strlen(cityName) == 0) return res;

    GasPriceInfo info = getGas();
    if (!info.is_valid || info.city_count == 0) return res;

    // 1. Tìm khớp trực tiếp
    for (uint8_t i = 0; i < info.city_count; i++) {
        if (strstr(info.cities[i].city, cityName) != NULL || strstr(cityName, info.cities[i].city) != NULL) {
            return info.cities[i];
        }
    }

    // 2. Fallback theo miền
    if (strstr(cityName, "Phòng") != NULL || strstr(cityName, "Phong") != NULL ||
        strstr(cityName, "Thủy Nguyên") != NULL || strstr(cityName, "Thuy Nguyen") != NULL ||
        strstr(cityName, "An Sơn") != NULL || strstr(cityName, "An Son") != NULL) {
        // Ưu tiên Hải Phòng
        for (uint8_t i = 0; i < info.city_count; i++) {
            if (strstr(info.cities[i].city, "Phòng") != NULL || strstr(info.cities[i].city, "Phong") != NULL) {
                return info.cities[i];
            }
        }
    }

    // Fallback mặc định về thành phố đầu tiên (thường là Hà Nội)
    return info.cities[0];
}

GasCityPrice VietnameseGasPrices::getCurrentCityPrice() {
    char target[32];
    if (gasMutex != NULL && xSemaphoreTake(gasMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        strncpy(target, target_city, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
        xSemaphoreGive(gasMutex);
    } else {
        strncpy(target, target_city, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    }
    return getPriceForCity(target);
}

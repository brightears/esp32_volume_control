#include "wifi_manager.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_system.h>

WiFiManager::WiFiManager()
    : _isConnected(false)
    , _apActive(false)
    , _lastConnectionAttempt(0) {
    WiFi.setAutoReconnect(true);  // Enable auto-reconnect
    WiFi.persistent(false);
}

bool WiFiManager::begin() {
    Serial.println("Initializing WiFiManager...");
    
    if (!initNVS()) {
        Serial.println("Failed to initialize NVS");
        return false;
    }
    
    bool result = loadCredentials();
    if (result) {
        Serial.printf("Loaded credentials - SSID: %s\n", _ssid.c_str());
    } else {
        Serial.println("No stored credentials found");
    }
    
    // Initialize WiFi with increased buffer sizes
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 16;
    cfg.static_tx_buf_num = 16;
    cfg.dynamic_rx_buf_num = 32;
    cfg.dynamic_tx_buf_num = 32;
    esp_wifi_init(&cfg);
    
    logMemoryStatus();
    return result;
}

bool WiFiManager::initNVS() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err == ESP_OK;
}

void WiFiManager::configureWiFiPower() {
    // Disable power saving
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    // Set to maximum power
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    
    // Set protocols for AP and STA modes
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    
    // Set minimum RSSI threshold for better connection stability
    esp_wifi_set_rssi_threshold(-80);
    
    // Configure bandwidth
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
}

void WiFiManager::setupWiFiConnection() {
    Serial.println("Setting up WiFi connection...");
    
    // Only disconnect if we're not already connected
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(true, true);
        delay(1000);
    }
    
    // Only change mode if we're not in the correct mode
    wifi_mode_t currentMode;
    esp_wifi_get_mode(&currentMode);
    if (currentMode != WIFI_MODE_APSTA) {
        WiFi.mode(WIFI_AP_STA);
        delay(500);
    }
    
    // Set static DNS to improve connection reliability
    IPAddress dns1(8, 8, 8, 8);     // Google DNS
    IPAddress dns2(8, 8, 4, 4);     // Google DNS alternate
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);
    
    configureWiFiPower();
}

void WiFiManager::maintainWiFiConnection() {
    unsigned long currentMillis = millis();
    
    // Check connection state periodically
    if (currentMillis - _lastConnectionAttempt >= CONNECTION_CHECK_INTERVAL) {
        _lastConnectionAttempt = currentMillis;
        
        if (!_isConnected || WiFi.status() != WL_CONNECTED) {
            if (hasStoredCredentials()) {
                Serial.println("Connection lost, attempting to reconnect...");
                connect();
            }
        }
    }
}
bool WiFiManager::connect() {
    if (_ssid.length() == 0 || _password.length() == 0) {
        Serial.println("No WiFi credentials stored");
        return false;
    }

    int32_t rssi;
    if (!scanForNetwork(_ssid, rssi)) {
        Serial.println("Network not found in scan");
        return false;
    }

    if (getNetworkQuality(rssi) < 2) {
        Serial.println("Poor signal quality, connection may be unstable");
    }

    // If already connected to the correct network, just verify
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == _ssid) {
        if (verifyWiFiConnection()) {
            _isConnected = true;
            logWiFiStatus();
            return true;
        }
    }

    setupWiFiConnection();

    Serial.printf("Attempting to connect to WiFi network: %s\n", _ssid.c_str());
    WiFi.begin(_ssid.c_str(), _password.c_str());

    if (!waitForConnection(CONNECTION_TIMEOUT)) {
        handleConnectionFailure();
        return false;
    }

    if (!verifyWiFiConnection()) {
        Serial.println("Connection verification failed");
        return false;
    }

    _isConnected = true;
    logWiFiStatus();
    return true;
}

bool WiFiManager::waitForConnection(unsigned long timeout) {
    unsigned long startTime = millis();
    int dots = 0;

    Serial.print("Connecting");
    while (millis() - startTime < timeout) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            delay(500); // Allow connection to stabilize
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("Connected to %s\n", _ssid.c_str());
                return true;
            }
        }

        delay(100); // Shorter delay interval
        if (++dots > 30) {
            Serial.println();
            dots = 0;
        }
        Serial.print(".");
        yield(); // Allow system tasks to run
    }

    Serial.println("\nConnection timeout");
    return false;
}

bool WiFiManager::scanForNetwork(const String& ssid, int32_t& rssi) {
    Serial.println("Scanning for networks...");

    WiFi.scanDelete(); // Clear previous scan results
    wifi_scan_config_t config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };

    esp_wifi_scan_start(&config, true); // true for blocking scan
    int numNetworks = WiFi.scanComplete();

    if (numNetworks == 0) {
        Serial.println("No networks found");
        return false;
    }

    for (int i = 0; i < numNetworks; i++) {
        if (WiFi.SSID(i) == ssid) {
            rssi = WiFi.RSSI(i);
            Serial.printf("Found network %s with signal strength %d dBm\n",
                        ssid.c_str(), rssi);
            WiFi.scanDelete();
            return true;
        }
    }

    Serial.printf("Network %s not found in scan results\n", ssid.c_str());
    WiFi.scanDelete();
    return false;
}

void WiFiManager::handleConnectionFailure() {
    Serial.println("Connection attempt failed");
    _isConnected = false;

    if (!reconnectWithBackoff()) {
        Serial.println("Reconnection attempts failed");
        // Don't automatically switch to AP mode here
        // Let the main loop handle mode changes
    }
}

bool WiFiManager::reconnectWithBackoff() {
    unsigned long startTime = millis();
    unsigned long currentDelay = RECONNECT_DELAY;
    int attempts = 0;

    while (millis() - startTime < MAX_RECONNECT_TIME && attempts < MAX_CONNECTION_ATTEMPTS) {
        Serial.printf("Reconnection attempt %d...\n", attempts + 1);

        WiFi.disconnect(true);
        delay(1000);
        WiFi.begin(_ssid.c_str(), _password.c_str());

        if (waitForConnection(CONNECTION_TIMEOUT)) {
            Serial.println("Reconnection successful");
            return true;
        }

        if (currentDelay < MAX_BACKOFF_DELAY) {
            currentDelay *= 2;
            if (currentDelay > MAX_BACKOFF_DELAY) {
                currentDelay = MAX_BACKOFF_DELAY;
            }
        }

        delay(currentDelay);
        attempts++;
    }

    return false;
}
bool WiFiManager::verifyWiFiConnection() {
    if (!testConnection()) {
        Serial.println("Connection test failed");
        return false;
    }

    wifi_ap_record_t apInfo;
    esp_wifi_sta_get_ap_info(&apInfo);

    if (apInfo.rssi < MIN_RSSI_THRESHOLD) {
        Serial.printf("Warning: Poor signal strength (%d dBm)\n", apInfo.rssi);
    }

    return true;
}

bool WiFiManager::testConnection() {
    IPAddress ip = WiFi.localIP();
    if (ip[0] == 0) {
        Serial.println("No valid IP address assigned");
        return false;
    }
    return true;
}

void WiFiManager::logWiFiStatus() {
    Serial.println("\nWiFi Connection Status:");
    Serial.printf("Connected to: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal strength (RSSI): %d dBm\n", WiFi.RSSI());
    Serial.printf("Channel: %d\n", WiFi.channel());

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    Serial.printf("WiFi Mode: %d\n", mode);
}

void WiFiManager::setupAP() {
    Serial.println("Setting up Access Point...");
    
    // Only disconnect if we're not in AP or AP_STA mode
    wifi_mode_t currentMode;
    esp_wifi_get_mode(&currentMode);
    if (currentMode != WIFI_MODE_AP && currentMode != WIFI_MODE_APSTA) {
        WiFi.disconnect(true, true);
        delay(500);
    }

    if (!startAP()) {
        Serial.println("Failed to start AP on first attempt, retrying...");
        delay(1000);
        if (!startAP()) {
            Serial.println("Failed to start AP after retry");
            return;
        }
    }

    _apActive = true;
    configureWiFiPower();
}

bool WiFiManager::startAP() {
    WiFi.mode(WIFI_AP);
    delay(500);

    if (!configureAP()) {
        return false;
    }

    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, WIFI_CHANNEL, 0, MAX_CLIENTS);
    if (apStarted) {
        delay(500);
        Serial.println("Access Point created successfully");
        Serial.printf("AP SSID: %s\n", AP_SSID);
        Serial.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    }

    return apStarted;
}

bool WiFiManager::configureAP() {
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    bool configSuccess = WiFi.softAPConfig(apIP, gateway, subnet);
    Serial.printf("AP Config %s\n", configSuccess ? "Success" : "Failed");
    delay(200);

    return configSuccess;
}

void WiFiManager::maintainAPMode() {
    static unsigned long lastCheck = 0;
    const unsigned long checkInterval = 1000; // Check every second

    unsigned long currentMillis = millis();
    if (currentMillis - lastCheck >= checkInterval) {
        lastCheck = currentMillis;

        if (!WiFi.softAPIP()) {
            Serial.println("AP mode lost, restarting AP...");
            restartAP();
        }

        // Log connected clients
        wifi_sta_list_t stationList;
        esp_wifi_ap_get_sta_list(&stationList);
        if (stationList.num > 0) {
            Serial.printf("Connected clients: %d\n", stationList.num);
        }
    }
}

void WiFiManager::restartAP() {
    Serial.println("Restarting AP...");
    WiFi.softAPdisconnect(true);
    delay(1000);
    setupAP();
}

void WiFiManager::createAP() {
    wifi_mode_t currentMode;
    esp_wifi_get_mode(&currentMode);
    
    if (currentMode != WIFI_MODE_AP && currentMode != WIFI_MODE_APSTA) {
        WiFi.disconnect(true, true);
        delay(500);
    }
    setupAP();
}

// The remaining methods (storeCredentials, saveToNVS, loadFromNVS, etc.) 
// remain unchanged as they were working correctly in the original code

bool WiFiManager::isConnected() {
    updateConnectionStatus();
    return _isConnected;
}

void WiFiManager::updateConnectionStatus() {
    bool previousState = _isConnected;
    _isConnected = (WiFi.status() == WL_CONNECTED);
    
    // Log state changes
    if (previousState != _isConnected) {
        if (_isConnected) {
            Serial.println("WiFi connection established");
        } else {
            Serial.println("WiFi connection lost");
        }
    }
}

void WiFiManager::disconnect() {
    if (_isConnected) {
        WiFi.disconnect(true);
        _isConnected = false;
        delay(500);
    }
    
    // Only change mode if we're not already in AP mode
    wifi_mode_t currentMode;
    esp_wifi_get_mode(&currentMode);
    if (currentMode != WIFI_MODE_AP) {
        WiFi.mode(WIFI_AP);
        delay(500);
    }
    maintainAPMode();
}

// Keep all the credential management methods (storeCredentials, loadCredentials, etc.)
// exactly as they were in the original code since they were working correctly

void WiFiManager::logMemoryStatus() {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Largest free heap block: %d bytes\n", ESP.getMaxAllocHeap());
}

int WiFiManager::getNetworkQuality(int32_t rssi) {
    if (rssi >= -50) return 4;       // Excellent
    else if (rssi >= -60) return 3;  // Good
    else if (rssi >= -70) return 2;  // Fair
    else if (rssi >= MIN_RSSI_THRESHOLD) return 1; // Poor
    return 0;                        // Very Poor
}


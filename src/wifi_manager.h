#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Update.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <nvs.h>

class WiFiManager {
public:
    WiFiManager();

    // Basic WiFi Operations
    bool begin();
    bool connect();
    void disconnect();
    bool isConnected();
    void maintainWiFiConnection();  // New method

    // AP Mode Management
    void createAP();
    void setupAP();
    void maintainAPMode();
    void restartAP();

    // Credentials Management
    void storeCredentials(const char* ssid, const char* password);
    bool hasStoredCredentials();
    void clearCredentials();

    // API Credentials Management
    void storeAPICredentials(const char* apiUrl, const char* clientId,
                           const char* clientSecret, const char* soundZoneId);
    bool loadAPICredentials(String& apiUrl, String& clientId,
                          String& clientSecret, String& soundZoneId);
    bool hasStoredAPICredentials();

    // Getters for stored values
    String getStoredSSID();
    String getStoredAPIUrl();
    String getStoredClientId();
    String getStoredSoundZoneId();

    // Sensitivity Management
    void storeSensitivity(int sensitivity);
    int getSensitivity();

    // AP Configuration Constants
    static constexpr const char* AP_SSID = "ESP32_SETUP";
    static constexpr const char* AP_PASSWORD = "12345678";

    // Network scanning and connection
    bool scanForNetwork(const String& ssid, int32_t& rssi);
    bool waitForConnection(unsigned long timeout);
    bool testConnection();

private:
    // Private member variables
    Preferences preferences;
    String _ssid;
    String _password;
    bool _isConnected;
    bool _apActive;
    unsigned long _lastConnectionAttempt;  // New variable

    // WiFi configuration constants
    static constexpr uint8_t WIFI_CHANNEL = 6;
    static constexpr uint8_t MAX_CLIENTS = 4;
    static constexpr uint32_t CONNECTION_TIMEOUT = 30000; // 30 seconds
    static constexpr uint16_t RETRY_DELAY = 500;
    static constexpr uint8_t MAX_CONNECTION_ATTEMPTS = 3;
    static constexpr int MIN_RSSI_THRESHOLD = -80; // Minimum acceptable signal strength

    // Connection timing parameters
    static constexpr uint32_t RECONNECT_DELAY = 5000; // Initial retry delay (5 seconds)
    static constexpr uint32_t MAX_RECONNECT_TIME = 300000; // Maximum total retry time (5 minutes)
    static constexpr uint32_t MAX_BACKOFF_DELAY = 60000; // Maximum delay between retries (1 minute)
    static constexpr uint32_t CONNECTION_CHECK_INTERVAL = 30000; // Check connection every 30 seconds

    // NVS storage keys
    static constexpr const char* PREF_NAMESPACE = "wifi_creds";
    static constexpr const char* PREF_SSID_KEY = "ssid";
    static constexpr const char* PREF_PASS_KEY = "password";
    static constexpr const char* PREF_API_URL = "api_url";
    static constexpr const char* PREF_CLIENT_ID = "client_id";
    static constexpr const char* PREF_CLIENT_SECRET = "client_secret";
    static constexpr const char* PREF_SOUND_ZONE_ID = "sound_zone";
    static constexpr const char* PREF_SENSITIVITY = "sensitivity";

    // Private helper methods
    bool loadCredentials();
    bool saveToNVS(const char* key, const char* value);
    String loadFromNVS(const char* key);
    bool verifyPreferencesStorage(const char* key, const char* value);
    void configureWiFiPower();
    void setupWiFiConnection();
    bool initNVS();
    void logWiFiStatus();
    bool verifyWiFiConnection();
    bool reconnectWithBackoff();

    // Connection management
    void handleConnectionFailure();
    void updateConnectionStatus();

    // AP management
    bool startAP();
    bool configureAP();

    // Utility functions
    void logMemoryStatus();
    int getNetworkQuality(int32_t rssi);
};

#endif // WIFI_MANAGER_H

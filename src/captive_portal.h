#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "wifi_manager.h"
#include "api_client.h"

class CaptivePortal {
public:
    CaptivePortal(WiFiManager& wifiManager, APIClient& apiClient,
                 AsyncWebServer& webServer, DNSServer& dnsServer);
    
    bool begin();
    void handleClient();
    bool isConfigured();
    
    // Constants
    static constexpr int DNS_PORT = 53;
    static constexpr const char* AP_REDIRECT_URL = "http://192.168.4.1/";
    static constexpr size_t MAX_CONFIG_SIZE = 1024;
    static constexpr uint32_t RESTART_DELAY = 1000;

private:
    WiFiManager& _wifiManager;
    APIClient& _apiClient;
    AsyncWebServer& _webServer;
    DNSServer& _dnsServer;
    bool _isConfigured;
    bool _dnsServerStarted;
    bool _needsRestart;
    unsigned long _restartTime;

    // Request handlers
    void handleRoot(AsyncWebServerRequest *request);
    void handleSave(AsyncWebServerRequest *request);
    void handleGetSensitivity(AsyncWebServerRequest *request);
    void handleTestConnection(AsyncWebServerRequest *request);
    void handleGetStoredConfig(AsyncWebServerRequest *request);

    // Helper methods
    bool validateCredentials(const String& apiUrl, const String& clientId,
                           const String& clientSecret, const String& soundZoneId);
    bool setupServerRoutes();
    bool setupCaptivePortalRoutes();
    bool startDNSServer();
    void setupDNSServer();
    bool verifyConfiguration();
    void scheduleRestart(uint32_t delayMs = RESTART_DELAY);
    void performRestart();
    void addCORSHeaders(AsyncWebServerResponse* response);
    void addStandardHeaders(AsyncWebServerResponse* response);

    // JSON helpers
    bool parseAndValidateConfig(AsyncWebServerRequest *request,
                              DynamicJsonDocument& doc);
    void sendJsonResponse(AsyncWebServerRequest *request,
                         const JsonDocument& doc,
                         int code = 200);
    void sendErrorResponse(AsyncWebServerRequest *request,
                         const char* message,
                         int code = 400);

    // File system helpers
    bool checkFileSystem();
    bool verifyRequiredFiles();

    // Connection helpers
    bool testAPIConnection(const String& apiUrl, const String& clientId,
                         const String& clientSecret, const String& soundZoneId);

    // CORS Constants
    static constexpr const char* CORS_HEADERS =
        "Content-Type,Authorization,Access-Control-Allow-Headers,Origin,Accept,"
        "Access-Control-Allow-Origin,Access-Control-Allow-Methods";
    static constexpr const char* CORS_METHODS = "GET,HEAD,POST,OPTIONS";

    // Required files for web interface
    static constexpr const char* REQUIRED_FILES[] = {
        "/index.html",
        "/styles.css",
        "/script.js"
    };
    static constexpr size_t REQUIRED_FILES_COUNT = 3;
};

#endif // CAPTIVE_PORTAL_H

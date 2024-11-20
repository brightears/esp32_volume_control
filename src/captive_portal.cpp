#include "captive_portal.h"
#include <IPAddress.h>

CaptivePortal::CaptivePortal(WiFiManager& wifiManager, APIClient& apiClient,
                           AsyncWebServer& webServer, DNSServer& dnsServer)
    : _wifiManager(wifiManager)
    , _apiClient(apiClient)
    , _webServer(webServer)
    , _dnsServer(dnsServer)
    , _isConfigured(false)
    , _dnsServerStarted(false) {
}

bool CaptivePortal::begin() {
    // Reset web server to clear any existing handlers
    _webServer.reset();
    
    // Stop any existing DNS server
    _dnsServer.stop();
    _dnsServerStarted = false;
    delay(100);
    
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    
    // Ensure WiFi is in the correct mode with proper delays
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Configure AP with a stronger signal
    WiFi.enableAP(true);
    if (!WiFi.softAPConfig(apIP, apIP, netMsk)) {
        Serial.println("AP Config Failed");
        return false;
    }
    
    delay(100);
    
    if (!WiFi.softAP(_wifiManager.AP_SSID, _wifiManager.AP_PASSWORD, 1, 0, 4)) {
        Serial.println("AP Start Failed");
        return false;
    }
    
    delay(100);
    
    // Start DNS server
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    if (!_dnsServer.start(53, "*", apIP)) {
        Serial.println("DNS Server Start Failed");
        return false;
    }
    _dnsServerStarted = true;
    Serial.println("DNS Server started successfully");

    // Setup web server routes
    if (!setupCaptivePortalRoutes()) {
        Serial.println("Failed to setup captive portal routes");
        return false;
    }
    
    if (!setupServerRoutes()) {
        Serial.println("Failed to setup server routes");
        return false;
    }
    
    // Enable CORS globally
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    // Start web server
    _webServer.begin();
    
    Serial.print("AP IP address: ");
    Serial.println(apIP);
    Serial.println("Captive portal started");
    
    return true;
}

bool CaptivePortal::setupServerRoutes() {
    // Root handler
    _webServer.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("Serving root page");
        if (!SPIFFS.exists("/index.html")) {
            Serial.println("index.html not found!");
            request->send(500, "text/plain", "Setup files missing");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html");
        addStandardHeaders(response);
        request->send(response);
    });
    
    // Serve static files
    _webServer.on("/styles.css", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("Serving CSS");
        if (!SPIFFS.exists("/styles.css")) {
            request->send(404, "text/plain", "CSS file not found");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/styles.css", "text/css");
        addStandardHeaders(response);
        request->send(response);
    });
    
    _webServer.on("/script.js", HTTP_GET, [this](AsyncWebServerRequest *request) {
        Serial.println("Serving JavaScript");
        if (!SPIFFS.exists("/script.js")) {
            request->send(404, "text/plain", "JavaScript file not found");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/script.js", "application/javascript");
        addStandardHeaders(response);
        request->send(response);
    });
    
    // API endpoints
    _webServer.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSave(request);
    });
    
    _webServer.on("/get-sensitivity", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetSensitivity(request);
    });
    
    _webServer.on("/test-connection", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleTestConnection(request);
    });
    
    _webServer.on("/get-stored-config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetStoredConfig(request);
    });
    
    return true;
}
bool CaptivePortal::setupCaptivePortalRoutes() {
    // Simple response for connection testing
    _webServer.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "pong");
    });

    // Apple Captive Portal detection
    _webServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        String redirectHtml = "<html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'/></head><body>Redirecting...</body></html>";
        request->send(200, "text/html", redirectHtml);
    });

    // Android Captive Portal detection
    _webServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        String redirectHtml = "<html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'/></head><body>Redirecting...</body></html>";
        request->send(200, "text/html", redirectHtml);
    });
    
    _webServer.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        String redirectHtml = "<html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'/></head><body>Redirecting...</body></html>";
        request->send(200, "text/html", redirectHtml);
    });
    
    // Windows Captive Portal detection
    _webServer.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Microsoft NCSI");
    });
    
    _webServer.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Microsoft Connect Test");
    });
    
    // Handle all other requests
    _webServer.onNotFound([this](AsyncWebServerRequest *request) {
        Serial.printf("Handling request for: %s\n", request->url().c_str());
        
        if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse *response = request->beginResponse(204);
            addCORSHeaders(response);
            request->send(response);
            return;
        }
        
        if (SPIFFS.exists("/index.html")) {
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html");
            addStandardHeaders(response);
            request->send(response);
        } else {
            String redirectHtml = "<html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1/'/></head><body>Redirecting to setup page...</body></html>";
            AsyncWebServerResponse *response = request->beginResponse(200, "text/html", redirectHtml);
            addStandardHeaders(response);
            request->send(response);
        }
    });
    
    return true;
}

void CaptivePortal::handleClient() {
    if (_dnsServerStarted) {
        _dnsServer.processNextRequest();
        yield(); // Allow other tasks to process
    }
}

void CaptivePortal::handleGetStoredConfig(AsyncWebServerRequest *request) {
    Serial.println("Getting stored configuration");
    
    DynamicJsonDocument doc(1024);
    
    if (_wifiManager.hasStoredCredentials()) {
        doc["ssid"] = _wifiManager.getStoredSSID();
        
        // Don't send password for security
        
        if (_wifiManager.hasStoredAPICredentials()) {
            doc["api-url"] = _wifiManager.getStoredAPIUrl();
            doc["client-id"] = _wifiManager.getStoredClientId();
            doc["client-secret"] = ""; // Don't send client secret for security
            doc["sound-zone"] = _wifiManager.getStoredSoundZoneId();
        }
        
        // Get current WiFi status
        doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
        if (WiFi.status() == WL_CONNECTED) {
            doc["ip_address"] = WiFi.localIP().toString();
            doc["signal_strength"] = WiFi.RSSI();
        }
        
        // Get current sensitivity setting
        int sensitivity = _wifiManager.getSensitivity();
        doc["sensitivity"] = sensitivity;
        Serial.printf("Current sensitivity: %d\n", sensitivity);
        
        // Add API connection status if credentials exist
        if (_apiClient.hasValidCredentials()) {
            doc["api_connected"] = true;
        }
    }
    
    String response;
    serializeJson(doc, response);
    Serial.println("Sending config response: " + response);
    
    AsyncWebServerResponse *webResponse = request->beginResponse(200, "application/json", response);
    addCORSHeaders(webResponse);
    request->send(webResponse);
}
void CaptivePortal::handleSave(AsyncWebServerRequest *request) {
    Serial.println("Handling save request");
    
    if (request->method() != HTTP_POST) {
        request->send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    String ssid, password, apiUrl, clientId, clientSecret, soundZoneId;
    int sensitivity = 50; // Default value
    bool sensitivityFound = false;
    
    // Get all parameters
    for (size_t i = 0; i < request->params(); i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
            if (p->name() == "ssid") ssid = p->value();
            else if (p->name() == "password") password = p->value();
            else if (p->name() == "api-url") apiUrl = p->value();
            else if (p->name() == "client-id") clientId = p->value();
            else if (p->name() == "client-secret") clientSecret = p->value();
            else if (p->name() == "sound-zone") soundZoneId = p->value();
            else if (p->name() == "sensitivity") {
                sensitivity = p->value().toInt();
                sensitivityFound = true;
                Serial.printf("Received sensitivity value: %d\n", sensitivity);
            }
        }
    }
    
    // Validate required fields
    if (ssid.length() == 0 || password.length() == 0 ||
        apiUrl.length() == 0 || clientId.length() == 0 ||
        clientSecret.length() == 0 || soundZoneId.length() == 0) {
        request->send(400, "text/plain", "Missing required fields");
        return;
    }
    
    try {
        // Store WiFi credentials
        _wifiManager.storeCredentials(ssid.c_str(), password.c_str());
        
        // Store API credentials
        _wifiManager.storeAPICredentials(
            apiUrl.c_str(),
            clientId.c_str(),
            clientSecret.c_str(),
            soundZoneId.c_str()
        );
        
        // Store sensitivity with validation
        if (sensitivityFound) {
            if (sensitivity >= 0 && sensitivity <= 100) {
                Serial.printf("Storing sensitivity value: %d\n", sensitivity);
                _wifiManager.storeSensitivity(sensitivity);
                
                // Verify the stored value
                int storedSensitivity = _wifiManager.getSensitivity();
                Serial.printf("Verified stored sensitivity: %d\n", storedSensitivity);
            } else {
                Serial.println("Invalid sensitivity value received");
            }
        } else {
            Serial.println("No sensitivity value in request");
        }
        
        _isConfigured = true;
        
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain",
            "Configuration saved. ESP32 will restart.");
        addStandardHeaders(response);
        addCORSHeaders(response);
        request->send(response);
        
        // Give the response time to send
        delay(1000);
        ESP.restart();
        
    } catch (const std::exception& e) {
        Serial.printf("Error saving configuration: %s\n", e.what());
        request->send(500, "text/plain", "Internal server error");
    }
}

void CaptivePortal::handleGetSensitivity(AsyncWebServerRequest *request) {
    int sensitivity = _wifiManager.getSensitivity();
    Serial.printf("Returning current sensitivity value: %d\n", sensitivity);
    
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", String(sensitivity));
    addCORSHeaders(response);
    request->send(response);
}

void CaptivePortal::handleTestConnection(AsyncWebServerRequest *request) {
    if (request->method() != HTTP_POST) {
        request->send(405, "text/plain", "Method Not Allowed");
        return;
    }
    
    String apiUrl, clientId, clientSecret, soundZoneId;
    
    // Get API credentials from request
    for (size_t i = 0; i < request->params(); i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
            if (p->name() == "api-url") apiUrl = p->value();
            else if (p->name() == "client-id") clientId = p->value();
            else if (p->name() == "client-secret") clientSecret = p->value();
            else if (p->name() == "sound-zone") soundZoneId = p->value();
        }
    }
    
    // Validate credentials
    if (!validateCredentials(apiUrl, clientId, clientSecret, soundZoneId)) {
        AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "Invalid API credentials");
        addCORSHeaders(response);
        request->send(response);
        return;
    }
    
    // Initialize API client with test credentials
    if (_apiClient.begin(apiUrl.c_str(), clientId.c_str(), clientSecret.c_str(), soundZoneId.c_str())) {
        // Test API connection by trying to get current volume
        int volume = _apiClient.getCurrentVolume();
        if (volume != -1) {
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Connection successful");
            addCORSHeaders(response);
            request->send(response);
        } else {
            AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "Could not retrieve volume information");
            addCORSHeaders(response);
            request->send(response);
        }
    } else {
        AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "Failed to initialize API client");
        addCORSHeaders(response);
        request->send(response);
    }
}

bool CaptivePortal::validateCredentials(const String& apiUrl, const String& clientId,
                                      const String& clientSecret, const String& soundZoneId) {
    // Basic validation
    if (apiUrl.length() == 0 || clientId.length() == 0 ||
        clientSecret.length() == 0 || soundZoneId.length() == 0) {
        return false;
    }
    
    // Validate API URL format
    if (!apiUrl.startsWith("http://") && !apiUrl.startsWith("https://")) {
        return false;
    }
    
    return true;
}

void CaptivePortal::addCORSHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", CORS_METHODS);
    response->addHeader("Access-Control-Allow-Headers", CORS_HEADERS);
}

void CaptivePortal::addStandardHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
}

bool CaptivePortal::isConfigured() {
    return _isConfigured;
}


#include "api_client.h"

APIClient::APIClient() : _isInitialized(false) {
    if(esp_random() == 0) {
        Serial.println("Warning: Hardware RNG not initialized");
    }
}

APIClient::~APIClient() {}

bool APIClient::begin(const char* apiUrl, const char* clientId,
                     const char* clientSecret, const char* soundZoneId) {
    if (!apiUrl || !clientId || !clientSecret || !soundZoneId) {
        Serial.println("API Client: Invalid credentials provided");
        return false;
    }

    _apiUrl = String(apiUrl);
    _clientId = String(clientId);
    _clientSecret = String(clientSecret);
    _soundZoneId = String(soundZoneId);
    _isInitialized = true;
   
    // Initialize the secure client early
    _client.setInsecure(); // Skip certificate validation
    _client.setTimeout(30); // 30 seconds timeout
   
    Serial.println("API Client: Initialized successfully");
    return true;
}

String APIClient::getAuthHeader() {
    if (!_isInitialized) {
        Serial.println("API Client: Not initialized");
        return "";
    }
   
    String auth = _clientId + ":" + _clientSecret;
    return "Basic " + base64::encode(auth);
}

void APIClient::printHTTPResponse(int httpCode, String payload) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpCode);
    Serial.println("Response:");
    Serial.println(payload);
}

String APIClient::makeRequest(const String& query) {
    if (!_isInitialized) {
        Serial.println("API Client: Not initialized");
        return "";
    }
    HTTPClient https;
   
    // Extract host from URL and start connection
    String host = _apiUrl;
    host.replace("https://", "");
    if (host.indexOf('/') >= 0) {
        host = host.substring(0, host.indexOf('/'));
    }
    Serial.println("Connecting to: " + host);
    if (https.begin(_client, _apiUrl)) {
        https.addHeader("Content-Type", "application/json");
        https.addHeader("Authorization", getAuthHeader());
        https.addHeader("Connection", "close"); // Important for memory management
       
        Serial.println("Making request...");
        Serial.println("Query: " + query);
        int httpCode = https.POST(query);
        String payload = https.getString();
       
        printHTTPResponse(httpCode, payload);
        https.end();
        _client.stop();
        if (httpCode == 200) {
            return payload;
        }
    } else {
        Serial.println("HTTPS begin failed");
    }
   
    return "";
}

int APIClient::parseVolumeFromResponse(const String& response, const String& path) {
    if (response.length() == 0) {
        return -1;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.println("Error parsing JSON response");
        Serial.println(error.c_str());
        return -1;
    }

    JsonVariant volumeVar;
    if (path == "get") {
        volumeVar = doc["data"]["soundZone"]["playback"]["volume"];
    } else {
        volumeVar = doc["data"]["setVolume"]["volume"];
    }

    if (!volumeVar.isNull()) {
        int volume = volumeVar.as<int>();
        Serial.printf("Current volume: %d\n", volume);
        return volume;
    }

    Serial.println("Volume not found in response");
    return -1;
}

int APIClient::getCurrentVolume() {
    if (!_isInitialized) {
        Serial.println("API Client: Not initialized");
        return -1;
    }
    String query = "{\"query\":\"query { soundZone(id: \\\"" + 
                  _soundZoneId +
                  "\\\") { playback { volume } } }\"}";
    String response = makeRequest(query);
    return parseVolumeFromResponse(response, "get");
}

bool APIClient::setPlayerVolume(int volume) {
    if (!_isInitialized) {
        Serial.println("API Client: Not initialized");
        return false;
    }
    if (volume < 0 || volume > 16) {
        Serial.println("API Client: Invalid volume value");
        return false;
    }
    String mutation = "{\"query\":\"mutation { setVolume(input: { soundZone: \\\"" +
                     _soundZoneId +
                     "\\\", volume: " + String(volume) +
                     " }) { volume } }\"}";
    String response = makeRequest(mutation);
    int newVolume = parseVolumeFromResponse(response, "set");
    return newVolume == volume;
}

bool APIClient::hasValidCredentials() const {
    return _isInitialized &&
           _apiUrl.length() > 0 &&
           _clientId.length() > 0 &&
           _clientSecret.length() > 0 &&
           _soundZoneId.length() > 0;
}

void APIClient::clearCredentials() {
    _apiUrl = "";
    _clientId = "";
    _clientSecret = "";
    _soundZoneId = "";
    _isInitialized = false;
    _client.stop();
}
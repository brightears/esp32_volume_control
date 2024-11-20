#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <base64.h>

class APIClient {
public:
    APIClient();
    ~APIClient();
    
    bool begin(const char* apiUrl, const char* clientId,
               const char* clientSecret, const char* soundZoneId);
   
    int getCurrentVolume();
    bool setPlayerVolume(int volume);
    bool hasValidCredentials() const;
    void clearCredentials();

private:
    String _apiUrl;
    String _clientId;
    String _clientSecret;
    String _soundZoneId;
    bool _isInitialized;
    WiFiClientSecure _client;
    uint8_t _entropy[32];
   
    // Helper methods
    String getAuthHeader();
    String makeRequest(const String& query);
    void printHTTPResponse(int httpCode, String payload);
    int parseVolumeFromResponse(const String& response, const String& path);
};

#endif // API_CLIENT_H
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <esp_task_wdt.h>
#include "wifi_manager.h"
#include "sound_sensor.h"
#include "api_client.h"
#include "captive_portal.h"

// Pin Definitions
#define RESET_PIN 0  // GPIO 0 for the hardware reset button
#define SOUND_PIN 36 // GPIO 36 (VP) for sound sensor input

// Constants
const int WDT_TIMEOUT = 60; // Extended watchdog timeout in seconds
const int VOLUME_CHANGE_AMOUNT = 1;
constexpr unsigned long SOUND_CHECK_INTERVAL = 5000;    // 5 seconds
constexpr unsigned long WIFI_CHECK_INTERVAL = 5000;     // 5 seconds
constexpr unsigned long AP_CHECK_INTERVAL = 1000;       // 1 second
constexpr unsigned long MEMORY_CHECK_INTERVAL = 30000;  // 30 seconds
constexpr int MAX_STARTUP_ATTEMPTS = 3;
constexpr int STARTUP_RETRY_DELAY = 1000; // 1 second

// System states
enum class SystemState {
    INITIALIZING,
    AP_MODE,
    CONNECTING,
    CONNECTED,
    ERROR
};

// Global objects
WiFiManager wifiManager;
AsyncWebServer webServer(80);
DNSServer dnsServer;
SoundSensor soundSensor(SOUND_PIN);
APIClient apiClient;
CaptivePortal captivePortal(wifiManager, apiClient, webServer, dnsServer);

// Global variables
int soundSensitivity = 50;  // Will be loaded from stored value
bool isSTAConnected = false;
bool apiInitialized = false;
SystemState currentState = SystemState::INITIALIZING;
unsigned long lastSoundCheck = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastAPCheck = 0;
unsigned long lastMemoryCheck = 0;
unsigned long lastVolumeUpdate = 0;
int lastVolume = -1;

// Basic setup functions
void setupHardware() {
    pinMode(RESET_PIN, INPUT_PULLUP);
    analogReadResolution(12);  // Set ADC resolution to 12 bits
}

void setupWatchdog() {
    disableLoopWDT();
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);
}

bool setupFileSystem() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return false;
    }

    // Verify required files exist
    const char* requiredFiles[] = {"/index.html", "/styles.css", "/script.js"};
    for (const char* file : requiredFiles) {
        if (!SPIFFS.exists(file)) {
            Serial.printf("Required file missing: %s\n", file);
            return false;
        }
    }

    // List files for debugging
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    Serial.println("Files in SPIFFS:");
    while (file) {
        Serial.printf("- %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
    return true;
}

bool initializeModules() {
    bool success = true;
    if (!wifiManager.begin()) {
        Serial.println("Failed to initialize WiFiManager");
        success = false;
    }
    return success;
}

bool setupSystem() {
    bool success = true;
    
    setupHardware();
    setupWatchdog();
    
    if (!setupFileSystem()) {
        Serial.println("File system setup failed");
        success = false;
    }
    
    if (!initializeModules()) {
        Serial.println("Module initialization failed");
        success = false;
    }
    
    return success;
}
// Network and API functions
bool initializeAPIClient() {
    if (apiInitialized) {
        return true;  // Already initialized
    }

    String apiUrl, clientId, clientSecret, soundZoneId;
    Serial.println("Initializing API client...");
    
    if (!wifiManager.loadAPICredentials(apiUrl, clientId, clientSecret, soundZoneId)) {
        Serial.println("No stored API credentials found");
        return false;
    }
    
    Serial.println("API credentials loaded successfully");
    if (!apiClient.begin(apiUrl.c_str(), clientId.c_str(),
                        clientSecret.c_str(), soundZoneId.c_str())) {
        Serial.println("Failed to initialize API client with stored credentials");
        return false;
    }
    
    Serial.println("API client initialized successfully");
    
    // Test connection
    int volume = apiClient.getCurrentVolume();
    if (volume != -1) {
        Serial.printf("API connection test successful. Current volume: %d\n", volume);
        lastVolume = volume;  // Store the initial volume
        lastVolumeUpdate = millis();  // Reset the volume update timer
        apiInitialized = true;
        return true;
    }
    
    Serial.println("API connection test failed");
    return false;
}

void startAPMode() {
    // Only change mode if we're not already in AP mode
    wifi_mode_t currentMode;
    esp_wifi_get_mode(&currentMode);
    if (currentMode != WIFI_MODE_AP) {
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        delay(100);
    }
    
    wifiManager.createAP();
    captivePortal.begin();
    currentState = SystemState::AP_MODE;
    Serial.println("Device is now in AP mode");
}

void handleWiFiConnection() {
    static unsigned long lastReconnectAttempt = 0;
    static int failedAttempts = 0;
    constexpr unsigned long reconnectInterval = 60000; // 60 seconds
    constexpr int maxFailedAttempts = 3;

    // First, maintain AP mode if needed
    if (!WiFi.softAPIP()) {
        Serial.println("AP mode not active, restarting AP...");
        wifiManager.restartAP();
        yield();
    }

    if (wifiManager.hasStoredCredentials()) {
        if (!isSTAConnected && !WiFi.isConnected()) {
            unsigned long currentMillis = millis();
            if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
                if (failedAttempts < maxFailedAttempts) {
                    lastReconnectAttempt = currentMillis;
                    Serial.println("Attempting to reconnect to WiFi network...");
                    
                    wifi_mode_t currentMode;
                    esp_wifi_get_mode(&currentMode);
                    if (currentMode != WIFI_MODE_APSTA) {
                        WiFi.mode(WIFI_AP_STA);
                        delay(100);
                    }
                    
                    if (wifiManager.connect()) {
                        Serial.println("Successfully reconnected to WiFi");
                        isSTAConnected = true;
                        failedAttempts = 0;
                        currentState = SystemState::CONNECTED;
                        
                        if (!apiInitialized) {
                            if (initializeAPIClient()) {
                                Serial.println("API client initialized after reconnection");
                            } else {
                                Serial.println("Failed to initialize API client after reconnection");
                            }
                        }
                    } else {
                        failedAttempts++;
                        isSTAConnected = false;
                        apiInitialized = false;
                        Serial.printf("Failed to connect (attempt %d of %d)\n",
                                    failedAttempts, maxFailedAttempts);
                    }
                } else if (currentMillis - lastReconnectAttempt >= (reconnectInterval * 4)) {
                    failedAttempts = 0; // Reset counter after extended wait
                }
            }
        } else if (WiFi.isConnected() && !isSTAConnected) {
            isSTAConnected = true;
            failedAttempts = 0;
            currentState = SystemState::CONNECTED;
            
            if (!apiInitialized) {
                initializeAPIClient();
            }
        }
    }

    // Update state if connection is lost
    if (!WiFi.isConnected() && isSTAConnected) {
        Serial.println("Lost WiFi connection");
        isSTAConnected = false;
        apiInitialized = false;
    }
}

void handleVolumeControl() {
    static unsigned long lastVolumeCheck = 0;
    constexpr unsigned long VOLUME_CHECK_INTERVAL = 30000; // 30 seconds
    
    if (!apiInitialized) {
        return;
    }
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastVolumeCheck >= VOLUME_CHECK_INTERVAL) {
        lastVolumeCheck = currentMillis;
        
        int currentVolume = apiClient.getCurrentVolume();
        if (currentVolume != -1 && currentVolume != lastVolume) {
            Serial.printf("Volume changed externally: %d -> %d\n", lastVolume, currentVolume);
            lastVolume = currentVolume;
        }
    }
}
// Core functionality functions
void processSound() {
    if (!isSTAConnected || !WiFi.isConnected() || !apiInitialized) {
        return;
    }

    float soundLevel = soundSensor.getSoundLevel();
    Serial.printf("Sound level: %.2f\n", soundLevel);

    // Verify we have a valid last volume reading
    if (lastVolume == -1) {
        lastVolume = apiClient.getCurrentVolume();
        if (lastVolume == -1) {
            Serial.println("Failed to get current volume. Skipping volume adjustment.");
            return;
        }
    }

    // Calculate target volume based on sound level and sensitivity
    float sensitivityFactor = soundSensitivity / 50.0f; // Convert 0-100 to 0-2 range
    int targetVolume = constrain(
        map(round(soundLevel * 100 * sensitivityFactor), 0, 100, 0, 16),
        0, 16
    );

    // Only change volume if difference is significant
    if (abs(targetVolume - lastVolume) >= VOLUME_CHANGE_AMOUNT) {
        int newVolume = (targetVolume > lastVolume)
            ? min(16, lastVolume + VOLUME_CHANGE_AMOUNT)
            : max(0, lastVolume - VOLUME_CHANGE_AMOUNT);
            
        if (apiClient.setPlayerVolume(newVolume)) {
            Serial.printf("Volume changed: %d -> %d\n", lastVolume, newVolume);
            lastVolume = newVolume;
            lastVolumeUpdate = millis();
        }
    }
}

void handleReset() {
    if (digitalRead(RESET_PIN) == LOW) {
        delay(50); // debounce
        if (digitalRead(RESET_PIN) == LOW) {
            Serial.println("Reset button pressed. Clearing credentials.");
            wifiManager.clearCredentials();
            apiInitialized = false;
            ESP.restart();
        }
    }
}

// System monitoring functions
void logSystemStatus() {
    Serial.println("\nSystem Status:");
    Serial.printf("Current State: %d\n", static_cast<int>(currentState));
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Largest free heap block: %d bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("WiFi Status: %s\n", isSTAConnected ? "Connected" : "Disconnected");
    Serial.printf("API Status: %s\n", apiInitialized ? "Initialized" : "Not Initialized");
    
    if (isSTAConnected) {
        Serial.printf("Signal strength (RSSI): %d dBm\n", WiFi.RSSI());
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    }
    if (apiInitialized && lastVolume != -1) {
        Serial.printf("Current Volume: %d\n", lastVolume);
    }
}

bool checkSystemHealth() {
    const size_t MIN_FREE_HEAP = 20000;  // 20KB minimum free heap
    const size_t MIN_BLOCK_SIZE = 10000; // 10KB minimum block size

    if (ESP.getFreeHeap() < MIN_FREE_HEAP) {
        Serial.println("Low memory condition detected");
        return false;
    }

    if (ESP.getMaxAllocHeap() < MIN_BLOCK_SIZE) {
        Serial.println("Memory fragmentation detected");
        return false;
    }

    return true;
}

void monitorSystem() {
    logSystemStatus();
    if (!checkSystemHealth()) {
        Serial.println("System health check failed, restarting...");
        ESP.restart();
    }
}

// Main Arduino functions
void setup() {
    // Initialize Serial communication
    Serial.begin(115200);
    Serial.println("\nESP32 starting up...");

    // Initialize system components
    if (!setupSystem()) {
        Serial.println("System setup failed, entering error state");
        currentState = SystemState::ERROR;
        return;
    }

    // Load sensitivity setting
    soundSensitivity = wifiManager.getSensitivity();
    Serial.printf("Loaded saved sensitivity: %d\n", soundSensitivity);

    // Try to connect to saved network if credentials exist
    if (wifiManager.hasStoredCredentials()) {
        currentState = SystemState::CONNECTING;
        Serial.println("Found saved credentials, attempting connection...");

        // Start in dual mode
        wifi_mode_t currentMode;
        esp_wifi_get_mode(&currentMode);
        if (currentMode != WIFI_MODE_APSTA) {
            WiFi.mode(WIFI_AP_STA);
            delay(100);
        }

        bool connected = false;
        for (int attempt = 0; attempt < MAX_STARTUP_ATTEMPTS; attempt++) {
            if (wifiManager.connect()) {
                connected = true;
                isSTAConnected = true;
                Serial.println("Connected to saved WiFi network");
                if (initializeAPIClient()) {
                    currentState = SystemState::CONNECTED;
                } else {
                    Serial.println("API client initialization failed");
                }
                break;
            }
            delay(STARTUP_RETRY_DELAY);
            esp_task_wdt_reset();
        }

        if (!connected) {
            Serial.println("Failed to connect to saved network");
            currentState = SystemState::AP_MODE;
        }
    } else {
        Serial.println("No saved credentials found, starting in AP mode");
        currentState = SystemState::AP_MODE;
    }

    // Always ensure AP is running
    wifiManager.createAP();
    if (!captivePortal.begin()) {
        Serial.println("Failed to start captive portal");
        currentState = SystemState::ERROR;
        return;
    }

    logSystemStatus();
}

void loop() {
    // Reset watchdog timer at start of loop
    esp_task_wdt_reset();

    // Process DNS and captive portal requests
    captivePortal.handleClient();
    yield();

    unsigned long currentMillis = millis();

    // Check AP mode
    if (currentMillis - lastAPCheck >= AP_CHECK_INTERVAL) {
        lastAPCheck = currentMillis;
        wifiManager.maintainAPMode();
        esp_task_wdt_reset();
    }

    // Handle WiFi connection
    if (currentMillis - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
        lastWiFiCheck = currentMillis;
        handleWiFiConnection();
        esp_task_wdt_reset();
    }

    // Process sound measurement if connected and API initialized
    if (currentState == SystemState::CONNECTED && 
        currentMillis - lastSoundCheck >= SOUND_CHECK_INTERVAL) {
        lastSoundCheck = currentMillis;
        processSound();
        handleVolumeControl();  // Check for external volume changes
        esp_task_wdt_reset();
    }

    // Check for reset button press
    handleReset();

    // Monitor system health
    if (currentMillis - lastMemoryCheck >= MEMORY_CHECK_INTERVAL) {
        lastMemoryCheck = currentMillis;
        monitorSystem();
        esp_task_wdt_reset();
    }

    yield();
}


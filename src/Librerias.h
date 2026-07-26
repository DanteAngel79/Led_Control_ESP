/*
 * Librerias.h - Archivo de cabecera con librerías y variables globales
 *
 * Este archivo contiene todas las includes de librerías, declaraciones
 * y definiciones de variables y objetos globales del proyecto.
 *
 * IMPORTANTE: Este archivo debe incluirse en TiraLed.ino únicamente
 */

#ifndef LIBRERIAS_H
#define LIBRERIAS_H

// ===== INCLUDES DE LIBRERÍAS =====
// CRÍTICO: ArduinoJson debe ir ANTES que ESPAsyncWebServer.h.
// Ese header hace `#if __has_include("ArduinoJson.h")`; si aún no está en el
// include path, la consulta falla, el compilador cachea el fallo y cualquier
// #include <ArduinoJson.h> posterior se ignora en silencio (sin error de
// "No such file"), por lo que el IDE nunca añade la librería al build.
#include <ArduinoJson.h>

#include "Config.h"            // Incluye WiFi.h o ESP8266WiFi.h según plataforma
#include <Adafruit_NeoPixel.h>

// CRÍTICO: Definir ESPALEXA_ASYNC antes de incluir Espalexa
#define ESPALEXA_ASYNC
#include <Espalexa.h>          // Biblioteca para Alexa con soporte de color

// Servidor web asíncrono (requerido para Espalexa async)
#if defined(ESP8266)
  #include <ESPAsyncTCP.h>
#elif defined(ESP32)
  #include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#include <ArduinoOTA.h>
#include <Arduino.h>

// ===== CONFIGURACIÓN =====
#include "BoardPins.h"         // Pines válidos según la placa compilada

// ===== DECLARACIÓN DE OBJETOS GLOBALES (extern) =====
extern Adafruit_NeoPixel strip;
extern Espalexa espalexa;
extern AsyncWebServer server;

// ===== DECLARACIÓN DE VARIABLES GLOBALES DE ESTADO LED (extern) =====
extern uint8_t currentBrightness;
extern uint8_t currentRed;
extern uint8_t currentGreen;
extern uint8_t currentBlue;
extern String currentEffect;
extern bool ledsOn;

// ===== SISTEMA DE ACCIONES DIFERIDAS =====
// Los handlers de AsyncWebServer SOLO marcan flags; el loop() las ejecuta.
// Esto evita bloquear el servidor con strip.show(), LittleFS, etc.
enum PendingAction {
    ACTION_NONE,
    ACTION_TURN_ON,
    ACTION_TURN_OFF,
    ACTION_SET_COLOR,
    ACTION_SET_BRIGHTNESS,
    ACTION_SET_EFFECT
};

extern PendingAction pendingAction;
extern uint8_t pendingR;
extern uint8_t pendingG;
extern uint8_t pendingB;
extern uint8_t pendingBrightness;
extern String pendingEffect;

// Reinicio y guardado diferidos: los handlers async NO pueden llamar a
// ESP.restart() ni escribir en LittleFS; marcan estos flags y el loop() actúa.
extern bool pendingRestart;
extern unsigned long pendingRestartAt;
extern bool pendingConfigSave;
void processPendingRestart();

// Guardado diferido de settings (debounce) para no saturar LittleFS
extern bool settingsNeedSave;
extern unsigned long settingsLastChange;
extern const unsigned long SETTINGS_SAVE_DEBOUNCE_MS;
void scheduleSettingsSave();
void processPendingActions();

// ===== DECLARACIÓN DE VARIABLES PARA EFECTOS (extern) =====
extern unsigned long lastEffectUpdate;
extern uint16_t effectStep;
extern uint8_t effectBrightness;
extern int8_t effectDirection;
extern uint16_t effectInterval;

// ===== SISTEMA DE TIMING BASADO EN millis() =====
// Usa millis() para control de tiempo + delay(0)/ESP.wdtFeed() para watchdog
// delay(0) cede CPU SIN bloquear (equivale a yield pero más explícito)

// Variable global para tracking de tiempo de procesamiento
static unsigned long _lastWatchdogFeed = 0;

// Función inline para verificar si debe continuar procesando
// Retorna true si aún hay tiempo disponible
inline bool shouldContinueProcessing() {
    // Siempre ceder un poco de tiempo al sistema (muy barato)
    delay(0);

    unsigned long currentTime = millis();

    // Inicializar en primera llamada
    if (_lastWatchdogFeed == 0) {
        _lastWatchdogFeed = currentTime;
        return true;
    }

    // Alimentar watchdog explícitamente cada ~10ms
    if ((currentTime - _lastWatchdogFeed) >= 10) {
#if defined(ESP8266)
        ESP.wdtFeed();
#elif defined(ESP32)
        yield();
#endif
        _lastWatchdogFeed = currentTime;
    }

    return true;
}

// Función de compatibilidad - reemplaza smartYield()
// Alimenta watchdog cada 10ms usando delay(0) + millis() + ESP.wdtFeed() (ESP8266) o yield() (ESP32)
inline void smartYield() {
    shouldContinueProcessing();
}

// ===== DECLARACIÓN DE VARIABLES OTA (extern) =====
extern bool evitarDeepSleep;

// ===== ALIASES ELIMINADOS =====
// Se eliminaron aliases para mayor claridad y mantenibilidad del código

// ===== DECLARACIONES DE FUNCIONES =====

// Funciones de TiraLed.ino
void setColor(uint8_t r, uint8_t g, uint8_t b);

// Funciones de Effects.cpp
uint32_t Wheel(byte WheelPos);
void effectRainbowAsync();
void effectFadeAsync();
void effectStrobeAsync();
void effectTheaterAsync();
void effectFireAsync();
void effectSparkleAsync();
void effectWaveAsync();
void effectRunningAsync();
void effectBouncingBallsAsync();
void effectKittAsync();
void effectColorWipeAsync();
void effectTwinkleAsync();
void effectRunningLightsAsync();
void effectMeteorAsync();
void effectPoliceAsync();
void effectRainbowLoopAsync();
void effectPulseAsync();
void effectTheaterRainbowAsync();
void effectSnowSparkleAsync();
void effectScannerAsync();
void updateEffects();
void resetEffectVariables();
void setEffect(String effect);
bool isValidEffect(const String& effect);

// Funciones de OTA.cpp
void setupOTA();

// Funciones de WebServer.cpp
void handleRoot(AsyncWebServerRequest *request);
void handleApiOn(AsyncWebServerRequest *request);
void handleApiOff(AsyncWebServerRequest *request);
void handleApiColor(AsyncWebServerRequest *request);
void handleApiBrightness(AsyncWebServerRequest *request);
void handleApiEffect(AsyncWebServerRequest *request);
void handleApiStatus(AsyncWebServerRequest *request);
void handleApiInfo(AsyncWebServerRequest *request);
void handleApiSettings(AsyncWebServerRequest *request);
void handleApiPins(AsyncWebServerRequest *request);
void handleApiSettingsSave(AsyncWebServerRequest *request);
void handleApiSettingsReset(AsyncWebServerRequest *request);
void handleApiReset(AsyncWebServerRequest *request);
void setupWebServer();
void startWebServer();
void setupWebServerNotFound();

// Funciones de Alexa_Setup.cpp
void setupAlexa();
void alexaCallback(EspalexaDevice* dev);
void syncStateWithAlexa();

// Funciones de WiFiManager.cpp
#include "WiFiManager.h"

// Funciones de Settings.cpp
bool initFileSystem();
bool loadSettings();
bool saveSettings();
void applySettings();
void updateCurrentSettings();
uint16_t getConfiguredNumLeds();
void setConfiguredNumLeds(uint16_t numLeds);
String getSettingsJSON();
bool resetToDefaults();
const char* getAlexaDeviceName();
void setAlexaDeviceName(const char* name);
bool getUseStaticIP();
IPAddress getConfiguredStaticIP();
IPAddress getConfiguredGateway();
IPAddress getConfiguredSubnet();
void setNetworkConfig(bool useStatic, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4,
                      uint8_t gw1, uint8_t gw2, uint8_t gw3, uint8_t gw4);

// Credenciales WiFi y Alexa runtime
const char* getWiFiSsid();
const char* getWiFiPassword();
bool hasWiFiCredentials();
void setWiFiCredentials(const char* ssid, const char* password);
bool getAlexaEnabled();
void setAlexaEnabled(bool enabled);

// Pin de datos de la tira (runtime, validado contra BoardPins.h)
uint8_t getConfiguredLedPin();
bool setConfiguredLedPin(uint8_t gpio);

// Función inline para obtener número de LEDs activos (usa configurado, o NUM_LEDS por defecto)
inline uint16_t getActiveLedCount() {
    uint16_t configured = getConfiguredNumLeds();
    return (configured > 0 && configured <= NUM_LEDS) ? configured : NUM_LEDS;
}

#endif // LIBRERIAS_H

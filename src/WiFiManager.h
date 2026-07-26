/*
 * WiFiManager.h - Gestión de WiFi con provisión por AP y portal cautivo
 *
 * Si no hay credenciales WiFi guardadas, o si la conexión STA falla
 * repetidamente, el dispositivo crea un AP llamado TiraLed-XXXX con un
 * portal cautivo para configurar la red.
 */

#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <DNSServer.h>

// ===== ESTADOS WiFi =====
enum WiFiModeState {
    WIFI_STATE_CONNECTING,    // Intentando conectar a STA
    WIFI_STATE_CONNECTED,     // Conectado a STA
    WIFI_STATE_AP             // En modo AP (portal de configuración)
};

// ===== CONFIGURACIÓN =====
// constexpr, no `static const` en el header: `static` daba una copia por cada
// .cpp que incluyera este archivo.
constexpr const char* AP_SSID_PREFIX     = "TiraLed-";
constexpr const char* AP_PASSWORD        = "tecnomatico";  // mínimo 8 caracteres (WPA2)
constexpr int         MAX_CONN_ATTEMPTS  = 20;      // 20 x 500ms = 10s de timeout
constexpr int         MAX_FAIL_BEFORE_AP = 3;       // Fallos seguidos antes de abrir el AP
constexpr unsigned long RECONNECT_INTERVAL_MS = 30000UL;   // Reintento en STA
constexpr unsigned long AP_RETRY_STA_MS       = 300000UL;  // Reintento de STA estando en AP (5 min)

// La contraseña del AP es fija para facilitar la provisión. Si se prefiere una
// clave única por dispositivo, puede derivarse de WiFi.macAddress() en getApPassword().
String getApPassword();

// ===== FUNCIONES PÚBLICAS =====
bool setupWiFi();
void loopWiFi();
bool isInApMode();
bool isWiFiConnected();
void startAccessPoint();
void stopAccessPoint();
String getApSsid();

#endif // WIFIMANAGER_H

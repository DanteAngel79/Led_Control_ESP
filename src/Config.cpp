/*
 * Config.cpp - Archivo de configuración centralizada
 *
 * Este archivo contiene los parámetros de compilación del proyecto.
 * Las credenciales WiFi y el estado de Alexa ahora se configuran en runtime
 * a través del portal cautivo (modo AP) y se guardan en LittleFS.
 */

#include "Config.h"

// ===== CONFIGURACIÓN DE RED =====
// Por defecto DHCP: el router asigna la IP y funciona en cualquier red.
// Estos valores solo se usan si activas la IP fija desde la interfaz web.
const bool USE_STATIC_IP = false;             // true = IP fija, false = DHCP
const IPAddress STATIC_IP(192, 168, 1, 50);   // Ejemplo; ajústalo a tu red
const IPAddress GATEWAY_IP(192, 168, 1, 1);   // IP del router/gateway
const IPAddress SUBNET_MASK(255, 255, 255, 0);
const IPAddress PRIMARY_DNS(8, 8, 8, 8);     // DNS primario (Google)
const IPAddress SECONDARY_DNS(1, 0, 0, 1);   // DNS secundario (Cloudflare)

// ===== CONFIGURACIÓN LED STRIP =====
// El pin de datos NO está aquí: es un ajuste de runtime que el usuario elige
// desde la web. La lista de pines válidos y el valor por defecto de cada
// placa viven en BoardPins.h, que es la única fuente de verdad.
const uint16_t NUM_LEDS = 150;               // MÁXIMO de LEDs soportados (reserva de memoria)
                                              // IMPORTANTE: 150 LEDs es el máximo recomendado para ESP8266
                                              // con AsyncWebServer + Espalexa sin crashear
                                              // La cantidad real se configura desde la interfaz web
const uint8_t BRIGHTNESS = 45;               // Brillo inicial (0-255)

// Tipo de tira LED - Descomentar el que corresponda
const uint16_t LED_TYPE = NEO_GRB + NEO_KHZ800;  // WS2812B (más común)
// const uint16_t LED_TYPE = NEO_RGB + NEO_KHZ800;  // NeoPixel RGB
// const uint16_t LED_TYPE = NEO_RGBW + NEO_KHZ800; // NeoPixel RGBW

// ===== CONFIGURACIÓN ALEXA / FAUXMO =====
// Alexa se habilita/deshabilita en runtime desde el portal de configuración.
const char* ALEXA_DEVICE_NAME = "Led Name";  // Nombre por defecto (se puede cambiar desde la web)

// ===== CONFIGURACIÓN OTA (Over-The-Air Updates) =====
// ⚠️ CAMBIA OTA_PASSWORD ANTES DE USAR ESTO EN TU RED.
// Con la contraseña por defecto, cualquiera que esté en tu WiFi puede
// reprogramar el dispositivo por aire.
const bool OTA_ENABLED = true;               // true = habilitar OTA, false = deshabilitar
const char* OTA_HOSTNAME = "tiraled";        // Nombre del dispositivo en la red
const char* OTA_PASSWORD = "cambiame";       // <-- CÁMBIALA
const uint16_t OTA_PORT = 8266;              // Puerto para OTA (por defecto 8266)

// ===== CONFIGURACIÓN SERVIDOR WEB =====
const uint16_t WEB_SERVER_PORT = 80;         // Puerto del servidor web

// ===== CONFIGURACIÓN DE EFECTOS (sin delay, en milisegundos) =====
// IMPORTANTE: Intervalos optimizados para AsyncWebServer (no bloquear el loop)
const uint16_t EFFECT_SPEED_RAINBOW = 50;     // Intervalo para efecto arcoíris (antes 20ms)
const uint16_t EFFECT_SPEED_FADE = 50;        // Intervalo para efecto fade (antes 30ms)
const uint16_t EFFECT_SPEED_STROBE = 100;     // Intervalo para efecto strobe (antes 50ms)
const uint16_t EFFECT_SPEED_THEATER = 150;    // Intervalo para efecto theater (antes 100ms)
const uint16_t EFFECT_SPEED_FIRE_MIN = 80;    // Intervalo mínimo para efecto fuego (antes 50ms)
const uint16_t EFFECT_SPEED_FIRE_MAX = 200;   // Intervalo máximo para efecto fuego (antes 150ms)
const uint8_t EFFECT_FADE_STEP = 8;           // Incremento de brillo en fade (antes 5)
const uint8_t EFFECT_STROBE_FLASH_COUNT = 2;  // Número de flashes en strobe

// ===== CONFIGURACIÓN DE DEPURACIÓN =====
const bool SERIAL_DEBUG = false;             // DESHABILITADO: reduce uso de memoria y CPU
const uint32_t SERIAL_BAUDRATE = 115200;     // Velocidad del puerto serial

// ===== INFORMACIÓN DEL PROYECTO =====
const char* PROJECT_VERSION = "2.0.0";

/*
 * Config.h - Declaraciones de configuración centralizada
 *
 * Este archivo contiene las declaraciones extern de todos los parámetros
 * configurables definidos en Config.cpp. Incluir desde cualquier archivo
 * que necesite acceso a la configuración.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ===== COMPATIBILIDAD ENTRE PLATAFORMAS =====
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#else
  #error "Plataforma no soportada. Use ESP8266 o ESP32."
#endif

// ===== CONFIGURACIÓN DE RED =====
extern const bool USE_STATIC_IP;
extern const IPAddress STATIC_IP;
extern const IPAddress GATEWAY_IP;
extern const IPAddress SUBNET_MASK;
extern const IPAddress PRIMARY_DNS;
extern const IPAddress SECONDARY_DNS;

// ===== CONFIGURACIÓN LED STRIP =====
extern const uint16_t NUM_LEDS;
extern const uint8_t BRIGHTNESS;
extern const uint16_t LED_TYPE;

// ===== CONFIGURACIÓN ALEXA / FAUXMO =====
extern const char* ALEXA_DEVICE_NAME;

// ===== CONFIGURACIÓN OTA =====
extern const bool OTA_ENABLED;
extern const char* OTA_HOSTNAME;
extern const char* OTA_PASSWORD;
extern const uint16_t OTA_PORT;

// ===== CONFIGURACIÓN SERVIDOR WEB =====
extern const uint16_t WEB_SERVER_PORT;

// ===== CONFIGURACIÓN DE EFECTOS =====
extern const uint16_t EFFECT_SPEED_RAINBOW;
extern const uint16_t EFFECT_SPEED_FADE;
extern const uint16_t EFFECT_SPEED_STROBE;
extern const uint16_t EFFECT_SPEED_THEATER;
extern const uint16_t EFFECT_SPEED_FIRE_MIN;
extern const uint16_t EFFECT_SPEED_FIRE_MAX;
extern const uint8_t EFFECT_FADE_STEP;
extern const uint8_t EFFECT_STROBE_FLASH_COUNT;

// ===== CONFIGURACIÓN DE DEPURACIÓN =====
extern const bool SERIAL_DEBUG;
extern const uint32_t SERIAL_BAUDRATE;

// ===== INFORMACIÓN DEL PROYECTO =====
extern const char* PROJECT_VERSION;

#endif // CONFIG_H

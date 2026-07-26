/*
 * OTA.cpp - Funcionalidad de actualización Over-The-Air
 *
 * Este archivo maneja las actualizaciones OTA (Over-The-Air) que permiten
 * actualizar el firmware del ESP8266 de forma inalámbrica sin necesidad
 * de conectar el cable USB.
 *
 * Configuración en Config.cpp:
 * - OTA_ENABLED: habilitar/deshabilitar OTA
 * - OTA_HOSTNAME: usa getAlexaDeviceName() dinámicamente
 * - OTA_PASSWORD: cámbiala antes de desplegar (por defecto es un placeholder)
 * - OTA_PORT: puerto para OTA (por defecto 8266)
 *
 * Variables y librerías en Librerias.h
 */

#include "Librerias.h"

// ===== CONFIGURACIÓN OTA =====

void setupOTA() {
    if (!OTA_ENABLED) {
        if (SERIAL_DEBUG) {
            Serial.println("[OTA] Deshabilitado en configuración");
        }
        return;
    }

    if (SERIAL_DEBUG) {
        Serial.println("\n--- Configurando OTA ---");
    }

    // Configurar hostname, password y puerto
    ArduinoOTA.setHostname(getAlexaDeviceName());
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.setPort(OTA_PORT);

    // Callback cuando comienza la actualización
    ArduinoOTA.onStart([]() {
        evitarDeepSleep = true; // Activar flag durante OTA

        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }

        // Apagar LEDs durante actualización
        for(int i = 0; i < getActiveLedCount(); i++) {
            strip.setPixelColor(i, 0);
        }
        strip.show();

        if (SERIAL_DEBUG) {
            Serial.println("\n[OTA] Iniciando actualización de " + type);
        }
    });

    // Callback al finalizar la actualización
    ArduinoOTA.onEnd([]() {
        evitarDeepSleep = false; // Desactivar flag al terminar OTA

        if (SERIAL_DEBUG) {
            Serial.println("\n[OTA] Actualización completada");
        }

        // Animación de éxito (verde) - SIN DELAY
        uint16_t numLeds = getActiveLedCount();
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, strip.Color(0, 255, 0));
            if (i % 50 == 0) smartYield();
        }
        strip.show();
    });

    // Callback de progreso - SIMPLIFICADO para máxima estabilidad
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // Solo mostrar progreso por serial cada 10%
        static unsigned int lastPercent = 255;
        unsigned int percent = (progress / (total / 100));

        if (percent != lastPercent && percent % 10 == 0) {
            lastPercent = percent;
            if (SERIAL_DEBUG) {
                Serial.printf("[OTA] Progreso: %u%%\n", percent);
            }
        }

        // CRÍTICO: No actualizar LEDs durante progreso para evitar watchdog reset
        // Solo se actualizan en onStart (apagar), onEnd (verde) y onError (rojo)
    });

    // Callback de error
    ArduinoOTA.onError([](ota_error_t error) {
        evitarDeepSleep = false; // Desactivar flag en caso de error

        if (SERIAL_DEBUG) {
            Serial.printf("\n[OTA] Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) {
                Serial.println("Fallo de autenticación");
            } else if (error == OTA_BEGIN_ERROR) {
                Serial.println("Fallo al iniciar");
            } else if (error == OTA_CONNECT_ERROR) {
                Serial.println("Fallo de conexión");
            } else if (error == OTA_RECEIVE_ERROR) {
                Serial.println("Fallo al recibir");
            } else if (error == OTA_END_ERROR) {
                Serial.println("Fallo al finalizar");
            }
        }

        // Animación de error (rojo sólido) - SIN DELAYS
        uint16_t numLeds = getActiveLedCount();
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, strip.Color(255, 0, 0));
            if (i % 50 == 0) smartYield();
        }
        strip.show();
    });

    // Iniciar OTA
    ArduinoOTA.begin();

    if (SERIAL_DEBUG) {
        Serial.println("[OTA] OTA habilitado con hostname: " + String(getAlexaDeviceName()));
        Serial.printf("[OTA] Puerto: %d\n", OTA_PORT);
        Serial.println("[OTA] Busca el dispositivo en Arduino IDE:");
        Serial.println("      Herramientas > Puerto > Network Ports");
    }
}

/*
 * BoardPins.h - Pines de datos válidos para la tira LED según la placa
 *
 * El binario de ESP8266 y el de ESP32 son distintos, así que la plataforma se
 * conoce en compilación. El pin concreto, en cambio, es una preferencia del
 * usuario y se guarda en LittleFS: la interfaz web solo puede ofrecer los
 * pines que esa placa admite de verdad.
 *
 * Criterio de exclusión:
 *  - Pines cableados a la memoria flash (rompen el arranque).
 *  - Pines de UART0 (TX/RX): inutilizan el puerto serie.
 *  - Pines solo-entrada (ESP32 34-39): no pueden pilotar la tira.
 *  - ESP8266 GPIO16: no tiene la misma lógica de salida que el resto y
 *    Adafruit_NeoPixel no lo maneja de forma fiable.
 */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <Arduino.h>

struct LedPinOption {
    uint8_t gpio;        // Número de GPIO real (lo que se pasa a la librería)
    const char* label;   // Cómo lo serigrafía la placa (D2, GPIO4...)
    bool recommended;    // false = funciona, pero con advertencias
};

#if defined(ESP8266)

// NodeMCU / Wemos D1 mini y compatibles ESP-12.
// D0/GPIO16 excluido; GPIO6-11 son la flash; GPIO1/3 son TX/RX.
static const LedPinOption LED_PIN_OPTIONS[] = {
    { 4,  "D2 (GPIO4)",  true  },
    { 5,  "D1 (GPIO5)",  true  },
    { 14, "D5 (GPIO14)", true  },
    { 12, "D6 (GPIO12)", true  },
    { 13, "D7 (GPIO13)", true  },
    { 2,  "D4 (GPIO2)",  false },  // LED interno + pin de arranque
    { 0,  "D3 (GPIO0)",  false },  // Pin de arranque: en LOW entra en flasheo
    { 15, "D8 (GPIO15)", false }   // Pin de arranque: debe estar en LOW al inicio
};

#define DEFAULT_LED_PIN 4          // D2
#define BOARD_FAMILY_NAME "ESP8266"

// OJO: ESP32-C3/S2/S3/C6 TAMBIÉN definen ESP32, así que hay que distinguirlos
// por su CONFIG_IDF_TARGET_* ANTES de caer en el caso del ESP32 clásico.
// Si no, un C3 (que solo llega hasta GPIO21) recibiría la lista del clásico y
// ofrecería pines que no existen en el chip.

#elif defined(CONFIG_IDF_TARGET_ESP32C3)

// ESP32-C3: solo GPIO 0-21. Los 11-17 van a la flash en casi todos los módulos.
// 18/19 son el USB-JTAG y 20/21 el UART0.
static const LedPinOption LED_PIN_OPTIONS[] = {
    { 3,  "GPIO3",  true  },
    { 4,  "GPIO4",  true  },
    { 5,  "GPIO5",  true  },
    { 6,  "GPIO6",  true  },
    { 7,  "GPIO7",  true  },
    { 10, "GPIO10", true  },
    { 1,  "GPIO1",  false },  // ADC1
    { 0,  "GPIO0",  false },  // Strapping
    { 2,  "GPIO2",  false },  // Strapping
    { 8,  "GPIO8",  false },  // Strapping + LED interno en muchas placas
    { 9,  "GPIO9",  false }   // Strapping (BOOT)
};

#define DEFAULT_LED_PIN 4
#define BOARD_FAMILY_NAME "ESP32-C3"

#elif defined(CONFIG_IDF_TARGET_ESP32S3)

// ESP32-S3: GPIO 0-21 y 33-48. Los 26-32 son flash/PSRAM.
// 19/20 son el USB nativo y 43/44 el UART0.
static const LedPinOption LED_PIN_OPTIONS[] = {
    { 4,  "GPIO4",  true  },
    { 5,  "GPIO5",  true  },
    { 6,  "GPIO6",  true  },
    { 7,  "GPIO7",  true  },
    { 15, "GPIO15", true  },
    { 16, "GPIO16", true  },
    { 17, "GPIO17", true  },
    { 18, "GPIO18", true  },
    { 8,  "GPIO8",  true  },
    { 9,  "GPIO9",  true  },
    { 10, "GPIO10", true  },
    { 11, "GPIO11", true  },
    { 12, "GPIO12", true  },
    { 13, "GPIO13", true  },
    { 14, "GPIO14", true  },
    { 21, "GPIO21", true  },
    { 47, "GPIO47", true  },
    { 48, "GPIO48", true  },  // LED interno en muchas placas
    { 0,  "GPIO0",  false },  // Strapping (BOOT)
    { 3,  "GPIO3",  false },  // Strapping
    { 45, "GPIO45", false },  // Strapping
    { 46, "GPIO46", false }   // Strapping
};

#define DEFAULT_LED_PIN 16
#define BOARD_FAMILY_NAME "ESP32-S3"

#elif defined(CONFIG_IDF_TARGET_ESP32S2)

// ESP32-S2: GPIO 0-21 y 33-46. Los 26-32 son la flash.
static const LedPinOption LED_PIN_OPTIONS[] = {
    { 4,  "GPIO4",  true  },
    { 5,  "GPIO5",  true  },
    { 6,  "GPIO6",  true  },
    { 7,  "GPIO7",  true  },
    { 8,  "GPIO8",  true  },
    { 9,  "GPIO9",  true  },
    { 10, "GPIO10", true  },
    { 11, "GPIO11", true  },
    { 12, "GPIO12", true  },
    { 13, "GPIO13", true  },
    { 14, "GPIO14", true  },
    { 16, "GPIO16", true  },
    { 17, "GPIO17", true  },
    { 18, "GPIO18", true  },
    { 21, "GPIO21", true  },
    { 33, "GPIO33", true  },
    { 34, "GPIO34", true  },
    { 35, "GPIO35", true  },
    { 0,  "GPIO0",  false },  // Strapping (BOOT)
    { 45, "GPIO45", false },  // Strapping
    { 46, "GPIO46", false }   // Strapping
};

#define DEFAULT_LED_PIN 16
#define BOARD_FAMILY_NAME "ESP32-S2"

// OJO: aquí va CONFIG_IDF_TARGET_ESP32, no el ESP32 genérico. Las variantes sin
// rama propia (C5, C6, H2, P4) también definen ESP32, así que con el genérico
// caerían aquí y recibirían la lista del clásico: pines que no existen en el chip
// (GPIO32/33 en un C6, que solo llega a GPIO30) o que van a la flash. Con el
// target concreto, una variante no soportada falla al compilar en vez de ofrecer
// pines inválidos en silencio.
#elif defined(CONFIG_IDF_TARGET_ESP32)

// ESP32 clásico: DevKit v1, WROOM-32, NodeMCU-32S, WROVER, D1 R32...
// 6-11 son la flash; 34-39 solo entrada; 1/3 son TX/RX.
static const LedPinOption LED_PIN_OPTIONS[] = {
    { 16, "GPIO16", true  },
    { 17, "GPIO17", true  },
    { 18, "GPIO18", true  },
    { 19, "GPIO19", true  },
    { 21, "GPIO21", true  },
    { 22, "GPIO22", true  },
    { 23, "GPIO23", true  },
    { 25, "GPIO25", true  },
    { 26, "GPIO26", true  },
    { 27, "GPIO27", true  },
    { 32, "GPIO32", true  },
    { 33, "GPIO33", true  },
    { 4,  "GPIO4",  true  },
    { 13, "GPIO13", true  },
    { 5,  "GPIO5",  false },  // Strapping: debe quedar HIGH al arrancar
    { 2,  "GPIO2",  false },  // Strapping + LED interno
    { 14, "GPIO14", false },  // Emite pulsos al arrancar
    { 12, "GPIO12", false },  // Strapping: en HIGH al arrancar puede impedir el boot
    { 15, "GPIO15", false }   // Strapping: silencia el log de arranque
};

#define DEFAULT_LED_PIN 16
#define BOARD_FAMILY_NAME "ESP32"

#else
#error "Plataforma no soportada. Use ESP8266, ESP32, ESP32-C3, ESP32-S2 o ESP32-S3. Para C6/H2/P4 hay que añadir antes su lista de pines aquí."
#endif

inline uint8_t ledPinOptionCount() {
    return sizeof(LED_PIN_OPTIONS) / sizeof(LED_PIN_OPTIONS[0]);
}

// Solo se acepta un pin que esté en la lista de la placa actual.
inline bool isValidLedPin(uint8_t gpio) {
    for (uint8_t i = 0; i < ledPinOptionCount(); i++) {
        if (LED_PIN_OPTIONS[i].gpio == gpio) return true;
    }
    return false;
}

inline const char* ledPinLabel(uint8_t gpio) {
    for (uint8_t i = 0; i < ledPinOptionCount(); i++) {
        if (LED_PIN_OPTIONS[i].gpio == gpio) return LED_PIN_OPTIONS[i].label;
    }
    return "desconocido";
}

#endif // BOARD_PINS_H

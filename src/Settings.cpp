/*
 * Settings.cpp - Sistema de configuración persistente
 *
 * Maneja la carga y guardado de configuraciones en la memoria flash
 * usando LittleFS (SPIFFS) con formato JSON
 */

#include "Librerias.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// ===== DEFINICIONES LOCALES =====
const char* SETTINGS_FILE = "/settings.json";
const size_t JSON_BUFFER_SIZE = 1024;

// ===== ESTRUCTURA DE CONFIGURACIÓN PERSISTENTE =====
struct PersistentSettings {
    uint16_t numLeds;
    uint8_t brightness;
    uint8_t colorR;
    uint8_t colorG;
    uint8_t colorB;
    char effect[32];
    bool ledsOn;
    char deviceName[64];

    // Configuración de red
    bool useStaticIP;
    uint8_t staticIP[4];
    uint8_t gateway[4];
    uint8_t subnet[4];

    // Credenciales WiFi (provisión por AP)
    char wifiSsid[64];
    char wifiPassword[64];

    // Alexa habilitada en runtime
    bool alexaEnabled;

    // Pin de datos de la tira (validado contra la whitelist de la placa)
    uint8_t ledPin;

    // Constructor con valores por defecto
    PersistentSettings() {
        numLeds = 90;  // Valor por defecto razonable
        brightness = BRIGHTNESS;
        colorR = 255;
        colorG = 255;
        colorB = 255;
        strcpy(effect, "static");
        ledsOn = false; // Iniciar apagado tras reinicio/corte de luz
        strcpy(deviceName, ALEXA_DEVICE_NAME);

        // IP por defecto
        useStaticIP = USE_STATIC_IP;
        staticIP[0] = STATIC_IP[0];
        staticIP[1] = STATIC_IP[1];
        staticIP[2] = STATIC_IP[2];
        staticIP[3] = STATIC_IP[3];

        gateway[0] = GATEWAY_IP[0];
        gateway[1] = GATEWAY_IP[1];
        gateway[2] = GATEWAY_IP[2];
        gateway[3] = GATEWAY_IP[3];

        subnet[0] = SUBNET_MASK[0];
        subnet[1] = SUBNET_MASK[1];
        subnet[2] = SUBNET_MASK[2];
        subnet[3] = SUBNET_MASK[3];

        // WiFi vacío por defecto: entrará en modo AP para provisión
        wifiSsid[0] = '\0';
        wifiPassword[0] = '\0';

        // Alexa deshabilitada por defecto (configurable desde el portal)
        alexaEnabled = false;

        // Pin por defecto de la placa actual (D2 en ESP8266, GPIO16 en ESP32)
        ledPin = DEFAULT_LED_PIN;
    }
};

PersistentSettings settings;

// ===== FUNCIÓN: INICIALIZAR SISTEMA DE ARCHIVOS =====
bool initFileSystem() {
    if (SERIAL_DEBUG) {
        Serial.println("\n[Settings] Inicializando sistema de archivos...");
    }

    if (!LittleFS.begin()) {
        if (SERIAL_DEBUG) {
            Serial.println("[Settings] Error al montar LittleFS. Intentando formatear...");
        }

        if (LittleFS.format()) {
            if (SERIAL_DEBUG) {
                Serial.println("[Settings] LittleFS formateado exitosamente");
            }

            if (LittleFS.begin()) {
                if (SERIAL_DEBUG) {
                    Serial.println("[Settings] LittleFS montado después de formatear");
                }
                return true;
            }
        }

        if (SERIAL_DEBUG) {
            Serial.println("[Settings] ERROR: No se pudo inicializar LittleFS");
        }
        return false;
    }

    if (SERIAL_DEBUG) {
        Serial.println("[Settings] LittleFS montado correctamente");
#if defined(ESP8266)
        FSInfo fs_info;
        LittleFS.info(fs_info);
        Serial.printf("[Settings] Espacio total: %d bytes\n", fs_info.totalBytes);
        Serial.printf("[Settings] Espacio usado: %d bytes\n", fs_info.usedBytes);
#elif defined(ESP32)
        Serial.printf("[Settings] Espacio total: %d bytes\n", LittleFS.totalBytes());
        Serial.printf("[Settings] Espacio usado: %d bytes\n", LittleFS.usedBytes());
#endif
    }

    return true;
}

// ===== FUNCIÓN: CARGAR CONFIGURACIÓN DESDE ARCHIVO =====
bool loadSettings() {
    if (!LittleFS.exists(SETTINGS_FILE)) {
        if (SERIAL_DEBUG) {
            Serial.println("[Settings] Archivo de configuración no encontrado. Usando valores por defecto.");
        }
        return false;
    }

    File file = LittleFS.open(SETTINGS_FILE, "r");
    if (!file) {
        if (SERIAL_DEBUG) {
            Serial.println("[Settings] Error al abrir archivo de configuración");
        }
        return false;
    }

    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        if (SERIAL_DEBUG) {
            Serial.printf("[Settings] Error al parsear JSON: %s\n", error.c_str());
        }
        return false;
    }

    // Cargar valores del JSON
    settings.numLeds = doc["numLeds"] | 90;  // Default 90 LEDs si no está configurado
    settings.brightness = doc["brightness"] | BRIGHTNESS;
    settings.colorR = doc["colorR"] | 255;
    settings.colorG = doc["colorG"] | 255;
    settings.colorB = doc["colorB"] | 255;
    strlcpy(settings.effect, doc["effect"] | "static", sizeof(settings.effect));
    settings.ledsOn = doc["ledsOn"] | false; // Iniciar apagado tras reinicio/corte de luz
    strlcpy(settings.deviceName, doc["deviceName"] | ALEXA_DEVICE_NAME, sizeof(settings.deviceName));

    // Cargar credenciales WiFi
    strlcpy(settings.wifiSsid, doc["wifiSsid"] | "", sizeof(settings.wifiSsid));
    strlcpy(settings.wifiPassword, doc["wifiPassword"] | "", sizeof(settings.wifiPassword));

    // Cargar Alexa runtime
    settings.alexaEnabled = doc["alexaEnabled"] | false;

    // Si el JSON trae un pin que esta placa no admite (por ejemplo, settings
    // copiados de un ESP32 a un ESP8266), caer al pin por defecto en vez de
    // dejar la tira apuntando a un GPIO inválido.
    uint8_t storedPin = doc["ledPin"] | DEFAULT_LED_PIN;
    settings.ledPin = isValidLedPin(storedPin) ? storedPin : DEFAULT_LED_PIN;

    // Cargar configuración de red
    settings.useStaticIP = doc["useStaticIP"] | USE_STATIC_IP;

    JsonArray ipArray = doc["staticIP"];
    if (ipArray.size() == 4) {
        for (int i = 0; i < 4; i++) {
            settings.staticIP[i] = ipArray[i];
        }
    }

    JsonArray gwArray = doc["gateway"];
    if (gwArray.size() == 4) {
        for (int i = 0; i < 4; i++) {
            settings.gateway[i] = gwArray[i];
        }
    }

    JsonArray snArray = doc["subnet"];
    if (snArray.size() == 4) {
        for (int i = 0; i < 4; i++) {
            settings.subnet[i] = snArray[i];
        }
    }

    if (SERIAL_DEBUG) {
        Serial.println("[Settings] Configuración cargada desde archivo:");
        Serial.printf("  - LEDs: %d\n", settings.numLeds);
        Serial.printf("  - Brillo: %d\n", settings.brightness);
        Serial.printf("  - Color RGB: (%d, %d, %d)\n", settings.colorR, settings.colorG, settings.colorB);
        Serial.printf("  - Efecto: %s\n", settings.effect);
        Serial.printf("  - Estado: %s\n", settings.ledsOn ? "Encendido" : "Apagado");
        Serial.printf("  - Dispositivo Alexa: %s\n", settings.deviceName);
        Serial.printf("  - IP Estática: %s\n", settings.useStaticIP ? "Habilitada" : "DHCP");
        if (settings.useStaticIP) {
            Serial.printf("  - IP: %d.%d.%d.%d\n",
                settings.staticIP[0], settings.staticIP[1], settings.staticIP[2], settings.staticIP[3]);
            Serial.printf("  - Gateway: %d.%d.%d.%d\n",
                settings.gateway[0], settings.gateway[1], settings.gateway[2], settings.gateway[3]);
        }
    }

    return true;
}

// ===== FUNCIÓN: GUARDAR CONFIGURACIÓN EN ARCHIVO =====
bool saveSettings() {
    if (SERIAL_DEBUG) {
        Serial.println("[Settings] Iniciando guardado de configuración...");
    }

    // Verificar que LittleFS esté montado
    if (!LittleFS.begin()) {
        if (SERIAL_DEBUG) {
            Serial.println("[Settings] ERROR: LittleFS no está montado");
        }
        return false;
    }

    StaticJsonDocument<JSON_BUFFER_SIZE> doc;

    doc["numLeds"] = settings.numLeds;
    doc["brightness"] = settings.brightness;
    doc["colorR"] = settings.colorR;
    doc["colorG"] = settings.colorG;
    doc["colorB"] = settings.colorB;
    doc["effect"] = settings.effect;
    doc["ledsOn"] = settings.ledsOn;
    doc["deviceName"] = settings.deviceName;

    // Guardar credenciales WiFi
    doc["wifiSsid"] = settings.wifiSsid;
    doc["wifiPassword"] = settings.wifiPassword;

    // Guardar Alexa runtime
    doc["alexaEnabled"] = settings.alexaEnabled;
    doc["ledPin"] = settings.ledPin;

    // Guardar configuración de red
    doc["useStaticIP"] = settings.useStaticIP;

    JsonArray ipArray = doc.createNestedArray("staticIP");
    for (int i = 0; i < 4; i++) {
        ipArray.add(settings.staticIP[i]);
    }

    JsonArray gwArray = doc.createNestedArray("gateway");
    for (int i = 0; i < 4; i++) {
        gwArray.add(settings.gateway[i]);
    }

    JsonArray snArray = doc.createNestedArray("subnet");
    for (int i = 0; i < 4; i++) {
        snArray.add(settings.subnet[i]);
    }

    doc["version"] = PROJECT_VERSION;

    if (SERIAL_DEBUG) {
        Serial.printf("[Settings] Abriendo archivo: %s\n", SETTINGS_FILE);
    }

    File file = LittleFS.open(SETTINGS_FILE, "w");
    if (!file) {
        if (SERIAL_DEBUG) {
            Serial.println("[Settings] ERROR: No se pudo crear archivo de configuración");
        }
        return false;
    }

    if (SERIAL_DEBUG) {
        Serial.println("[Settings] Archivo abierto, serializando JSON...");
    }

    size_t bytesWritten = serializeJson(doc, file);
    if (bytesWritten == 0) {
        if (SERIAL_DEBUG) {
            Serial.println("[Settings] ERROR: No se pudo escribir JSON (0 bytes)");
        }
        file.close();
        return false;
    }

    file.close();

    if (SERIAL_DEBUG) {
        Serial.printf("[Settings] ✅ Configuración guardada exitosamente (%d bytes)\n", bytesWritten);
        Serial.println("[Settings] Contenido guardado:");
        serializeJsonPretty(doc, Serial);
        Serial.println();
    }

    return true;
}

// ===== FUNCIÓN: APLICAR CONFIGURACIÓN CARGADA =====
void applySettings() {
    // Aplicar brillo
    currentBrightness = settings.brightness;
    strip.setBrightness(currentBrightness);

    // Aplicar color
    currentRed = settings.colorR;
    currentGreen = settings.colorG;
    currentBlue = settings.colorB;

    // Aplicar estado
    ledsOn = settings.ledsOn;

    // Aplicar efecto
    currentEffect = String(settings.effect);

    if (ledsOn) {
        setColor(currentRed, currentGreen, currentBlue);
    } else {
        for(int i = 0; i < getActiveLedCount(); i++) {
            strip.setPixelColor(i, 0);
        }
        strip.show();
    }

    if (SERIAL_DEBUG) {
        Serial.println("[Settings] Configuración aplicada al sistema");
    }
}

// ===== FUNCIÓN: ACTUALIZAR CONFIGURACIÓN ACTUAL =====
void updateCurrentSettings() {
    settings.brightness = currentBrightness;
    settings.colorR = currentRed;
    settings.colorG = currentGreen;
    settings.colorB = currentBlue;
    strlcpy(settings.effect, currentEffect.c_str(), sizeof(settings.effect));
    settings.ledsOn = ledsOn;
}

// ===== FUNCIÓN: OBTENER NÚMERO DE LEDS CONFIGURADO =====
uint16_t getConfiguredNumLeds() {
    return settings.numLeds;
}

// ===== FUNCIÓN: ESTABLECER NÚMERO DE LEDS =====
void setConfiguredNumLeds(uint16_t numLeds) {
    settings.numLeds = numLeds;
}



// ===== FUNCIÓN: OBTENER CONFIGURACIÓN COMO JSON =====
String getSettingsJSON() {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;

    doc["numLeds"] = settings.numLeds;
    doc["brightness"] = settings.brightness;
    doc["colorR"] = settings.colorR;
    doc["colorG"] = settings.colorG;
    doc["colorB"] = settings.colorB;
    doc["effect"] = settings.effect;
    doc["ledsOn"] = settings.ledsOn;
    doc["deviceName"] = settings.deviceName;

    // Información de red y WiFi
    // NUNCA exponer wifiPassword aquí: este JSON lo sirve /control/settings sin
    // autenticación, así que cualquiera en la LAN podría leer la clave del WiFi.
    // Solo se informa si hay contraseña guardada, no cuál es.
    doc["wifiSsid"] = settings.wifiSsid;
    doc["hasWifiPassword"] = (settings.wifiPassword[0] != '\0');
    doc["alexaEnabled"] = settings.alexaEnabled;
    doc["useStaticIP"] = settings.useStaticIP;

    // Solo el pin actual y la familia de placa. La lista de pines admitidos
    // va en /control/pins: son hasta 19 objetos y aquí no caben, el
    // StaticJsonDocument de 1 KB se desbordaría en silencio.
    doc["board"] = BOARD_FAMILY_NAME;
    doc["ledPin"] = settings.ledPin;

    JsonArray ipArray = doc.createNestedArray("staticIP");
    for (int i = 0; i < 4; i++) {
        ipArray.add(settings.staticIP[i]);
    }

    JsonArray gwArray = doc.createNestedArray("gateway");
    for (int i = 0; i < 4; i++) {
        gwArray.add(settings.gateway[i]);
    }

    JsonArray snArray = doc.createNestedArray("subnet");
    for (int i = 0; i < 4; i++) {
        snArray.add(settings.subnet[i]);
    }

    // IP como string para facilitar lectura
    char ipStr[16], gwStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
        settings.staticIP[0], settings.staticIP[1], settings.staticIP[2], settings.staticIP[3]);
    snprintf(gwStr, sizeof(gwStr), "%d.%d.%d.%d",
        settings.gateway[0], settings.gateway[1], settings.gateway[2], settings.gateway[3]);

    doc["staticIPStr"] = ipStr;
    doc["gatewayStr"] = gwStr;

    String output;
    serializeJson(doc, output);
    return output;
}

// ===== FUNCIÓN: OBTENER NOMBRE DEL DISPOSITIVO ALEXA =====
const char* getAlexaDeviceName() {
    return settings.deviceName;
}

// ===== FUNCIÓN: ESTABLECER NOMBRE DEL DISPOSITIVO ALEXA =====
void setAlexaDeviceName(const char* name) {
    strlcpy(settings.deviceName, name, sizeof(settings.deviceName));
}

// ===== FUNCIÓN: OBTENER CONFIGURACIÓN DE IP =====
bool getUseStaticIP() {
    return settings.useStaticIP;
}

IPAddress getConfiguredStaticIP() {
    return IPAddress(settings.staticIP[0], settings.staticIP[1],
                     settings.staticIP[2], settings.staticIP[3]);
}

IPAddress getConfiguredGateway() {
    return IPAddress(settings.gateway[0], settings.gateway[1],
                     settings.gateway[2], settings.gateway[3]);
}

IPAddress getConfiguredSubnet() {
    return IPAddress(settings.subnet[0], settings.subnet[1],
                     settings.subnet[2], settings.subnet[3]);
}

// ===== FUNCIÓN: ESTABLECER CONFIGURACIÓN DE IP =====
void setNetworkConfig(bool useStatic, uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4,
                      uint8_t gw1, uint8_t gw2, uint8_t gw3, uint8_t gw4) {
    settings.useStaticIP = useStatic;

    settings.staticIP[0] = ip1;
    settings.staticIP[1] = ip2;
    settings.staticIP[2] = ip3;
    settings.staticIP[3] = ip4;

    settings.gateway[0] = gw1;
    settings.gateway[1] = gw2;
    settings.gateway[2] = gw3;
    settings.gateway[3] = gw4;

    // Subnet por defecto 255.255.255.0
    settings.subnet[0] = 255;
    settings.subnet[1] = 255;
    settings.subnet[2] = 255;
    settings.subnet[3] = 0;
}

// ===== FUNCIÓN: RESTABLECER VALORES DE FÁBRICA =====
bool resetToDefaults() {
    // Eliminar archivo de configuración
    if (LittleFS.exists(SETTINGS_FILE)) {
        LittleFS.remove(SETTINGS_FILE);
    }

    // Restaurar valores por defecto
    settings = PersistentSettings();

    if (SERIAL_DEBUG) {
        Serial.println("[Settings] Configuración restablecida a valores de fábrica");
    }

    // Guardar valores por defecto
    return saveSettings();
}

// ===== FUNCIÓN: OBTENER/ESTABLECER CREDENCIALES WiFi =====
const char* getWiFiSsid() {
    return settings.wifiSsid;
}

const char* getWiFiPassword() {
    return settings.wifiPassword;
}

bool hasWiFiCredentials() {
    return settings.wifiSsid[0] != '\0';
}

void setWiFiCredentials(const char* ssid, const char* password) {
    strlcpy(settings.wifiSsid, ssid, sizeof(settings.wifiSsid));
    strlcpy(settings.wifiPassword, password, sizeof(settings.wifiPassword));
}


// ===== FUNCIÓN: OBTENER/ESTABLECER ALEXA RUNTIME =====
bool getAlexaEnabled() {
    return settings.alexaEnabled;
}

void setAlexaEnabled(bool enabled) {
    settings.alexaEnabled = enabled;
}

// ===== FUNCIÓN: OBTENER/ESTABLECER PIN DE LA TIRA =====
uint8_t getConfiguredLedPin() {
    // Doble red de seguridad: aunque en flash hubiera un valor inválido,
    // nunca se devuelve un pin que esta placa no admita.
    return isValidLedPin(settings.ledPin) ? settings.ledPin : DEFAULT_LED_PIN;
}

bool setConfiguredLedPin(uint8_t gpio) {
    if (!isValidLedPin(gpio)) return false;
    settings.ledPin = gpio;
    return true;
}


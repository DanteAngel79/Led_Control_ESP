#include "src/Librerias.h"

// ===== DEFINICIÓN DE OBJETOS GLOBALES (una sola vez) =====
// El pin real se aplica en setup() con strip.setPin() desde los settings;
// aquí solo hace falta un valor inicial válido para la placa.
Adafruit_NeoPixel strip(NUM_LEDS, DEFAULT_LED_PIN, LED_TYPE);
Espalexa espalexa;
AsyncWebServer server(WEB_SERVER_PORT);

// ===== DEFINICIÓN DE VARIABLES GLOBALES DE ESTADO LED =====
uint8_t currentBrightness = BRIGHTNESS;
uint8_t currentRed = 255;
uint8_t currentGreen = 255;
uint8_t currentBlue = 255;
String currentEffect = "static";
bool ledsOn = false; // Iniciar apagado tras reinicio/corte de luz

// ===== DEFINICIÓN DE VARIABLES PARA EFECTOS =====
unsigned long lastEffectUpdate = 0;
uint16_t effectStep = 0;
uint8_t effectBrightness = 0;
int8_t effectDirection = 1;
uint16_t effectInterval = 0;

// ===== TEMPORIZADOR DE AUTO APAGADO =====
// 40 minutos para reducir brillo a 15%
const unsigned long DIM_BRIGHTNESS_MS = 40UL * 60UL * 1000UL;
// 80 minutos para apagado total
const unsigned long AUTO_OFF_MS = 80UL * 60UL * 1000UL;
unsigned long ledsOnSince = 0;
bool _prevLedsOnState = false;
bool _brightnessReduced = false;

// ===== DEFINICIÓN DE VARIABLES OTA =====
bool evitarDeepSleep = false;

// ===== CONTROL DE RECONEXIÓN WiFi =====
bool wifiConectadoAnterior = false;
bool alexaInicializada = false;

// ===== SISTEMA DE ACCIONES DIFERIDAS =====
// Los handlers de AsyncWebServer marcan estas flags y el loop() las ejecuta.
// Esto evita bloquear el servidor con strip.show() / LittleFS dentro de un callback.
PendingAction pendingAction = ACTION_NONE;
uint8_t pendingR = 0;
uint8_t pendingG = 0;
uint8_t pendingB = 0;
uint8_t pendingBrightness = 0;
String pendingEffect = "";

// Reinicio y guardado diferidos solicitados desde handlers async
bool pendingRestart = false;
unsigned long pendingRestartAt = 0;
bool pendingConfigSave = false;

// Margen tras responder antes de reiniciar, para que el socket se vacíe
static const unsigned long RESTART_DELAY_MS = 800;

// Guardado diferido de settings (debounce)
bool settingsNeedSave = false;
unsigned long settingsLastChange = 0;
const unsigned long SETTINGS_SAVE_DEBOUNCE_MS = 5000; // Guardar como máximo cada 5s

// ===== FUNCIÓN AUXILIAR PARA ESTABLECER COLOR =====
void setColor(uint8_t r, uint8_t g, uint8_t b) {
    currentRed = r;
    currentGreen = g;
    currentBlue = b;
    if (currentEffect == "static" || currentEffect == "off") {
        uint16_t numLeds = getActiveLedCount();
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, strip.Color(r, g, b));
            // Yield cada 50 LEDs para evitar watchdog reset
            if (i % 50 == 0) smartYield();
        }
        strip.show();
    }
}

// ===== GUARDADO DIFERIDO DE SETTINGS =====
void scheduleSettingsSave() {
    settingsNeedSave = true;
    settingsLastChange = millis();
}

// ===== GUARDADO Y REINICIO DIFERIDOS (solicitados por handlers async) =====
void processPendingRestart() {
    // Guardar primero: si el usuario acaba de configurar el WiFi, esos datos
    // tienen que estar en flash antes de reiniciar.
    if (pendingConfigSave) {
        pendingConfigSave = false;
        updateCurrentSettings();

        if (saveSettings()) {
            Serial.println("[CONFIG] Configuración guardada. Reiniciando...");
            pendingRestart = true;
            pendingRestartAt = millis();
        } else {
            Serial.println("[CONFIG] ERROR: no se pudo guardar; no se reinicia.");
        }
    }

    if (pendingRestart && (millis() - pendingRestartAt >= RESTART_DELAY_MS)) {
        Serial.println("[SYSTEM] Reiniciando ahora.");
        ESP.restart();
    }
}

void processSettingsSave() {
    if (!settingsNeedSave) return;

    // Esperar el debounce para no saturar LittleFS
    if (millis() - settingsLastChange >= SETTINGS_SAVE_DEBOUNCE_MS) {
        settingsNeedSave = false;
        updateCurrentSettings();
        saveSettings();
    }
}

// ===== PROCESADOR DE ACCIONES DIFERIDAS =====
// Ejecuta en el loop() principal las acciones marcadas por los handlers web/Alexa.
void processPendingActions() {
    if (pendingAction == ACTION_NONE) return;

    switch (pendingAction) {
        case ACTION_TURN_ON:
            ledsOn = true;
            currentEffect = "static";
            strip.setBrightness(currentBrightness);
            setColor(currentRed, currentGreen, currentBlue);
            syncStateWithAlexa();
            scheduleSettingsSave();
            break;

        case ACTION_TURN_OFF: {
            ledsOn = false;
            currentEffect = "off";

            uint16_t numLeds = getActiveLedCount();
            for (int i = 0; i < numLeds; i++) {
                strip.setPixelColor(i, 0);
                if (i % 50 == 0) smartYield();
            }
            strip.show();

            syncStateWithAlexa();
            scheduleSettingsSave();
            break;
        }

        case ACTION_SET_COLOR:
            ledsOn = true;
            currentEffect = "static";
            setColor(pendingR, pendingG, pendingB);
            syncStateWithAlexa();
            scheduleSettingsSave();
            break;

        case ACTION_SET_BRIGHTNESS:
            currentBrightness = pendingBrightness;
            strip.setBrightness(currentBrightness);

            if (ledsOn && currentEffect == "static") {
                setColor(currentRed, currentGreen, currentBlue);
            } else {
                strip.show();
            }

            syncStateWithAlexa();
            scheduleSettingsSave();
            break;

        case ACTION_SET_EFFECT:
            setEffect(pendingEffect);
            ledsOn = true;
            scheduleSettingsSave();
            break;

        default:
            break;
    }

    pendingAction = ACTION_NONE;
}



// ===== SETUP =====

void setup() {
    Serial.begin(SERIAL_BAUDRATE);

    // ✅ 1. INICIALIZAR SISTEMA DE ARCHIVOS Y CONFIGURACIÓN
    if (initFileSystem()) {
        // Cargar configuración guardada
        if (loadSettings()) {
            applySettings();
        } else {
            // Guardar configuración por defecto
            updateCurrentSettings();
            saveSettings();
        }
    }

    // ✅ 2. INICIALIZAR TIRA LED
    // El pin viene de LittleFS, así que hay que aplicarlo ANTES de begin():
    // el objeto global se construyó con el pin por defecto de la placa.
    strip.setPin(getConfiguredLedPin());
    strip.begin();
    strip.setBrightness(currentBrightness);
    strip.show(); // Inicializar todos los LEDs apagados

    // ✅ 3. CONFIGURAR WiFi (STA o AP con portal cautivo)
    bool wifiConnected = setupWiFi();

    // ✅ 4. CONFIGURAR SERVIDOR WEB Y ALEXA
    setupWebServer();
    startWebServer();

    if (getAlexaEnabled() && wifiConnected) {
        setupAlexa();
        setupWebServerNotFound();
        alexaInicializada = true;
    }

    // ✅ 5. CONFIGURAR OTA
    setupOTA();

    // ✅ MOSTRAR INFORMACIÓN ESENCIAL (UNA VEZ)
    Serial.println("\n========================================");
    if (isInApMode()) {
        wifiConectadoAnterior = false;
        Serial.printf("Modo AP: %s\n", getApSsid().c_str());
        Serial.printf("IP del AP: %s\n", WiFi.softAPIP().toString().c_str());
    } else if (isWiFiConnected()) {
        wifiConectadoAnterior = true;
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        if (getAlexaEnabled()) {
            Serial.printf("Dispositivo Alexa: %s\n", getAlexaDeviceName());
        }
    } else {
        wifiConectadoAnterior = false;
        Serial.println("WiFi: Error de conexion");
    }
    Serial.println("========================================\n");
}

// ===== LOOP =====

void loop() {
    // CRÍTICO: Alimentar watchdog para evitar resets
    smartYield();

    // ===== LÓGICA TEMPORIZADOR AUTO-APAGADO =====
    // Detectar cambio de estado ON -> iniciar contador
    if (ledsOn && !_prevLedsOnState) {
        ledsOnSince = millis();
        _prevLedsOnState = true;
        _brightnessReduced = false; // Resetear flag de brillo reducido
    } else if (!ledsOn && _prevLedsOnState) {
        // Reseteamos el contador si se apaga manualmente
        _prevLedsOnState = false;
        ledsOnSince = 0;
        _brightnessReduced = false;
    }

    // Si está encendido, verificar hitos de tiempo
    if (ledsOn && ledsOnSince != 0) {
        unsigned long now = millis();
        // La resta maneja automáticamente el overflow de millis()
        unsigned long elapsed = now - ledsOnSince;

        // A los 40 minutos: reducir brillo a 15%
        if (!_brightnessReduced && elapsed >= DIM_BRIGHTNESS_MS) {
            Serial.println("[AUTO_DIM] 40 minutos alcanzados, reduciendo brillo a 15%");

            currentBrightness = 38; // 15% de 255 = ~38
            strip.setBrightness(currentBrightness);
            strip.show();

            smartYield();
            scheduleSettingsSave();

            _brightnessReduced = true;
        }

        // A los 80 minutos: apagar completamente
        if (elapsed >= AUTO_OFF_MS) {
            Serial.println("[AUTO_OFF] 80 minutos alcanzados, apagando LEDs para evitar sobrecalentamiento.");

            ledsOn = false;
            currentEffect = "off";

            // Apagar todos los LEDs de forma amable
            smartYield();
            uint16_t numLeds = getActiveLedCount();
            for (int i = 0; i < numLeds; i++) {
                strip.setPixelColor(i, 0);
                if (i % 50 == 0) smartYield();
            }
            strip.show();

            smartYield();
            syncStateWithAlexa();
            scheduleSettingsSave();

            // Reiniciar tracking
            _prevLedsOnState = false;
            ledsOnSince = 0;
            _brightnessReduced = false;
        }
    }

    // Ejecutar acciones pendientes marcadas por handlers web/Alexa
    // (debe ir ANTES del trabajo de red para no bloquear respuestas)
    processPendingActions();
    processSettingsSave();
    processPendingRestart();

    // Mantener lógica de WiFi (reconexión / fallback a AP)
    loopWiFi();

    // Verificar conexión WiFi
    bool wifiConectadoAhora = isWiFiConnected();

    // Detectar transición desconectado -> conectado
    if (wifiConectadoAhora && !wifiConectadoAnterior) {
        Serial.println();
        Serial.println("[WIFI] Conexión WiFi detectada!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());

        // Si Alexa no se inicializó en setup por falta de WiFi, hacerlo ahora
        if (getAlexaEnabled() && !alexaInicializada) {
            Serial.println("[ALEXA] Inicializando Alexa tras recuperar WiFi...");
            setupAlexa();
            setupWebServerNotFound();
            alexaInicializada = true;
        }

        wifiConectadoAnterior = true;
    }

    // Detectar transición conectado -> desconectado
    if (!wifiConectadoAhora && wifiConectadoAnterior) {
        Serial.println();
        Serial.println("[WIFI] Conexión WiFi perdida - Esperando reconexión automática...");
        wifiConectadoAnterior = false;
        alexaInicializada = false;
    }

    if (wifiConectadoAhora) {
        // CRÍTICO: Manejar OTA primero para máxima estabilidad
        if (OTA_ENABLED) {
            ArduinoOTA.handle();
        }

        // Manejar Espalexa (incluye SSDP para descubrimiento de Alexa)
        if (getAlexaEnabled() && alexaInicializada) {
            espalexa.loop();
        }

        // Actualizar efectos LED solo si no hay OTA en progreso
        if (!evitarDeepSleep) {
            updateEffects();
        }
    } else if (!isInApMode()) {
        // En modo STA desconectado, loopWiFi ya maneja la reconexión
    } else {
        // En modo AP, seguir ejecutando efectos para que los LEDs respondan localmente
        if (!evitarDeepSleep) {
            updateEffects();
        }
    }
}

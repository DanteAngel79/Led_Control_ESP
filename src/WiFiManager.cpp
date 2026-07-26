/*
 * WiFiManager.cpp - Gestión de WiFi con provisión por AP y portal cautivo
 */

#include "Librerias.h"
#include "WiFiManager.h"

// ===== VARIABLES DE ESTADO =====
static WiFiModeState wifiState = WIFI_STATE_CONNECTING;
static bool apMode = false;
static int consecutiveFailures = 0;
static unsigned long lastReconnectAttempt = 0;
static String apSsidName;
static DNSServer dnsServer;
static const byte DNS_PORT = 53;
static unsigned long apStartedAt = 0;

// ===== AUXILIAR: CONTRASEÑA DEL AP =====
// Clave fija y conocida: el AP solo existe durante la provisión y se apaga en
// cuanto el equipo entra en la red del usuario, así que se prioriza que sea
// fácil de teclear. Quien quiera una única por dispositivo puede derivarla de
// WiFi.macAddress() aquí mismo.
String getApPassword() {
    return String(AP_PASSWORD);
}

// ===== AUXILIAR: GENERAR SSID DEL AP =====
String getApSsid() {
    if (apSsidName.length() == 0) {
        String mac = WiFi.macAddress();
        // Usar últimos 4 caracteres de la MAC como sufijo
        String suffix = mac.substring(mac.length() - 5);
        suffix.replace(":", "");
        apSsidName = String(AP_SSID_PREFIX) + suffix;
    }
    return apSsidName;
}

// ===== CONFIGURAR MODO STA =====
static bool connectToSta() {
    if (!hasWiFiCredentials()) {
        if (SERIAL_DEBUG) {
            Serial.println("[WiFi] Sin credenciales guardadas");
        }
        return false;
    }

    const char* ssid = getWiFiSsid();
    const char* password = getWiFiPassword();

    if (SERIAL_DEBUG) {
        Serial.printf("[WiFi] Conectando a: %s\n", ssid);
    }

    WiFi.mode(WIFI_STA);

    // IP estática si está configurada
    if (getUseStaticIP()) {
        WiFi.config(getConfiguredStaticIP(), getConfiguredGateway(), getConfiguredSubnet(), PRIMARY_DNS, SECONDARY_DNS);
    } else {
        WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(255, 255, 255, 0));
    }

    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);

    // Esperar conexión con timeout
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_CONN_ATTEMPTS) {
        delay(500);
        attempts++;
        smartYield();
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (SERIAL_DEBUG) {
            Serial.printf("[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
        }
        consecutiveFailures = 0;
        wifiState = WIFI_STATE_CONNECTED;
        apMode = false;
        return true;
    }

    if (SERIAL_DEBUG) {
        Serial.println("[WiFi] No se pudo conectar");
    }
    consecutiveFailures++;
    wifiState = WIFI_STATE_CONNECTING;
    return false;
}

// ===== INICIAR MODO AP =====
void startAccessPoint() {
    if (SERIAL_DEBUG) {
        Serial.println("[WiFi] Iniciando modo AP con portal cautivo");
    }

    WiFi.mode(WIFI_AP);
    delay(100);

    String ssid = getApSsid();
    String pass = getApPassword();
    bool ok = WiFi.softAP(ssid.c_str(), pass.c_str());

    if (!ok) {
        if (SERIAL_DEBUG) {
            Serial.println("[WiFi] ERROR: No se pudo iniciar AP");
        }
        return;
    }

    apMode = true;
    wifiState = WIFI_STATE_AP;
    apStartedAt = millis();

    // Iniciar DNS server para captive portal: redirige cualquier dominio a la IP del AP
    dnsServer.setTTL(300);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    if (SERIAL_DEBUG) {
        Serial.printf("[WiFi] AP: %s / Pass: %s\n", ssid.c_str(), pass.c_str());
        Serial.printf("[WiFi] IP del AP: %s\n", WiFi.softAPIP().toString().c_str());
    }
}

void stopAccessPoint() {
    if (apMode) {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        apMode = false;
    }
}

// ===== SETUP WiFi =====
bool setupWiFi() {
    if (connectToSta()) {
        return true;
    }

    // Si falló y ya hemos fallado varias veces, o no hay credenciales, iniciar AP
    startAccessPoint();
    return apMode;
}

// ===== LOOP WiFi =====
// Sin delay() en ninguna rama: este loop comparte hilo con el servidor async
// y los efectos, así que bloquear aquí congela todo lo demás.
void loopWiFi() {
    unsigned long now = millis();

    if (apMode) {
        dnsServer.processNextRequest();

        // Con alguien conectado al portal, el AP no se toca. El reintento llama
        // a stopAccessPoint(), que corta la asociación, y connectToSta() bloquea
        // hasta 10 s (MAX_CONN_ATTEMPTS x 500 ms): eso echaba al usuario en
        // mitad de la configuración y dejaba sin responder el DNS y el portal.
        // El contador se rearma en vez de dejarse vencido porque los móviles se
        // despegan solos de un AP sin internet; si no, cada despegue disparaba
        // el corte justo cuando el usuario intentaba volver a entrar.
        if (WiFi.softAPgetStationNum() > 0) {
            apStartedAt = now;
            return;
        }

        // No quedarse en AP para siempre: si el usuario ya tenía credenciales
        // (p. ej. se cayó el router y volvió), reintentar STA cada 5 minutos.
        // Sin esto, un corte de luz obligaba a reconfigurar a mano.
        if (hasWiFiCredentials() && (now - apStartedAt >= AP_RETRY_STA_MS)) {
            // Sin guardar tras SERIAL_DEBUG a propósito: es un evento cada 5
            // minutos y es la unica forma de ver desde fuera que el AP se cae
            // sola. Sirve para confirmar o descartar esta causa con el monitor.
            Serial.println("[WiFi] Reintentando la red guardada: el AP se cae unos segundos.");
            apStartedAt = now;  // rearmar el contador pase lo que pase

            stopAccessPoint();
            if (connectToSta()) {
                if (SERIAL_DEBUG) {
                    Serial.println("[WiFi] Recuperada la red. Reiniciando para reanudar servicios.");
                }
                // Alexa/OTA se inicializan en setup() según el modo, así que
                // la vía limpia para volver a STA es reiniciar.
                pendingRestart = true;
                pendingRestartAt = now;
            } else {
                startAccessPoint();  // seguía sin haber red: volver al portal
            }
        }
        return;
    }

    bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected) {
        if (wifiState != WIFI_STATE_CONNECTED) {
            if (SERIAL_DEBUG) {
                Serial.printf("[WiFi] Conexión recuperada. IP: %s\n", WiFi.localIP().toString().c_str());
            }
            wifiState = WIFI_STATE_CONNECTED;
            consecutiveFailures = 0;
        }
        return;
    }

    if (wifiState == WIFI_STATE_CONNECTED) {
        if (SERIAL_DEBUG) {
            Serial.println("[WiFi] Conexión perdida. Intentando reconectar...");
        }
        wifiState = WIFI_STATE_CONNECTING;
        lastReconnectAttempt = now;  // dar margen al reconector automático
        return;
    }

    // Un intento cada 30 s. No se espera al resultado: la siguiente vuelta del
    // loop ya verá si WiFi.status() cambió.
    if (now - lastReconnectAttempt < RECONNECT_INTERVAL_MS) return;

    lastReconnectAttempt = now;
    consecutiveFailures++;

    if (SERIAL_DEBUG) {
        Serial.printf("[WiFi] Reintento de conexión #%d\n", consecutiveFailures);
    }

    if (consecutiveFailures >= MAX_FAIL_BEFORE_AP) {
        if (SERIAL_DEBUG) {
            Serial.println("[WiFi] Demasiados fallos. Abriendo portal en modo AP.");
        }
        consecutiveFailures = 0;
        startAccessPoint();
        return;
    }

    WiFi.reconnect();
}

// ===== CONSULTAS DE ESTADO =====
bool isInApMode() {
    return apMode;
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}


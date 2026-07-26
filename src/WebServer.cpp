/*
 * WebServer.cpp - Servidor Web con ESPAsyncWebServer
 * Compatible con Espalexa (modo async) para control de Alexa con colores
 */

#include "Librerias.h"

// ===== PÁGINA HTML =====
// La página vive en device_page.html y se compila comprimida en PageIndex.h
// (array index_html_gz). Regenerar con: python3 gzip_page.py
//
// CRÍTICO: se sirve en gzip a propósito. Sin comprimir son ~18 KB por
// conexión; con varios clientes a la vez AsyncWebServer agota el heap,
// devuelve respuestas truncadas y el servidor deja de responder.
#include "PageIndex.h"

// ===== PORTAL DE CONFIGURACIÓN WiFi (modo AP) =====
const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configurar TiraLed</title>
  <style>
    *{box-sizing:border-box}body{font-family:system-ui,sans-serif;margin:0;padding:1rem;background:#121212;color:#e0e0e0}
    .container{max-width:480px;margin:0 auto}
    h1{color:#ff9800}label{display:block;margin:1rem 0 .3rem}
    input,select{width:100%;padding:.7rem;border:1px solid #444;background:#2a2a2a;color:#e0e0e0;border-radius:6px}
    button{width:100%;padding:.9rem;margin-top:1rem;border:none;border-radius:6px;background:#ff9800;color:#121212;font-weight:700;cursor:pointer}
    button:disabled{opacity:.5;cursor:not-allowed}.toggle{display:flex;align-items:center;gap:.5rem;margin:1rem 0}
    .toggle input{width:auto}#networks{margin-top:.5rem}#status{margin-top:1rem;padding:.75rem;border-radius:6px;background:#1e1e1e}
    .network{padding:.6rem;background:#2a2a2a;margin:.3rem 0;border-radius:4px;cursor:pointer}
    .network:hover{background:#333}
  </style>
</head>
<body>
  <div class="container">
    <h1>Configurar TiraLed</h1>
    <p>Conecta el dispositivo a tu red WiFi.</p>
    <label>Red WiFi</label>
    <select id="ssid"><option value="">Cargando redes...</option></select>
    <button id="scanBtn" type="button">Escanear redes</button>
    <label>Contraseña</label>
    <input type="password" id="password" placeholder="Contraseña de la red">
    <label>Pin de datos de la tira <span id="boardName"></span></label>
    <select id="ledPin"><option value="">Cargando pines...</option></select>
    <label class="toggle"><input type="checkbox" id="alexa"> <span>Habilitar Alexa</span></label>
    <label class="toggle"><input type="checkbox" id="useStatic"> <span>Usar IP fija</span></label>
    <div id="ipFields" style="display:none">
      <label>Dirección IP</label>
      <input type="text" id="staticIP" placeholder="192.168.1.50">
      <label>Puerta de enlace (router)</label>
      <input type="text" id="gateway" placeholder="192.168.1.1">
      <p style="font-size:.85rem;color:#999;margin:.4rem 0 0">
        Si lo dejas desmarcado, el router le asigna la IP automáticamente (DHCP).
        Es lo recomendable si no estás seguro: siempre puedes fijarla después
        desde la página del dispositivo.
      </p>
    </div>
    <button id="saveBtn" type="button">Guardar y reiniciar</button>
    <div id="status"></div>
  </div>
  <script>
    // El firmware escanea en segundo plano y responde 202 mientras no hay
    // resultado, así que aquí se consulta hasta que llegue.
    async function scan() {
      document.getElementById('scanBtn').disabled = true;
      setStatus('Escaneando...');
      try {
        let data = null;
        for (let intento = 0; intento < 15; intento++) {
          const res = await fetch('/wifi/scan');
          data = await res.json();
          if (data.status === 'ok') break;
          setStatus('Escaneando' + '.'.repeat((intento % 3) + 1));
          await new Promise(r => setTimeout(r, 1000));
        }
        if (!data || data.status !== 'ok') {
          setStatus('El escaneo tardó demasiado. Reintenta.');
          return;
        }
        const sel = document.getElementById('ssid');
        sel.innerHTML = '';
        data.networks.forEach(n => {
          const opt = document.createElement('option');
          opt.value = n.ssid;
          opt.textContent = n.ssid + ' (' + n.rssi + ' dBm)' + (n.open ? '' : ' 🔒');
          sel.appendChild(opt);
        });
        setStatus('Redes encontradas: ' + data.networks.length);
      } catch (e) {
        setStatus('Error escaneando: ' + e.message);
      } finally {
        document.getElementById('scanBtn').disabled = false;
      }
    }
    function setStatus(msg) { document.getElementById('status').textContent = msg; }
    // Los pines los dicta el firmware según la placa compilada: aquí no se
    // inventa ninguna lista, solo se pinta la que llega.
    async function loadPins() {
      try {
        const res = await fetch('/control/pins');
        const data = await res.json();
        const sel = document.getElementById('ledPin');
        sel.innerHTML = '';
        (data.ledPinOptions || []).forEach(p => {
          const opt = document.createElement('option');
          opt.value = p.gpio;
          opt.textContent = p.label + (p.recommended ? '' : ' - no recomendado');
          if (p.gpio === data.ledPin) opt.selected = true;
          sel.appendChild(opt);
        });
        if (data.board) document.getElementById('boardName').textContent = '(' + data.board + ')';
      } catch (e) {
        document.getElementById('ledPin').innerHTML = '<option value="">Error cargando pines</option>';
      }
    }
    async function save() {
      const ssid = document.getElementById('ssid').value;
      const password = document.getElementById('password').value;
      const alexa = document.getElementById('alexa').checked;
      const ledPin = document.getElementById('ledPin').value;
      const useStatic = document.getElementById('useStatic').checked;
      const ip = document.getElementById('staticIP').value.trim();
      const gw = document.getElementById('gateway').value.trim();
      if (!ssid) { setStatus('Selecciona una red'); return; }
      if (useStatic && (!ip || !gw)) { setStatus('Indica la IP y la puerta de enlace'); return; }
      document.getElementById('saveBtn').disabled = true;
      setStatus('Guardando...');
      try {
        let cuerpo = 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password) +
                     '&alexa=' + alexa + '&useStaticIP=' + useStatic;
        if (ledPin !== '') cuerpo += '&ledPin=' + encodeURIComponent(ledPin);
        if (useStatic) cuerpo += '&staticIP=' + encodeURIComponent(ip) + '&gateway=' + encodeURIComponent(gw);
        const res = await fetch('/wifi/save', {
          method: 'POST',
          headers: {'Content-Type': 'application/x-www-form-urlencoded'},
          body: cuerpo
        });
        const data = await res.json();
        setStatus(data.message || data.error || 'OK');
      } catch (e) {
        setStatus('Error guardando: ' + e.message);
        document.getElementById('saveBtn').disabled = false;
      }
    }
    document.getElementById('scanBtn').addEventListener('click', scan);
    document.getElementById('saveBtn').addEventListener('click', save);
    document.getElementById('useStatic').addEventListener('change', function() {
      document.getElementById('ipFields').style.display = this.checked ? 'block' : 'none';
    });
    loadPins();
    scan();
  </script>
</body>
</html>
)rawliteral";

// ===== HANDLER: PÁGINA PRINCIPAL =====
void handleRoot(AsyncWebServerRequest *request) {
  if (isInApMode()) {
    // send_P, no send(): PORTAL_HTML vive en PROGMEM y pasarlo como String
    // lo leería como si fuera RAM.
    request->send_P(200, "text/html", PORTAL_HTML);
    return;
  }
  AsyncWebServerResponse *response =
      request->beginResponse_P(200, "text/html", index_html_gz, index_html_gz_len);
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "public, max-age=86400");
  request->send(response);
}

// ===== HANDLER: ENCENDER LEDs =====
void handleApiOn(AsyncWebServerRequest *request) {
  // Solo marcar acción pendiente; el loop() la ejecuta.
  // NUNCA hacer strip.show()/saveSettings() aquí: bloquea AsyncWebServer.
  pendingAction = ACTION_TURN_ON;
  request->send(200, "application/json", "{\"status\":\"ok\",\"state\":true}");
}

// ===== HANDLER: APAGAR LEDs =====
void handleApiOff(AsyncWebServerRequest *request) {
  // Solo marcar acción pendiente; el loop() la ejecuta.
  pendingAction = ACTION_TURN_OFF;
  request->send(200, "application/json", "{\"status\":\"ok\",\"state\":false}");
}

// ===== VALIDACIÓN DE PARÁMETROS NUMÉRICOS =====
// Devuelve false si el texto no es un entero válido o cae fuera de [min,max].
// Sin esto "300" desbordaba a 44 y "abc" se colaba como 0 (apagando la tira).
static bool parseIntArg(const String &raw, long min, long max, long *out) {
  if (raw.length() == 0) return false;

  size_t i = (raw[0] == '-' || raw[0] == '+') ? 1 : 0;
  if (i >= raw.length()) return false;
  for (size_t j = i; j < raw.length(); j++) {
    if (!isDigit(raw[j])) return false;
  }

  long value = raw.toInt();
  if (value < min || value > max) return false;

  *out = value;
  return true;
}

// ===== HANDLER: CAMBIAR COLOR =====
void handleApiColor(AsyncWebServerRequest *request) {
  if(request->hasArg("r") && request->hasArg("g") && request->hasArg("b")) {
    long rv, gv, bv;
    if(!parseIntArg(request->arg("r"), 0, 255, &rv) ||
       !parseIntArg(request->arg("g"), 0, 255, &gv) ||
       !parseIntArg(request->arg("b"), 0, 255, &bv)) {
      request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"RGB fuera de rango (0-255)\"}");
      return;
    }

    uint8_t r = (uint8_t)rv;
    uint8_t g = (uint8_t)gv;
    uint8_t b = (uint8_t)bv;

    // Solo marcar acción pendiente
    pendingAction = ACTION_SET_COLOR;
    pendingR = r;
    pendingG = g;
    pendingB = b;

    // Respuesta sin construir String dinámico (menos fragmentación de heap)
    char json[64];
    snprintf(json, sizeof(json), "{\"status\":\"ok\",\"r\":%u,\"g\":%u,\"b\":%u}", r, g, b);
    request->send(200, "application/json", json);
  } else {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
  }
}

// ===== HANDLER: CAMBIAR BRILLO =====
void handleApiBrightness(AsyncWebServerRequest *request) {
  if(request->hasArg("value")) {
    long percent;
    if(!parseIntArg(request->arg("value"), 0, 100, &percent)) {
      request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"Brillo fuera de rango (0-100)\"}");
      return;
    }

    uint8_t brightness = (uint8_t)map(percent, 0, 100, 0, 255);

    // Solo marcar acción pendiente
    pendingAction = ACTION_SET_BRIGHTNESS;
    pendingBrightness = brightness;

    char json[64];
    snprintf(json, sizeof(json), "{\"status\":\"ok\",\"brightness\":%u}", brightness);
    request->send(200, "application/json", json);
  } else {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing value\"}");
  }
}

// ===== HANDLER: CAMBIAR EFECTO =====
void handleApiEffect(AsyncWebServerRequest *request) {
  if(request->hasArg("name")) {
    String effect = request->arg("name");

    if(!isValidEffect(effect)) {
      request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"Efecto desconocido\"}");
      return;
    }

    // Solo marcar acción pendiente
    pendingAction = ACTION_SET_EFFECT;
    pendingEffect = effect;

    // Respuesta estática concatenada mínimamente
    String json = "{\"status\":\"ok\",\"effect\":\"" + effect + "\"}";
    request->send(200, "application/json", json);
  } else {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing name\"}");
  }
}

// ===== HANDLER: ESTADO =====
void handleApiStatus(AsyncWebServerRequest *request) {
  // Buffer estático para evitar fragmentación de heap en cada poll
  char json[256];
  snprintf(json, sizeof(json),
           "{\"status\":\"ok\",\"state\":%s,\"brightness\":%u,\"r\":%u,\"g\":%u,\"b\":%u,\"effect\":\"%s\"}",
           ledsOn ? "true" : "false",
           currentBrightness,
           currentRed,
           currentGreen,
           currentBlue,
           currentEffect.c_str());

  request->send(200, "application/json", json);
}

// ===== HANDLER: INFORMACIÓN DEL DISPOSITIVO =====
void handleApiInfo(AsyncWebServerRequest *request) {
  String json = "{\"status\":\"ok\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"deviceName\":\"" + String(getAlexaDeviceName()) + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"alexaEnabled\":" + String(getAlexaEnabled() ? "true" : "false") + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"inApMode\":" + String(isInApMode() ? "true" : "false") + ",";
  json += "\"uptime\":" + String(millis() / 1000) + "}";

  request->send(200, "application/json", json);
}

// ===== HANDLER: CONFIGURACIÓN =====
void handleApiSettings(AsyncWebServerRequest *request) {
  String json = getSettingsJSON();
  request->send(200, "application/json", json);
}

// ===== HANDLER: PINES ADMITIDOS POR ESTA PLACA =====
// Endpoint aparte porque la lista no cabe en el JSON de /control/settings.
// Es estática, así que se construye a mano sin ArduinoJson.
void handleApiPins(AsyncWebServerRequest *request) {
  String json = "{\"status\":\"ok\",\"board\":\"" BOARD_FAMILY_NAME "\",";
  json += "\"ledPin\":" + String(getConfiguredLedPin()) + ",";
  json += "\"ledPinOptions\":[";

  for (uint8_t i = 0; i < ledPinOptionCount(); i++) {
    if (i > 0) json += ",";
    json += "{\"gpio\":" + String(LED_PIN_OPTIONS[i].gpio) + ",";
    json += "\"label\":\"" + String(LED_PIN_OPTIONS[i].label) + "\",";
    json += "\"recommended\":" + String(LED_PIN_OPTIONS[i].recommended ? "true" : "false") + "}";
  }

  json += "]}";
  request->send(200, "application/json", json);
}

// ===== HANDLER: GUARDAR CONFIGURACIÓN =====
void handleApiSettingsSave(AsyncWebServerRequest *request) {
  // Pin de datos: se rechaza cualquiera que no esté en la whitelist de la placa
  if(request->hasArg("ledPin")) {
    long pin;
    if(!parseIntArg(request->arg("ledPin"), 0, 39, &pin) || !setConfiguredLedPin((uint8_t)pin)) {
      request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"Pin no válido para esta placa\"}");
      return;
    }
    Serial.printf("[CONFIG] Pin de la tira actualizado: %d\n", (int)pin);
  }

  // Procesar parámetros de configuración
  if(request->hasArg("numLeds")) {
    uint16_t numLeds = request->arg("numLeds").toInt();
    setConfiguredNumLeds(numLeds);
    Serial.printf("[CONFIG] Cantidad de LEDs actualizada: %d\n", numLeds);
  }

  if(request->hasArg("alexaName")) {
    String alexaName = request->arg("alexaName");
    setAlexaDeviceName(alexaName.c_str());
    Serial.printf("[CONFIG] Nombre Alexa actualizado: %s\n", alexaName.c_str());
  }

  if(request->hasArg("useStaticIP")) {
    bool useStatic = request->arg("useStaticIP") == "true";

    if(useStatic && request->hasArg("staticIP") && request->hasArg("gateway")) {
      String ipStr = request->arg("staticIP");
      String gwStr = request->arg("gateway");

      // Parsear IP estática
      IPAddress staticIP;
      IPAddress gateway;

      if(staticIP.fromString(ipStr) && gateway.fromString(gwStr)) {
        // Usar máscara de subred por defecto 255.255.255.0
        setNetworkConfig(true,
                        staticIP[0], staticIP[1], staticIP[2], staticIP[3],
                        gateway[0], gateway[1], gateway[2], gateway[3]);
        Serial.printf("[CONFIG] IP estática configurada: %s\n", ipStr.c_str());
        Serial.printf("[CONFIG] Gateway configurado: %s\n", gwStr.c_str());
      } else {
        Serial.println("[CONFIG] Error al parsear IPs");
      }
    } else if(!useStatic) {
      // Deshabilitar IP estática (usar DHCP)
      setNetworkConfig(false, 0, 0, 0, 0, 0, 0, 0, 0);
      Serial.println("[CONFIG] DHCP habilitado");
    }
  }

  // Guardar inmediatamente (no diferido) porque el usuario puede reiniciar enseguida
  updateCurrentSettings();
  saveSettings();
  Serial.println("[CONFIG] Configuración guardada en flash");

  request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuración guardada correctamente\"}");
}

// ===== HANDLER: RESET CONFIGURACIÓN =====
void handleApiSettingsReset(AsyncWebServerRequest *request) {
  Serial.println("[CONFIG] Restaurando configuración por defecto...");

  if(resetToDefaults()) {
    Serial.println("[CONFIG] Configuración restaurada exitosamente");
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Configuración restaurada a valores por defecto\"}");
  } else {
    Serial.println("[CONFIG] Error al restaurar configuración");
    request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Error al restaurar configuración\"}");
  }
}

// ===== HANDLER: REINICIAR DISPOSITIVO =====
void handleApiReset(AsyncWebServerRequest *request) {
  Serial.println("[SYSTEM] Reinicio solicitado...");

  request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Reiniciando dispositivo\"}");

  // ESP.restart() aquí cortaría la conexión antes de que la respuesta salga:
  // se marca y el loop() reinicia cuando el socket ya se ha vaciado.
  pendingRestart = true;
  pendingRestartAt = millis();
}


// ===== HANDLERS DEL PORTAL WiFi =====
// El escaneo NUNCA se hace dentro del handler: WiFi.scanNetworks() bloquea
// 2-4 s y aquí estamos en el contexto de AsyncTCP, donde bloquear cuelga el
// servidor y dispara el watchdog. Se lanza en modo asíncrono y el handler
// responde de inmediato con lo último que haya en caché.
void handleWifiScan(AsyncWebServerRequest *request) {
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    request->send(202, "application/json",
                  "{\"status\":\"scanning\",\"networks\":[]}");
    return;
  }

  if (n == WIFI_SCAN_FAILED) {
    // Aún no se ha lanzado ninguno (o el anterior falló): pedir uno y que el
    // cliente vuelva a preguntar.
    WiFi.scanNetworks(true);
    request->send(202, "application/json",
                  "{\"status\":\"scanning\",\"networks\":[]}");
    return;
  }

  String json = "{\"status\":\"ok\",\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    bool isOpen = false;
#if defined(ESP8266)
    isOpen = (WiFi.encryptionType(i) == ENC_TYPE_NONE);
#elif defined(ESP32)
    isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
#endif
    String ssid = WiFi.SSID(i);
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");

    json += "{";
    json += "\"ssid\":\"" + ssid + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"open\":" + String(isOpen ? "true" : "false");
    json += "}";
  }
  json += "]}";

  request->send(200, "application/json", json);

  // Liberar el resultado y dejar pedido el siguiente para la próxima consulta.
  WiFi.scanDelete();
}

void handleWifiSave(AsyncWebServerRequest *request) {
  // Solo se aceptan credenciales durante la provisión (modo AP). En STA
  // cualquiera de la LAN podría reescribir la red del dispositivo y secuestrarlo.
  if (!isInApMode()) {
    request->send(403, "application/json",
                  "{\"status\":\"error\",\"message\":\"Solo disponible en modo AP\"}");
    return;
  }

  if (!request->hasArg("ssid") || !request->hasArg("password")) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Faltan SSID o password\"}");
    return;
  }

  String ssid = request->arg("ssid");
  String password = request->arg("password");
  bool alexa = request->hasArg("alexa") && request->arg("alexa") == "true";

  if (ssid.length() == 0) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID vacío\"}");
    return;
  }

  // Pin de la tira: opcional, pero si viene debe existir en esta placa.
  if (request->hasArg("ledPin")) {
    long pin;
    if (!parseIntArg(request->arg("ledPin"), 0, 39, &pin) || !setConfiguredLedPin((uint8_t)pin)) {
      request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"Pin no válido para esta placa\"}");
      return;
    }
  }

  // IP fija: opcional. Si viene marcada, ambas direcciones deben ser válidas;
  // no se acepta a medias, porque una IP mal puesta deja el equipo inaccesible.
  if (request->hasArg("useStaticIP") && request->arg("useStaticIP") == "true") {
    IPAddress ip, gw;
    if (!request->hasArg("staticIP") || !request->hasArg("gateway") ||
        !ip.fromString(request->arg("staticIP")) ||
        !gw.fromString(request->arg("gateway"))) {
      request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"IP o puerta de enlace no válidas\"}");
      return;
    }
    setNetworkConfig(true, ip[0], ip[1], ip[2], ip[3], gw[0], gw[1], gw[2], gw[3]);
  } else {
    setNetworkConfig(false, 0, 0, 0, 0, 0, 0, 0, 0);  // DHCP
  }

  setWiFiCredentials(ssid.c_str(), password.c_str());
  setAlexaEnabled(alexa);

  // NO escribir en flash ni reiniciar aquí: estamos en el contexto de AsyncTCP.
  // Se marca y el loop() lo ejecuta tras haber enviado la respuesta.
  pendingConfigSave = true;

  request->send(200, "application/json",
                "{\"status\":\"ok\",\"message\":\"Configuración guardada. Reiniciando...\"}");
}

void handleWifiStatus(AsyncWebServerRequest *request) {
  String json = "{\"status\":\"ok\",";
  json += "\"apMode\":" + String(isInApMode() ? "true" : "false") + ",";
  json += "\"connected\":" + String(isWiFiConnected() ? "true" : "false") + ",";
  json += "\"ssid\":\"" + String(getWiFiSsid()) + "\",";
  json += "\"ip\":\"" + (isWiFiConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
  json += "}";
  request->send(200, "application/json", json);
}

// ===== HANDLERS PARA CAPTIVE PORTAL =====
void handleCaptiveRedirect(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
  response->addHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
  request->send(response);
}

// ===== SETUP DEL SERVIDOR WEB =====
void setupWebServer() {
  // Rutas principales - usando lambdas para AsyncWebServer
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    handleRoot(request);
  });

  // Control LED - IMPORTANTE: NO usar /api/* porque colisiona con Espalexa
  server.on("/control/on", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiOn(request);
  });
  server.on("/control/off", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiOff(request);
  });
  server.on("/control/color", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiColor(request);
  });
  server.on("/control/brightness", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiBrightness(request);
  });
  server.on("/control/effect", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiEffect(request);
  });
  server.on("/control/status", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiStatus(request);
  });

  // Información y Configuración
  server.on("/control/info", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiInfo(request);
  });
  server.on("/control/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiSettings(request);
  });
  server.on("/control/pins", HTTP_GET, [](AsyncWebServerRequest *request){
    handleApiPins(request);
  });
  server.on("/control/settings/save", HTTP_POST, [](AsyncWebServerRequest *request){
    handleApiSettingsSave(request);
  });
  server.on("/control/settings/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    handleApiSettingsReset(request);
  });
  server.on("/control/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    handleApiReset(request);
  });

  // Rutas del portal WiFi (disponibles siempre, útiles en modo AP)
  server.on("/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    handleWifiScan(request);
  });
  server.on("/wifi/save", HTTP_POST, [](AsyncWebServerRequest *request){
    handleWifiSave(request);
  });
  server.on("/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request){
    handleWifiStatus(request);
  });

  // Captive portal: redirigir requests de detección de captive portal a /
  server.on("/generate_204", HTTP_GET, handleCaptiveRedirect);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect);
  server.on("/ncsi.txt", HTTP_GET, handleCaptiveRedirect);
  server.on("/connecttest.txt", HTTP_GET, handleCaptiveRedirect);
  server.on("/redirect", HTTP_GET, handleCaptiveRedirect);
  server.on("/success.txt", HTTP_GET, handleCaptiveRedirect);
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(204);
  });

  Serial.println("[WEB] Rutas HTTP configuradas");
}

// ===== INICIAR SERVIDOR WEB =====
void startWebServer() {
  if (getAlexaEnabled() && isWiFiConnected()) {
    // Alexa iniciará el servidor con espalexa.begin()
    Serial.println("[WEB] Servidor será iniciado por Espalexa");
    return;
  }

  server.onNotFound([](AsyncWebServerRequest *request){
    if (isInApMode()) {
      handleCaptiveRedirect(request);
      return;
    }
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("[WEB] Servidor HTTP iniciado en puerto 80");
}

// ===== CONFIGURAR HANDLER NOT FOUND (después de Espalexa) =====
void setupWebServerNotFound() {
  server.onNotFound([](AsyncWebServerRequest *request){
    if (isInApMode()) {
      handleCaptiveRedirect(request);
      return;
    }
    if (!espalexa.handleAlexaApiCall(request)) {
      if (SERIAL_DEBUG) {
        Serial.printf("[WEB] 404: %s %s\n", request->methodToString(), request->url().c_str());
      }
      request->send(404, "text/plain", "Not found");
    }
  });

  Serial.println("[WEB] Handler 404 configurado (Espalexa maneja peticiones automáticamente)");
}

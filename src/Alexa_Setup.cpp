#include "Librerias.h"

// ===== CALLBACK PARA ALEXA =====
// Esta función se ejecuta cuando Alexa envía comandos al dispositivo
void alexaCallback(EspalexaDevice* dev) {
  Serial.println("[ALEXA] *** CALLBACK EJECUTADO ***");

  if (dev == nullptr) {
    Serial.println("[ALEXA] ERROR: dev es nullptr");
    return;
  }

  // Verificar operaciones críticas antes de procesar comandos de Alexa
  if (evitarDeepSleep) {
    Serial.println("[ALEXA] Operación OTA en curso - ignorando comando");
    return;
  }

  // Obtener estado del dispositivo
  bool state = dev->getValue() > 0;
  uint8_t brightness = dev->getValue();  // 0-255

  Serial.println("\n[ALEXA] ========================================");
  Serial.printf("[ALEXA] Comando recibido de Alexa\n");
  Serial.printf("[ALEXA] Estado: %s\n", state ? "ON" : "OFF");
  Serial.printf("[ALEXA] Brillo: %d/255\n", brightness);

  if (state) {
    // ===== ENCENDER =====
    // Actualizar variables de estado inmediatamente para que /control/status refleje el cambio
    ledsOn = true;
    currentEffect = "static";
    currentBrightness = brightness;

    // Color de Alexa (si es 0,0,0 se mantiene el actual)
    uint8_t r = dev->getR();
    uint8_t g = dev->getG();
    uint8_t b = dev->getB();
    if (r > 0 || g > 0 || b > 0) {
      currentRed = r;
      currentGreen = g;
      currentBlue = b;
    }

    // Marcar acción pendiente para ejecutar en loop() (NO bloquear callback de Alexa)
    pendingAction = ACTION_TURN_ON;

    Serial.println("[ALEXA] ✅ LEDs encendidos (acción diferida)");
  } else {
    // ===== APAGAR =====
    ledsOn = false;
    currentEffect = "off";

    pendingAction = ACTION_TURN_OFF;

    Serial.println("[ALEXA] ✅ LEDs apagados (acción diferida)");
  }

  // Guardar estado con debounce (el loop() se encarga)
  scheduleSettingsSave();

  Serial.println("[ALEXA] ========================================\n");
}

// ===== FUNCIÓN PARA SINCRONIZAR ESTADO CON ALEXA =====
// Llamar esta función después de cambiar el estado desde la interfaz web
void syncStateWithAlexa() {
  if (!getAlexaEnabled()) return;

  EspalexaDevice* device = espalexa.getDevice(0);
  if (device != nullptr) {
    if (ledsOn) {
      device->setValue(currentBrightness);
      device->setColor(currentRed, currentGreen, currentBlue);
    } else {
      device->setValue(0);
    }
  }
}

// ===== CONFIGURACIÓN DE ALEXA =====
void setupAlexa() {
  Serial.println("[ALEXA] Iniciando configuración de Espalexa...");

  // Obtener nombre del dispositivo desde configuración
  const char* deviceName = getAlexaDeviceName();

  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║     REGISTRANDO DISPOSITIVO EN ALEXA               ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  Serial.printf("  📱 Nombre: '%s'\n", deviceName);
  Serial.printf("  🔧 MAC: %s\n", WiFi.macAddress().c_str());
  Serial.printf("  🌐 IP: %s\n", WiFi.localIP().toString().c_str());

  // Agregar dispositivo con soporte de color RGB
  // EspalexaDeviceType::color = dispositivo tipo Philips Hue con color
  espalexa.addDevice(deviceName, alexaCallback, EspalexaDeviceType::color);

  Serial.println("\n  ✅ Dispositivo registrado con soporte de COLOR RGB");
  Serial.println("  ⏳ Di: 'Alexa, buscar dispositivos'\n");

  // Iniciar Espalexa (esto también inicia el servidor web)
  espalexa.begin(&server);
  Serial.println("[ALEXA] Servidor web iniciado por Espalexa en puerto 80");

  // ===== SINCRONIZAR ESTADO INICIAL CON ALEXA =====
  // CRÍTICO: Informar a Alexa del estado actual del dispositivo
  EspalexaDevice* device = espalexa.getDevice(0);
  if (device != nullptr) {
    // Configurar estado inicial según variables globales
    if (ledsOn) {
      // Calcular brillo para Alexa (0-255)
      device->setValue(currentBrightness);
      // Establecer color RGB actual
      device->setColor(currentRed, currentGreen, currentBlue);
      Serial.printf("[ALEXA] Estado inicial sincronizado: ON, RGB(%d,%d,%d), Brillo=%d\n",
                    currentRed, currentGreen, currentBlue, currentBrightness);
    } else {
      // Dispositivo apagado
      device->setValue(0);
      Serial.println("[ALEXA] Estado inicial sincronizado: OFF");
    }
  } else {
    Serial.println("[ALEXA] ⚠️ ADVERTENCIA: No se pudo obtener dispositivo para sincronizar estado");
  }

  Serial.println("[ALEXA] ========================================");
  Serial.println("[ALEXA] ✅ Configuración completada");
  Serial.println("[ALEXA] ========================================");
  Serial.printf("[ALEXA] Dispositivo: '%s'\n", deviceName);
  Serial.println("[ALEXA] Tipo: Philips Hue (color RGB)");
  Serial.println("[ALEXA] ========================================");
  Serial.println("[ALEXA] COMANDOS DE VOZ DISPONIBLES:");
  Serial.printf("[ALEXA] - 'Alexa, enciende %s'\n", deviceName);
  Serial.printf("[ALEXA] - 'Alexa, apaga %s'\n", deviceName);
  Serial.printf("[ALEXA] - 'Alexa, pon %s en rojo'\n", deviceName);
  Serial.printf("[ALEXA] - 'Alexa, pon %s en azul'\n", deviceName);
  Serial.printf("[ALEXA] - 'Alexa, pon %s en verde'\n", deviceName);
  Serial.printf("[ALEXA] - 'Alexa, pon %s al 50%%'\n", deviceName);
  Serial.printf("[ALEXA] - 'Alexa, pon %s en amarillo al 75%%'\n", deviceName);
  Serial.println("[ALEXA] ========================================");
  Serial.println("[ALEXA] COLORES SOPORTADOS:");
  Serial.println("[ALEXA] Rojo, Azul, Verde, Amarillo, Naranja,");
  Serial.println("[ALEXA] Púrpura, Rosa, Cyan, Blanco, etc.");
  Serial.println("[ALEXA] ========================================");
  Serial.println("[ALEXA] ESTADO: HABILITADO - Esperando comandos...");
  Serial.println("[ALEXA] ========================================\n");
}

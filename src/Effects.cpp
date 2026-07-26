/*
 * Effects.ino - Efectos LED sin delay
 *
 * Este archivo contiene todos los efectos de iluminación LED
 * implementados de forma asíncrona usando millis() en lugar de delay().
 * Esto permite que el loop principal no se bloquee.
 *
 * Variables y librerías en Librerias.h
 */

#include "Librerias.h"

// ===== VARIABLES ESTÁTICAS DE EFECTOS (para poder resetearlas) =====
static unsigned long kitt2PauseStartTime = 0;
static bool kitt2IsPaused = false;
static int kitt2LastPosLeft = 0;
static int kitt2LastPosRight = 0;
static int kittLastPos = 0;
static uint8_t scannerCycleCount = 0;
static uint16_t scannerSpeed = 60;
static uint8_t meteorColorHue = 0;
static int meteorDirection = 1;
static uint8_t meteorCycleCount = 0;
static uint16_t meteorSpeed = 40;
static uint8_t claudeHue = 0;
static uint8_t claudePhase = 0;
static unsigned long claudeLastSparkle = 0;

// ===== FUNCIÓN AUXILIAR PARA EL ARCOÍRIS =====
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if(WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// ===== FUNCIÓN AUXILIAR PARA SENO (onda suave 0-255) =====
uint8_t sin8(uint8_t theta) {
    // Aproximación de seno usando tabla lookup optimizada
    // Devuelve valores de 0-255 para theta 0-255
    static const uint8_t sinTable[64] = {
        0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92,
        98, 103, 109, 115, 120, 126, 131, 136, 142, 147, 152, 157, 162, 167, 171, 176,
        180, 185, 189, 193, 197, 201, 205, 208, 212, 215, 219, 222, 225, 228, 231, 233,
        236, 238, 240, 242, 244, 246, 247, 249, 250, 251, 252, 253, 254, 254, 255, 255
    };

    uint8_t offset = theta & 0x3F; // 0-63

    if (theta < 64) {
        return sinTable[offset];
    } else if (theta < 128) {
        return sinTable[63 - offset];
    } else if (theta < 192) {
        return 255 - sinTable[offset];
    } else {
        return 255 - sinTable[63 - offset];
    }
}

// ===== EFECTO ARCOÍRIS (sin delay) =====
void effectRainbowAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= EFFECT_SPEED_RAINBOW) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, Wheel((i + effectStep) & 255));
            // CRÍTICO: smartYield() cada 50 LEDs para permitir que AsyncWebServer responda
            if (i % 50 == 0) smartYield();
        }
        strip.show();

        effectStep = (effectStep + 1) % 256;
    }
}

// ===== EFECTO KITT2 (Doble KITT - desde extremos al centro) =====
void effectFadeAsync() {
    unsigned long currentMillis = millis();

    // Si está en pausa, verificar si han pasado 750ms
    if (kitt2IsPaused) {
        if (currentMillis - kitt2PauseStartTime >= 750) {
            kitt2IsPaused = false;
            effectDirection = -1;
        } else {
            return;
        }
    }

    uint16_t numLeds = getActiveLedCount();
    // Velocidad aumentada en 40% (60% del intervalo original)
    uint16_t dynamicSpeed = max(30, (3000 * 9 * 6) / (numLeds * 100)); // Mínimo 30ms para estabilidad

    if (currentMillis - lastEffectUpdate >= dynamicSpeed) {
        lastEffectUpdate = currentMillis;

        int eyeSize = 4;
        int halfLeds = numLeds / 2;
        int posLeft, posRight;

        // Borrar solo regiones anteriores con smartYield
        for(int i = max(0, kitt2LastPosLeft - 12); i < min((int)numLeds, kitt2LastPosLeft + 16); i++) {
            strip.setPixelColor(i, 0);
            if (i % 50 == 0) smartYield();
        }
        for(int i = max(0, kitt2LastPosRight - 12); i < min((int)numLeds, kitt2LastPosRight + 16); i++) {
            strip.setPixelColor(i, 0);
            if (i % 50 == 0) smartYield();
        }

        if (effectDirection == 1) {
            posLeft = effectStep;
            posRight = numLeds - effectStep - eyeSize - 1;

            if (effectStep >= halfLeds - eyeSize) {
                kitt2PauseStartTime = currentMillis;
                kitt2IsPaused = true;
            }
        } else {
            // Regresando a los extremos
            posLeft = halfLeds - eyeSize - (effectStep - (halfLeds - eyeSize));
            posRight = halfLeds + eyeSize + (effectStep - (halfLeds - eyeSize));

            if (posLeft <= 0 || posRight >= numLeds - eyeSize - 1) {
                effectDirection = 1;
                effectStep = 0;
            }
        }

        // Dibujar LED IZQUIERDO con rastro
        if (effectDirection == 1) {
            // Yendo hacia el centro: rastro atrás (izquierda)
            for(int i = 1; i <= 8; i++) {
                int trailPos = posLeft - i;
                if (trailPos >= 0) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        } else {
            // Regresando a extremo: rastro adelante (derecha)
            for(int i = 1; i <= 8; i++) {
                int trailPos = posLeft + eyeSize + i - 1;
                if (trailPos < numLeds) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        }

        // Ojo principal izquierdo
        for(int i = 0; i < eyeSize; i++) {
            if (posLeft + i < numLeds) {
                strip.setPixelColor(posLeft + i, strip.Color(currentRed, currentGreen, currentBlue));
            }
        }

        // Rastro delantero izquierdo (corto)
        if (effectDirection == 1) {
            // Yendo hacia el centro: rastro corto adelante (derecha)
            for(int i = 1; i <= 3; i++) {
                int trailPos = posLeft + eyeSize + i - 1;
                if (trailPos < numLeds) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        } else {
            // Regresando: rastro corto atrás (izquierda)
            for(int i = 1; i <= 3; i++) {
                int trailPos = posLeft - i;
                if (trailPos >= 0) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        }

        // Dibujar LED DERECHO con rastro
        if (effectDirection == 1) {
            // Yendo hacia el centro: rastro atrás (derecha)
            for(int i = 1; i <= 8; i++) {
                int trailPos = posRight + eyeSize + i;
                if (trailPos < numLeds) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        } else {
            // Regresando a extremo: rastro adelante (izquierda)
            for(int i = 1; i <= 8; i++) {
                int trailPos = posRight - i;
                if (trailPos >= 0) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        }

        // Ojo principal derecho
        for(int i = 0; i < eyeSize; i++) {
            if (posRight + i < numLeds && posRight + i >= 0) {
                strip.setPixelColor(posRight + i, strip.Color(currentRed, currentGreen, currentBlue));
            }
        }

        // Rastro delantero derecho (corto)
        if (effectDirection == 1) {
            // Yendo hacia el centro: rastro corto adelante (izquierda)
            for(int i = 1; i <= 3; i++) {
                int trailPos = posRight - i;
                if (trailPos >= 0) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        } else {
            // Regresando: rastro corto atrás (derecha)
            for(int i = 1; i <= 3; i++) {
                int trailPos = posRight + eyeSize + i - 1;
                if (trailPos < numLeds) {
                    uint8_t fade = 255 / (i + 2);
                    strip.setPixelColor(trailPos, strip.Color(
                        (currentRed * fade) / 255,
                        (currentGreen * fade) / 255,
                        (currentBlue * fade) / 255
                    ));
                }
            }
        }

        strip.show();

        kitt2LastPosLeft = posLeft;
        kitt2LastPosRight = posRight;
        effectStep++;
    }
}

// ===== EFECTO STROBE (sin delay) =====
void effectStrobeAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= EFFECT_SPEED_STROBE) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();
        uint32_t color = (effectStep % 2 == 0) ? strip.Color(255, 255, 255) : strip.Color(0, 0, 0);

        // Procesar todos los LEDs con smartYield() para no bloquear AsyncWebServer
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, color);
            if (i % 50 == 0) smartYield();
        }
        strip.show();

        effectStep = (effectStep + 1) % (EFFECT_STROBE_FLASH_COUNT * 2);
    }
}

// ===== EFECTO THEATER CHASE (sin delay) =====
void effectTheaterAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= EFFECT_SPEED_THEATER) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Apagar todos los LEDs con smartYield()
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, 0);
            if (i % 50 == 0) smartYield();
        }

        // Encender cada tercer LED
        for(int i = 0; i < numLeds; i += 3) {
            int ledIndex = (i + effectStep) % numLeds;
            strip.setPixelColor(ledIndex, strip.Color(currentRed, currentGreen, currentBlue));
        }

        strip.show();
        effectStep = (effectStep + 1) % 3;
    }
}

// ===== EFECTO FUEGO (sin delay) =====
void effectFireAsync() {
    unsigned long currentMillis = millis();

    // Intervalo aleatorio para simular parpadeo del fuego
    if (effectInterval == 0) {
        effectInterval = random(EFFECT_SPEED_FIRE_MIN, EFFECT_SPEED_FIRE_MAX);
    }

    if (currentMillis - lastEffectUpdate >= effectInterval) {
        lastEffectUpdate = currentMillis;
        effectInterval = 0; // Reset para generar nuevo intervalo aleatorio

        uint16_t numLeds = getActiveLedCount();
        for(int i = 0; i < numLeds; i++) {
            int flicker = random(150, 255);
            int red = flicker;
            int green = flicker / 3;
            int blue = 0;
            strip.setPixelColor(i, strip.Color(red, green, blue));
            if (i % 50 == 0) smartYield();
        }
        strip.show();
    }
}

// ===== EFECTO SPARKLE (nuevo) =====
void effectSparkleAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= 100) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Apagar todos con yield periódico
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, strip.Color(currentRed/4, currentGreen/4, currentBlue/4));
            if (i % 50 == 0) smartYield();
        }

        // Encender LEDs aleatorios
        for(int i = 0; i < 3; i++) {
            int randomLed = random(numLeds);
            strip.setPixelColor(randomLed, strip.Color(currentRed, currentGreen, currentBlue));
        }

        strip.show();
    }
}

// ===== EFECTO WAVE (nuevo) =====
void effectWaveAsync() {
    unsigned long currentMillis = millis();

    // Intervalo aumentado para dejar tiempo al SDK WiFi entre frames
    if (currentMillis - lastEffectUpdate >= 60) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Usar sin8() (entero 0-255) en lugar de sin() de float para reducir carga
        for(int i = 0; i < numLeds; i++) {
            uint8_t angle = effectStep + (i * 10);
            uint8_t brightness = sin8(angle);
            uint8_t r = (currentRed * brightness) / 255;
            uint8_t g = (currentGreen * brightness) / 255;
            uint8_t b = (currentBlue * brightness) / 255;
            strip.setPixelColor(i, strip.Color(r, g, b));
            if (i % 50 == 0) smartYield();
        }

        strip.show();
        effectStep = (effectStep + 1) % 256;
    }
}

// ===== EFECTO RUNNING LIGHT (nuevo) =====
void effectRunningAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= 50) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Apagar todos con yield periódico
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, 0);
            if (i % 50 == 0) smartYield();
        }

        // Encender LED actual y vecinos con fade
        for(int i = -2; i <= 2; i++) {
            int ledIndex = (effectStep + i + numLeds) % numLeds;
            uint8_t brightness = 255 - abs(i) * 80;
            uint8_t r = (currentRed * brightness) / 255;
            uint8_t g = (currentGreen * brightness) / 255;
            uint8_t b = (currentBlue * brightness) / 255;
            strip.setPixelColor(ledIndex, strip.Color(r, g, b));
        }

        strip.show();
        effectStep = (effectStep + 1) % numLeds;
    }
}

// ===== EFECTO BOUNCING BALLS (Física realista simplificada) =====
// Variables estáticas para bouncing balls
static float ballHeight[3];
static float ballVelocity[3];
static int ballPosition[3];
static unsigned long ballLastTime[3];
static bool ballsInitialized = false;

void effectBouncingBallsAsync() {
    unsigned long currentMillis = millis();

    // Inicializar bolas la primera vez
    if (!ballsInitialized) {
        for(int i = 0; i < 3; i++) {
            ballHeight[i] = 1.0f;
            ballVelocity[i] = 0.0f;
            ballPosition[i] = 0;
            ballLastTime[i] = currentMillis;
        }
        ballsInitialized = true;
    }

    // Intervalo aumentado: el cálculo de floats + strip.show() consume mucho tiempo
    if (currentMillis - lastEffectUpdate >= 40) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Limpiar LEDs con yield periódico
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, 0);
            if (i % 50 == 0) smartYield();
        }

        // Física de cada bola
        const float gravity = 9.81f;
        const float dampening[3] = {0.90f, 0.85f, 0.80f};
        const float maxVelocity = 4.43f; // sqrt(2 * gravity * 1.0)

        for(int i = 0; i < 3; i++) {
            float timeSinceBounce = (currentMillis - ballLastTime[i]) / 1000.0f;
            ballHeight[i] = (0.5f * -gravity * timeSinceBounce * timeSinceBounce) +
                            (ballVelocity[i] * timeSinceBounce);

            if (ballHeight[i] < 0.0f) {
                ballHeight[i] = 0.0f;
                ballVelocity[i] = dampening[i] * ballVelocity[i];
                ballLastTime[i] = currentMillis;

                if (ballVelocity[i] < 0.01f) {
                    ballVelocity[i] = maxVelocity;
                }
            }

            // Limitar altura a [0, 1] para evitar posiciones fuera de rango
            if (ballHeight[i] > 1.0f) ballHeight[i] = 1.0f;

            int pos = (int)(ballHeight[i] * (numLeds - 1) + 0.5f);
            if (pos < 0) pos = 0;
            if (pos >= (int)numLeds) pos = numLeds - 1;
            ballPosition[i] = pos;

            // Colores diferentes para cada bola
            if (i == 0) strip.setPixelColor(ballPosition[i], strip.Color(255, 0, 0));
            else if (i == 1) strip.setPixelColor(ballPosition[i], strip.Color(0, 255, 0));
            else strip.setPixelColor(ballPosition[i], strip.Color(0, 0, 255));
        }

        strip.show();
    }
}

// ===== EFECTO KITT (Knight Rider - Auto Fantástico) =====
void effectKittAsync() {
    unsigned long currentMillis = millis();

    // Calcular velocidad dinámica basada en cantidad de LEDs
    // Más LEDs = movimiento más rápido para mantener velocidad visual constante
    // Fórmula: velocidad base (3000ms) dividido entre número de LEDs
    uint16_t numLeds = getActiveLedCount();
    uint16_t dynamicSpeed = max(25, 3000 / numLeds); // Mínimo 25ms para no saturar WiFi

    if (currentMillis - lastEffectUpdate >= dynamicSpeed) {
        lastEffectUpdate = currentMillis;

        // OPTIMIZACIÓN: Solo apagar la región donde estaba el LED anterior
        // Esto evita recorrer todos los LEDs en cada frame (crítico para AsyncWebServer)
        for(int i = max(0, kittLastPos - 12); i < min((int)numLeds, kittLastPos + 12); i++) {
            strip.setPixelColor(i, 0);
        }

        // Configuración del "ojo" de KITT (más grande y con más fade)
        int eyeSize = 4;  // Ojo principal más grande
        int pos = effectStep;

        // Movimiento de ida y vuelta
        if (effectDirection == 1) {
            pos = effectStep;
            if (effectStep >= numLeds - eyeSize - 1) {
                effectDirection = -1;
            }
        } else {
            pos = numLeds - effectStep - eyeSize - 1;
            if (effectStep >= numLeds - eyeSize - 1) {
                effectDirection = 1;
                effectStep = 0;
            }
        }

        // Dibujar rastro LARGO de fade (efecto KITT auténtico)
        // Rastro trasero (hasta 8 LEDs con degradado)
        for(int i = 1; i <= 8; i++) {
            int trailPos = pos - i;
            if (trailPos >= 0) {
                uint8_t fade = 255 / (i + 2); // Degradado exponencial
                strip.setPixelColor(trailPos, strip.Color(
                    (currentRed * fade) / 255,
                    (currentGreen * fade) / 255,
                    (currentBlue * fade) / 255
                ));
            }
        }

        // Dibujar el "ojo" principal brillante (4 LEDs)
        for(int i = 0; i < eyeSize; i++) {
            if (pos + i < numLeds) {
                strip.setPixelColor(pos + i, strip.Color(currentRed, currentGreen, currentBlue));
            }
        }

        // Rastro delantero (más corto, 3 LEDs)
        for(int i = 1; i <= 3; i++) {
            int trailPos = pos + eyeSize + i - 1;
            if (trailPos < numLeds) {
                uint8_t fade = 255 / (i + 2);
                strip.setPixelColor(trailPos, strip.Color(
                    (currentRed * fade) / 255,
                    (currentGreen * fade) / 255,
                    (currentBlue * fade) / 255
                ));
            }
        }

        strip.show();

        // Guardar posición actual para optimización en próximo frame
        kittLastPos = pos;
        effectStep++;
    }
}

// ===== EFECTO COLOR WIPE (Llenar gradualmente) =====
void effectColorWipeAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= 50) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        strip.setPixelColor(effectStep, strip.Color(currentRed, currentGreen, currentBlue));
        strip.show();

        effectStep++;
        if (effectStep >= numLeds) {
            effectStep = 0;
            // Cambiar a color aleatorio cada ciclo
            if (random(100) > 50) {
                // Apagar todos para próximo ciclo
                for(int i = 0; i < numLeds; i++) {
                    strip.setPixelColor(i, 0);
                    if (i % 50 == 0) smartYield();
                }
                strip.show();
            }
        }
    }
}

// ===== EFECTO TWINKLE RANDOM (Parpadeo aleatorio) =====
void effectTwinkleAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= 100) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Fade out todos los LEDs con yield periódico
        for(int i = 0; i < numLeds; i++) {
            uint32_t color = strip.getPixelColor(i);
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            r = r > 10 ? r - 10 : 0;
            g = g > 10 ? g - 10 : 0;
            b = b > 10 ? b - 10 : 0;

            strip.setPixelColor(i, strip.Color(r, g, b));
            if (i % 50 == 0) smartYield();
        }

        // Encender 2-3 LEDs aleatorios
        int count = random(2, 4);
        for(int i = 0; i < count; i++) {
            int pixel = random(numLeds);
            strip.setPixelColor(pixel, strip.Color(currentRed, currentGreen, currentBlue));
        }

        strip.show();
    }
}

// ===== EFECTO RUNNING LIGHTS (Onda sinusoidal) =====
void effectRunningLightsAsync() {
    unsigned long currentMillis = millis();

    // Intervalo aumentado para reducir carga de punto flotante + strip.show()
    if (currentMillis - lastEffectUpdate >= 60) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Usar sin8() (entero 0-255) en lugar de sin() de float
        for(int i = 0; i < numLeds; i++) {
            uint8_t angle = (i + effectStep) % 256;
            uint8_t brightness = sin8(angle);
            uint8_t r = (currentRed * brightness) / 255;
            uint8_t g = (currentGreen * brightness) / 255;
            uint8_t b = (currentBlue * brightness) / 255;
            strip.setPixelColor(i, strip.Color(r, g, b));
            if (i % 50 == 0) smartYield();
        }

        strip.show();
        effectStep = (effectStep + 1) % 256;
    }
}

// ===== EFECTO METEOR RAIN (Lluvia de meteoritos con aceleración) =====
void effectMeteorAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= meteorSpeed) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Fade all LEDs con smartYield
        for(int i = 0; i < numLeds; i++) {
            uint32_t color = strip.getPixelColor(i);
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            r = r > 20 ? r - 20 : 0;
            g = g > 20 ? g - 20 : 0;
            b = b > 20 ? b - 20 : 0;

            strip.setPixelColor(i, strip.Color(r, g, b));
            if (i % 50 == 0) smartYield();
        }

        // Obtener color actual del ciclo usando Wheel
        uint32_t meteorColor = Wheel(meteorColorHue);
        uint8_t meteorR = (meteorColor >> 16) & 0xFF;
        uint8_t meteorG = (meteorColor >> 8) & 0xFF;
        uint8_t meteorB = meteorColor & 0xFF;

        // Dibujar meteoro con cola larga (8 LEDs)
        int meteorSize = 8;
        int pos;

        if (meteorDirection == 1) {
            // Yendo hacia adelante
            pos = effectStep;
        } else {
            // Regresando
            pos = numLeds - 1 - effectStep;
        }

        for(int i = 0; i < meteorSize; i++) {
            int trailPos;
            if (meteorDirection == 1) {
                trailPos = pos - i;
            } else {
                trailPos = pos + i;
            }

            if (trailPos >= 0 && trailPos < numLeds) {
                uint8_t brightness = 255 - (i * 32);
                strip.setPixelColor(trailPos, strip.Color(
                    (meteorR * brightness) / 255,
                    (meteorG * brightness) / 255,
                    (meteorB * brightness) / 255
                ));
            }
        }

        strip.show();
        effectStep++;

        // Cambiar dirección y color al completar un recorrido
        if (effectStep >= numLeds) {
            effectStep = 0;
            meteorDirection = -meteorDirection;

            // Cambiar de color y acelerar solo cuando completa un ciclo (ida y vuelta)
            if (meteorDirection == 1) {
                meteorCycleCount++;
                meteorColorHue += 32; // Avanza ~1/8 del espectro por ciclo completo

                // Acelerar progresivamente: reduce 2ms por ciclo hasta velocidad mínima
                // CRÍTICO: no bajar de 30ms para no saturar strip.show() y matar WiFi
                if (meteorCycleCount <= 15 && meteorSpeed > 30) {
                    meteorSpeed -= 2;
                }

                // Después de 15 ciclos acelerando, hacer 5 ciclos a máxima velocidad
                // Después de 20 ciclos totales, resetear a velocidad inicial
                if (meteorCycleCount >= 20) {
                    meteorSpeed = 40;
                    meteorCycleCount = 0;
                }
            }
        }
    }
}

// ===== EFECTO POLICE (Luces de policía/emergencia) =====
void effectPoliceAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= 80) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();
        int half = numLeds / 2;

        // Alternar entre rojo y azul
        if (effectStep < 6) {
            // Flash rojo lado izquierdo
            for(int i = 0; i < half; i++) {
                if (effectStep % 2 == 0) {
                    strip.setPixelColor(i, strip.Color(255, 0, 0));
                } else {
                    strip.setPixelColor(i, 0);
                }
                if (i % 50 == 0) smartYield();
            }
            // Apagar lado derecho
            for(int i = half; i < (int)numLeds; i++) {
                strip.setPixelColor(i, 0);
                if (i % 50 == 0) smartYield();
            }
        } else {
            // Flash azul lado derecho
            for(int i = 0; i < half; i++) {
                strip.setPixelColor(i, 0);
                if (i % 50 == 0) smartYield();
            }
            for(int i = half; i < (int)numLeds; i++) {
                if (effectStep % 2 == 0) {
                    strip.setPixelColor(i, strip.Color(0, 0, 255));
                } else {
                    strip.setPixelColor(i, 0);
                }
                if (i % 50 == 0) smartYield();
            }
        }

        strip.show();
        effectStep = (effectStep + 1) % 12;
    }
}

// ===== EFECTO RAINBOW LOOP (Arcoíris viajero) =====
void effectRainbowLoopAsync() {
    unsigned long currentMillis = millis();

    // Intervalo aumentado: 20ms + strip.show() deja poco tiempo al SDK WiFi
    if (currentMillis - lastEffectUpdate >= 35) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        for(int i = 0; i < numLeds; i++) {
            int pixelHue = (effectStep + (i * 256 / numLeds)) & 255;
            strip.setPixelColor(i, Wheel(pixelHue));
            if (i % 50 == 0) smartYield();
        }

        strip.show();
        effectStep = (effectStep + 1) % 256;
    }
}

// ===== EFECTO PULSE (Pulso de brillo) =====
void effectPulseAsync() {
    unsigned long currentMillis = millis();

    // Intervalo aumentado: 15ms + strip.show() es demasiado agresivo para ESP8266
    if (currentMillis - lastEffectUpdate >= 35) {
        lastEffectUpdate = currentMillis;

        if (effectDirection == 1) {
            effectBrightness += 5;
            if (effectBrightness >= 255) {
                effectBrightness = 255;
                effectDirection = -1;
            }
        } else {
            effectBrightness -= 5;
            if (effectBrightness <= 0) {
                effectBrightness = 0;
                effectDirection = 1;
            }
        }

        uint16_t numLeds = getActiveLedCount();
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, strip.Color(
                (currentRed * effectBrightness) / 255,
                (currentGreen * effectBrightness) / 255,
                (currentBlue * effectBrightness) / 255
            ));
            if (i % 50 == 0) smartYield();
        }

        strip.show();
    }
}

// ===== EFECTO THEATER CHASE RAINBOW (Theater con arcoíris) =====
void effectTheaterRainbowAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= 100) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Apagar todos con yield periódico
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, 0);
            if (i % 50 == 0) smartYield();
        }

        // Encender cada tercer LED con color arcoíris
        for(int i = 0; i < numLeds; i += 3) {
            int ledIndex = (i + (effectStep % 3)) % numLeds;
            int pixelHue = (effectStep * 10 + i * 256 / numLeds) & 255;
            strip.setPixelColor(ledIndex, Wheel(pixelHue));
        }

        strip.show();
        effectStep++;
    }
}

// ===== EFECTO SNOW SPARKLE (Brillo de nieve) =====
void effectSnowSparkleAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= 50) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();

        // Establecer color de fondo (tono azul frío para nieve) con yield
        for(int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, strip.Color(
                currentRed / 10,
                currentGreen / 10,
                currentBlue / 10
            ));
            if (i % 50 == 0) smartYield();
        }

        // Agregar destello blanco aleatorio
        if (random(100) > 70) {
            int pixel = random(numLeds);
            strip.setPixelColor(pixel, strip.Color(255, 255, 255));
        }

        strip.show();
    }
}

// ===== EFECTO SCANNER (Escáner bidireccional mejorado) =====
void effectScannerAsync() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEffectUpdate >= scannerSpeed) {
        lastEffectUpdate = currentMillis;

        // Fade out todos los LEDs con smartYield
        for(int i = 0; i < getActiveLedCount(); i++) {
            uint32_t color = strip.getPixelColor(i);
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;

            r = r > 15 ? r - 15 : 0;
            g = g > 15 ? g - 15 : 0;
            b = b > 15 ? b - 15 : 0;

            strip.setPixelColor(i, strip.Color(r, g, b));
            if (i % 50 == 0) smartYield();
        }

        // Calcular posición
        int pos;
        if (effectDirection == 1) {
            pos = effectStep;
            if (effectStep >= getActiveLedCount() - 1) {
                effectDirection = -1;
            }
        } else {
            pos = getActiveLedCount() - 1 - effectStep;
            if (effectStep >= getActiveLedCount() - 1) {
                effectDirection = 1;
                effectStep = 0;

                // Completó un ciclo (ida y vuelta)
                scannerCycleCount++;

                // Acelerar progresivamente: reduce 2ms por ciclo hasta velocidad mínima
                // CRÍTICO: no bajar de 30ms para no saturar strip.show() y matar WiFi
                if (scannerCycleCount <= 20 && scannerSpeed > 30) {
                    scannerSpeed -= 2;
                }

                // Después de 20 ciclos acelerando, hacer 5 ciclos a máxima velocidad
                // Después de 25 ciclos totales, resetear a velocidad inicial
                if (scannerCycleCount >= 25) {
                    scannerSpeed = 60;
                    scannerCycleCount = 0;
                }
            }
        }

        // Dibujar LED brillante con efecto de cola mejorado
        strip.setPixelColor(pos, strip.Color(currentRed, currentGreen, currentBlue));

        // Cola larga (6 LEDs detrás con degradado suave)
        if (effectDirection == 1) {
            // Avanzando hacia la derecha, cola a la izquierda
            if (pos > 0) strip.setPixelColor(pos - 1, strip.Color(currentRed*4/5, currentGreen*4/5, currentBlue*4/5));      // 80%
            if (pos > 1) strip.setPixelColor(pos - 2, strip.Color(currentRed*3/5, currentGreen*3/5, currentBlue*3/5));      // 60%
            if (pos > 2) strip.setPixelColor(pos - 3, strip.Color(currentRed*2/5, currentGreen*2/5, currentBlue*2/5));      // 40%
            if (pos > 3) strip.setPixelColor(pos - 4, strip.Color(currentRed/5, currentGreen/5, currentBlue/5));            // 20%
            if (pos > 4) strip.setPixelColor(pos - 5, strip.Color(currentRed/8, currentGreen/8, currentBlue/8));            // 12.5%
            if (pos > 5) strip.setPixelColor(pos - 6, strip.Color(currentRed/16, currentGreen/16, currentBlue/16));         // 6.25%
        } else {
            // Avanzando hacia la izquierda, cola a la derecha
            if (pos < getActiveLedCount() - 1) strip.setPixelColor(pos + 1, strip.Color(currentRed*4/5, currentGreen*4/5, currentBlue*4/5));
            if (pos < getActiveLedCount() - 2) strip.setPixelColor(pos + 2, strip.Color(currentRed*3/5, currentGreen*3/5, currentBlue*3/5));
            if (pos < getActiveLedCount() - 3) strip.setPixelColor(pos + 3, strip.Color(currentRed*2/5, currentGreen*2/5, currentBlue*2/5));
            if (pos < getActiveLedCount() - 4) strip.setPixelColor(pos + 4, strip.Color(currentRed/5, currentGreen/5, currentBlue/5));
            if (pos < getActiveLedCount() - 5) strip.setPixelColor(pos + 5, strip.Color(currentRed/8, currentGreen/8, currentBlue/8));
            if (pos < getActiveLedCount() - 6) strip.setPixelColor(pos + 6, strip.Color(currentRed/16, currentGreen/16, currentBlue/16));
        }

        strip.show();
        effectStep++;
    }
}

// ===== EFECTO CLAUDE IA (Pensamiento de IA - Mágico) =====
void effectClaudeAsync() {
    unsigned long currentMillis = millis();

    // Intervalo aumentado: este efecto es el más pesado (random, Wheel, fades)
    if (currentMillis - lastEffectUpdate >= 50) {
        lastEffectUpdate = currentMillis;

        uint16_t numLeds = getActiveLedCount();
        int center = numLeds / 2;

        // Fase 0-1: Ondas pulsantes desde el centro
        // Fase 2: Explosión de partículas
        // Fase 3: Respiración con arcoíris

        if (claudePhase == 0 || claudePhase == 1) {
            // ONDAS PULSANTES desde el centro
            // Fade suave de todos los LEDs
            for(int i = 0; i < numLeds; i++) {
                uint32_t color = strip.getPixelColor(i);
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;

                r = r > 8 ? r - 8 : 0;
                g = g > 8 ? g - 8 : 0;
                b = b > 8 ? b - 8 : 0;

                strip.setPixelColor(i, strip.Color(r, g, b));
                if (i % 50 == 0) smartYield();
            }

            // Calcular onda expansiva desde el centro
            int wavePos = effectStep % 40;
            uint32_t waveColor = Wheel(claudeHue);
            uint8_t waveR = (waveColor >> 16) & 0xFF;
            uint8_t waveG = (waveColor >> 8) & 0xFF;
            uint8_t waveB = waveColor & 0xFF;

            // Onda hacia la derecha
            for(int i = 0; i < 6; i++) {
                int pos = center + wavePos - i;
                if (pos >= 0 && pos < numLeds) {
                    uint8_t brightness = 255 - (i * 42);
                    strip.setPixelColor(pos, strip.Color(
                        (waveR * brightness) / 255,
                        (waveG * brightness) / 255,
                        (waveB * brightness) / 255
                    ));
                }
            }

            // Onda hacia la izquierda (simétrica)
            for(int i = 0; i < 6; i++) {
                int pos = center - wavePos + i;
                if (pos >= 0 && pos < numLeds) {
                    uint8_t brightness = 255 - (i * 42);
                    strip.setPixelColor(pos, strip.Color(
                        (waveR * brightness) / 255,
                        (waveG * brightness) / 255,
                        (waveB * brightness) / 255
                    ));
                }
            }

            // Partículas aleatorias (neuronas activándose)
            if (currentMillis - claudeLastSparkle >= 100) {
                claudeLastSparkle = currentMillis;
                int sparklePos = random(numLeds);
                uint32_t sparkleColor = Wheel(random(255));
                strip.setPixelColor(sparklePos, sparkleColor);
            }

        } else if (claudePhase == 2) {
            // EXPLOSIÓN de partículas brillantes
            for(int i = 0; i < numLeds; i++) {
                if (random(100) < 30) {
                    uint32_t burstColor = Wheel(claudeHue + random(60));
                    strip.setPixelColor(i, burstColor);
                } else {
                    strip.setPixelColor(i, 0);
                }
                if (i % 50 == 0) smartYield();
            }

        } else {
            // RESPIRACIÓN con arcoíris suave
            uint8_t breath = (sin8(effectStep * 3) / 2) + 128;

            for(int i = 0; i < numLeds; i++) {
                uint8_t hue = claudeHue + (i * 255 / numLeds);
                uint32_t color = Wheel(hue);
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;

                strip.setPixelColor(i, strip.Color(
                    (r * breath) / 255,
                    (g * breath) / 255,
                    (b * breath) / 255
                ));
                if (i % 50 == 0) smartYield();
            }
        }

        strip.show();
        effectStep++;

        // Cambiar de fase cada 80 steps
        if (effectStep >= 80) {
            effectStep = 0;
            claudePhase = (claudePhase + 1) % 4;
            claudeHue += 21; // Cambio suave de color base
        }
    }
}

// ===== MANEJADOR PRINCIPAL DE EFECTOS =====
// ===== NOMBRES DE EFECTO VÁLIDOS =====
// Mantener sincronizado con el dispatcher de updateEffects() de abajo.
// Sin esta validación un nombre inexistente se aceptaba, se guardaba en
// flash y sobrevivía al reinicio dejando la tira encendida sin efecto.
static const char* const EFFECT_NAMES[] = {
    "static", "off", "rainbow", "rainbowloop", "pulse", "kitt", "kitt2",
    "fire", "wave", "sparkle", "twinkle", "snow", "meteor", "strobe",
    "police", "scanner", "bounce", "theater", "theaterrainbow", "running",
    "runninglights", "wipe", "claude"
};

bool isValidEffect(const String& effect) {
    for (uint8_t i = 0; i < sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]); i++) {
        if (effect == EFFECT_NAMES[i]) return true;
    }
    return false;
}

void updateEffects() {
    // Todos los efectos usan SOLO millis() para timing (no delay, no yield)
    if (!ledsOn || currentEffect == "static" || currentEffect == "off") {
        return;
    }

    if (currentEffect == "rainbow") {
        effectRainbowAsync();
    } else if (currentEffect == "kitt2") {
        effectFadeAsync();
    } else if (currentEffect == "strobe") {
        effectStrobeAsync();
    } else if (currentEffect == "theater") {
        effectTheaterAsync();
    } else if (currentEffect == "fire") {
        effectFireAsync();
    } else if (currentEffect == "sparkle") {
        effectSparkleAsync();
    } else if (currentEffect == "wave") {
        effectWaveAsync();
    } else if (currentEffect == "running") {
        effectRunningAsync();
    } else if (currentEffect == "bounce") {
        effectBouncingBallsAsync();
    } else if (currentEffect == "kitt") {
        effectKittAsync();
    } else if (currentEffect == "wipe") {
        effectColorWipeAsync();
    } else if (currentEffect == "twinkle") {
        effectTwinkleAsync();
    } else if (currentEffect == "runninglights") {
        effectRunningLightsAsync();
    } else if (currentEffect == "meteor") {
        effectMeteorAsync();
    } else if (currentEffect == "police") {
        effectPoliceAsync();
    } else if (currentEffect == "rainbowloop") {
        effectRainbowLoopAsync();
    } else if (currentEffect == "pulse") {
        effectPulseAsync();
    } else if (currentEffect == "theaterrainbow") {
        effectTheaterRainbowAsync();
    } else if (currentEffect == "snow") {
        effectSnowSparkleAsync();
    } else if (currentEffect == "scanner") {
        effectScannerAsync();
    } else if (currentEffect == "claude") {
        effectClaudeAsync();
    }
}

// ===== RESETEAR VARIABLES DE EFECTOS =====
void resetEffectVariables() {
    lastEffectUpdate = 0;
    effectStep = 0;
    effectBrightness = 0;
    effectDirection = 1;
    effectInterval = 0;

    // Reset bouncing balls
    extern bool ballsInitialized;
    ballsInitialized = false;

    // Reset KITT2 variables
    kitt2PauseStartTime = 0;
    kitt2IsPaused = false;
    kitt2LastPosLeft = 0;
    kitt2LastPosRight = 0;

    // Reset KITT variable
    kittLastPos = 0;

    // Reset Scanner variables
    scannerCycleCount = 0;
    scannerSpeed = 60;

    // Reset Meteor variables
    meteorColorHue = 0;
    meteorDirection = 1;
    meteorCycleCount = 0;
    meteorSpeed = 40;

    // Reset Claude IA variables
    claudeHue = 0;
    claudePhase = 0;
    claudeLastSparkle = 0;
}

// ===== FUNCIÓN PARA CAMBIAR EFECTO =====
void setEffect(String effect) {
    currentEffect = effect;
    resetEffectVariables();

    // Restaurar brillo si venimos de kitt2
    if (effect != "kitt2") {
        strip.setBrightness(currentBrightness);
    }

    if (SERIAL_DEBUG) {
        Serial.printf("[Effects] Efecto cambiado a: %s\n", effect.c_str());
    }
}

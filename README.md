# Led_Control_ESP

Control de tiras LED direccionables (WS2812B / NeoPixel) con **ESP8266** o **ESP32**:
página web propia, 22 efectos, integración con Alexa y actualización por aire (OTA).

La configuración WiFi **no se compila dentro del firmware**. Al encender por primera
vez el dispositivo crea su propia red y desde ahí eliges tu WiFi, el pin de datos y
si quieres Alexa. Puedes usar los binarios ya compilados o compilarlo tú mismo.

---

## ⚡ Antes de nada: seguridad eléctrica

Si es tu primera vez con tiras LED, lee esto. No es burocracia: la mayoría de
tiras quemadas y placas muertas vienen de estos cinco errores.

### 1. No alimentes la tira desde el USB de la placa

Es el error más común. El puerto USB de un PC da como mucho 0,5 A, y el regulador
de la placa aguanta menos todavía. Una tira de 30 LEDs en blanco al máximo ya pide
más que eso: intentarlo recalienta el regulador, reinicia la placa y puede dañar
el puerto USB del ordenador.

**La tira se alimenta siempre desde una fuente externa de 5 V.**

### 2. Calcula la fuente antes de comprarla

Cada LED WS2812B consume hasta **60 mA** en blanco al 100% de brillo (20 mA por
cada canal de color). La cuenta es:

```
Corriente máxima (A) = nº de LEDs × 0,06
```

| LEDs | Consumo máximo | Fuente recomendada (+30% margen) |
|------|----------------|----------------------------------|
| 30   | 1,8 A          | 5 V 3 A                          |
| 60   | 3,6 A          | 5 V 5 A                          |
| 100  | 6,0 A          | 5 V 8 A                          |
| 150  | 9,0 A          | 5 V 12 A                         |

En la práctica casi nunca se alcanza ese máximo (harían falta todos los LEDs en
blanco puro a tope), pero **la fuente se dimensiona para el peor caso**. Una fuente
que va justa se calienta, mete ruido y acaba fallando.

> Este firmware limita la tira a **150 LEDs** (`NUM_LEDS` en `Config.cpp`). Es el
> máximo que el ESP8266 maneja con el servidor web y Alexa activos sin quedarse
> sin memoria. En ESP32 hay más margen, pero el límite se mantiene por seguridad.

### 3. Masa común: obligatorio

El **GND de la fuente y el GND de la placa deben ir unidos**. Si no, la señal de
datos no tiene referencia: verás parpadeos, colores aleatorios o directamente nada.

```
   Fuente 5V ──┬── +5V ──────────► tira (+5V)
               └── GND ──┬───────► tira (GND)
                         └───────► ESP (GND)      ← esta unión es obligatoria

   ESP (pin de datos) ──[ 330Ω ]──► tira (DIN)
```

### 4. Dos componentes que evitan la mayoría de los fallos

- **Resistencia de 330 Ω** (vale entre 300 y 500) en serie con el cable de datos,
  lo más pegada posible a la tira. Protege el primer LED de picos de tensión.
- **Condensador electrolítico de 1000 µF / 6,3 V o más**, entre +5 V y GND en la
  entrada de la tira. Absorbe el pico de corriente del encendido. **Respeta la
  polaridad**: la patilla marcada con la franja va a GND. Al revés, explota.

### 5. Comprueba antes de dar corriente

- **Revisa la polaridad dos veces.** Invertir +5 V y GND destruye la tira al instante.
- **Respeta la flecha.** Las tiras tienen sentido: los datos entran por `DIN`.
  Conectar por el extremo `DOUT` no funciona.
- **No conectes ni desconectes la tira con la fuente enchufada.**
- **Cable suficiente.** Para más de 3 A no uses cablecillo fino de protoboard:
  se calienta. Usa al menos 0,75 mm² en los tramos de alimentación.
- **Inyección de corriente.** En tiras de más de ~100 LEDs, lleva +5 V y GND
  también al otro extremo. Si no, los últimos LEDs se ven amarillentos o apagados
  porque la tensión cae por el camino.
- Si la tira es de 12 V (WS2811, por ejemplo), **no la alimentes con 5 V** ni al revés.

### Calor

Una tira larga al máximo de brillo calienta de verdad, sobre todo si está enrollada
en su carrete o metida en un perfil cerrado. Desenróllala antes de usarla a
brillo alto y deja que ventile.

---

## 🕒 Apagado automático por seguridad

El firmware **ya trae protección contra dejarla encendida**, precisamente por el
punto anterior. Está en `Led_Control_ESP.ino` y funciona sola:

| Tiempo encendida | Qué hace |
|------------------|----------|
| **40 minutos**   | Baja el brillo al **15%** |
| **80 minutos**   | **Apaga** la tira por completo |

El contador arranca cuando la tira se enciende y **se reinicia al apagarla**, así
que cualquier apagado (web, Alexa o botón) pone el reloj a cero. Si vuelves a
subir el brillo manualmente después del primer escalón, sigues teniendo el
apagado total a los 80 minutos.

Es una red de seguridad para el caso típico: encenderla y olvidarse. Reduce el
calor acumulado y el consumo.

**Para cambiar los tiempos**, edita estas dos líneas al principio de `Led_Control_ESP.ino`
y recompila:

```cpp
const unsigned long DIM_BRIGHTNESS_MS = 40UL * 60UL * 1000UL;  // 40 min → 15%
const unsigned long AUTO_OFF_MS       = 80UL * 60UL * 1000UL;  // 80 min → apagar
```

Para desactivarlo del todo, pon valores muy altos (por ejemplo `999UL * 60UL * 1000UL`).

---

## 🔌 Conexión

Hay **cinco binarios**, uno por familia de chip. Cada uno cubre muchas placas:

| Familia | Placas que cubre | Pin por defecto | Otros pines válidos |
|---------|------------------|-----------------|---------------------|
| **ESP8266** | NodeMCU v2/v3, Wemos D1 mini y mini Pro, D1 R2, ESP-12E/F | **D2** (GPIO4) | D1, D5, D6, D7 |
| **ESP32 clásico** | DevKit v1, NodeMCU-32S, WROOM-32, WROVER, DOIT, D1 R32, TTGO | **GPIO16** | 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 4, 13 |
| **ESP32-C3** | DevKitM/C, C3 SuperMini, XIAO ESP32C3, Lolin C3 mini | **GPIO4** | 3, 5, 6, 7, 10 |
| **ESP32-S2** | DevKitM-1, Saola-1, Lolin S2 Mini y S2 Pico, Feather S2 | **GPIO16** | 4-14, 17, 18, 21, 33, 34, 35 |
| **ESP32-S3** | DevKitC-1, XIAO ESP32S3, Lolin S3 y **S3 Mini**, S3 SuperMini | **GPIO16** | 4-18, 21, 47, 48 |

Las familias **no son intercambiables**: el C3 es RISC-V y el S3 tiene otro juego
de pines, así que cada uno necesita su binario. El flasheador detecta el chip real
y **se niega a escribir** si no coincide con lo que seleccionaste.

> El binario de **ESP32-S2** compila y arranca, pero todavía no se ha verificado en
> una placa S2 física. Los otros cuatro sí.

> El ESP8266 necesita **4 MB de flash**. Las ESP-01 (1 MB) no sirven: no cabe el
> firmware con su partición de LittleFS.
>
> ESP32-S2 y C6 no traen binario precompilado, pero `src/BoardPins.h` ya tiene sus
> pines definidos, así que puedes compilarlos tú cambiando el FQBN.

No hace falta recompilar para cambiar el pin: se elige desde la web. La lista de
pines la sirve el propio firmware (`BoardPins.h`), así que solo te ofrece pines
que esa placa admite de verdad — los pines de la memoria flash, los del puerto
serie y los que solo son de entrada quedan fuera automáticamente.

---

## 🚀 Puesta en marcha

### Opción A — flashear desde el navegador (lo más rápido)

Si no quieres instalar nada, hay un flasheador web que graba la placa por USB
usando Web Serial, con los binarios ya compilados:

**➡️ [tecnomatico.cl/LED_ESP](https://tecnomatico.cl/LED_ESP/)**

1. Conecta la ESP por USB.
2. Selecciona tu placa.
3. Pulsa **Conectar y Flashear** y elige el puerto COM.

Requiere **Chrome o Edge** (Firefox y Safari no soportan Web Serial). Detecta el
chip real y se niega a escribir si no coincide con la placa que seleccionaste, así
que no puedes dejar la placa inservible por equivocarte de binario.

Ese flasheador no forma parte de este repositorio: aquí está el código fuente.

### Opción B — compilarlo tú mismo

Necesitas [arduino-cli](https://arduino.github.io/arduino-cli/) o el Arduino IDE.

**Cores:**
```bash
arduino-cli core install esp8266:esp8266    # para ESP8266
arduino-cli core install esp32:esp32        # para ESP32
```

**Librerías:**
```bash
arduino-cli lib install "Adafruit NeoPixel"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "Espalexa"
```

`ESPAsyncWebServer` y su TCP correspondiente (`ESPAsyncTCP` en ESP8266,
`AsyncTCP` en ESP32) se instalan desde GitHub, no están en el gestor de librerías.

#### Compilar

**Usa los FQBN completos tal cual. No los acortes.** Llevan las opciones puestas a
propósito: un `--fqbn esp8266:esp8266:nodemcuv2` pelado depende de los valores por
defecto del core, y si cambian, cambia la partición de LittleFS y el dispositivo
pierde los ajustes guardados al actualizar.

En ESP32 la opción crítica es `PartitionScheme=min_spiffs`. Sin ella el core reserva
solo 1,25 MB para la aplicación (el firmware ocupa 1,22 MB: entraría al 93%, sin
margen) y desperdicia 1,4 MB en un sistema de archivos donde solo se guardan ~600
bytes. Con `min_spiffs` la app dispone de 1,92 MB y baja al 62%, manteniendo OTA.

```bash
# Si editaste web/device_page.html, regenera primero la página comprimida.
# Si te lo saltas, el firmware sirve la página anterior sin avisar.
python3 web/gzip_page.py

# ESP8266 (se graba en 0x00000)
arduino-cli compile \
  --fqbn "esp8266:esp8266:nodemcuv2:led=2,baud=115200,xtal=80,eesz=4M2M,dbg=Disabled,lvl=None____,ip=lm2f,vt=flash,exception=disabled,stacksmash=disabled,wipe=none,ssl=all,mmu=3232,non32xfer=fast" \
  --output-dir build/esp8266 .

# ESP32 clásico (usa el .merged.bin, que se graba en 0x0)
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=min_spiffs" \
  --output-dir build/esp32 .

# ESP32-C3
arduino-cli compile --fqbn "esp32:esp32:esp32c3:PartitionScheme=min_spiffs" \
  --output-dir build/esp32c3 .

# ESP32-S2
arduino-cli compile --fqbn "esp32:esp32:esp32s2:PartitionScheme=min_spiffs" \
  --output-dir build/esp32s2 .

# ESP32-S3
arduino-cli compile --fqbn "esp32:esp32:esp32s3:PartitionScheme=min_spiffs" \
  --output-dir build/esp32s3 .
```

**C6, C5, H2 y P4 todavía no valen**: no tienen lista de pines en `src/BoardPins.h`,
así que la compilación se detiene con un `#error`. Añadir una familia es escribir su
bloque de pines ahí; no basta con cambiar el FQBN.

#### Ocupación de referencia

| | Binario | Partición | Ocupación | RAM |
|---|---------|-----------|-----------|-----|
| ESP8266   | 459 KB  | 1 MB    | 44% | 50% de 80 KB |
| ESP32     | 1,22 MB | 1,92 MB | 62% | 16% de 320 KB |
| ESP32-C3  | 1,24 MB | 1,92 MB | 62% | 13% de 320 KB |
| ESP32-S2  | 1,15 MB | 1,92 MB | 58% | 14% de 320 KB |
| ESP32-S3  | 1,18 MB | 1,92 MB | 60% | 15% de 320 KB |

El binario de la familia ESP32 pesa más porque `arduino-esp32` 3.x se apoya en
ESP-IDF 5 (FreeRTOS, LWIP completo, mbedTLS). A cambio tiene ~274 KB de RAM libre
frente a los ~40 KB del ESP8266, que es lo que de verdad limita a este proyecto al
servir la página web.

---

## 📶 Primer encendido

Al arrancar sin WiFi configurado, el dispositivo crea su propia red:

- **Red:** `TiraLed-XXXX` (las X son los últimos dígitos de su MAC)
- **Contraseña:** `tecnomatico`

Conéctate y se abrirá el portal solo. Si no, entra a `http://192.168.4.1`. Ahí eliges:

- Tu red WiFi y su contraseña
- El pin donde conectaste la tira
- Si quieres Alexa

Al guardar se reinicia y se conecta a tu red. A partir de ahí lo controlas desde
su IP (la ves en tu router o por el puerto serie).

Si algún día se cae el router, el dispositivo reabre su portal — y cuando la red
vuelve, se reconecta solo sin que tengas que tocar nada.

---

## ⚙️ Configuración que quizá quieras cambiar

En `Config.cpp`, antes de compilar:

```cpp
const char* OTA_PASSWORD = "cambiame";   // ⚠️ CÁMBIALA
const uint16_t NUM_LEDS = 150;           // máximo de LEDs
const uint8_t BRIGHTNESS = 45;           // brillo inicial (0-255)
```

**Cambia `OTA_PASSWORD` sí o sí.** Con la que viene por defecto, cualquiera que
esté en tu WiFi puede reprogramar el dispositivo por aire.

El resto (nº real de LEDs, nombre para Alexa, IP fija, color, efecto) se configura
desde la web y se guarda en el propio dispositivo.

---

## 🎨 Efectos

22 efectos: estático, arcoíris, arcoíris en bucle, pulso, KITT, KITT2, fuego, onda,
destellos, estrellas, nieve, meteorito, strobe, policía, scanner, rebote, teatro,
teatro arcoíris, running, luces corrientes, barrido y uno extra.

---

## 🗣️ Alexa

Se activa desde el portal. Una vez encendida, di *"Alexa, busca dispositivos"* y
aparecerá como una bombilla de color: encender, apagar, brillo y color por voz.

---

## 🔧 Estructura

```
Led_Control_ESP.ino      Sketch principal: setup, loop y apagado automático
src/
  Config.cpp/.h          Parámetros de compilación
  BoardPins.h            Pines válidos por placa (fuente única de verdad)
  Librerias.h            Includes y declaraciones globales
  WiFiManager.cpp/.h     Conexión WiFi, modo AP y portal cautivo
  WebServer.cpp          Servidor HTTP, API y portal de configuración
  Settings.cpp           Configuración persistente en LittleFS
  Effects.cpp            Los efectos
  Alexa_Setup.cpp        Integración con Alexa
  OTA.cpp                Actualización por aire
  PageIndex.h            Página comprimida (GENERADO, no editar)
web/
  device_page.html       Página de control del dispositivo (edítala aquí)
  gzip_page.py           Regenera src/PageIndex.h desde device_page.html
```

> **El `.ino` debe llamarse igual que la carpeta que lo contiene.** Si renombras
> el directorio, renombra también `Led_Control_ESP.ino`. Arduino compila además
> todo lo que haya en `src/` de forma recursiva; por eso las fuentes viven ahí
> y la raíz queda limpia.

Si editas `web/device_page.html`, ejecuta `python3 web/gzip_page.py` antes de compilar o se
publicará la página anterior sin avisar.

---

## Licencia

MIT — úsalo, modifícalo y compártelo.

**Sin garantía.** Trabajas con electricidad y calor: revisa tus conexiones y usa
material adecuado. La responsabilidad de tu montaje es tuya.

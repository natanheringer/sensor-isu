/**
 * =============================================================
 *  ISU TRONIK - Integrated Sensor Unit
 *  Firmware v2.0 (MVP Wi-Fi)
 * =============================================================
 *  Plataforma : ESP32 DOIT DevKit V1
 *  Sensores   : 3x VL53L0X (ToF) + MPU6050 (wake-on-impact)
 *  Comunicacao: HTTP POST via Wi-Fi
 *  Energia    : 18650 externa + deep sleep
 * =============================================================
 *
 *  BIBLIOTECAS NECESSARIAS (Arduino IDE Library Manager):
 *    - ArduinoJson       (por Benoit Blanchon)
 *    - Adafruit VL53L0X  (por Adafruit)
 *    - Adafruit MPU6050  (por Adafruit)
 *    - Adafruit Unified Sensor
 *
 *  Bibliotecas do core ESP32:
 *    - WiFi
 *    - HTTPClient
 *
 *  PLACA no Arduino IDE:
 *    DOIT ESP32 DEVKIT V1 ou ESP32 Dev Module
 *
 *  IMPORTANTE:
 *    - Edite Wi-Fi, URL do backend e IDs no bloco de configuracao.
 *    - Ciclo: wake -> ler -> conectar Wi-Fi -> POST -> dormir.
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// --- Wi-Fi / HTTP ------------------------------------------------
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// --- Sensores ----------------------------------------------------
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- JSON --------------------------------------------------------
#include <ArduinoJson.h>

// =============================================================
//  CONFIGURACAO DO USUARIO - EDITE AQUI
// =============================================================

// --- Identificacao do dispositivo ---
static const int SENSOR_ID  = 1;   // ID do sensor no banco da dashboard
static const int COLETOR_ID = 3;   // ID do coletor associado
static const char* API_KEY  = "tronik-isu-mvp-2026";  // chave simples

// --- Wi-Fi do MVP ---
static const char* WIFI_SSID = "SEU_WIFI";
static const char* WIFI_PASS = "SUA_SENHA";
static const uint32_t WIFI_TIMEOUT_MS = 20000;

// --- Backend da dashboard ---
static const char* SERVER_URL = "https://seu-servidor.railway.app/api/sensor/telemetria";
static const bool  TLS_INSECURE_FOR_MVP = true;  // MVP: ignora validacao de certificado

// --- Timing / geometria ---
static const uint32_t SLEEP_MINUTES       = 30;     // wake periodico
static const float    CONTAINER_HEIGHT_MM = 500.0f; // altura interna do coletor

// --- Pinos ESP32 DOIT DevKit V1 ---
static const int PIN_SDA = 21;
static const int PIN_SCL = 19;

// GPIOs digitais para ligar os 3 VL53L0X sequencialmente via XSHUT
static const int PIN_XSHUT_TOF1 = 25;
static const int PIN_XSHUT_TOF2 = 26;
static const int PIN_XSHUT_TOF3 = 27;

// GPIO33 e um pino RTC, adequado para wake por ext0 no deep sleep
static const int PIN_MPU_INT = 33;

// GPIO34 e somente entrada. Use divisor resistivo externo para medir 18650.
static const int PIN_BAT_ADC = 34;

// --- Enderecos I2C dos ToF atribuidos em runtime ---
static const uint8_t TOF_ADDR1 = 0x30;
static const uint8_t TOF_ADDR2 = 0x31;
static const uint8_t TOF_ADDR3 = 0x32;

// =============================================================
//  OBJETOS GLOBAIS
// =============================================================

Adafruit_VL53L0X tof1 = Adafruit_VL53L0X();
Adafruit_VL53L0X tof2 = Adafruit_VL53L0X();
Adafruit_VL53L0X tof3 = Adafruit_VL53L0X();

Adafruit_MPU6050 mpu;

// =============================================================
//  UTILITARIOS
// =============================================================

static inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

static float median3(float a, float b, float c) {
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }
    return b;
}

static float read_battery_percent() {
    uint32_t raw = 0;
    for (int i = 0; i < 16; i++) {
        raw += analogRead(PIN_BAT_ADC);
    }
    raw /= 16;

    // Divisor externo 100k/100k: Vbat = ADC * 2 * 3.3 / 4095.
    float voltage = (float)raw / 4095.0f * 3.3f * 2.0f;
    float pct = (voltage - 3.0f) / (4.2f - 3.0f) * 100.0f;
    return clampf(pct, 0.0f, 100.0f);
}

// =============================================================
//  INICIALIZACAO DOS VL53L0X COM ENDERECOS DISTINTOS
// =============================================================

static bool init_tof_sensors() {
    pinMode(PIN_XSHUT_TOF1, OUTPUT);
    pinMode(PIN_XSHUT_TOF2, OUTPUT);
    pinMode(PIN_XSHUT_TOF3, OUTPUT);

    digitalWrite(PIN_XSHUT_TOF1, LOW);
    digitalWrite(PIN_XSHUT_TOF2, LOW);
    digitalWrite(PIN_XSHUT_TOF3, LOW);
    delay(50);

    digitalWrite(PIN_XSHUT_TOF1, HIGH);
    delay(50);
    if (!tof1.begin(TOF_ADDR1)) {
        Serial.println("[ToF1] FALHA na inicializacao");
        return false;
    }
    Serial.printf("[ToF1] VL53L0X OK em 0x%02X\n", TOF_ADDR1);

    digitalWrite(PIN_XSHUT_TOF2, HIGH);
    delay(50);
    if (!tof2.begin(TOF_ADDR2)) {
        Serial.println("[ToF2] FALHA na inicializacao");
        return false;
    }
    Serial.printf("[ToF2] VL53L0X OK em 0x%02X\n", TOF_ADDR2);

    digitalWrite(PIN_XSHUT_TOF3, HIGH);
    delay(50);
    if (!tof3.begin(TOF_ADDR3)) {
        Serial.println("[ToF3] FALHA na inicializacao");
        return false;
    }
    Serial.printf("[ToF3] VL53L0X OK em 0x%02X\n", TOF_ADDR3);

    return true;
}

// =============================================================
//  LEITURA DE NIVEL (MULTI-PONTO COM MEDIANA)
// =============================================================

struct NivelLeitura {
    float porcentagem;     // 0.0 a 100.0
    float distancias[3];   // distancias brutas em mm
    bool  valida;
};

static float ler_distancia_mm(Adafruit_VL53L0X& sensor) {
    VL53L0X_RangingMeasurementData_t measure;
    sensor.rangingTest(&measure, false);

    if (measure.RangeStatus == 4) {
        return -1.0f;  // leitura invalida (fora de alcance)
    }
    return (float)measure.RangeMilliMeter;
}

static NivelLeitura medir_nivel() {
    NivelLeitura resultado;
    resultado.valida = false;

    const int BURST = 5;
    float buf[BURST];

    for (int s = 0; s < 3; s++) {
        Adafruit_VL53L0X* sensor;
        if (s == 0) sensor = &tof1;
        else if (s == 1) sensor = &tof2;
        else sensor = &tof3;

        int validas = 0;
        for (int i = 0; i < BURST; i++) {
            float d = ler_distancia_mm(*sensor);
            if (d > 0) {
                buf[validas++] = d;
            }
            delay(30);
        }

        if (validas >= 3) {
            for (int i = 1; i < validas; i++) {
                float key = buf[i];
                int j = i - 1;
                while (j >= 0 && buf[j] > key) {
                    buf[j + 1] = buf[j];
                    j--;
                }
                buf[j + 1] = key;
            }
            resultado.distancias[s] = buf[validas / 2];
        } else if (validas > 0) {
            resultado.distancias[s] = buf[0];
        } else {
            resultado.distancias[s] = -1.0f;
        }
    }

    float dists_validas[3];
    int n_validas = 0;
    for (int i = 0; i < 3; i++) {
        if (resultado.distancias[i] > 0) {
            dists_validas[n_validas++] = resultado.distancias[i];
        }
    }

    float distancia_final;
    if (n_validas == 3) {
        distancia_final = median3(dists_validas[0], dists_validas[1], dists_validas[2]);
        resultado.valida = true;
    } else if (n_validas == 2) {
        distancia_final = (dists_validas[0] + dists_validas[1]) / 2.0f;
        resultado.valida = true;
    } else if (n_validas == 1) {
        distancia_final = dists_validas[0];
        resultado.valida = true;
    } else {
        resultado.porcentagem = -1.0f;
        return resultado;
    }

    float fill = (CONTAINER_HEIGHT_MM - distancia_final) / CONTAINER_HEIGHT_MM * 100.0f;
    resultado.porcentagem = clampf(fill, 0.0f, 100.0f);

    return resultado;
}

// =============================================================
//  WI-FI / HTTP
// =============================================================

static bool conectar_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("[WiFi] Conectando em %s", WIFI_SSID);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < WIFI_TIMEOUT_MS)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] FALHA conexao");
        return false;
    }

    Serial.print("[WiFi] Conectado. IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

static bool enviar_telemetria(const NivelLeitura& nivel, float bateria, const char* motivo_wake) {
    StaticJsonDocument<512> doc;
    doc["api_key"]              = API_KEY;
    doc["sensor_id"]            = SENSOR_ID;
    doc["coletor_id"]           = COLETOR_ID;
    doc["nivel_preenchimento"]  = roundf(nivel.porcentagem * 10.0f) / 10.0f;
    doc["bateria"]              = roundf(bateria * 10.0f) / 10.0f;
    doc["wake_reason"]          = motivo_wake;
    doc["wifi_rssi"]            = WiFi.RSSI();

    JsonArray arr = doc.createNestedArray("leituras_tof");
    for (int i = 0; i < 3; i++) {
        arr.add(roundf(nivel.distancias[i] * 10.0f) / 10.0f);
    }

    char payload[512];
    size_t len = serializeJson(doc, payload, sizeof(payload));

    Serial.printf("[HTTP] Enviando %d bytes para %s\n", (int)len, SERVER_URL);

    HTTPClient http;
    WiFiClientSecure secure_client;
    WiFiClient plain_client;

    bool is_https = strncmp(SERVER_URL, "https://", 8) == 0;
    bool began = false;

    if (is_https) {
        if (TLS_INSECURE_FOR_MVP) {
            secure_client.setInsecure();
        }
        began = http.begin(secure_client, SERVER_URL);
    } else {
        began = http.begin(plain_client, SERVER_URL);
    }

    if (!began) {
        Serial.println("[HTTP] FALHA ao iniciar cliente");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    int code = http.POST((uint8_t*)payload, len);
    Serial.printf("[HTTP] Status: %d\n", code);
    http.end();

    return (code >= 200 && code < 300);
}

static void desligar_wifi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WiFi] Desligado");
}

// =============================================================
//  MPU6050: CONFIGURAR WAKE-ON-MOTION
// =============================================================

static bool init_mpu_wakeup() {
    if (!mpu.begin()) {
        Serial.println("[MPU6050] FALHA");
        return false;
    }

    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    mpu.setMotionDetectionThreshold(40);
    mpu.setMotionDetectionDuration(20);
    mpu.setMotionInterrupt(true);

    Serial.println("[MPU6050] Wake-on-motion configurado");
    return true;
}

// =============================================================
//  DEEP SLEEP
// =============================================================

static void entrar_deep_sleep() {
    uint64_t sleep_us = (uint64_t)SLEEP_MINUTES * 60ULL * 1000000ULL;

    esp_sleep_enable_timer_wakeup(sleep_us);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_MPU_INT, HIGH);

    Serial.printf("[Sleep] Dormindo por %d minutos (ou impacto)...\n", SLEEP_MINUTES);
    Serial.flush();

    esp_deep_sleep_start();
}

static const char* razao_wake() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:   return "timer";
        case ESP_SLEEP_WAKEUP_EXT0:    return "impacto";
        default:                       return "boot";
    }
}

// =============================================================
//  SETUP (executa a cada wake)
// =============================================================

void setup() {
    Serial.begin(115200);
    delay(100);

    const char* motivo = razao_wake();
    Serial.printf("\n=== ISU TRONIK v2.0 Wi-Fi | Wake: %s ===\n", motivo);

    Wire.begin(PIN_SDA, PIN_SCL);
    analogReadResolution(12);

    float bateria = read_battery_percent();
    Serial.printf("[Bateria] %.1f%%\n", bateria);

    if (bateria < 5.0f) {
        Serial.println("[ALERTA] Bateria critica! Dormindo...");
        entrar_deep_sleep();
        return;
    }

    bool tof_ok = init_tof_sensors();
    NivelLeitura nivel;

    if (tof_ok) {
        nivel = medir_nivel();
        if (nivel.valida) {
            Serial.printf("[Nivel] %.1f%% (d1=%.0f d2=%.0f d3=%.0f mm)\n",
                nivel.porcentagem,
                nivel.distancias[0], nivel.distancias[1], nivel.distancias[2]);
        } else {
            Serial.println("[Nivel] Leitura invalida");
        }
    } else {
        Serial.println("[ToF] Falha nos sensores - enviando erro");
        nivel.valida = false;
        nivel.porcentagem = -1.0f;
        nivel.distancias[0] = nivel.distancias[1] = nivel.distancias[2] = -1.0f;
    }

    bool enviado = false;
    if (conectar_wifi()) {
        enviado = enviar_telemetria(nivel, bateria, motivo);
        if (enviado) {
            Serial.println("[OK] Telemetria enviada com sucesso");
        } else {
            Serial.println("[ERRO] Falha no envio - tentara no proximo ciclo");
        }
        desligar_wifi();
    } else {
        Serial.println("[ERRO] Falha no Wi-Fi - tentara no proximo ciclo");
    }

    init_mpu_wakeup();
    entrar_deep_sleep();
}

void loop() {
    // Nao utilizado. O ESP32 reinicia pelo setup() a cada wake.
}

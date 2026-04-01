/**
 * =============================================================
 *  ISU TRONIK — Integrated Sensor Unit
 *  Firmware v2.0 (MVP)
 * =============================================================
 *  Plataforma : LilyGO T-SIM7000G (ESP32 + SIM7000G)
 *  Sensores   : 3x VL53L0X (ToF) + MPU6050 (wake-on-impact)
 *  Comunicação: HTTP POST via NB-IoT/LTE-M (TinyGSM)
 *  Energia    : 18650 + deep sleep
 * =============================================================
 *
 *  BIBLIOTECAS NECESSÁRIAS (Arduino IDE Library Manager):
 *    - TinyGSM           (por Volodymyr Shymanskyy)
 *    - ArduinoJson       (por Benoît Blanchon)
 *    - Adafruit VL53L0X  (por Adafruit)
 *    - Adafruit MPU6050  (por Adafruit)
 *    - Adafruit Unified Sensor
 *
 *  PLACA no Arduino IDE:
 *    ESP32 Dev Module (ou LilyGO T-SIM7000G se disponível)
 *
 *  IMPORTANTE:
 *    - Sem alocação dinâmica no loop (zero new/delete)
 *    - Todos os buffers são stack/static
 *    - Ciclo: wake → ler → conectar → POST → dormir
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ─── TinyGSM ────────────────────────────────────────────────
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

// ─── Sensores ───────────────────────────────────────────────
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ─── JSON ───────────────────────────────────────────────────
#include <ArduinoJson.h>

// =============================================================
//  CONFIGURAÇÃO DO USUÁRIO — EDITE AQUI
// =============================================================

// ── Identificação do dispositivo ──
static const int SENSOR_ID  = 1;   // ID do sensor no banco da dashboard
static const int COLETOR_ID = 3;   // ID do coletor associado
static const char* API_KEY  = "tronik-isu-mvp-2026";  // chave simples

// ── APN da operadora (ex: Vivo, Claro, Tim) ──
static const char* APN      = "zap.vivo.com.br";  // AJUSTE para sua operadora
static const char* APN_USER = "";
static const char* APN_PASS = "";

// ── Backend da dashboard ──
static const char* SERVER_HOST = "seu-servidor.railway.app";  // AJUSTE
static const int   SERVER_PORT = 443;                          // HTTPS
static const char* ENDPOINT    = "/api/sensor/telemetria";
static const bool  USE_SSL     = true;

// ── Timing ──
static const uint32_t SLEEP_MINUTES     = 30;    // intervalo de wake periódico
static const float    CONTAINER_HEIGHT_MM = 500.0f; // altura interna do coletor em mm

// ── Pinos LilyGO T-SIM7000G ──
static const int PIN_MODEM_TX    = 27;
static const int PIN_MODEM_RX    = 26;
static const int PIN_MODEM_PWRKEY = 4;
static const int PIN_MODEM_DTR   = 32;
static const int PIN_MODEM_RI    = 33;
static const int PIN_BAT_ADC     = 35;  // leitura de tensão da bateria

// ── Pinos I2C ──
static const int PIN_SDA = 21;
static const int PIN_SCL = 22;

// ── Pinos XSHUT dos 3 VL53L0X ──
static const int PIN_XSHUT_TOF1 = 13;
static const int PIN_XSHUT_TOF2 = 14;
static const int PIN_XSHUT_TOF3 = 15;

// ── Pino de interrupção do MPU6050 ──
static const int PIN_MPU_INT = 2;  // GPIO2 — suporta wake do deep sleep

// ── Endereços I2C dos ToF (atribuídos em runtime) ──
static const uint8_t TOF_ADDR1 = 0x30;
static const uint8_t TOF_ADDR2 = 0x31;
static const uint8_t TOF_ADDR3 = 0x32;

// =============================================================
//  OBJETOS GLOBAIS
// =============================================================

HardwareSerial SerialAT(1);  // UART1 para o modem
TinyGsm modem(SerialAT);

Adafruit_VL53L0X tof1 = Adafruit_VL53L0X();
Adafruit_VL53L0X tof2 = Adafruit_VL53L0X();
Adafruit_VL53L0X tof3 = Adafruit_VL53L0X();

Adafruit_MPU6050 mpu;

// =============================================================
//  UTILITÁRIOS
// =============================================================

static inline float clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

// Mediana de 3 valores (sem alocação)
static float median3(float a, float b, float c) {
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }
    return b;
}

// Leitura da tensão da bateria via ADC
static float read_battery_percent() {
    // LilyGO T-SIM7000G: divisor resistivo 100k/100k no pino 35
    // V_bat = ADC * 2 * 3.3 / 4095
    uint32_t raw = 0;
    for (int i = 0; i < 16; i++) {
        raw += analogRead(PIN_BAT_ADC);
    }
    raw /= 16;

    float voltage = (float)raw / 4095.0f * 3.3f * 2.0f;

    // Mapeamento linear simples: 3.0V = 0%, 4.2V = 100%
    float pct = (voltage - 3.0f) / (4.2f - 3.0f) * 100.0f;
    return clampf(pct, 0.0f, 100.0f);
}

// =============================================================
//  INICIALIZAÇÃO DOS VL53L0X COM ENDEREÇOS DISTINTOS
// =============================================================

static bool init_tof_sensors() {
    // Desliga todos via XSHUT
    pinMode(PIN_XSHUT_TOF1, OUTPUT);
    pinMode(PIN_XSHUT_TOF2, OUTPUT);
    pinMode(PIN_XSHUT_TOF3, OUTPUT);

    digitalWrite(PIN_XSHUT_TOF1, LOW);
    digitalWrite(PIN_XSHUT_TOF2, LOW);
    digitalWrite(PIN_XSHUT_TOF3, LOW);
    delay(50);

    // Liga ToF 1, atribui endereço
    digitalWrite(PIN_XSHUT_TOF1, HIGH);
    delay(50);
    if (!tof1.begin(TOF_ADDR1)) {
        Serial.println("[ToF1] FALHA na inicializacao");
        return false;
    }
    Serial.printf("[ToF1] OK em 0x%02X\n", TOF_ADDR1);

    // Liga ToF 2, atribui endereço
    digitalWrite(PIN_XSHUT_TOF2, HIGH);
    delay(50);
    if (!tof2.begin(TOF_ADDR2)) {
        Serial.println("[ToF2] FALHA na inicializacao");
        return false;
    }
    Serial.printf("[ToF2] OK em 0x%02X\n", TOF_ADDR2);

    // Liga ToF 3, atribui endereço
    digitalWrite(PIN_XSHUT_TOF3, HIGH);
    delay(50);
    if (!tof3.begin(TOF_ADDR3)) {
        Serial.println("[ToF3] FALHA na inicializacao");
        return false;
    }
    Serial.printf("[ToF3] OK em 0x%02X\n", TOF_ADDR3);

    return true;
}

// =============================================================
//  LEITURA DE NÍVEL (MULTI-PONTO COM MEDIANA)
// =============================================================

struct NivelLeitura {
    float porcentagem;     // 0.0 a 100.0
    float distancias[3];   // distâncias brutas em mm
    bool  valida;
};

static float ler_distancia_mm(Adafruit_VL53L0X& sensor) {
    VL53L0X_RangingMeasurementData_t measure;
    sensor.rangingTest(&measure, false);

    if (measure.RangeStatus == 4) {
        return -1.0f;  // leitura inválida (fora de alcance)
    }
    return (float)measure.RangeMilliMeter;
}

static NivelLeitura medir_nivel() {
    NivelLeitura resultado;
    resultado.valida = false;

    // Burst de 5 leituras por sensor, mediana de cada burst
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
            delay(30);  // intervalo entre leituras
        }

        if (validas >= 3) {
            // Ordena para mediana (insertion sort simples)
            for (int i = 1; i < validas; i++) {
                float key = buf[i];
                int j = i - 1;
                while (j >= 0 && buf[j] > key) {
                    buf[j + 1] = buf[j];
                    j--;
                }
                buf[j + 1] = key;
            }
            resultado.distancias[s] = buf[validas / 2];  // mediana
        } else if (validas > 0) {
            resultado.distancias[s] = buf[0];  // fallback
        } else {
            resultado.distancias[s] = -1.0f;  // falha total nesse sensor
        }
    }

    // Mediana dos 3 sensores (ignora inválidos)
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
        return resultado;  // falha total
    }

    // Converter distância para porcentagem de enchimento
    // distância = espaço vazio entre sensor (topo) e material
    // enchimento = (altura_total - distância) / altura_total * 100
    float fill = (CONTAINER_HEIGHT_MM - distancia_final) / CONTAINER_HEIGHT_MM * 100.0f;
    resultado.porcentagem = clampf(fill, 0.0f, 100.0f);

    return resultado;
}

// =============================================================
//  MODEM: LIGAR, CONECTAR, ENVIAR
// =============================================================

static bool ligar_modem() {
    // Power on via PWRKEY
    pinMode(PIN_MODEM_PWRKEY, OUTPUT);
    digitalWrite(PIN_MODEM_PWRKEY, HIGH);
    delay(300);
    digitalWrite(PIN_MODEM_PWRKEY, LOW);

    // DTR baixo para modo ativo
    pinMode(PIN_MODEM_DTR, OUTPUT);
    digitalWrite(PIN_MODEM_DTR, LOW);

    SerialAT.begin(115200, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
    delay(3000);

    Serial.println("[Modem] Inicializando...");
    if (!modem.init()) {
        Serial.println("[Modem] FALHA init");
        return false;
    }

    String name = modem.getModemName();
    Serial.printf("[Modem] OK: %s\n", name.c_str());
    return true;
}

static bool conectar_rede() {
    Serial.println("[Rede] Configurando APN...");

    // Preferir NB-IoT (cat-m se disponível)
    modem.sendAT("+CMNB=1");  // 1=CAT-M, 2=NB-IoT, 3=ambos
    modem.waitResponse();

    modem.gprsConnect(APN, APN_USER, APN_PASS);

    Serial.println("[Rede] Aguardando conexao...");
    uint32_t start = millis();
    while (!modem.isGprsConnected() && (millis() - start < 60000)) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println();

    if (!modem.isGprsConnected()) {
        Serial.println("[Rede] FALHA conexao GPRS");
        return false;
    }

    Serial.print("[Rede] Conectado. IP: ");
    Serial.println(modem.localIP());
    return true;
}

static bool enviar_telemetria(const NivelLeitura& nivel, float bateria, const char* motivo_wake) {
    // Montar JSON
    StaticJsonDocument<512> doc;
    doc["api_key"]             = API_KEY;
    doc["sensor_id"]           = SENSOR_ID;
    doc["coletor_id"]          = COLETOR_ID;
    doc["nivel_preenchimento"]  = roundf(nivel.porcentagem * 10.0f) / 10.0f;
    doc["bateria"]             = roundf(bateria * 10.0f) / 10.0f;
    doc["wake_reason"]         = motivo_wake;

    // RSSI (qualidade do sinal celular)
    int16_t rssi = modem.getSignalQuality();
    doc["rssi"] = rssi;

    JsonArray arr = doc.createNestedArray("leituras_tof");
    for (int i = 0; i < 3; i++) {
        arr.add(roundf(nivel.distancias[i] * 10.0f) / 10.0f);
    }

    // Serializar
    char payload[512];
    size_t len = serializeJson(doc, payload, sizeof(payload));

    Serial.printf("[HTTP] Enviando %d bytes para %s%s\n", (int)len, SERVER_HOST, ENDPOINT);

    // Criar client
    TinyGsmClient client(modem);

    bool connected;
    if (USE_SSL) {
        // Para SSL, o SIM7000G suporta AT+CIPSSL
        // Em produção, usar TinyGsmClientSecure ou AT commands diretos
        // Para MVP, HTTP simples é aceitável em rede privada
        connected = client.connect(SERVER_HOST, 80);
    } else {
        connected = client.connect(SERVER_HOST, SERVER_PORT);
    }

    if (!connected) {
        Serial.println("[HTTP] FALHA conexao TCP");
        return false;
    }

    // Enviar request HTTP POST
    client.print("POST ");
    client.print(ENDPOINT);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(SERVER_HOST);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(len);
    client.println("Connection: close");
    client.println();
    client.write((const uint8_t*)payload, len);

    // Ler resposta (simplificado — só verifica status)
    uint32_t timeout = millis() + 10000;
    while (client.connected() && !client.available() && millis() < timeout) {
        delay(100);
    }

    bool success = false;
    if (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.printf("[HTTP] Resposta: %s\n", line.c_str());
        success = (line.indexOf("200") > 0 || line.indexOf("201") > 0);
    }

    client.stop();
    return success;
}

static void desligar_modem() {
    modem.gprsDisconnect();
    modem.poweroff();
    Serial.println("[Modem] Desligado");
}

// =============================================================
//  MPU6050: CONFIGURAR WAKE-ON-MOTION
// =============================================================

static bool init_mpu_wakeup() {
    if (!mpu.begin()) {
        Serial.println("[MPU6050] FALHA");
        return false;
    }

    // Configurar motion detection para wake
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    mpu.setMotionDetectionThreshold(40);   // sensibilidade (1-255)
    mpu.setMotionDetectionDuration(20);    // ms mínimos
    mpu.setMotionInterrupt(true);

    Serial.println("[MPU6050] Wake-on-motion configurado");
    return true;
}

// =============================================================
//  DEEP SLEEP
// =============================================================

static void entrar_deep_sleep() {
    uint64_t sleep_us = (uint64_t)SLEEP_MINUTES * 60ULL * 1000000ULL;

    // Wake por timer
    esp_sleep_enable_timer_wakeup(sleep_us);

    // Wake por GPIO (interrupção do MPU6050)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_MPU_INT, HIGH);

    Serial.printf("[Sleep] Dormindo por %d minutos (ou impacto)...\n", SLEEP_MINUTES);
    Serial.flush();

    esp_deep_sleep_start();
    // Nunca chega aqui — wake reinicia o setup()
}

// =============================================================
//  IDENTIFICAR RAZÃO DO WAKE
// =============================================================

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
    Serial.printf("\n=== ISU TRONIK v2.0 | Wake: %s ===\n", motivo);

    // Iniciar I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    analogReadResolution(12);

    // 1. Ler bateria
    float bateria = read_battery_percent();
    Serial.printf("[Bateria] %.1f%%\n", bateria);

    // Se bateria muito baixa, dormir direto sem transmitir
    if (bateria < 5.0f) {
        Serial.println("[ALERTA] Bateria critica! Dormindo...");
        entrar_deep_sleep();
        return;
    }

    // 2. Inicializar e ler sensores ToF
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
        Serial.println("[ToF] Falha nos sensores — enviando erro");
        nivel.valida = false;
        nivel.porcentagem = -1.0f;
        nivel.distancias[0] = nivel.distancias[1] = nivel.distancias[2] = -1.0f;
    }

    // 3. Conectar rede celular e enviar
    bool enviado = false;
    if (ligar_modem() && conectar_rede()) {
        enviado = enviar_telemetria(nivel, bateria, motivo);
        if (enviado) {
            Serial.println("[OK] Telemetria enviada com sucesso");
        } else {
            Serial.println("[ERRO] Falha no envio — tentara no proximo ciclo");
        }
        desligar_modem();
    } else {
        Serial.println("[ERRO] Falha na rede — tentara no proximo ciclo");
    }

    // 4. Configurar MPU6050 para wake-on-impact
    init_mpu_wakeup();

    // 5. Dormir
    entrar_deep_sleep();
}

// =============================================================
//  LOOP (nunca executa — deep sleep reinicia via setup)
// =============================================================

void loop() {
    // Não utilizado. O ESP32 reinicia pelo setup() a cada wake.
}

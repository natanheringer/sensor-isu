#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Pinos canonicos do projeto
#define SDA_PIN 21
#define SCL_PIN 19

// ---------------------------------------------------------------------------
// WiFi, Railway, IDs do banco, chave de telemetria
// ---------------------------------------------------------------------------
static const char* WIFI_SSID = "";
static const char* WIFI_PASS = "";

static const char* HTTPS_HOST = "";
static const char* HTTPS_PATH = "";
static const char* API_KEY = "";  // = TELEMETRY_SHARED_SECRET no Railway

static const int COLETOR_ID = 38;   // ID do coletor no Postgres
static const int SENSOR_ID = 1;    // ID do sensor (coletor_id deve bater)

// Altura util do vaso: 1 m = 1000 mm (0 mm ToF = cheio, 1000 mm = vazio)
static const int TANK_HEIGHT_MM = 1000;

static const int HTTPS_PORT = 443;
static const uint32_t WIFI_TIMEOUT_MS = 20000;
static const uint32_t POST_INTERVAL_MS = 10000;

Adafruit_VL53L0X vl53 = Adafruit_VL53L0X();
Adafruit_MPU6050 mpu;

static bool vl_ok = false;
static bool mpu_ok = false;
static uint32_t last_post_ms = 0;

struct SensorReadings {
  int vl53_mm;
  float ax;
  float ay;
  float az;
};

// Distancia menor = mais cheio. 500 mm => 50%, 1000 mm => 0%, 0 mm => 100%
static float mm_to_fill_percent(int mm) {
  if (mm < 0) {
    return 0.0f;
  }
  if (mm >= TANK_HEIGHT_MM) {
    return 0.0f;
  }
  return 100.0f - ((float)mm * 100.0f / (float)TANK_HEIGHT_MM);
}

static bool connect_wifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("[WiFi] conectando");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] falhou");
    return false;
  }

  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

static SensorReadings read_sensors() {
  SensorReadings readings;
  readings.vl53_mm = -1;
  readings.ax = 0.0f;
  readings.ay = 0.0f;
  readings.az = 0.0f;

  if (vl_ok) {
    VL53L0X_RangingMeasurementData_t measure;
    vl53.rangingTest(&measure, false);
    if (measure.RangeStatus != 4) {
      readings.vl53_mm = measure.RangeMilliMeter;
    }
  }

  if (mpu_ok) {
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    mpu.getEvent(&accel, &gyro, &temp);
    readings.ax = accel.acceleration.x;
    readings.ay = accel.acceleration.y;
    readings.az = accel.acceleration.z;
  }

  return readings;
}

static bool post_https(const SensorReadings& readings) {
  if (!connect_wifi()) {
    return false;
  }

  const float nivel_pct = mm_to_fill_percent(readings.vl53_mm);
  const float bateria_pct = 100.0f;  // ajuste se tiver leitura real de bateria

  char payload[384];
  int payload_len;

  if (readings.vl53_mm >= 0) {
    const float tof_cm = (float)readings.vl53_mm / 10.0f;
    payload_len = snprintf(
      payload,
      sizeof(payload),
      "{\"api_key\":\"%s\",\"sensor_id\":%d,\"coletor_id\":%d,"
      "\"nivel_preenchimento\":%.1f,\"bateria\":%.1f,"
      "\"leituras_tof\":[%.1f],\"firmware_version\":\"esp32-vl53-mpu-1\"}",
      API_KEY,
      SENSOR_ID,
      COLETOR_ID,
      nivel_pct,
      bateria_pct,
      tof_cm
    );
  } else {
    payload_len = snprintf(
      payload,
      sizeof(payload),
      "{\"api_key\":\"%s\",\"sensor_id\":%d,\"coletor_id\":%d,"
      "\"nivel_preenchimento\":%.1f,\"bateria\":%.1f,"
      "\"firmware_version\":\"esp32-vl53-mpu-1\"}",
      API_KEY,
      SENSOR_ID,
      COLETOR_ID,
      nivel_pct,
      bateria_pct
    );
  }

  if (payload_len <= 0 || payload_len >= (int)sizeof(payload)) {
    Serial.println("[HTTPS] payload grande demais");
    return false;
  }

  Serial.print("[HTTPS] payload: ");
  Serial.println(payload);

  WiFiClientSecure client;
  client.setInsecure();  // MVP: nao valida certificado TLS
  client.setTimeout(8000);

  Serial.print("[HTTPS] conectando em ");
  Serial.println(HTTPS_HOST);

  if (!client.connect(HTTPS_HOST, HTTPS_PORT)) {
    Serial.println("[HTTPS] falha na conexao");
    return false;
  }

  client.print("POST ");
  client.print(HTTPS_PATH);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(HTTPS_HOST);
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(payload_len);
  client.println("Connection: close");
  client.println();
  client.write((const uint8_t*)payload, payload_len);

  uint32_t start = millis();
  while (!client.available() && millis() - start < 8000) {
    delay(20);
  }

  char status_line[64] = {0};
  size_t n = client.readBytesUntil('\n', status_line, sizeof(status_line) - 1);

  while (client.available()) {
    client.read();
  }
  client.stop();

  if (n == 0) {
    Serial.println("[HTTPS] sem resposta");
    return false;
  }

  Serial.print("[HTTPS] ");
  Serial.println(status_line);

  return strstr(status_line, " 200 ") != nullptr || strstr(status_line, " 201 ") != nullptr;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("TRONIK telemetria: MPU6050 + VL53L0X + HTTPS");

  Wire.begin(SDA_PIN, SCL_PIN);

  vl_ok = vl53.begin();
  Serial.println(vl_ok ? "[VL53L0X] OK" : "[VL53L0X] FALHA");

  mpu_ok = mpu.begin();
  if (mpu_ok) {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("[MPU6050] OK");
  } else {
    Serial.println("[MPU6050] FALHA");
  }

  connect_wifi();
  Serial.println("---------------");
}

void loop() {
  if (millis() - last_post_ms < POST_INTERVAL_MS) {
    delay(50);
    return;
  }
  last_post_ms = millis();

  SensorReadings readings = read_sensors();
  const float nivel_pct = mm_to_fill_percent(readings.vl53_mm);

  Serial.printf(
    "[LEITURA] VL53=%d mm | nivel=%.1f%% | ax=%.2f ay=%.2f az=%.2f\n",
    readings.vl53_mm,
    nivel_pct,
    readings.ax,
    readings.ay,
    readings.az
  );

  bool ok = post_https(readings);
  Serial.println(ok ? "[OK] POST HTTPS enviado" : "[ERRO] POST HTTPS falhou");
  Serial.println("---------------");
}
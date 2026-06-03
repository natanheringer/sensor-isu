#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Pinos canonicos do projeto
#define SDA_PIN 21
#define SCL_PIN 19

Adafruit_VL53L0X vl53 = Adafruit_VL53L0X();
Adafruit_MPU6050 mpu;

bool vl_ok = false;
bool mpu_ok = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println();
  Serial.println("Teste MPU6050 + VL53L0X iniciado");

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

  Serial.println("---------------");
}

void loop() {
  if (vl_ok) {
    VL53L0X_RangingMeasurementData_t measure;
    vl53.rangingTest(&measure, false);

    if (measure.RangeStatus == 4) {
      Serial.print("VL53L0X: fora de alcance");
    } else {
      Serial.print("VL53L0X: ");
      Serial.print(measure.RangeMilliMeter);
      Serial.print(" mm");
    }
  } else {
    Serial.print("VL53L0X: falha");
  }

  Serial.print(" | ");

  if (mpu_ok) {
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    mpu.getEvent(&accel, &gyro, &temp);

    Serial.print("MPU ax=");
    Serial.print(accel.acceleration.x, 2);
    Serial.print(" ay=");
    Serial.print(accel.acceleration.y, 2);
    Serial.print(" az=");
    Serial.print(accel.acceleration.z, 2);
  } else {
    Serial.print("MPU6050: falha");
  }

  Serial.println();
  delay(1000);
}

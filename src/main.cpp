#include <Wire.h>
#include <MPU6050.h>
#include <RadioLib.h>


#define LORA_CS  10
#define LORA_DIO1  3
#define LORA_RST  9
#define LORA_BUSY  7
#define AUDIO_PIN 5

// ================= MPU =================
MPU6050 mpu;

// ================= LoRa =================
Module* module = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
SX1262 lora = SX1262(module);

// ================= CONFIG =================
#define MOTION_THRESHOLD 0.15
#define TIMEOUT_MS 30000  // 30 seconds

unsigned long lastMovementTime = 0;
bool alarmTriggered = false;

// ================= AUDIO =================
#define AUDIO_CHANNEL 0
#define AUDIO_FREQ 2000
#define AUDIO_RES 8

void playAlarm() {
  ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
  ledcSetup(AUDIO_CHANNEL, AUDIO_FREQ, AUDIO_RES);

  // simple tone loop
  for (int i = 0; i < 100; i++) {
    ledcWrite(AUDIO_CHANNEL, 128); // 50% duty
    delay(200);
    ledcWrite(AUDIO_CHANNEL, 0);
    delay(200);
  }
}

void sendLoRaAlert() {
  String msg = "DEAD_MAN_ALERT";

  int state = lora.transmit(msg);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa sent");
  } else {
    Serial.print("LoRa error: ");
    Serial.println(state);
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 not connected!");
    while (1);
  }

  Serial.println("MPU6050 OK");

  // LoRa init
  Serial.print("Initializing LoRa...");
  int state = lora.begin(868.0); // EU frequency

  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("failed!");
  
  }
  Serial.println("success!");

  lastMovementTime = millis();
}

// ================= LOOP =================
void loop() {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Normalize accel (rough)
  float axf = ax / 16384.0;
  float ayf = ay / 16384.0;
  float azf = az / 16384.0;

  float magnitude = sqrt(axf * axf + ayf * ayf + azf * azf);

  static float lastMagnitude = 0;

  float delta = abs(magnitude - lastMagnitude);
  lastMagnitude = magnitude;

  // Movement detected
  if (delta > MOTION_THRESHOLD) {
    lastMovementTime = millis();
    alarmTriggered = false;
    Serial.println("Movement detected");
  }

  // Timeout check
  if (!alarmTriggered && (millis() - lastMovementTime > TIMEOUT_MS)) {
    Serial.println("ALARM TRIGGERED!");

    alarmTriggered = true;

    playAlarm();
    sendLoRaAlert();
  }

  delay(200);
}
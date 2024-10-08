#include <Arduino.h>
#include <CAN.h>
#include <ESP32Servo.h>
#include <PS4Controller.h>

const int WHEELBASE_X = 1;
const int WHEELBASE_Y = 1;
const int DEAD_ZONE = 30;

const unsigned int ID = 0x555; // ID
unsigned long long data;

uint8_t canData[8];

const uint8_t LAUNCHING_SERVO_PIN = 32;
Servo launchingServo;
bool launch_flag;

const int SET_DEGREE = 0;      // 装填角度
const int LAUNCH_DEGREE = 45;  // 発射角度
const int DEBOUNCE_DELAY = 50; // チャタリング防止

struct Motor_RPMs {
  int16_t frontLeft;
  int16_t frontRight;
  int16_t rearLeft;
  int16_t rearRight;
};

unsigned long long combineMotorRPMs(Motor_RPMs RPMs) {
  unsigned long long RPMdata = 0;

  // 各モーターの RPM 値をシフトして結合（16ビットに制限）
  RPMdata |= ((unsigned long long)((uint16_t)RPMs.frontLeft) << 48);
  RPMdata |= ((unsigned long long)((uint16_t)RPMs.frontRight) << 32);
  RPMdata |= ((unsigned long long)((uint16_t)RPMs.rearLeft) << 16);
  RPMdata |= (unsigned long long)((uint16_t)RPMs.rearRight);

  return RPMdata;
}

Motor_RPMs calculateWheelRPMs(int x, int y, int rotation) { // メカナムの計算
  Motor_RPMs RPMs;

  RPMs.frontLeft = (int16_t)(-x + y + (WHEELBASE_X + WHEELBASE_Y) * rotation);
  RPMs.frontRight = (int16_t)(x + y + (WHEELBASE_X + WHEELBASE_Y) * rotation);
  RPMs.rearLeft = (int16_t)(-x - y + (WHEELBASE_X + WHEELBASE_Y) * rotation);
  RPMs.rearRight = (int16_t)(x - y + (WHEELBASE_X + WHEELBASE_Y) * rotation);
  return RPMs;
}

void setup() {
  launchingServo.attach(LAUNCHING_SERVO_PIN);
  launchingServo.write(0);
  launch_flag = false; // サーボのロック状態

  Serial.begin(115200);
  if (!CAN.begin(1000E3)) {
    Serial.println("ERROR:Starting CAN failed!");
  }
  PS4.begin("08:B6:1F:ED:5E:34");
}

void loop() {

  if (!PS4.isConnected()) {
    Serial.println("ERROR:Cant PS4Connect!!");
    return;
  }
  static bool circle_pressed = false;
  static unsigned long circle_debounce_time = 0;

  if (PS4.Circle()) {
    if (!circle_pressed && millis() - circle_debounce_time > DEBOUNCE_DELAY) {
      if (!launch_flag) {
        launchingServo.write(LAUNCH_DEGREE);
      } else {
        launchingServo.write(SET_DEGREE);
      }
      launch_flag = !launch_flag;
      circle_debounce_time = millis();
    }
    circle_pressed = true;
  } else {
    circle_pressed = false;
  }

  int left_x = PS4.LStickX();
  int left_y = PS4.LStickY();
  int right_x = PS4.RStickX();
  // int left_y = PS4.LStickY();
  
  if (abs(left_x) < DEAD_ZONE) {
    left_x = 0;
  }
  if (abs(left_y) < DEAD_ZONE) {
    left_y = 0;
  }
  if (abs(right_x) < DEAD_ZONE) {
    right_x = 0;
  }

  Motor_RPMs RPMs;
  
  RPMs = calculateWheelRPMs(left_x, left_y, right_x);
  data = combineMotorRPMs(RPMs);

  Serial.printf("data: 0x%016llX\r\n", data);
  Serial.printf("FL::%x\r\n", uint16_t(RPMs.frontLeft));
  Serial.printf("FR::%X\r\n", uint16_t(RPMs.frontRight));
  Serial.printf("RL::%X\r\n", uint16_t(RPMs.rearLeft));
  Serial.printf("RR::%X\r\n", uint16_t(RPMs.rearRight));

  for (int i = 0; i < 8; i++) {
    canData[i] = (data >> (56 - i * 8)) & 0xFF;
  }

  CAN.beginPacket(ID);
  CAN.write(canData, sizeof(canData));
  CAN.endPacket();
}
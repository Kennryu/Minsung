/*
 * 리모컨으로 도어락 열기 (IR Remote Door Lock)
 *
 * 하드웨어
 *  - Arduino Uno
 *  - IR 수신 모듈 (VS1838B / TSOP38238)  -> D2
 *  - 서보 모터 (SG90)  -> D9   (또는 릴레이 + 솔레노이드 잠금장치)
 *  - 초록 LED (열림)    -> D5  (220Ω 저항)
 *  - 빨강 LED (잠김)    -> D6  (220Ω 저항)
 *  - 부저 (선택)        -> D8
 *
 * 사용법
 *  1. 리모컨 숫자 버튼으로 비밀번호(기본 1234)를 누르고 OK 버튼을 누르면 문이 열립니다.
 *  2. 열린 뒤 AUTO_LOCK_MS(기본 5초)가 지나면 자동으로 잠깁니다.
 *  3. 열려 있을 때 OK 버튼을 다시 누르면 바로 잠깁니다.
 *  4. 비밀번호를 MAX_FAILS 번 틀리면 LOCKOUT_MS 동안 입력이 막힙니다.
 *
 * 리모컨마다 버튼 코드가 다릅니다. 시리얼 모니터(9600bps)를 열고 버튼을 누르면
 * 코드가 출력되니, 아래 KEY_* 값을 내 리모컨 코드로 바꿔 주세요.
 *
 * 필요 라이브러리: IRremote (v4.x), Servo
 */

#include <IRremote.hpp>
#include <Servo.h>

// ---------------- 핀 설정 ----------------
const uint8_t PIN_IR       = 2;
const uint8_t PIN_SERVO    = 9;
const uint8_t PIN_LED_OPEN = 5;
const uint8_t PIN_LED_LOCK = 6;
const uint8_t PIN_BUZZER   = 8;

// ---------------- 동작 설정 ----------------
const char     PASSWORD[]    = "1234";   // 비밀번호 (숫자만, 최대 8자리)
const uint32_t AUTO_LOCK_MS  = 5000;     // 열린 뒤 자동 잠김 시간
const uint32_t INPUT_TIMEOUT = 10000;    // 입력 도중 멈추면 초기화
const uint8_t  MAX_FAILS     = 3;        // 허용 실패 횟수
const uint32_t LOCKOUT_MS    = 30000;    // 입력 차단 시간
const uint8_t  SERVO_LOCKED  = 0;        // 잠김 각도
const uint8_t  SERVO_OPEN    = 90;       // 열림 각도

// ---------------- 리모컨 버튼 코드 ----------------
// 아두이노 키트에 흔히 들어있는 21키 NEC 리모컨 기준 command 값입니다.
// 시리얼 모니터에 찍힌 "cmd=0x.." 값으로 바꾸세요.
const uint8_t KEY_0   = 0x16;
const uint8_t KEY_1   = 0x0C;
const uint8_t KEY_2   = 0x18;
const uint8_t KEY_3   = 0x5E;
const uint8_t KEY_4   = 0x08;
const uint8_t KEY_5   = 0x1C;
const uint8_t KEY_6   = 0x5A;
const uint8_t KEY_7   = 0x42;
const uint8_t KEY_8   = 0x52;
const uint8_t KEY_9   = 0x4A;
const uint8_t KEY_OK  = 0x40;   // 확인
const uint8_t KEY_CLR = 0x44;   // 입력 지우기

// ---------------- 상태 ----------------
Servo    lockServo;
bool     isOpen       = false;
uint32_t openedAt     = 0;
char     input[9]     = "";
uint8_t  inputLen     = 0;
uint32_t lastKeyAt    = 0;
uint8_t  failCount    = 0;
bool     lockedOut    = false;
uint32_t lockoutStart = 0;

int8_t keyToDigit(uint8_t cmd) {
  const uint8_t digits[10] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
                              KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
  for (int8_t i = 0; i < 10; i++) {
    if (digits[i] == cmd) return i;
  }
  return -1;
}

void beep(uint16_t freq, uint16_t ms) {
  tone(PIN_BUZZER, freq, ms);
  delay(ms + 20);
}

void clearInput() {
  inputLen = 0;
  input[0] = '\0';
}

void openDoor() {
  lockServo.write(SERVO_OPEN);
  digitalWrite(PIN_LED_OPEN, HIGH);
  digitalWrite(PIN_LED_LOCK, LOW);
  isOpen = true;
  openedAt = millis();
  beep(1500, 100);
  beep(2000, 150);
  Serial.println(F("\n>> 문 열림"));
}

void lockDoor() {
  lockServo.write(SERVO_LOCKED);
  digitalWrite(PIN_LED_OPEN, LOW);
  digitalWrite(PIN_LED_LOCK, HIGH);
  isOpen = false;
  beep(1000, 200);
  Serial.println(F(">> 문 잠김"));
}

// 비밀번호 비교 (중간에 멈추지 않고 끝까지 비교)
bool checkPassword() {
  size_t pwLen = strlen(PASSWORD);
  bool ok = (inputLen == pwLen);
  for (size_t i = 0; i < pwLen; i++) {
    char c = (i < inputLen) ? input[i] : 0;
    if (c != PASSWORD[i]) ok = false;
  }
  return ok;
}

void handleKey(uint8_t cmd) {
  uint32_t now = millis();

  if (lockedOut) {
    if (now - lockoutStart < LOCKOUT_MS) {
      Serial.println(F("입력 차단 중입니다. 잠시 후 다시 시도하세요."));
      beep(300, 300);
      return;
    }
    lockedOut = false;
  }

  if (cmd == KEY_OK) {
    if (isOpen) {            // 열려 있으면 OK로 바로 잠금
      lockDoor();
      clearInput();
      return;
    }
    if (checkPassword()) {
      failCount = 0;
      openDoor();
    } else {
      failCount++;
      Serial.print(F("\n비밀번호 틀림 ("));
      Serial.print(failCount);
      Serial.print('/');
      Serial.print(MAX_FAILS);
      Serial.println(')');
      beep(400, 150);
      beep(300, 300);
      if (failCount >= MAX_FAILS) {
        lockedOut = true;
        lockoutStart = now;
        failCount = 0;
        Serial.println(F("실패 횟수 초과: 잠시 동안 입력이 차단됩니다."));
      }
    }
    clearInput();
    return;
  }

  if (cmd == KEY_CLR) {
    clearInput();
    Serial.println(F("\n입력 초기화"));
    beep(800, 80);
    return;
  }

  int8_t d = keyToDigit(cmd);
  if (d >= 0 && inputLen < sizeof(input) - 1) {
    input[inputLen++] = '0' + d;
    input[inputLen] = '\0';
    lastKeyAt = now;
    Serial.print('*');
    beep(1200, 40);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_LED_OPEN, OUTPUT);
  pinMode(PIN_LED_LOCK, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lockServo.attach(PIN_SERVO);
  IrReceiver.begin(PIN_IR, DISABLE_LED_FEEDBACK);

  lockDoor();
  Serial.println(F("리모컨 도어락 준비 완료. 비밀번호 입력 후 OK를 누르세요."));
}

void loop() {
  if (IrReceiver.decode()) {
    // 버튼을 누르고 있을 때 오는 반복 신호는 무시
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      uint8_t cmd = IrReceiver.decodedIRData.command;
      Serial.print(F("[IR] addr=0x"));
      Serial.print(IrReceiver.decodedIRData.address, HEX);
      Serial.print(F(" cmd=0x"));
      Serial.println(cmd, HEX);
      handleKey(cmd);
    }
    IrReceiver.resume();
  }

  uint32_t now = millis();

  // 자동 잠금
  if (isOpen && now - openedAt >= AUTO_LOCK_MS) {
    lockDoor();
  }

  // 입력 도중 오래 멈추면 초기화
  if (inputLen > 0 && now - lastKeyAt >= INPUT_TIMEOUT) {
    clearInput();
    Serial.println(F("\n입력 시간 초과로 초기화"));
  }
}

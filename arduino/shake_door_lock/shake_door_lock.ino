/*
 * 폰 흔들기 도어락 (Shake-to-Open Door Lock) — ESP32 펌웨어
 * ---------------------------------------------------------
 * 스마트폰 웹페이지(docs/index.html)가 흔들기를 감지하면
 * BLE(블루투스 저전력)로 "OPEN:<PIN>" 명령을 보낸다.
 * ESP32는 PIN을 확인한 뒤 릴레이를 UNLOCK_MS 동안 켜서 도어락을 연다.
 *
 * 보드 : ESP32 Dev Module (Arduino IDE > 보드 매니저 > "esp32" by Espressif 설치)
 * 라이브러리 : ESP32 코어에 포함된 BLE 라이브러리 (추가 설치 불필요)
 *
 * 배선(기본값)
 *   GPIO 26  -> 릴레이 모듈 IN   (릴레이가 12V 솔레노이드 도어락 전원을 개폐)
 *   GPIO 2   -> 상태 LED (대부분의 ESP32 보드에 내장)
 *   GPIO 27  -> 부저(선택)
 *   GPIO 14  -> 실내 수동 열림 버튼(선택, 버튼 반대쪽은 GND)
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ======================= 설정값 =======================
const char* DEVICE_NAME = "ShakeLock";   // 폰에서 보이는 BLE 이름
const char* SECRET_PIN  = "1234";        // ★ 반드시 바꾸세요 (웹페이지에도 같은 값 입력)

const int  RELAY_PIN   = 26;
const int  LED_PIN     = 2;
const int  BUZZER_PIN  = 27;
const int  BUTTON_PIN  = 14;
const bool RELAY_ACTIVE_HIGH = true;     // 릴레이가 LOW에서 동작하는 모듈이면 false

const unsigned long UNLOCK_MS        = 3000;   // 문 열림 유지 시간
const unsigned long COOLDOWN_MS      = 2000;   // 연속 열림 방지
const int           MAX_FAILS        = 5;      // PIN 연속 오류 허용 횟수
const unsigned long LOCKOUT_MS       = 60000;  // 오류 초과 시 잠금 시간
// =====================================================

// UUID (웹페이지와 반드시 동일해야 함)
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dc4a10"
#define CMD_UUID     "6e400002-b5a3-f393-e0a9-e50e24dc4a10"  // 폰 -> ESP32 (write)
#define STATUS_UUID  "6e400003-b5a3-f393-e0a9-e50e24dc4a10"  // ESP32 -> 폰 (notify)

BLECharacteristic* statusChar = nullptr;
bool deviceConnected = false;
bool wasConnected = false;

volatile bool openRequested = false;     // BLE 콜백 -> loop 로 전달
bool unlocked = false;
unsigned long unlockedAt = 0;
unsigned long lastOpenAt = 0;
int failCount = 0;
unsigned long lockoutUntil = 0;
unsigned long openCount = 0;             // 리빙랩 사용 통계용

void setRelay(bool on) {
  digitalWrite(RELAY_PIN, (on == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
}

void beep(int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

void sendStatus(const String& msg) {
  Serial.println("[STATUS] " + msg);
  if (statusChar && deviceConnected) {
    statusChar->setValue(msg.c_str());
    statusChar->notify();
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer*) override {
    deviceConnected = false;
  }
};

class CmdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    // ESP32 코어 2.x(std::string)와 3.x(String) 모두 호환
    String v = String(c->getValue().c_str());
    v.trim();

    unsigned long now = millis();
    if (now < lockoutUntil) {
      sendStatus("LOCKOUT:" + String((lockoutUntil - now) / 1000));
      return;
    }

    if (v == "PING") {
      sendStatus(unlocked ? "OPEN" : "LOCKED");
      return;
    }

    if (v.startsWith("OPEN:")) {
      String pin = v.substring(5);
      if (pin == SECRET_PIN) {
        failCount = 0;
        openRequested = true;
      } else {
        failCount++;
        if (failCount >= MAX_FAILS) {
          lockoutUntil = now + LOCKOUT_MS;
          failCount = 0;
          sendStatus("LOCKOUT:" + String(LOCKOUT_MS / 1000));
        } else {
          sendStatus("DENIED:" + String(MAX_FAILS - failCount));
        }
      }
      return;
    }

    sendStatus("UNKNOWN");
  }
};

void doUnlock(const char* source) {
  unsigned long now = millis();
  if (unlocked) return;
  if (lastOpenAt != 0 && now - lastOpenAt < COOLDOWN_MS) {
    sendStatus("COOLDOWN");
    return;
  }
  unlocked = true;
  unlockedAt = now;
  lastOpenAt = now;
  openCount++;
  setRelay(true);
  digitalWrite(LED_PIN, HIGH);
  Serial.printf("[OPEN] source=%s count=%lu uptime=%lus\n", source, openCount, now / 1000);
  sendStatus("OPEN");
  beep(80);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setRelay(false);

  BLEDevice::init(DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);

  BLECharacteristic* cmdChar = service->createCharacteristic(
      CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
  cmdChar->setCallbacks(new CmdCallbacks());

  statusChar = service->createCharacteristic(
      STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusChar->addDescriptor(new BLE2902());
  statusChar->setValue("LOCKED");

  service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("ShakeLock 준비 완료. 폰에서 연결하세요.");
}

void loop() {
  unsigned long now = millis();

  if (openRequested) {
    openRequested = false;
    doUnlock("phone");
  }

  // 실내 수동 버튼 (선택)
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(30);
    if (digitalRead(BUTTON_PIN) == LOW) doUnlock("button");
  }

  // 자동 재잠금
  if (unlocked && now - unlockedAt >= UNLOCK_MS) {
    unlocked = false;
    setRelay(false);
    digitalWrite(LED_PIN, LOW);
    sendStatus("LOCKED");
  }

  // 연결 끊기면 다시 광고 시작
  if (!deviceConnected && wasConnected) {
    delay(300);
    BLEDevice::startAdvertising();
    Serial.println("연결 끊김 -> 재광고");
  }
  wasConnected = deviceConnected;

  delay(10);
}

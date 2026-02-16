/*
   SERVER ESP32 (KANTAR TARAFI)
   - BLE Server oluşturur.
   - Kantardan (Serial2) veri okur -> Client'a (PC tarafına) yollar.
   - Client'tan gelen Baudrate bilgisini alıp Kalıcı Hafızaya (Preferences) kaydeder.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

#define DPSW5 23 // 1
#define DPSW4 22 // 2
#define DPSW3 21 // 3
#define DPSW2 19 // 4
#define DPSW1 18 // 5
#define DPSW0 5 // 6

#define LED_STA 26 // mavi
#define LED_RUN 27 // yesil

#define RXD2 16
#define TXD2 17

const String serviceUuidBase = "8bc3c602-a3dd-430a-bcb5-79401f8c83";
#define CHARACTERISTIC_WEIGHT  "beb5483e-36e1-4688-b7f5-ea07361b26a8" // Ağırlık yollamak için (Notify)
#define CHARACTERISTIC_BAUD    "e3223119-9413-4296-bc32-1327c41c7b8c" // Ayar almak için (Write)
BLEUUID serviceUUID;

BLEServer* pServer = NULL;
BLECharacteristic* pWeightCharacteristic = NULL;
BLECharacteristic* pBaudCharacteristic = NULL;

bool deviceConnected = false;
Preferences preferences;
int kantarBaudRate = 9600;

uint8_t readDipSwitches() {
  uint8_t address = 0;
  address += digitalRead(DPSW0) * 32;
  address += digitalRead(DPSW1) * 16;
  address += digitalRead(DPSW2) * 8;
  address += digitalRead(DPSW3) * 4;
  address += digitalRead(DPSW4) * 2;
  address += digitalRead(DPSW5);
  return address;
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(LED_STA, LOW);
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(LED_STA, HIGH);
      pServer->startAdvertising();
    }
};

class MyBaudCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue().c_str();
      if (value.length() > 0) {

        int yeniHiz = value.toInt();
        if (yeniHiz > 0) {
           preferences.begin("ayarlar", false);
           preferences.putInt("baud", yeniHiz);
           preferences.end();
           
           delay(1000);
           ESP.restart();
        }
      }
    }
};

void setup() {
  Serial.begin(115200); // debug icin
  
  pinMode(LED_STA, OUTPUT);
  pinMode(LED_RUN, OUTPUT);
  digitalWrite(LED_STA, HIGH);
  digitalWrite(LED_RUN, LOW);

  pinMode(DPSW5, INPUT);
  pinMode(DPSW4, INPUT);
  pinMode(DPSW3, INPUT);
  pinMode(DPSW2, INPUT);
  pinMode(DPSW1, INPUT);
  pinMode(DPSW0, INPUT);

  uint8_t dipAddress = readDipSwitches();
  char hexBuffer[3];
  sprintf(hexBuffer, "%02x", dipAddress);

  String fullServiceStr = serviceUuidBase + String(hexBuffer);
  serviceUUID = BLEUUID(fullServiceStr.c_str());

  String deviceName = "Server_" + String(hexBuffer);

  preferences.begin("ayarlar", true);
  kantarBaudRate = preferences.getInt("baud", 9600);
  preferences.end();

  Serial2.begin(kantarBaudRate, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(200);

  BLEDevice::init("");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(serviceUUID);

  pWeightCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_WEIGHT,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pWeightCharacteristic->addDescriptor(new BLE2902());

  pBaudCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_BAUD,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pBaudCharacteristic->setCallbacks(new MyBaudCallbacks());

  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0); 
  BLEDevice::startAdvertising();
}

String sonOkunanVeri = "";
unsigned long sonGondermeZamani = 0;
const int gondermeAraligi = 500;

void loop() {
  if (Serial2.available()) {
    String gelen = Serial2.readStringUntil('\r');
    gelen.trim();
    if (gelen.length() > 0) {
      sonOkunanVeri = gelen; 
    }
  }
  
  if (deviceConnected) {
    unsigned long suAn = millis();
    if (suAn - sonGondermeZamani >= gondermeAraligi) {
      if (sonOkunanVeri.length() > 0) {
        pWeightCharacteristic->setValue(sonOkunanVeri.c_str());
        pWeightCharacteristic->notify();
      }
      sonGondermeZamani = suAn;
    }
  }
  delay(20);
}
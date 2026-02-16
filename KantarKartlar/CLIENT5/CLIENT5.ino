/*
   CLIENT ESP32 (PC TARAFI)
   - USB ile bilgisayara bağlıdır.
   - BLE Server'ı (Kantar) bulur ve bağlanır.
   - Server'dan gelen Notify (Ağırlık) verisini Serial.print ile PC'ye basar.
   - PC'den "REMOTE_BAUD:xxxx" gelirse Server'a yazar.
*/
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

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

BLEUUID serviceUUID;
static BLEUUID charWeightUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID charBaudUUID("e3223119-9413-4296-bc32-1327c41c7b8c");
const String uuidBase = "8bc3c602-a3dd-430a-bcb5-79401f8c83";

bool doConnect = false;
bool connected = false;
BLERemoteCharacteristic* pRemoteWeight;
BLERemoteCharacteristic* pRemoteBaud;
BLEAdvertisedDevice* myDevice;


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


// --- VERİ GELDİĞİNDE ÇALIŞAN FONKSİYON (CALLBACK) ---
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    for (int i = 0; i < length; i++) {
       char c = (char)pData[i];
       if (isdigit(c) || c == '.' || c == '-') {
          Serial.print(c);
       }
    }
    Serial.println();

    /*
    static String sonGelenVeri = ""; // static yerine global olacak şekilde dışta tanımlayalım
    String gelen = "";
    for (int i = 0; i < length; i++) {
       char c = (char)pData[i];
       if (isdigit(c) || c == '.' || c == '-') {
          gelen += c;
       }
    }
    
    if (gelen != sonGelenVeri) {
      Serial.println(gelen);
    }
    
    sonGelenVeri = gelen;
    */
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    digitalWrite(LED_STA, HIGH);
  }
};

bool connectToServer() {

    BLEClient* pClient = BLEDevice::createClient();

    pClient->setClientCallbacks(new MyClientCallback());

    if(!pClient->connect(myDevice)) {
      return false;
    }

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      pClient->disconnect();
      return false;
    }

    // 1. Ağırlık Karakteristiğini Bul ve Abone Ol (Notify)
    pRemoteWeight = pRemoteService->getCharacteristic(charWeightUUID);
    if (pRemoteWeight == nullptr) return false;

    if(pRemoteWeight->canNotify()) {
      pRemoteWeight->registerForNotify(notifyCallback);
    }

    // 2. Baudrate Ayar Karakteristiğini Bul (Write)
    pRemoteBaud = pRemoteService->getCharacteristic(charBaudUUID);
    if (pRemoteBaud == nullptr) return false;
    
    connected = true;
    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();

      if (myDevice != nullptr) {
        delete myDevice;
        myDevice = nullptr;
      }

      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

void setup() {
  Serial.begin(115200, SERIAL_8N1, RXD2, TXD2);

  pinMode(LED_STA, OUTPUT);
  pinMode(LED_RUN, OUTPUT);
  digitalWrite(LED_RUN, LOW); // güç varsa yanacak

  pinMode(DPSW5, INPUT);
  pinMode(DPSW4, INPUT);
  pinMode(DPSW3, INPUT);
  pinMode(DPSW2, INPUT);
  pinMode(DPSW1, INPUT);
  pinMode(DPSW0, INPUT);

  digitalWrite(LED_STA, HIGH); // bağlanırsa yanacak

  uint8_t dipAddress = readDipSwitches();
  char hexBuffer[3];
  sprintf(hexBuffer, "%02x", dipAddress);

  String fullUUIDStr = uuidBase + String(hexBuffer); // Ana parçayla birleştir
  serviceUUID = BLEUUID(fullUUIDStr.c_str()); // BLEUUID objesini oluştur

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);  // İlk taramayı başlat (5 saniye)
}

void loop() {
  if (doConnect == true) {
    if(connectToServer()){
      digitalWrite(LED_STA, LOW);
    } else {
      connected = false; 
    }
    doConnect = false;
  }

  if (connected) {
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      if (command.startsWith("REMOTE_BAUD:")) {
        String baudValue = command.substring(12);
        if (pRemoteBaud != nullptr) { // Server'a gönder
           pRemoteBaud->writeValue(baudValue.c_str(), baudValue.length());
        }
      }
    }
  } else {
    BLEDevice::getScan()->start(5, false);
  }
  
  delay(100);
}
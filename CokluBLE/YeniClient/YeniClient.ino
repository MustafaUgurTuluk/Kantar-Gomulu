#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>

#define LED_STA 26 // mavi
#define LED_RUN 27 // yesil

#define RXD2 16
#define TXD2 17

// Sunucu ile BİREBİR AYNI olması gereken UUID'lerimiz
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// Bulunan yeni cihazları ve zaten bağlı olduklarımızı takip edeceğimiz listeler
std::vector<BLEAdvertisedDevice*> devicesToConnect;
std::vector<String> connectedMACs;

bool doScan = false;

// Sunuculardan veri (Notify) geldiğinde tetiklenen fonksiyon
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    
    String receivedData = "";
    for (int i = 0; i < length; i++) {
        receivedData += (char)pData[i];
    }
    
    // VIRGÜLDEN PARÇALAMA İŞLEMİ
    int commaIndex = receivedData.indexOf(',');
    if (commaIndex > 0) {
        String macAddress = receivedData.substring(0, commaIndex);
        String sensorValue = receivedData.substring(commaIndex + 1);
        
        Serial.print(macAddress);
        Serial.print(" | ");
        Serial.println(sensorValue);
    }
}

// Bağlantı kopmalarını anında yakalayan sınıf
class MyClientCallback : public BLEClientCallbacks {

  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    // Kopan cihazın MAC adresini bul
    String disconnectedMAC = pclient->getPeerAddress().toString().c_str();
    Serial.print("Baglanti KOPARILDI -> MAC: ");
    Serial.println(disconnectedMAC);

    // Kopan cihazı "Bağlılar" listemizden (connectedMACs) sil
    for (int i = 0; i < connectedMACs.size(); i++) {
        if (connectedMACs[i] == disconnectedMAC) {
            connectedMACs.erase(connectedMACs.begin() + i); // Listeden çıkar
            break;
        }
    }

    // --- HİÇ BAĞLI CİHAZ KALMADIYSA LED'İ SÖNDÜR ---
    if (connectedMACs.size() == 0) {
        digitalWrite(LED_STA, HIGH); // LED Söndür (Active-Low)
        Serial.println("Sistemde bagli cihaz kalmadi. LED sondu.");
    }
  }
};


// Hedefe bağlanma işlemini yapan fonksiyon
bool connectToServer(BLEAdvertisedDevice* myDevice) {
    String deviceMAC = myDevice->getAddress().toString().c_str();
    Serial.print("Baglaniliyor -> MAC: ");
    Serial.println(deviceMAC);
    
    // HER SUNUCU İÇİN AYRI BİR İSTEMCİ (CLIENT) NESNESİ OLUŞTURUYORUZ
    BLEClient* pClient = BLEDevice::createClient();

    pClient->setClientCallbacks(new MyClientCallback());

    // Cihaza bağlan
    if (!pClient->connect(myDevice)) {
        Serial.println("Fiziksel baglanti basarisiz.");
        return false;
    }
    Serial.println("Fiziksel baglanti kuruldu!");

    // Departmanı (Service) bul
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.println("Hata: Servis bulunamadi.");
      pClient->disconnect();
      return false;
    }

    // Veri Kutusunu (Characteristic) bul
    BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.println("Hata: Karakteristik bulunamadi.");
      pClient->disconnect();
      return false;
    }

    // Bildirimleri (Notify) aç
    if(pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    // Başarıyla bağlandıysak MAC adresini "Bağlılar" listesine ekle
    connectedMACs.push_back(deviceMAC);
    digitalWrite(LED_STA, LOW);

    return true;
}

// Ortamı dinleyip doğru cihazları listeye ekleyen "Hafiye" sınıfı
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Bizim UUID mi?
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      String foundMAC = advertisedDevice.getAddress().toString().c_str();
      
      // Bu MAC adresi zaten bağlı listemizde var mı?
      bool isAlreadyConnected = false;
      for (int i = 0; i < connectedMACs.size(); i++) {
          if (connectedMACs[i] == foundMAC) {
              isAlreadyConnected = true;
              break;
          }
      }

      // Bağlı değilsek ve bağlanılacaklar listesinde de yoksa listeye ekle
      if (!isAlreadyConnected) {
          Serial.print("Yeni Server Bulundu! MAC: ");
          Serial.println(foundMAC);
          devicesToConnect.push_back(new BLEAdvertisedDevice(advertisedDevice));
      }
    }
  }
};

void setup() {
  Serial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Coklu BLE Istemci (Multi-Client) baslatiliyor...");

  pinMode(LED_RUN, OUTPUT);
  digitalWrite(LED_RUN, LOW);
  pinMode(LED_STA, OUTPUT);
  digitalWrite(LED_STA, HIGH);

  BLEDevice::init("");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false); // 5 saniye dinle
}

void loop() {
  // Bağlanılmayı bekleyen cihazlar varsa sırayla bağlan
  if (devicesToConnect.size() > 0) {
      for (int i = 0; i < devicesToConnect.size(); i++) {
          connectToServer(devicesToConnect[i]);
          delete devicesToConnect[i]; // ÖNEMLİ: Hafızayı serbest bırak!
          delay(500); // Art arda bağlantılarda BLE yığınının çökmemesi için ufak bir bekleme
      }
      // Bağlantı denemeleri bittikten sonra listeyi temizle
      devicesToConnect.clear(); 
      doScan = true; // Yeniden tarama yapabilmek için bayrağı kaldır
  }

  // Listede bağlanacak cihaz kalmadıysa, ortamda kopan veya yeni gelen var mı diye tekrar tara
  if (devicesToConnect.size() == 0 && doScan) {
      doScan = false;
      BLEDevice::getScan()->start(5, false);
      doScan = true;
  }
  
  delay(2000); 
}
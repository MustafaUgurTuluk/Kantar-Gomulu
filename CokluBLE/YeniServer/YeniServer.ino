#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Bütün Server kartlarda AYNI kalacak olan ortak yolumuz (UUID'ler)
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define LED_STA 26 // mavi
#define LED_RUN 27 // yesil

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Cihazın kendi MAC adresini tutacağımız değişken
std::string myMacAddress;

// Bağlantı durumunu kontrol eden Callbacks sınıfı
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(LED_STA, LOW);
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(LED_STA, HIGH);
    }
};

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_STA, OUTPUT);
  pinMode(LED_RUN, OUTPUT);
  digitalWrite(LED_STA, HIGH);
  digitalWrite(LED_RUN, LOW);
  
  // Rastgele sayı üreticiyi başlat (Opsiyonel, daha iyi random için)
  randomSeed(analogRead(0));

  // BLE'yi başlat
  BLEDevice::init("ESP32_Ortak_Sensor"); 
  
  // İŞTE KRİTİK NOKTA: Cihazın kendi fabrikasyon MAC adresini otomatik okuyoruz
  myMacAddress = BLEDevice::getAddress().toString();
  Serial.print("Bu Cihazin MAC Adresi: ");
  Serial.println(myMacAddress.c_str());

  // BLE Sunucusunu oluştur
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // BLE Servisini oluştur (Departman)
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // BLE Karakteristiğini oluştur (Veri Dosyası)
  // Hem okunabilir (READ) hem de anlık bildirim gönderebilir (NOTIFY) yapıyoruz
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  // Notify özelliği için Descriptor ekliyoruz (Standart BLE prosedürü)
  pCharacteristic->addDescriptor(new BLE2902());

  // Servisi başlat
  pService->start();

  // Yayına (Advertising) başla ki Client bizi bulabilsin
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0);  
  BLEDevice::startAdvertising();
  Serial.println("BLE Sunucusu baslatildi, havaya UUID yayini yapiliyor...");
}

void loop() {
  // Eğer bir Client bize bağlandıysa veri gönderelim
  if (deviceConnected) {
      
      // 10 ile 99 arasında rastgele bir sensör verisi üretiyoruz
      int randomData = random(10, 100); 
      
      // Göndereceğimiz mesajı hazırlıyoruz. Format: "MAC_ADRESI,VERI"
      // Örnek: "24:6f:28:aa:bb:cc,45"
      char txString[50];
      sprintf(txString, "%s,%d", myMacAddress.c_str(), randomData);
      
      // Karakteristiğin değerini güncelliyoruz
      pCharacteristic->setValue((uint8_t*)txString, strlen(txString));
      
      // Bağlı olan Client'a "yeni veri var" diye bildiriyoruz (Notify)
      pCharacteristic->notify();
      
      Serial.print("Istemciye Gonderilen: ");
      Serial.println(txString);
      
      // Veriyi 2 saniyede bir gönder (Projenize göre ayarlayabilirsiniz)
      delay(2000); 
  }
  
  // Kopma durumunda yayını tekrar başlatma mekanizması
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // Bluetooth yığınının toparlanması için kısa bir bekleme
      // Eski satır: pServer->startAdvertising();
      // YENİ SATIR: (Ana advertising nesnesini çağırın)
      BLEDevice::startAdvertising(); 
      Serial.println("Yayin yeniden baslatildi...");
      oldDeviceConnected = deviceConnected;
  }
  
  // Yeni bağlantı kurulduğunda durumu güncelle
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
}
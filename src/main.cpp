#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BluetoothSerial.h>
#include <esp_bt_device.h>
#include <BLECharacteristic.h>
#include <BLERemoteCharacteristic.h>
#include <AESLib.h>
#include <EEPROM.h>

#define BAUD 115200
#define BUTTON_PIN 2
#define BUTTON_PRESSED (digitalRead(BUTTON_PIN) == HIGH)

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic_1 = NULL;
BLECharacteristic *pCharacteristic_2 = NULL;
BLECharacteristic *pCharacteristic_3 = NULL;
BLECharacteristic *pCharacteristic_4 = NULL;
BLECharacteristic *pCharacteristic_5 = NULL;
BLECharacteristic *pCharacteristic_6 = NULL;
BLE2902 *pBLE2902_1;
BLE2902 *pBLE2902_2;
BLE2902 *pBLE2902_3;
BLE2902 *pBLE2902_4;
BLE2902 *pBLE2902_5;
BLE2902 *pBLE2902_6;
// generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID          "d9327ccb-992b-4d78-98ce-2297ed2c09d6"
#define CHARACTERISTIC_UUID_1 "e95e7f63-f041-469f-90db-04d2e3e7619b"
#define CHARACTERISTIC_UUID_2 "8c233b56-4988-4c3c-95b5-ec5b3c179c91"
#define CHARACTERISTIC_UUID_3 "56bba80a-91f1-46ab-b892-7325e19c3429"
#define CHARACTERISTIC_UUID_4 "53a15a66-6dd7-4421-9468-38cf731a77db"
#define CHARACTERISTIC_UUID_5 "acc9a30f-9e44-4323-8193-7bac8c9bc484"
#define CHARACTERISTIC_UUID_6 "9800d290-19fb-4085-9610-f1e878725ad2"

bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("Connected to client");
  };
  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("Disconnected from client");
  }
  void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
  {
    BLEAddress remoteAddress(param->connect.remote_bda);
    std::string remoteAddressStr = remoteAddress.toString();
    Serial.println("/****************************/");
    Serial.print("Connected to device with address: ");
    Serial.println(remoteAddressStr.c_str());  
    Serial.println("/****************************/");
  }
};


AESLib aes;
#define INPUT_BUFFER_LIMIT (16 + 1) // TEST + \0
uint8_t enBuffer[INPUT_BUFFER_LIMIT] = {0};
uint8_t deBuffer[INPUT_BUFFER_LIMIT] = {0};
byte dec_iv[N_BLOCK]        = {0};

static byte aes_key[]       = { 0xdf, 0x4f, 0x66, 0x80, 0x9b, 0xaf, 0xe5, 0x8f, 0xe1, 0x6a, 0xba, 0x7f, 0x4c, 0xb3, 0xe8, 0xbc };
static byte aes_iv[N_BLOCK] = { 0x3b, 0x69, 0x88, 0x60, 0x53, 0x84, 0x01, 0xcd, 0xd1, 0x46, 0x50, 0x42, 0x78, 0xae, 0xec, 0xc9 };

int16_t encryptAndSend(byte msg[], uint16_t msgLen, byte iv[]) {
  Serial.print("Calling encrypt... "); Serial.print(msgLen); Serial.println(" Bytes");
  uint16_t enc_bytes = aes.encrypt(msg, msgLen, (byte*)enBuffer, aes_key, sizeof(aes_key), iv);
  pCharacteristic_1->setValue(enBuffer, msgLen);
  pCharacteristic_1->notify();
  return enc_bytes;
}

int16_t decryptAndPrint(byte msg[], uint16_t msgLen, byte iv[]) 
{
  Serial.print("Calling decrypt... "); Serial.print(msgLen); Serial.println(" Bytes");
  uint16_t dec_bytes = aes.decrypt(msg, msgLen, (byte*)deBuffer, aes_key, sizeof(aes_key), iv);
  Serial.print("Decrypted bytes: "); Serial.println(dec_bytes);
  return dec_bytes;
}

void setup() {
  Serial.begin(BAUD);
  Serial.setTimeout(60000);

  pinMode(BUTTON_PIN, INPUT);      /*when pressed should let us save new key and vector in memory*/

  // Create the BLE Device
  BLEDevice::init("ESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic_1 = pService->createCharacteristic(     /*AUTH STATUS*/
    CHARACTERISTIC_UUID_1,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_1 = new BLE2902();
  pBLE2902_1->setNotifications(true);
  pCharacteristic_1->addDescriptor(pBLE2902_1);

  pCharacteristic_2 = pService->createCharacteristic(         /*KEY*/
    CHARACTERISTIC_UUID_2,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_2 = new BLE2902();
  pBLE2902_2->setNotifications(true);
  pCharacteristic_2->addDescriptor(pBLE2902_2);

  pCharacteristic_3 = pService->createCharacteristic(           /*VECTOR*/
    CHARACTERISTIC_UUID_3,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_3 = new BLE2902();
  pBLE2902_3->setNotifications(true);
  pCharacteristic_3->addDescriptor(pBLE2902_3);

  pCharacteristic_4 = pService->createCharacteristic(         /*trans random value*/
    CHARACTERISTIC_UUID_4,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_4 = new BLE2902();
  pBLE2902_4->setNotifications(true);
  pCharacteristic_4->addDescriptor(pBLE2902_4);

  pCharacteristic_5 = pService->createCharacteristic(          /*recive the encrypted random value*/
    CHARACTERISTIC_UUID_5,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_5 = new BLE2902();
  pBLE2902_5->setNotifications(true);
  pCharacteristic_5->addDescriptor(pBLE2902_5);

  pCharacteristic_6 = pService->createCharacteristic(           /*action control*/
    CHARACTERISTIC_UUID_6,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_6 = new BLE2902();
  pBLE2902_6->setNotifications(true);
  pCharacteristic_6->addDescriptor(pBLE2902_6);

  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0); // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
  delay(2000);
}

void loop() {
  if (deviceConnected)
  {
    if (BUTTON_PRESSED)
    {
    }
    else
    {
    }
    
    
  }
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    //Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }
}

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

#define BAUD 115200

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic_1 = NULL;
BLECharacteristic *pCharacteristic_2 = NULL;
BLE2902 *pBLE2902_1;
BLE2902 *pBLE2902_2;
// generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_1 "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_2 "fc477e34-adb4-4d01-b56e-d0a2671ecc39"
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


AESLib aesLib;
#define INPUT_BUFFER_LIMIT (16 + 1) // TEST + \0
unsigned char enBuffer[INPUT_BUFFER_LIMIT] = {0};
unsigned char deBuffer[INPUT_BUFFER_LIMIT] = {0};
byte dec_iv[N_BLOCK]        = {0};

static byte aes_key[]       = { 0xdf, 0x4f, 0x66, 0x80, 0x9b, 0xaf, 0xe5, 0x8f, 0xe1, 0x6a, 0xba, 0x7f, 0x4c, 0xb3, 0xe8, 0xbc };
static byte aes_iv[N_BLOCK] = { 0x3b, 0x69, 0x88, 0x60, 0x53, 0x84, 0x01, 0xcd, 0xd1, 0x46, 0x50, 0x42, 0x78, 0xae, 0xec, 0xc9 };

int16_t encryptAndSend(byte msg[], uint16_t msgLen, byte iv[]) {
  Serial.print("Calling encrypt... "); Serial.print(msgLen); Serial.println(" Bytes");
  uint16_t enc_bytes = aesLib.encrypt(msg, msgLen, (byte*)enBuffer, aes_key, sizeof(aes_key), iv);
  pCharacteristic_1->setValue(enBuffer, msgLen);
  pCharacteristic_1->notify();
  return enc_bytes;
}

int16_t decryptAndPrint(byte msg[], uint16_t msgLen, byte iv[]) 
{
  Serial.print("Calling decrypt... "); Serial.print(msgLen); Serial.println(" Bytes");
  uint16_t dec_bytes = aesLib.decrypt(msg, msgLen, (byte*)deBuffer, aes_key, sizeof(aes_key), iv);
  Serial.print("Decrypted bytes: "); Serial.println(dec_bytes);
  return dec_bytes;
}

void setup() {
  Serial.begin(BAUD);
  Serial.setTimeout(60000);

  // Create the BLE Device
  BLEDevice::init("ESP32-server");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic_1 = pService->createCharacteristic(
      CHARACTERISTIC_UUID_1,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_INDICATE);
  pCharacteristic_2 = pService->createCharacteristic(
      CHARACTERISTIC_UUID_2,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_INDICATE);

  pBLE2902_1 = new BLE2902();
  pBLE2902_1->setNotifications(true);
  pCharacteristic_1->addDescriptor(pBLE2902_1);
  pBLE2902_2 = new BLE2902();
  pBLE2902_2->setNotifications(true);
  pCharacteristic_2->addDescriptor(pBLE2902_2);

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
  // reset initialization vector
  memcpy(dec_iv, aes_iv, sizeof(aes_iv));
  /***********send and encrypt data***********/
  unsigned char msgToSend[] = { "7madaaaaaaaaaaa" };
  int16_t encLen = encryptAndSend(msgToSend, sizeof(msgToSend), dec_iv);  
  Serial.print("Encrypted ciphertext of length: "); Serial.println(encLen);
  Serial.print("Encrypted ciphertext:\n"); Serial.println((char*)enBuffer);

  // reset initialization vector for decryption
  memcpy(dec_iv, aes_iv, sizeof(aes_iv));
  /***********wait and decrypt data***********/
  std::string receivedValue = pCharacteristic_2->getValue();
  if (!receivedValue.empty())
    {
      int receivedLength = receivedValue.length();
      byte receivedMsg[receivedLength];
      memcpy(receivedMsg, receivedValue.c_str(), receivedLength);
      int16_t decLen = decryptAndPrint(receivedMsg, receivedLength , dec_iv);   
      Serial.print("Decrypted cleartext of length: "); Serial.println(decLen);
      Serial.print("Decrypted cleartext:\n"); Serial.println((char*)deBuffer);
    }
  Serial.println("-----------------------");
  Serial.println();
  delay(1000);
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

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

#define BUTTON_PIN  4
#define LOCK_PIN    2

#define BUTTON_PRESSED (digitalRead(BUTTON_PIN) == HIGH)

bool isAuthorized = false;

bool deviceConnected = false;
bool oldDeviceConnected = false;

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

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    isAuthorized = false;
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
    Serial.print("Connected to device with address: ");
    Serial.println(remoteAddressStr.c_str());  
  }
};

AESLib aes;

static byte aesKey[16] = { 0 };
static byte aesIv[16]  = { 0 };

void setup() 
{
  Serial.begin(BAUD);
  Serial.setTimeout(60000);

  pinMode(BUTTON_PIN, INPUT);      /*when pressed should let us save new key and vector in memory*/
  pinMode(LOCK_PIN  , OUTPUT); 

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

  // Start advertising
  pService->start();
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
      // Button is pressed, receive new AES key and vector
      byte newKey[sizeof(aesKey)];
      byte newVector[16];
      // Receive new AES key via pCharacteristic_2
      std::string receivedKey    = pCharacteristic_2->getValue();
      if (!receivedKey.empty())    memcpy(newKey, receivedKey.c_str(), sizeof(aesKey));
      // Receive new vector via pCharacteristic_3
      std::string receivedVector = pCharacteristic_3->getValue();
      if (!receivedVector.empty()) memcpy(newVector, receivedVector.c_str(), 16);
      // Save new key and vector to EEPROM
      EEPROM.begin(sizeof(newKey) + sizeof(newVector));
      for (int i = 0; i < sizeof(newKey); i++) 
      {
        EEPROM.write(i, newKey[i]);
      }
      for (int i = 0; i < sizeof(newVector); i++) 
      {
        EEPROM.write(sizeof(newKey) + i, newVector[i]);
      }
      EEPROM.commit();
      EEPROM.end();
    }
    else 
    {
      // Button is not pressed, read key and vector from EEPROM
      byte savedKey[sizeof(aesKey)];
      byte savedVector[16];

      EEPROM.begin(sizeof(aesKey) + sizeof(aesIv));
      for (int i = 0; i < sizeof(aesKey); i++) 
      {
        savedKey[i] = EEPROM.read(i);
      }
      for (int i = 0; i < sizeof(savedVector); i++) 
      {
        savedVector[i] = EEPROM.read(sizeof(aesKey) + i);
      }
      EEPROM.end();

      // Use the saved key and vector for encryption/decryption
      memcpy(aesKey, savedKey,   sizeof(aesKey));
      memcpy(aesIv,  savedVector,sizeof(aesIv));
    }

    // Generate a random message
    byte randomMessage[16];  
    for (int i = 0; i < sizeof(randomMessage); i++) 
    {
      randomMessage[i] = random(256);  // Generate a random byte (0-255)
    }

    // Send the random message through pCharacteristic_4
    std::string randomStr(reinterpret_cast<const char*>(randomMessage), sizeof(randomMessage));
    pCharacteristic_4->setValue(randomStr);
    pCharacteristic_4->notify();

    // Wait to receive a message through pCharacteristic_5
    std::string enReceived = pCharacteristic_5->getValue();
    if (!enReceived.empty()) 
    {
      // Encrypt the generated random message for comparison
      byte encryptedRandomMessage[16];
      uint16_t enc_bytes = aes.encrypt(randomMessage, sizeof(randomMessage), encryptedRandomMessage, aesKey, sizeof(aesKey), aesIv);

      // Convert the encrypted random message to a string for comparison
      std::string encryptedStr(reinterpret_cast<const char*>(encryptedRandomMessage), enc_bytes);

      // Compare the received message with the encrypted random message
      bool isAuthorized = (enReceived == encryptedStr);
    }
    // Update pCharacteristic_1 with the authorization status
    std::string authStatus = isAuthorized ? "Authorized" : "Unauthorized";
    pCharacteristic_1->setValue(authStatus);
    pCharacteristic_1->notify();
    // Control the hardware lock based on the received message
    std::string controlMessage = pCharacteristic_6->getValue();
    if (!controlMessage.empty() && isAuthorized) 
    {
      if (controlMessage == "ON") {
        digitalWrite(LOCK_PIN,HIGH);
      } else if (controlMessage == "OFF") {
        digitalWrite(LOCK_PIN,LOW);
      }
    }
  }
  if (!deviceConnected && oldDeviceConnected)
  {
    isAuthorized = false ;
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

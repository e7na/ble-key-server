#include <Arduino.h>

#include <Effortless_SPIFFS.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BluetoothSerial.h>
#include <esp_bt_device.h>
#include <BLECharacteristic.h>
#include <BLERemoteCharacteristic.h>
#include <AESLib.h>

eSPIFFS fileSystem;

#define BAUD 115200

bool isAuthorized = false;

bool deviceConnected = false;
bool oldDeviceConnected = false;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic_1 = NULL;
BLECharacteristic* pCharacteristic_2 = NULL;
BLECharacteristic* pCharacteristic_3 = NULL;
BLECharacteristic* pCharacteristic_4 = NULL;
BLECharacteristic* pCharacteristic_5 = NULL;
BLECharacteristic* pCharacteristic_6 = NULL;
BLE2902* pBLE2902_1;
BLE2902* pBLE2902_2;
BLE2902* pBLE2902_3;
BLE2902* pBLE2902_4;
BLE2902* pBLE2902_5;
BLE2902* pBLE2902_6;

// generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID "d9327ccb-992b-4d78-98ce-2297ed2c09d6"
#define CHARACTERISTIC_UUID_1 "e95e7f63-f041-469f-90db-04d2e3e7619b"
#define CHARACTERISTIC_UUID_2 "8c233b56-4988-4c3c-95b5-ec5b3c179c91"
#define CHARACTERISTIC_UUID_3 "56bba80a-91f1-46ab-b892-7325e19c3429"
#define CHARACTERISTIC_UUID_4 "53a15a66-6dd7-4421-9468-38cf731a77db"
#define CHARACTERISTIC_UUID_5 "acc9a30f-9e44-4323-8193-7bac8c9bc484"
#define CHARACTERISTIC_UUID_6 "9800d290-19fb-4085-9610-f1e878725ad2"

std::string remoteAddressStr;


uint8_t keyFlag = false;
uint8_t vectorFlag = false;
void updateAuthStatus(bool newAuthStatus) {
  isAuthorized = newAuthStatus;

  if (!isAuthorized) {
    keyFlag = false;
    vectorFlag = false;
  }

  std::string authStatus = isAuthorized ? "authorized" : "unauthorized";
  pCharacteristic_1->setValue(authStatus);
  pCharacteristic_1->notify();
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;

    Serial.println("Connected to client");

    updateAuthStatus(false);
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Disconnected from client");

    updateAuthStatus(false);
  }

  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    BLEAddress remoteAddress(param->connect.remote_bda);
    remoteAddressStr = remoteAddress.toString();
    Serial.print("Connected to device with address: ");
    Serial.println(remoteAddressStr.c_str());
  }
};

#define STR_TERMINATOR 1
#define AES_KEY_LENGTH 32
#define AES_VECTOR_LENGTH 16
static uint8_t aesKey[AES_KEY_LENGTH] = {0};
static uint8_t aesIv[AES_VECTOR_LENGTH] = {0};

AESLib aes;

void setup() {
  Serial.begin(BAUD);
  Serial.setTimeout(60000);

  // Create the BLE Device
  BLEDevice::init("ESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(BLEUUID(SERVICE_UUID), 20, 0);

  // clang-format off
  // Create a BLE Characteristic
  pCharacteristic_1 = pService->createCharacteristic( /*AUTH STATUS*/
    CHARACTERISTIC_UUID_1,
      BLECharacteristic::PROPERTY_READ    |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE
  );
  pBLE2902_1 = new BLE2902();
  pBLE2902_1->setNotifications(true);
  pCharacteristic_1->addDescriptor(pBLE2902_1);

  pCharacteristic_2 = pService->createCharacteristic( /*KEY*/
    CHARACTERISTIC_UUID_2,
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE
  );
  pBLE2902_2 = new BLE2902();
  pBLE2902_2->setNotifications(true);
  pCharacteristic_2->addDescriptor(pBLE2902_2);

  pCharacteristic_3 = pService->createCharacteristic( /*VECTOR*/
    CHARACTERISTIC_UUID_3,
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE
  );
  pBLE2902_3 = new BLE2902();
  pBLE2902_3->setNotifications(true);
  pCharacteristic_3->addDescriptor(pBLE2902_3);

  pCharacteristic_4 = pService->createCharacteristic( /*trans random value*/
    CHARACTERISTIC_UUID_4,
      BLECharacteristic::PROPERTY_READ    |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE
  );
  pBLE2902_4 = new BLE2902();
  pBLE2902_4->setNotifications(true);
  pCharacteristic_4->addDescriptor(pBLE2902_4);

  pCharacteristic_5 = pService->createCharacteristic( /*recive the encrypted random value*/
    CHARACTERISTIC_UUID_5,
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE
  );
  pBLE2902_5 = new BLE2902();
  pBLE2902_5->setNotifications(true);
  pCharacteristic_5->addDescriptor(pBLE2902_5);

  pCharacteristic_6 = pService->createCharacteristic( /*action control*/
    CHARACTERISTIC_UUID_6,
      BLECharacteristic::PROPERTY_READ    |
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE
  );
  pBLE2902_6 = new BLE2902();
  pBLE2902_6->setNotifications(true);
  pCharacteristic_6->addDescriptor(pBLE2902_6);
  // clang-format on

  // Start advertising
  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
  delay(2000);
}

void loop() {
  if (deviceConnected) {
    char serialBuffer[UINT8_MAX];
    uint8_t randomMessage[16];

    static uint8_t newKey[sizeof(aesKey)] = {0};
    static uint8_t newVector[sizeof(aesIv)] = {0};

    if (!keyFlag || !vectorFlag) {
      std::string receivedKey = pCharacteristic_2->getValue();
      if (!keyFlag && !receivedKey.empty()) {
        memcpy(newKey, receivedKey.c_str(), sizeof(newKey));
        keyFlag = true;
      }

      // Receive new vector via pCharacteristic_3
      std::string receivedVector = pCharacteristic_3->getValue();
      if (!vectorFlag && !receivedVector.empty()) {
        memcpy(newVector, receivedVector.c_str(), sizeof(newVector));
        vectorFlag = true;
      }

      if (keyFlag & vectorFlag) {
        // write the key and vector in one string `writeBuffer`
        static char writeBuffer[sizeof(aesKey) + sizeof(aesIv) + STR_TERMINATOR];
        static const char* readBuffer;
        snprintf(writeBuffer, sizeof(writeBuffer), "%s%s", newVector, newKey);

        // fetch the mac address to save the key and vector under
#define FILENAME_LENGTH 33
        char filename[FILENAME_LENGTH];
        snprintf(filename, sizeof(filename), "/%s", remoteAddressStr.c_str());

        // attempt to save the key and vector then read the result
        if (fileSystem.saveToFile(filename, writeBuffer)) {
          if (fileSystem.openFromFile(filename, readBuffer)) {
            // updateAuthStatus(true);  //! mock successful auth
            //! and print the key and vector
            // clang-format off
            snprintf(
              serialBuffer,
              sizeof(serialBuffer),
              "Successfully read and parsed %d bytes from: ",
              strlen(readBuffer)
            );  // clang-format on
            Serial.print(serialBuffer);
            Serial.println(filename);
            Serial.println(readBuffer);

            // Generate a random message
            for (uint8_t i = 0; i < sizeof(randomMessage); i++) {
              randomMessage[i] = random(256);  // Generate a random uint8_t (0-255)
            }
            // Send the random message through pCharacteristic_4
            std::string randomStr(reinterpret_cast<const char*>(randomMessage), sizeof(randomMessage));
            pCharacteristic_4->setValue(randomStr);
            pCharacteristic_4->notify();
            Serial.print("Sent random message: ");
            Serial.println(randomStr.c_str());
          }
        } else {
          Serial.println("Failed to write data to file");
        }
      }
    } else if (!isAuthorized) {
      std::string enReceived = pCharacteristic_5->getValue();
      if (!enReceived.empty()) {
        // Encrypt the generated random message for comparison
        uint8_t encryptedRandomMessage[16];
        uint16_t enc_bytes = aes.encrypt(randomMessage, sizeof(randomMessage), encryptedRandomMessage, newKey, sizeof(aesKey), newVector);

        // Convert the encrypted random message to a string for comparison
        std::string encryptedStr(reinterpret_cast<const char*>(encryptedRandomMessage), enc_bytes);

        // Compare the received message with the encrypted random message
        updateAuthStatus(enReceived == encryptedStr);
        Serial.print("Received encrypted message: ");
        Serial.println(enReceived.c_str());
        Serial.print("Our encrypted message: ");
        Serial.println(encryptedStr.c_str());
        Serial.println(isAuthorized ? "Authorized" : "Not Authorized");
      }
    }
  }

  if (!deviceConnected && oldDeviceConnected) {
    isAuthorized = false;
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    oldDeviceConnected = deviceConnected;
  }

  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}

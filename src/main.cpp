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

#define LOCK_PIN 2
#define STR_TERM 1
#define AES_KEY_LENGTH 16
#define AES_VECTOR_LENGTH 16
#define BLOCK_WIDTH 16
static uint8_t aesKey[AES_KEY_LENGTH] = {0};
static uint8_t aesIv[AES_VECTOR_LENGTH] = {0};

#include "./debugPrint.hpp"

eSPIFFS fileSystem;

#define BAUD 115200

bool isAuthorized = false;

bool deviceConnected = false;
bool oldDeviceConnected = false;

uint8_t keyFlag = false;
uint8_t vectorFlag = false;

std::string remoteAddressStr;
#include "./bleConfig.hpp"

AESLib aes;

void setup() {
  aes.set_paddingmode(paddingMode::Null);

  Serial.begin(BAUD);
  Serial.setTimeout(60000);
  bleInit();

  pinMode(LOCK_PIN, OUTPUT);
}

void loop() {
  if (deviceConnected) {
    char serialBuffer[UINT8_MAX];
    uint8_t randomMessage[BLOCK_WIDTH] = {0};  // 30 char string

    static uint8_t newKey[sizeof(aesKey)] = {0};
    static uint8_t newVector[sizeof(aesIv)] = {0};

    if (!keyFlag || !vectorFlag) {
      std::string receivedKey = pCharacteristic_2->getValue();
      if (!keyFlag && !receivedKey.empty()) {
        memcpy(newKey, receivedKey.c_str(), sizeof(newKey));
        keyFlag = true;
      }

      std::string receivedVector = pCharacteristic_3->getValue();
      if (!vectorFlag && !receivedVector.empty()) {
        memcpy(newVector, receivedVector.c_str(), sizeof(newVector));
        vectorFlag = true;
      }

      if (keyFlag & vectorFlag) {
        // write the key and vector in one string `writeBuffer`
        static char writeBuffer[sizeof(aesKey) + sizeof(aesIv) + STR_TERM];
        static const char* readBuffer;
        snprintf(writeBuffer, sizeof(writeBuffer), "%s%s", newVector, newKey);

        // fetch the mac address to save the key and vector under
#define FILENAME_LENGTH 33
        char filename[FILENAME_LENGTH];
        snprintf(filename, sizeof(filename), "/%s", remoteAddressStr.c_str());

        // attempt to save the key and vector then read the result
        if (fileSystem.saveToFile(filename, writeBuffer)) {
          if (fileSystem.openFromFile(filename, readBuffer)) {
            snprintf(
                serialBuffer,
                sizeof(serialBuffer),
                "Successfully read and parsed %d bytes ",
                strlen(readBuffer));
            println(serialBuffer);

            // Generate a random message
            static const uint8_t alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
            for (uint8_t i = 0; i < sizeof(randomMessage) - STR_TERM; i++) {
              randomMessage[i] = alphanum[random(0, sizeof(alphanum) - STR_TERM)];  // Generate a random uint8_t (0-255)
              // randomMessage[i] = (uint8_t)random(1, 255);  // Generate a random uint8_t (0-255)
            }
            randomMessage[sizeof(randomMessage) - STR_TERM] = '\0';
            deciPrint("deci: ", randomMessage);

            // Send the random message through pCharacteristic_4
            std::string randomStr(reinterpret_cast<const char*>(randomMessage), sizeof(randomMessage));
            pCharacteristic_4->setValue(randomStr);
            pCharacteristic_4->notify();
            print("Sent random message: ");
            println(randomStr.c_str());
            snprintf(
                serialBuffer,
                sizeof(serialBuffer),
                "It's %d characters",
                strlen(randomStr.c_str()));
            println(serialBuffer);
          }  // clang-format off
        } else { println("Failed to write data to file"); }  // clang-format on
      }
    } else if (!isAuthorized) {
      std::string enReceived = pCharacteristic_5->getValue();
      if (!enReceived.empty()) {
        uint8_t receivedStr[BLOCK_WIDTH + STR_TERM] = {0};
        memcpy(receivedStr, enReceived.c_str(), sizeof(receivedStr));

        uint8_t encryptedStr[BLOCK_WIDTH + STR_TERM];
        uint16_t enc_bytes = aes.encrypt(
            randomMessage,
            sizeof(randomMessage),
            encryptedStr,
            newKey, sizeof(aesKey), newVector);

        uint8_t sameBytes = true;
        for (uint8_t i = 0; sameBytes && (i < BLOCK_WIDTH); i++) {
          if (!(encryptedStr[i] = receivedStr[i])) {
            sameBytes = false;
          }
        }

        // Compare the received message with the encrypted random message
        updateAuthStatus(sameBytes);
        deciPrint("recieved string: ", receivedStr);
        deciPrint("our encrypted string: ", encryptedStr);

        println(isAuthorized ? "Authorized" : "Not Authorized");  // clang-format off
      }  // else { println("No encrypted message received... yet"); } // clang-format on
    }

    //hardware Control based on the received message
    static std::string controlBuffer = "";
    std::string controlMessage = pCharacteristic_6->getValue();
    if (isAuthorized && (controlBuffer != controlMessage)) {
      controlBuffer = controlMessage;
      if (controlMessage == "ON") {
        digitalWrite(LOCK_PIN, HIGH);
      } else if (controlMessage == "OFF") {
        digitalWrite(LOCK_PIN, LOW);
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

#define SERVICE_UUID "d9327ccb-992b-4d78-98ce-2297ed2c09d6"
BLEServer* pServer = NULL;

#define CHARACTERISTIC_UUID_1 "e95e7f63-f041-469f-90db-04d2e3e7619b"
BLECharacteristic* pCharacteristic_1 = NULL;
BLE2902* pBLE2902_1;

#define CHARACTERISTIC_UUID_2 "8c233b56-4988-4c3c-95b5-ec5b3c179c91"
BLECharacteristic* pCharacteristic_2 = NULL;
BLE2902* pBLE2902_2;

#define CHARACTERISTIC_UUID_3 "56bba80a-91f1-46ab-b892-7325e19c3429"
BLECharacteristic* pCharacteristic_3 = NULL;
BLE2902* pBLE2902_3;

#define CHARACTERISTIC_UUID_4 "53a15a66-6dd7-4421-9468-38cf731a77db"
BLECharacteristic* pCharacteristic_4 = NULL;
BLE2902* pBLE2902_4;

#define CHARACTERISTIC_UUID_5 "acc9a30f-9e44-4323-8193-7bac8c9bc484"
BLECharacteristic* pCharacteristic_5 = NULL;
BLE2902* pBLE2902_5;

#define CHARACTERISTIC_UUID_6 "9800d290-19fb-4085-9610-f1e878725ad2"
BLECharacteristic* pCharacteristic_6 = NULL;
BLE2902* pBLE2902_6;

void updateAuthStatus(bool newAuthStatus) {
  isAuthorized = newAuthStatus;

  std::string authStatus = isAuthorized ? "authorized" : "unauthorized";
  pCharacteristic_1->setValue(authStatus);
  pCharacteristic_1->notify();

  keyFlag = false;
  vectorFlag = false;

  pCharacteristic_2->setValue("");
  pCharacteristic_3->setValue("");
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;

    println("Connected to client");

    updateAuthStatus(false);
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    println("Disconnected from client");

    updateAuthStatus(false);
  }

  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    BLEAddress remoteAddress(param->connect.remote_bda);
    remoteAddressStr = remoteAddress.toString();
    print("Connected to device with address: ");
    println(remoteAddressStr.c_str());
  }
};

void bleInit() {
  // Create the BLE Device
  BLEDevice::init("ESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(BLEUUID(SERVICE_UUID), 20, 0);

  // Create a BLE Characteristic
  // clang-format off
  pCharacteristic_1 = pService->createCharacteristic( /*AUTH STATUS*/
    CHARACTERISTIC_UUID_1,
      BLECharacteristic::PROPERTY_READ    |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_1 = new BLE2902();
  pBLE2902_1->setNotifications(true);
  pCharacteristic_1->addDescriptor(pBLE2902_1);

  pCharacteristic_2 = pService->createCharacteristic( /*KEY*/
    CHARACTERISTIC_UUID_2,
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_2 = new BLE2902();
  pBLE2902_2->setNotifications(true);
  pCharacteristic_2->addDescriptor(pBLE2902_2);

  pCharacteristic_3 = pService->createCharacteristic( /*VECTOR*/
    CHARACTERISTIC_UUID_3,
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_3 = new BLE2902();
  pBLE2902_3->setNotifications(true);
  pCharacteristic_3->addDescriptor(pBLE2902_3);

  pCharacteristic_4 = pService->createCharacteristic( /*trans random value*/
    CHARACTERISTIC_UUID_4,
      BLECharacteristic::PROPERTY_READ    |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_4 = new BLE2902();
  pBLE2902_4->setNotifications(true);
  pCharacteristic_4->addDescriptor(pBLE2902_4);

  pCharacteristic_5 = pService->createCharacteristic( /*recive the encrypted random value*/
    CHARACTERISTIC_UUID_5,
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE);
  pBLE2902_5 = new BLE2902();
  pBLE2902_5->setNotifications(true);
  pCharacteristic_5->addDescriptor(pBLE2902_5);

  pCharacteristic_6 = pService->createCharacteristic( /*action control*/
    CHARACTERISTIC_UUID_6,
      BLECharacteristic::PROPERTY_READ    |
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE);  // clang-format on
  pBLE2902_6 = new BLE2902();
  pBLE2902_6->setNotifications(true);
  pCharacteristic_6->addDescriptor(pBLE2902_6);

  // Start advertising
  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  println("Waiting a client connection to notify...");
}

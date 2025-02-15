#include "NimBLEDevice.h"
#include "BMS_Class.h"
//NIM umnbau

char* Geraetename0 = "JK_BD4A20S4P";
char* Geraetename1 = "";
char* Geraetename2 = "";
char* Geraetename3 = "";
char* Geraetename4 = "";
char* Geraetename5 = "";
char* Geraetename6 = "";
char* Geraetename7 = "";
char* Geraetename8 = "";

#define LED_PIN 2  // ESP32 pin GPIO connected to LED
int LED_state = LOW;

// The remote service we wish to connect to.
static NimBLEUUID serviceUUID("ffe0");
// The characteristic of the remote service we are interested in.
static NimBLEUUID charUUID("ffe1");

const NimBLEAdvertisedDevice* advDevice;


static JkBmsNimBLE* bms_list[9];
static unsigned long lastcheck;



static void notifyCallback(NimBLERemoteCharacteristic* pNimBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  for (int i = 0; i < 9; i++) {
    if (bms_list[i]->pRemoteCharacteristic == pNimBLERemoteCharacteristic) {
      bms_list[i]->notifyCallback(pNimBLERemoteCharacteristic, pData, length, isNotify);
    }
  }
};


class MyClientCallback : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pclient) {
  }

  void onDisconnect(NimBLEClient* pclient) {
    bms_list[Nr]->connected = false;
    bms_list[Nr]->settingsOK = false;
    Serial.print("Disconnect");
    Serial.print(Nr);
    Serial.print(" ");
    Serial.println(bms_list[Nr]->Geraetename);
  }
public:
  MyClientCallback(int nr) {
    Nr = nr;
  }
private:
  int Nr;
} ;

bool connectToServer(int Nr) {
  Serial.print("Forming a connection to: ");
  Serial.print(bms_list[Nr]->Geraetename);
  Serial.print("  ");
  Serial.println(bms_list[Nr]->myDevice->getAddress().toString().c_str());

  NimBLEClient* pClient = NimBLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback(Nr), false);


  // Connect to the remove NimBLE Server.
  pClient->connect(bms_list[Nr]->myDevice);  // if you pass NimBLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");
  // pClient->setMTU(517);  //set client to request maximum MTU from server (default is 23 otherwise)

  // Obtain a reference to the service we are after in the remote NimBLE server.
  NimBLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");


  // Obtain a reference to the characteristic in the service of the remote NimBLE server.
  bms_list[Nr]->pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (bms_list[Nr]->pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if (bms_list[Nr]->pRemoteCharacteristic->canRead()) {
    String value = bms_list[Nr]->pRemoteCharacteristic->readValue();
    Serial.print(" - The characteristic value was: ");
    Serial.println(value.c_str());
  }

  if (bms_list[Nr]->pRemoteCharacteristic->canNotify()) {
    bms_list[Nr]->pRemoteCharacteristic->subscribe(true, notifyCallback);
    Serial.println(" - Notify the characteristic");
  }

  // Sending getdevice info
  bms_list[Nr]->write_register(COMMAND_DEVICE_INFO, 0x00000000, 0x00);
  //bms_list[Nr]->pRemoteCharacteristic->writeValue(getdeviceInfo, 20);
  //bms_list[Nr]->sendingtime = millis();

  Serial.println(" - Sending device Info");

  bms_list[Nr]->connected = true;
  return true;
}
/**
 * Scan for NimBLE servers and find the first one that advertises the service we are looking for.
 */
class ScanCallbacks : public NimBLEScanCallbacks {
  /**
   * Called for each advertising NimBLE server.
   */
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    Serial.print("NimBLE Advertised Device found: ");
    Serial.println(advertisedDevice->toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    for (int i = 0; i < 9; i++) {
      if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID) && advertisedDevice->getName() == bms_list[i]->Geraetename && "" != bms_list[i]->Geraetename && !bms_list[i]->connected && !bms_list[i]->doConnect) {
        bms_list[i]->myDevice = advertisedDevice;
        bms_list[i]->doConnect = true;
        Serial.print("Geraetename");
        Serial.print(i);
        Serial.print(" OK ");
        Serial.println(bms_list[i]->Geraetename);
      }  // Found our server
    }    // for

    // wenn alle gefunden scan stoppen
    int k = 0;
    for (int j = 0; j < 9; j++) {
      if ("" == bms_list[j]->Geraetename || bms_list[j]->connected || bms_list[j]->doConnect) {
        k++;
      }
    }
    if (k == 9) {
      NimBLEDevice::getScan()->stop();
      Serial.println("NimBLE Scan stop");
    }

  }  // onResult
} scanCallbacks;   


void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino NimBLE Client application...");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_state);

  // Array of BMS
  for (int i = 0; i < 9; i++) {
    bms_list[i] = new JkBmsNimBLE();
  }
  bms_list[0]->Geraetename = Geraetename0;
  bms_list[1]->Geraetename = Geraetename1;
  bms_list[2]->Geraetename = Geraetename2;
  bms_list[3]->Geraetename = Geraetename3;
  bms_list[4]->Geraetename = Geraetename4;
  bms_list[5]->Geraetename = Geraetename5;
  bms_list[6]->Geraetename = Geraetename6;
  bms_list[7]->Geraetename = Geraetename7;
  bms_list[8]->Geraetename = Geraetename8;

 NimBLEDevice::init("NimBLE-Client");
  NimBLEDevice::setPower(3);

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  NimBLEScan* pNimBLEScan = NimBLEDevice::getScan();
  pNimBLEScan->setScanCallbacks(&scanCallbacks, false);
  pNimBLEScan->setInterval(100);
  pNimBLEScan->setWindow(100);
  pNimBLEScan->setActiveScan(true);
 }  // End of setup.


// This is the Arduino main loop function.
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // NimBLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  for (int i = 0; i < 9; i++) {
    if (bms_list[i]->doConnect == true) {
      if (connectToServer(i)) {
        Serial.println(" - We are now connected to the NimBLE Server.");
      } else {
        Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      }
      bms_list[i]->doConnect = false;
    }
  }

  // Settings Lesen und CellInfo Notify aktivieren (hierbei piept das BMS)
  for (int i = 0; i < 9; i++) {
    unsigned long difftime1 = millis() - bms_list[i]->sendingtime;
    unsigned long difftime2 = millis() - bms_list[i]->receivetime;
    if (difftime1 > 1000 && (difftime2 > 5000 || !bms_list[i]->settingsOK) && bms_list[i]->connected) {
      bms_list[i]->write_register(COMMAND_CELL_INFO, 0x00000000, 0x00);
      //bms_list[i]->pRemoteCharacteristic->writeValue(getInfo, 20);
      //bms_list[i]->sendingtime = millis();
    }
  }

  // LED Anzeige neue Daten
  bool wdchange = false;
  for (int i = 0; i < 9; i++) {
    if (bms_list[i]->wd != bms_list[i]->wdold) {
      bms_list[i]->wdold = bms_list[i]->wd;
      wdchange = true;
    }
  }
  if (wdchange) {
    if (LED_state == HIGH) {
      LED_state = LOW;
    } else {
      LED_state = HIGH;
    }
    digitalWrite(LED_PIN, LED_state);
  }





  // Prüfen ob alle Verbunden, wenn nicht Scan Starten
  unsigned long check = millis() / 30000;  // 30s
  if (check != lastcheck) {
    int k = 0;
    for (int j = 0; j < 9; j++) {
      if ("" == bms_list[j]->Geraetename || bms_list[j]->connected || bms_list[j]->doConnect) {
        k++;
      }
    }
    if (k == 9) {
      Serial.println("Check Connections OK");
    } else {
      Serial.println("Check Connections: NimBLE Scan start");
      NimBLEDevice::getScan()->start(5000, false, true);
    }
    lastcheck = check;
  }


  delay(10);  // Delay between loops.
              // Serial.println("Loop");
}  // End of loop

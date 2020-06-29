//----------------------------------------//
//  ttgo.ino
//
//  created 04/03/2019
//  by Luiz H. Cassettari
//----------------------------------------//
// Add ttgo_wait_connect
//----------------------------------------//

// Your GPRS credentials (leave empty, if missing)
const char apn[]      = ""; // Your APN
const char gprsUser[] = ""; // User
const char gprsPass[] = ""; // Password
const char simPIN[]   = ""; // SIM card PIN code, if any

// TTGO T-Call pin definitions
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to the module)
#define SerialAT  Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Define the serial console for debug prints, if needed
//#define TINY_GSM_DEBUG SerialMon
//#define DUMP_AT_COMMANDS

#include <Wire.h>
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

#define DEBUG_PRINT(...) { SerialMon.print(millis()); SerialMon.print(" - "); SerialMon.println(__VA_ARGS__); }
#define DEBUG_FATAL(...) { SerialMon.print(millis()); SerialMon.print(" - FATAL: "); SerialMon.println(__VA_ARGS__); delay(1000); ESP.restart(); }

void ttgo_setup()
{
  //SerialMon.setDebugOutput(true);
  printDeviceInfo();

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool   isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  SerialMon.println("  Firmware A is running");
  SerialMon.println("--------------------------");
  
  delay(10);

  // Set-up modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(10);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  DEBUG_PRINT(F("Initializing modem..."));
  modem.restart();
  // Or, use modem.init() if you don't need the complete restart

  String modemInfo = modem.getModemInfo();
  DEBUG_PRINT(String("Modem: ") + modemInfo);

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
  //ttgo_wait_connect();
}

bool ttgo_wait_connect()
{
  DEBUG_PRINT(F("Waiting for network..."));
  if (!modem.waitForNetwork(240000L)) {
    DEBUG_FATAL(F("Network failed to connect"));
    return false;
  }

  DEBUG_PRINT(F("Connecting to GPRS"));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    DEBUG_FATAL(F("APN failed to connect"));
    return false;
  }
  return true;
}

void ttgo_close()
{
  modem.gprsDisconnect();
  SerialMon.println(F("GPRS disconnected"));
  delay(1000);
  ESP.restart();
}

//----------------------------------------//
//  google
//----------------------------------------//

bool parseURL(String url, String& protocol, String& host, int& port, String& uri)
{
  int index = url.indexOf(':');
  if(index < 0) {
    return false;
  }
  
  protocol = url.substring(0, index);
  url.remove(0, (index + 3)); // remove protocol part

  index = url.indexOf('/');
  String server = url.substring(0, index);
  url.remove(0, index);       // remove server part

  index = server.indexOf(':');
  if(index >= 0) {
    host = server.substring(0, index);          // hostname
    port = server.substring(index + 1).toInt(); // port
  } else {
    host = server;
    if (protocol == "http") {
      port = 80;
    } else if (protocol == "https") {
      port = 443;
    }
  }

  if (url.length()) {
    uri = url;
  } else {
    uri = "/";
  }
  return true;
}

String post_gprs_google(String path, String body)
{
  if (path == "") return "";

  Serial.println(body);

  String payload = "";

  String protocol, host, url;
  int port;

  if (ttgo_wait_connect() == false) return "";

  if (!parseURL(path, protocol, host, port, url)) {
    DEBUG_FATAL(F("Cannot parse URL"));
  }

  DEBUG_PRINT(String("Connecting to ") + host + ":" + port);

  Client* client = NULL;
  if (protocol == "http") {
    client = new TinyGsmClient(modem);
    if (!client->connect(host.c_str(), port)) {
      DEBUG_FATAL(F("Client not connected"));
    }
  } else if (protocol == "https") {
    client = new TinyGsmClientSecure(modem);
    if (!client->connect(host.c_str(), port)) {
      DEBUG_FATAL(F("Client not connected"));
    }
  } else {
    DEBUG_FATAL(String("Unsupported protocol: ") + protocol);
  }
  
  DEBUG_PRINT(String("Requesting ") + url);

  client->print(String("post ") + url + " HTTP/1.1\r\n"
               + "Host: " + host + "\r\n"
               + "Connection: keep-alive\r\n"
                );

  client->print("Content-Length: ");
  client->println(body.length());
  client->println();
  client->print(body);
  client->println();    


  Serial.println(F("-------------------------"));

  unsigned long timeout = millis();
  while (client->connected() && millis() - timeout < 10000L) {
    // Print available data
    while (client->available()) {
      char c = client->read();
      //Serial.print(c);
      timeout = millis();
    }
  }
  //Serial.println();

  client->stop();
  Serial.println(F("-------------------------"));
  
  DEBUG_PRINT("Server disconnected");

  return payload;
}

//----------------------------------------//
//  util
//----------------------------------------//

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

bool setPowerBoostKeepOn(int en)
{
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

void printDeviceInfo()
{
  Serial.println();
  Serial.println("--------------------------");
  Serial.println(String("Build:    ") +  __DATE__ " " __TIME__);
#if defined(ESP8266)
  Serial.println(String("Flash:    ") + ESP.getFlashChipRealSize() / 1024 + "K");
  Serial.println(String("ESP core: ") + ESP.getCoreVersion());
  Serial.println(String("FW info:  ") + ESP.getSketchSize() + "/" + ESP.getFreeSketchSpace() + ", " + ESP.getSketchMD5());
#elif defined(ESP32)
  Serial.println(String("Flash:    ") + ESP.getFlashChipSize() / 1024 + "K");
  Serial.println(String("ESP sdk:  ") + ESP.getSdkVersion());
  Serial.println(String("Chip rev: ") + ESP.getChipRevision());
#endif
  Serial.println(String("Free mem: ") + ESP.getFreeHeap());

  String mac = WiFi.macAddress();
  Serial.println(String("Mac: ") + mac);
  
  Serial.println("--------------------------");
}

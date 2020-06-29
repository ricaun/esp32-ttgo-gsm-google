//----------------------------------------//
//  wifi.ino
//
//  created 29/06/2019
//  by Luiz H. Cassettari
//----------------------------------------//

#include <WiFi.h>
#include <WebServer.h>

/* WIFI */

const char *ssid = "";
const char *password = "";

// Define

#define WIFI_TIME_RECONNECT 10

// ----------------- server ------------------- //

WebServer server(80);

const char *script = "<script>function loop() {var resp = GET_NOW('values'); document.getElementById('values').innerHTML = resp; setTimeout('loop()', 1000);} function GET_NOW(get) { var xmlhttp; if (window.XMLHttpRequest) xmlhttp = new XMLHttpRequest(); else xmlhttp = new ActiveXObject('Microsoft.XMLHTTP'); xmlhttp.open('GET', get, false); xmlhttp.send(); return xmlhttp.responseText; }</script>";

void handleRoot()
{
  String str = "";
  str += "<html>";
  str += "<head>";
  str += "<title>ESP32</title>";
  str += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  str += script;
  str += "</head>";
  str += "<body onload='loop()'>";
  str += "<center>";
  str += "<div id='values'></div>";
  str += "</center>";
  str += "</body>";
  str += "</html>";
  server.send(200, "text/html", str);
}

void handleGetValues()
{
  String str = "";

  str += readAnalogPin();

  server.send(200, "text/plain", str);
}

void wifi_setup()
{
  wifi_start();
  server.on("/", handleRoot);
  server.on("/values", handleGetValues);
  server.begin();
  Serial.println("HTTP server started");
}

void wifi_start()
{
  WiFi.mode(WIFI_STA);
  if (ssid != "")
    WiFi.begin(ssid, password);
  WiFi.begin();
  Serial.print("wifi_start:");
  Serial.println(ssid);
}

void wifi_loop()
{
  if (wifi_runEvery(WIFI_TIME_RECONNECT * 1000))
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      wifi_start();
    }
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    server.handleClient();
  }
}

bool wifi_connected()
{
  return WiFi.status() == WL_CONNECTED;
}

boolean wifi_runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

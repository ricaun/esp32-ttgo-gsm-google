/*
  esp32-ttgo-gsm-google
  This project read analog input and sends to the google sheet over the gsm
  created 04 03 2020
  by Luiz H. Cassettari - ricaun@gmail.com
*/

#include <WiFi.h>

/* PINOS */

#define ANALOG_PIN A0 // pino 35
#define ANALOG_TIME 1

/* GOOGLE URL */

#define GOOGLE_URL "https://script.google.com/macros/s/#######################################################/exec"
#define GOOGLE_TIME 30

/* TaskHandle_t */

TaskHandle_t Task0;

// ----------------- setup ------------------- //

void setup() {
  // Set console baud rate
  Serial.begin(115200);
  delay(10);

  ttgo_setup();

  get_body();

  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
    TaskCode0,   /* Task function. */
    "IDLE0",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task0,      /* Task handle to keep track of created task */
    0);          /* pin task to core 0 */
  
}

void TaskCode0( void * pvParameters ) {
  while (1)
  {
    google_loop();
    vTaskDelay(10);
  }
}

void loop(void)
{
  if (analog_runEvery(ANALOG_TIME * 1000))
  {
    add_body();
  }
}

// ----------------- analog ------------------- //

float readAnalogPin()
{
  float f = analogRead(ANALOG_PIN);
  f *= 3.3 / 4095.0;
  Serial.print("ANALOG: ");
  Serial.println(f);
  return f;
}

boolean analog_runEvery(unsigned long interval)
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

// ----------------- body ------------------- //

static String _body = "";

String add_body()
{
  _body += ";";
  _body += readAnalogPin();
}

String get_body()
{
  String payload = _body;
  String mac = WiFi.macAddress();
  _body = mac;
  return payload;
}

// ----------------- google ------------------- //

void google_loop()
{
  if (google_runEvery(GOOGLE_TIME * 1000))
  {
    String post = post_google(GOOGLE_URL, get_body());
    Serial.println(post);
  }
}

boolean google_runEvery(unsigned long interval)
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

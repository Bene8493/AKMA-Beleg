#include <SPI.h>
#include "epd4in2.h"
#include "epdpaint.h"
#include <RTCZero.h>
#include <WiFi101.h>
#include <ArduinoJson.h>

#define COLORED     0
#define UNCOLORED   1

/* Create an rtc object */
RTCZero rtc;

/* Change these values to set the current initial time */
const byte seconds = 0;
const byte minutes = 00;
const byte hours = 1;

const byte UPDATE_INTERVAL_HOURS = 0;     // 0 - 23
const byte UPDATE_INTERVAL_MINUTES = 0;   // 0 - 59
const byte UPDATE_INTERVAL_SECONDS = 10;  // 0 - 59

/* Change these values to set the current initial date */
const byte day = 17;
const byte month = 11;
const byte year = 15;

// wifi info
//char ssid[] = "AndroidAPA41C";
//char password[] = "passwort123";
char ssid[] = "";
char password[] = "";
WiFiClient wifi;
int status = WL_IDLE_STATUS;

const String endpoint = "/bot1045691263:AAFFERxuJj1j9WIiL3ZylwKhgpLhzuhphTE/getUpdates";

boolean alarmMatched = false;

Epd epd;

DynamicJsonDocument doc(20000);

int lastUpdateId = 0;

String fetchMessage() {
  if(wifi.connectSSL("api.telegram.org", 443)) {
    Serial.println("client connected");

    wifi.println("GET " + endpoint + "?offset=" + (lastUpdateId + 1));
    wifi.println("Content-Type: application/json");
    wifi.println();

    Serial.println("Request sent");

    long timer = millis() + 10000;
    boolean comms_ok = false;
    while(millis() < timer) {
      if(wifi.available()) {
        comms_ok = true;
        break;
      }
    }

    String response = "";
    if(comms_ok) {
      Serial.println("Received Data");
      while(wifi.available()) {
        char c = wifi.read();
        response += c;
      }

       // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      Serial.println(response);
      wifi.stop();
      return "";
    } 

    JsonArray result = doc["result"].as<JsonArray>();
    if(result.size() == 0) {
      wifi.stop();
      return "";
    }

    JsonObject resultObject = result.getElement(result.size() - 1).as<JsonObject>();
    JsonObject lastMessage = resultObject["message"].as<JsonObject>();
    int updateId = resultObject["update_id"].as<int>();

    Serial.print("Fetched message update_id");
    Serial.println(updateId);

    lastUpdateId = updateId;
    String message = lastMessage["text"].as<String>();
    wifi.stop();
    return message;
     
    } else {
      Serial.println("No Data");
    }
    wifi.stop();
  } else {
    Serial.println("client failed to connect");
  }

  return "";
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial);

  WiFi.maxLowPowerMode();

  rtc.begin();

  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(day, month, year);

  rtc.setAlarmTime(
      hours + UPDATE_INTERVAL_HOURS, 
      minutes + UPDATE_INTERVAL_MINUTES, 
      seconds + UPDATE_INTERVAL_SECONDS
    );
  rtc.enableAlarm(rtc.MATCH_HHMMSS);

  rtc.attachInterrupt(alarmMatch);

  rtc.standbyMode();
}

void loop() {
  if(alarmMatched) {
    Serial.println("MATCHED");
    alarmMatched = false;

    while(status != WL_CONNECTED) {
        Serial.print("Attempting Wifi Connection to ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, password);
    }

    String message = fetchMessage();
    Serial.print("returned");
    Serial.println(message);
    status = WL_DISCONNECTED;
    WiFi.end();

    if(!message.equals("")) {

      epd.Reset();
      if (epd.Init() != 0) {
        return;
      }
      
      
      Serial.println("DISPLAY UPDATE");
      epd.ClearFrame();

      char *m = const_cast<char*>(message.c_str());
      char* ptr = strtok(m, "\n");

      unsigned char image[1500];
      int idx = 0;
      int y = 0;
      while(ptr != NULL) {
        bool smallFont = false;
        bool mediumFont = false;
        int height = 28;
        if(startsWith("[s]", ptr)) {
          ptr += 3;
          smallFont = true;
          height = 20;
        } else if(startsWith("[m]", ptr)) {
          ptr += 3;
          mediumFont = true;
          height = 24;
        }

        int bg = UNCOLORED;
        int color = COLORED;
        if(startsWith("[inv]", ptr)) {
          ptr += 5;
          bg = COLORED;
          color = UNCOLORED;
        }

        Paint paint(image, 400, height);    //width should be the multiple of 8 

        paint.Clear(bg);
        if(smallFont) {
          paint.DrawStringAt(0, 3, ptr, &Font16, color);
        } else if(mediumFont) {
          paint.DrawStringAt(0, 3, ptr, &Font20, color);
        } else {
          paint.DrawStringAt(0, 3, ptr, &Font24, color);
        }
        
        epd.SetPartialWindow(paint.GetImage(), 0, y, paint.GetWidth(), paint.GetHeight());

        y += height;
  
        ptr = strtok(NULL, "\n");
        idx++;
      }

      epd.DisplayFrame();
      epd.Sleep();
    }
  
    rtc.setTime(hours, minutes, seconds);

    rtc.setAlarmTime(
      hours + UPDATE_INTERVAL_HOURS, 
      minutes + UPDATE_INTERVAL_MINUTES, 
      seconds + UPDATE_INTERVAL_SECONDS
    );
    rtc.enableAlarm(rtc.MATCH_HHMMSS);

    rtc.attachInterrupt(alarmMatch);
    rtc.standbyMode();
  }
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

void alarmMatch() {
  alarmMatched = true;
}

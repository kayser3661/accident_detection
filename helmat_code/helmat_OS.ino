#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

#define DEBUG 0

#define gpsLed  D0
#define busyLed D3

#define BPM_MIN  30
#define BPM_MAX  50
#define SP02_MIN 70
#define SP02_MAX 90
#define THRESHOLD 5   // sec
#define GSM_DELAY 20  // sec / 2

String number = "017xxxxxxxx"; //phone number here

SoftwareSerial gsm, gpsSerial;
TinyGPSPlus gps;
WiFiClient client[2];
WiFiServer server(8080);
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(1234);
sensors_event_t event;

int x, y;
byte bpm, sp02, id, sec, fall;
double lati, longi;
bool watchMode = 1;
bool detection = 0;
bool isFall = 0;
bool isSent = 0;
long prevMs;

byte setSp02 = SP02_MAX;
byte setBpm = BPM_MAX;

void setup() {
  delay(1000);
#if DEBUG == 1
  Serial.begin(115200);
#endif
  pinMode(busyLed, OUTPUT);
  pinMode(gpsLed, OUTPUT);
  digitalWrite(busyLed, 1);

  gpsSerial.begin(9600, SWSERIAL_8N1, D5, -1);  // RX, TX
  gsm.begin(9600, SWSERIAL_8N1, -1, D7);  // RX, TX

  if (!adxl.begin()) {
    ESP.wdtFeed();
#if DEBUG == 1
    Serial.println("ADXL Failed!");
#endif
    delay(1000);
    ESP.restart();
  }
  adxl.setRange(ADXL345_RANGE_8_G);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("Helmat", "12345678");
  server.begin();
#if DEBUG == 1
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
#endif

  GSMinit();
#if DEBUG == 1
  Serial.println("OS Started.");
#endif
  digitalWrite(busyLed, 0);
}

void loop() {
  wifiCom();
  checkGPS();

  x = adxl.getX();
  y = adxl.getY();
  if ((x < -200 || x > 200) || (y < -200 || y > 200)) isFall = 1;
  else isFall = 0;

  if (watchMode == 1) {
    if ((bpm > BPM_MIN && bpm <= setBpm) ||
        (sp02 > SP02_MIN && sp02 <= setSp02) || isFall == 1) {
#if DEBUG == 1
      if (detection == 0)
        Serial.println("ACCIDENT DETECT!");
#endif
      detection = 1;
    }
    else {
      detection = 0;
      isSent = 0;
    }
  }

  if (millis() - prevMs >= 1000) {
    if (lati == 0) digitalWrite(gpsLed, !digitalRead(gpsLed));
    else digitalWrite(gpsLed, 1);

    if (detection == 1 && isSent == 0) {
      sec++;
      if (sec >= THRESHOLD) {
        isSent = 1;
#if DEBUG == 1
        Serial.println("THRESHOLD REACHED!");
#endif
        sendSMS();
      }
    }
    else sec = 0;

#if DEBUG == 1
    debugPrint();
#endif
    ESP.wdtFeed();
    prevMs = millis();
  }
}

void checkGPS() {
  while (gpsSerial.available() > 0) {
    ESP.wdtFeed();
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        lati = gps.location.lat();
        longi = gps.location.lng();
      }
    }
  }
  yield();
}

void GSMinit() {
#if DEBUG == 1
  Serial.println("GSM INIT");
#endif
  for (int i = 0; i < GSM_DELAY; i++) {
    ESP.wdtFeed();
    digitalWrite(busyLed, !digitalRead(busyLed));
    delay(500);
#if DEBUG == 1
    Serial.print(".");
#endif
  }
  gsm.println("AT");
  delay(500);
  gsm.println("ATE0");
  delay(500);
  gsm.println("AT+CMGF=1");
  delay(500);
  gsm.println("AT+CNMI=1,2,0,0,0");
  delay(500);
  yield();
}

void sendSMS() {
  digitalWrite(busyLed, 1);
#if DEBUG == 1
  Serial.println("Sending SMS");
#endif

  gsm.println("AT+CMGF=1");
  delay(500);
  gsm.println((String)"AT+CMGS=\"" + number + "\"");
  delay(500);
  gsm.println("Accident Detected!!");
  gsm.print("BPM: ");
  gsm.print(bpm);
  gsm.print("\nSP02: ");
  gsm.println(sp02);
  gsm.print("\nLink: https://www.google.com/maps/place/");
  gsm.print(lati, 6);
  gsm.print(",");
  gsm.print(longi, 6);
  gsm.print((char)26);

#if DEBUG == 1
  Serial.println("Sent.");
#endif
  digitalWrite(busyLed, 0);
  ESP.restart();
}

void debugPrint() {
  Serial.print("X: ");
  Serial.print(x);
  Serial.print(" | Y: ");
  Serial.print(y);
  Serial.print(" | BPM: ");
  Serial.print(bpm);
  Serial.print(" | SP02: ");
  Serial.print(sp02);
  Serial.print(" | LAT: ");
  Serial.print(lati);
  Serial.print(" | LONG: ");
  Serial.println(longi);
}

void wifiCom() {
  if (server.hasClient()) {
    if (!client[id]) {
      client[id] = server.available();
#if DEBUG == 1
      Serial.print("NEW CLIENT AT ");
      Serial.println(id);
#endif
      id++;
      if (id == 2) id = 0;
    }
  }

  for (byte i = 0; i < 2; i++) {
    if (client[i].available()) {
      String data = client[i].readStringUntil('\r');

      if (data.startsWith("NUM=")) {
        data.remove(0, 4);
        number = data;
#if DEBUG == 1
        Serial.println("NUMBER: " + number);
#endif
        client[i].println("NUMBER SET");
      }
      else if (data.startsWith("BPM=")) {
        data.remove(0, 4);
        setBpm = data.toFloat();
#if DEBUG == 1
        Serial.println((String)"setBPM: " + setBpm);
#endif
        client[i].println("BPM SET");
      }
      else if (data.startsWith("SP02=")) {
        data.remove(0, 5);
        setSp02 = data.toInt();
#if DEBUG == 1
        Serial.println((String)"setSP02: " + setSp02);
#endif
        client[i].println("SP02 SET");
      }
      else if (data.startsWith("ON")) {
        watchMode = 1;
        client[i].println("WATCH ON");
      }
      else if (data.startsWith("OFF")) {
        watchMode = 0;
        client[i].println("WATCH OFF");
      }
      else {
        byte n = data.indexOf(",");
        bpm = data.toFloat();
        data.remove(0, n + 1);
        sp02 = data.toInt();
      }
    }
  }
}

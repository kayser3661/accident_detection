#include <Wire.h>
#include <ESP8266WiFi.h>
#include "MAX30100_PulseOximeter.h"
#include "SH1106.h"

WiFiClient client;
PulseOximeter pox;
SH1106 display(0x3c, SDA, SCL);

long prevMs;

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);

  display.init();
  display.setContrast(255);
  display.clear();

  //WiFi.begin("IronHeart", "proplayer2021");
  WiFi.begin("Helmat", "12345678");

  display.drawString(0, 0, "WIFI SCANNING...");
  display.display();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WIFI OK");
  display.drawString(0, 10, "WIFI OK");
  display.display();

  display.drawString(0, 20, "CHECKING SERVER...");
  display.display();
  while (!client.connect("192.168.4.1", 8080)) {
    Serial.print(".");
  }
  Serial.println("SERVER OK");
  display.drawString(0, 30, "SERVER OK");
  display.display();
  delay(1000);

  display.clear();
  display.drawString(0, 0, "HEART RATE:");
  display.drawString(0, 30, "OXYGEN LEVEL:");
  display.display();
  
  Wire.begin();
  pox.begin();
}

void loop() {
  pox.update();

  if (millis() - prevMs > 1000) {
    float bpm = pox.getHeartRate();
    byte sp02 = pox.getSpO2();

    client.print(bpm);
    client.print(",");
    client.print(sp02);
    client.println(",");

    display.setColor(BLACK);
    display.fillRect(0, 15, 60, 15);
    display.fillRect(0, 45, 30, 15);
    display.setColor(WHITE);
    display.drawString(0, 15, (String)bpm + " BPM ");
    display.drawString(0, 45, (String)sp02 + "%  ");
    display.display();

    if(WiFi.status() != WL_CONNECTED) ESP.restart();
    prevMs = millis();
  }
}

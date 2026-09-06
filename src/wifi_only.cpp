#include <Arduino.h>
#include <WiFi.h>

constexpr char AP_SSID[] = "ESP32-C3-WIFI-TEST";
constexpr char AP_PASSWORD[] = "12345678";

void printStatus() {
  Serial.println("--- Wi-Fi diagnostic ---");
  Serial.printf("Mode: %d\n", static_cast<int>(WiFi.getMode()));
  Serial.printf("AP SSID: %s\n", AP_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
  Serial.printf("Stations: %u\n", WiFi.softAPgetStationNum());
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nESP32-C3 Wi-Fi only test");
  Serial.println("PN5180/SPI/Web/RF430 are NOT initialized.");

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(300);
  WiFi.mode(WIFI_AP);
  delay(200);

  const bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 2);
  Serial.printf("softAP result: %s\n", ok ? "SUCCESS" : "FAILED");
  printStatus();
}

void loop() {
  static uint32_t nextReport = 0;
  if (millis() >= nextReport) {
    printStatus();
    nextReport = millis() + 5000;
  }
  delay(100);
}

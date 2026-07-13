#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Pin definitions
#define MQ_PIN    1    // MQ-5 analog pin
#define DHT_PIN   11   // DHT22 data pin
#define DHT_TYPE  DHT22

// WiFi 
const char* WIFI_SSID = "Wokwi-GUEST";   
const char* WIFI_PASS = "";

// InfluxDB Cloud 
const char* INFLUX_URL    = "https://us-east-1-1.aws.cloud2.influxdata.com";
const char* INFLUX_ORG    = "cef08b8524e7b3e0";
const char* INFLUX_BUCKET = "iot_project";
const char* INFLUX_TOKEN  = "zaMUgIH7OrQkY3LcdLQrktumhbxU0aKqVKzwBP-FcMUc63Kb7C3ySmjL9ULTjUWIdi4pizvOjvrs2VbMy_KqlA==";  

// Components 
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);

const unsigned long SEND_INTERVAL_MS = 10000;
unsigned long lastSend = 0;

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
}

bool sendToInfluxDB(float temp, float humidity, int gasValue) {
  // InfluxDB line protocol: measurement,tags fields
  String line = "environment,device=esp32,location=lab ";
  line += "temperature=" + String(temp, 2);
  line += ",humidity=" + String(humidity, 2);
  line += ",gas=" + String(gasValue);

  String url = String(INFLUX_URL) + "/api/v2/write?org=" + INFLUX_ORG +
               "&bucket=" + INFLUX_BUCKET + "&precision=s";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Authorization", String("Token ") + INFLUX_TOKEN);
  http.addHeader("Content-Type", "text/plain; charset=utf-8");

  int code = http.POST(line);
  http.end();

  if (code == 204) {
    Serial.println("InfluxDB write OK: " + line);
    return true;
  }
  Serial.printf("InfluxDB write FAILED, HTTP %d\n", code);
  return false;
}

void setup() {
  Serial.begin(115200);

  // Start LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  delay(2000);
  lcd.clear();

  // Start DHT
  dht.begin();

  // Connect WiFi
  lcd.print("Connecting WiFi");
  connectWiFi();
  lcd.clear();
  lcd.print("WiFi OK");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Read DHT22
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Read MQ-5
  int gasValue = analogRead(MQ_PIN);

  // Check for bad DHT reading
  if (isnan(temp) || isnan(humidity)) {
    lcd.setCursor(0, 0);
    lcd.print("DHT Error!      ");
    delay(2000);
    return;
  }

  // Display temperature and humidity on line 1
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C H:");
  lcd.print(humidity, 1);
  lcd.print("%");

  // Display gas level on line 2
  lcd.setCursor(0, 1);
  lcd.print("Gas:");
  lcd.print(gasValue);

  // Gas safety alert
  if (gasValue > 2500) {
    lcd.setCursor(10, 1);
    lcd.print("DANGER");
  } else if (gasValue > 1000) {
    lcd.setCursor(10, 1);
    lcd.print("WARN  ");
  } else {
    lcd.setCursor(10, 1);
    lcd.print("SAFE  ");
  }

  // Print to serial monitor too
  Serial.print("Temp: "); Serial.print(temp);
  Serial.print(" Humidity: "); Serial.print(humidity);
  Serial.print(" Gas: "); Serial.println(gasValue);

  // Cloud upload every 10 s 
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    if (WiFi.status() != WL_CONNECTED) connectWiFi();

    bool ok = sendToInfluxDB(temp, humidity, gasValue);

    // Show upload status on the LCD (matches physical prototype behaviour)
    lcd.clear();
    lcd.setCursor(0, 0);
    if (ok) {
      lcd.print("Upload Success");
      lcd.setCursor(0, 1);
      lcd.print("Cloud Updated");
    } else {
      lcd.print("Upload Failed");
      lcd.setCursor(0, 1);
      lcd.print("Check WiFi/DB");
    }
    delay(1500);   // let the message be visible
    lcd.clear();   // next loop pass redraws the readings
  }

  delay(2000);
}

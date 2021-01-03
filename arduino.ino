#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "DHT.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

// Configuration
#define DHTPIN D4
#define DHTTYPE DHT22
#define WIFI_AP "AP"
#define WIFI_PASSWORD "Super-Wifi-Password"
#define DATADOG_PATH "/api/v1/series?api_key=123er"
#define TAGS "[\"position:rocket\",\"room:kitchen\",\"location:home\"]"

DHT dht(DHTPIN, DHTTYPE);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.fr.pool.ntp.org");

String StartSeries = String("{\"series\":[");

String TempSerie = String("{\"metric\":\"dht.temperature\",\"points\":[[");
String HumSerie = String("{\"metric\":\"dht.humidity\",\"points\":[[");

String UpSerie = String("{\"metric\":\"uptime.seconds\",\"points\":[[");
String RSSISerie = String("{\"metric\":\"network.wireless.rssi.dbm\",\"points\":[[");
String PrevLatencySerie = String("{\"metric\":\"arduino.loop.latency\",\"points\":[[");

String MetadataSerie;
unsigned int PrevLatencyMS = 0;

void connect() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }
    WiFi.begin(WIFI_AP, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
}

void send(unsigned long ts, float humidity, float temperature, unsigned long start) {
    BearSSL::WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient https;
    int checkBegin = https.begin(secureClient, "api.datadoghq.com", 443, DATADOG_PATH, true);
    if (checkBegin != 1) {
        Serial.println(checkBegin);
        return;
    }
    https.addHeader("Content-Type", "application/json");

    String payload = StartSeries;
    if (!isnan(humidity)) {
        payload += HumSerie + ts + String(",") + humidity + MetadataSerie + String(",");
    }
    if (!isnan(temperature)) {
        payload += TempSerie + ts + String(",") + temperature + MetadataSerie + String(",");
    }
    if (PrevLatencyMS > 0) {
        payload += PrevLatencySerie + ts + String(",") + PrevLatencyMS + MetadataSerie + String(",");
    }

    // Wifi
    payload += RSSISerie + ts + String(",") + String(WiFi.RSSI()) + MetadataSerie + String(",");

    // uptime
    unsigned long now = millis();

    payload += UpSerie + ts + String(",") + (now / 1000) + MetadataSerie + String("]}"); // end of JSON

    Serial.println(payload);
    int code = https.POST(payload);
    Serial.println(code);
    if (code > 0 ) {
        Serial.println(https.getString());
    }
    https.end();
    secureClient.stop();
    PrevLatencyMS = millis() - start;
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    connect();
    dht.begin();

    String hostname = WiFi.macAddress();
    hostname.replace(":", "-");
    hostname.toLowerCase();
    MetadataSerie = String("]],\"host\":\"") + hostname + String("\",\"tags\":") + TAGS + String("}");

    timeClient.update();
}

void loop() {
    unsigned long start = millis();
    connect();
    timeClient.update();
    send(timeClient.getEpochTime(), dht.readHumidity(), dht.readTemperature(), start);
    delay(10000);
}

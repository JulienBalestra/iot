#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "DHT.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

// Configuration
#define DHTPIN 2
#define DHTTYPE DHT22
#define WIFI_AP "ap"
#define WIFI_PASSWORD "password"
#define DATADOG_PATH "/api/v1/series?api_key=123er"
#define TAGS "[\"collector:arduino\",\"sensor:dht22\",\"room:kitchen\",\"location:place\",\"position:somewhere\"]"

DHT dht(DHTPIN, DHTTYPE);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.fr.pool.ntp.org");

String StartSeries = String("{series:[");

String TempSerie = String("{metric:\"temperature.celsius\",type:\"gauge\",points:[[");
String HumSerie = String("{metric:\"humidity.percent\",type:\"gauge\",points:[[");
String UpSerie = String("{metric:\"up.time\",type:\"gauge\",\"points\":[[");
String RSSISerie = String("{metric:\"network.wireless.rssi.dbm\",type:\"gauge\",points:[[");

String MetadataSerie;

void connect() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }
    WiFi.begin(WIFI_AP, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    timeClient.update();
}

void send(unsigned long ts, float humidity, float temperature) {
    digitalWrite(LED_BUILTIN, LOW);
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

    // Wifi
    payload += RSSISerie + ts + String(",") + String(WiFi.RSSI()) + MetadataSerie + String(",");

    // uptime
    payload += UpSerie + ts + String(",") + (millis() / 1000) + MetadataSerie + String("]}"); // end of JSON

    Serial.println(payload);
    int code = https.POST(payload);
    Serial.println(code);
    if (code > 0 ) {
        Serial.println(https.getString());
    }
    https.end();
    secureClient.stop();
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    dht.begin();
    connect();

    String hostname = WiFi.macAddress();
    hostname.replace(":", "-");
    hostname.toLowerCase();
    MetadataSerie = String("]],host:\"") + hostname + String("\",tags:") + TAGS + String("}");
}

void loop() {
    connect();
    send(timeClient.getEpochTime(), dht.readHumidity(), dht.readTemperature());
    delay(5000);
}

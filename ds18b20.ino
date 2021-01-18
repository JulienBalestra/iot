#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// Configuration
#define DS18B20PIN 2
#define WIFI_AP "ap"
#define WIFI_PASSWORD "password"
#define DATADOG_PATH "/api/v1/series?api_key=123er"
#define TAGS "[\"collector:arduino\",\"sensor:ds18b20\",\"room:kitchen\",\"location:place\",\"position:somewhere\"]"

OneWire oneWire(DS18B20PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorDeviceAddress;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.fr.pool.ntp.org");

String StartSeries = String("{series:[");

String TempSerie = String("{metric:\"temperature.celsius\",type:\"gauge\",points:[[");
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

void send(unsigned long ts, float temperature) {
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
    sensors.begin();
    sensors.getAddress(sensorDeviceAddress, 0);
    sensors.setResolution(sensorDeviceAddress, 12);
    connect();

    String hostname = WiFi.macAddress();
    hostname.replace(":", "-");
    hostname.toLowerCase();
    MetadataSerie = String("]],\"host\":\"") + hostname + String("\",\"tags\":") + TAGS + String("}");
}

void loop() {
    connect();
    sensors.requestTemperatures();
    send(timeClient.getEpochTime(), sensors.getTempCByIndex(0));
    delay(5000);
}

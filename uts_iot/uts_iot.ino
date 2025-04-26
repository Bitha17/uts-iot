#include <WiFi.h>
#include <PubSubClient.h>
#include "sample_image.h"  // this contains sample_jpg[] and sample_jpg_len

// WiFi and MQTT Configuration
const char* ssid = "LANTAI 3";
const char* password = "lantaitiga";
const char* mqtt_server = "192.168.0.127";
const int mqtt_port = 1883;
const char* topic = "uts/iot13521111/imagechunk";

WiFiClient espClient;
PubSubClient client(espClient);

// Sending Parameters
const int RAW_CHUNK_SIZE = 10240;
int totalChunks = sample_jpg_len / RAW_CHUNK_SIZE + (sample_jpg_len % RAW_CHUNK_SIZE != 0);

int imageID = 1;         // image counter
unsigned long lastSendTime = 0;
const int T = 2000;      // 2 seconds between transmissions
const int K = 20;        // Send this many images

int sentImages = 0;

void setup_wifi() {
  delay(100);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void sendImageChunks(int imgID, time_t timestamp) {
  for (int i = 0; i < totalChunks; i++) {
    int start = i * RAW_CHUNK_SIZE;
    int len = (i == totalChunks - 1) ? sample_jpg_len - start : RAW_CHUNK_SIZE;

    String payload = "{";
    payload += "\"id\":" + String(imgID) + ",";
    payload += "\"ts\":" + String(timestamp) + ",";
    payload += "\"chunk\":" + String(i) + ",";
    payload += "\"total\":" + String(totalChunks) + ",";
    payload += "\"data\":\"";

    for (int j = 0; j < len; j++) {
      char buf[3];
      sprintf(buf, "%02X", sample_jpg[start + j]);
      payload += buf;
    }

    payload += "\"}";

    // Try to publish with retry
    bool success = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
      if (client.publish(topic, payload.c_str())) {
        success = true;
        break;
      } else {
        Serial.printf("Failed to publish chunk %d (attempt %d), retrying...\n", i, attempt);
        reconnect();  // attempt reconnect
        delay(100);   // short delay before retry
      }
    }

    if (!success) {
      Serial.printf("Giving up on chunk %d of image %d\n", i, imgID);
      // Optional: break the loop if too many chunks fail
      return;
    } else {
      Serial.printf("Chunk %d of image %d sent successfully\n", i, imgID);
    }

    delay(50); // small pause to prevent flooding
  }

  Serial.println("Full image sent successfully.\n");
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(30720);
  configTime(0, 0, "pool.ntp.org"); // NTP time sync
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.println("Time synchronized");
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (sentImages < K && millis() - lastSendTime > T) {
    time_t now;
    time(&now);
    sendImageChunks(imageID, now);
    imageID++;
    sentImages++;
    lastSendTime = millis();
  }
}

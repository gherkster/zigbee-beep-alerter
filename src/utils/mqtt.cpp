#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

void publishDiscoveryMessage(PubSubClient& client) {
    JsonDocument doc;

    // Get the device's unique MAC address to use as a unique ID
    String mac_address = WiFi.macAddress();
    mac_address.replace(":", "");
    mac_address.toLowerCase();
    String unique_id = "washing_machine_sensor_" + mac_address;

    doc["automation_type"] = "trigger";
    doc["topic"] = MQTT_EVENT_TOPIC;
    doc["type"] = "completed"; 
    doc["subtype"] = "washing";

    // Device information
    JsonObject device = doc["device"].to<JsonObject>();
    device["ids"] = unique_id;
    device["name"] = "ESP32 Washing Machine Status";

    char buffer[512];
    size_t n = serializeJson(doc, buffer);

    Serial.println("Publishing MQTT discovery message...");

    Serial.println(buffer);

    // Publish with retain flag set to true so that Home Assistant can pick up the latest config from the topic after a server reboot.
    if (client.publish(MQTT_DISCOVERY_TOPIC, buffer, true)) {
        Serial.println("Discovery message sent successfully.");
    } else {
        Serial.println("Failed to send discovery message.");
        Serial.println(client.state());
    }
}

void publishDoneMessage(PubSubClient& client) {
    JsonDocument doc;
    doc["event_type"] = "washingmachine_done";
    
    char buffer[64];
    size_t n = serializeJson(doc, buffer);
    
    client.publish(MQTT_EVENT_TOPIC, buffer);
}

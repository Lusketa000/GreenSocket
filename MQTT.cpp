#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "MQTT.h"
#include "Relay.h"

static const char* MQTT_SERVER = "192.168.26.160";
static const int MQTT_PORT = 1883;
static const char* MQTT_USER = "";
static const char* MQTT_PASSWORD = "";
static const unsigned long HEARTBEAT_INTERVAL = 60000;
static const char* FW_VERSION = "1.0.0";

static const int MQTT_ACTION_OFF = 0;
static const int MQTT_ACTION_ON = 1;
static const int MQTT_ACTION_STATUS = 2;
static const int MQTT_STATE_OFF = 0;
static const int MQTT_STATE_ON = 1;

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static String deviceId;
static String mqttTopicCmd;
static String mqttTopicState;
static String mqttTopicHello;
static String mqttTopicPing;
static String mqttTopicPong;
static unsigned long mqttReconnectTimer = 0;
static unsigned long heartbeatTimer = 0;

struct JsonValue {
  String value;
  bool quoted = false;
  bool found = false;
};

bool isTimeValid(const tm &timeinfo) {
  return timeinfo.tm_year >= MIN_VALID_YEAR;
}

static JsonValue findJsonValue(const String& json, const char* key) {
  JsonValue result;
  String needle = String("\"") + key + "\"";
  int keyPos = json.indexOf(needle);
  if (keyPos < 0) {
    return result;
  }
  int colonPos = json.indexOf(':', keyPos + needle.length());
  if (colonPos < 0) {
    return result;
  }
  int i = colonPos + 1;
  while (i < static_cast<int>(json.length()) &&
         (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) {
    i++;
  }
  if (i >= static_cast<int>(json.length())) {
    return result;
  }
  if (json[i] == '"') {
    result.quoted = true;
    int end = json.indexOf('"', i + 1);
    if (end < 0) {
      return result;
    }
    result.value = json.substring(i + 1, end);
  } else {
    int end = i;
    while (end < static_cast<int>(json.length()) && json[end] != ',' && json[end] != '}') {
      end++;
    }
    result.value = json.substring(i, end);
    result.value.trim();
  }
  result.found = true;
  return result;
}

static void buildMqttTopics() {
  mqttTopicCmd = String("cargas/") + deviceId + "/cmd";
  mqttTopicState = String("cargas/") + deviceId + "/state";
  mqttTopicHello = String("cargas/") + deviceId + "/hello";
  mqttTopicPing = String("cargas/") + deviceId + "/ping";
  mqttTopicPong = String("cargas/") + deviceId + "/pong";
  Serial.print("[MQTT] Topics ready. cmd=");
  Serial.print(mqttTopicCmd);
  Serial.print(" state=");
  Serial.print(mqttTopicState);
  Serial.print(" hello=");
  Serial.print(mqttTopicHello);
  Serial.print(" ping=");
  Serial.print(mqttTopicPing);
  Serial.print(" pong=");
  Serial.println(mqttTopicPong);
}

static uint32_t currentTimestampSeconds() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  if (!isTimeValid(timeinfo)) {
    return 0;
  }
  return static_cast<uint32_t>(now);
}

static int relayStateAsMqtt() {
  return relayIsOn() ? MQTT_STATE_ON : MQTT_STATE_OFF;
}

static const char* mapReasonToSource(const String& reason) {
  if (reason == "load_shedding" || reason == "restore") {
    return "auto";
  }
  return "manual";
}

static void mqttPublishHello() {
  String payload = String("{\"device_id\":\"") + deviceId +
                   "\",\"fw\":\"" + FW_VERSION + "\",\"state\":" +
                   String(relayStateAsMqtt()) + ",\"ts\":" +
                   String(currentTimestampSeconds()) + "}";
  mqttClient.publish(mqttTopicHello.c_str(), payload.c_str());
  Serial.print("[MQTT] Publish hello: ");
  Serial.println(payload);
}

static void mqttPublishState(const char* source, const String& reqId, bool reqIdQuoted) {
  String payload = String("{\"state\":") + String(relayStateAsMqtt()) +
                   ",\"source\":\"" + source + "\"";
  if (reqId.length() > 0) {
    payload += ",\"req_id\":";
    payload += reqIdQuoted ? (String("\"") + reqId + "\"") : reqId;
  }
  payload += ",\"ts\":" + String(currentTimestampSeconds()) + "}";
  mqttClient.publish(mqttTopicState.c_str(), payload.c_str());
  Serial.print("[MQTT] Publish state: ");
  Serial.println(payload);
}

static void mqttPublishPong(const String& reqId, bool reqIdQuoted) {
  String payload = String("{\"req_id\":");
  payload += reqIdQuoted ? (String("\"") + reqId + "\"") : reqId;
  payload += String(",\"state\":") + String(relayStateAsMqtt()) +
             ",\"ts\":" + String(currentTimestampSeconds()) + "}";
  mqttClient.publish(mqttTopicPong.c_str(), payload.c_str());
  Serial.print("[MQTT] Publish pong: ");
  Serial.println(payload);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  String payloadStr;
  payloadStr.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += static_cast<char>(payload[i]);
  }

  Serial.print("[MQTT] Message on ");
  Serial.print(topicStr);
  Serial.print(" payload=");
  Serial.println(payloadStr);

  if (topicStr == mqttTopicCmd) {
    JsonValue actionValue = findJsonValue(payloadStr, "action");
    JsonValue reasonValue = findJsonValue(payloadStr, "reason");
    JsonValue reqIdValue = findJsonValue(payloadStr, "req_id");

    if (!actionValue.found) {
      Serial.println("[MQTT] Missing action in cmd payload");
      return;
    }

    String actionRaw = actionValue.value;
    actionRaw.toLowerCase();
    int action = -1;
    if (!actionValue.quoted) {
      action = actionRaw.toInt();
    } else if (actionRaw == "on") {
      action = MQTT_ACTION_ON;
    } else if (actionRaw == "off") {
      action = MQTT_ACTION_OFF;
    } else if (actionRaw == "status") {
      action = MQTT_ACTION_STATUS;
    }

    String reason = reasonValue.found ? reasonValue.value : String("manual");
    reason.toLowerCase();
    const char* source = mapReasonToSource(reason);

    if (action == MQTT_ACTION_STATUS) {
      Serial.println("[MQTT] CMD status request");
      mqttPublishState(source, reqIdValue.found ? reqIdValue.value : String(""), reqIdValue.quoted);
      return;
    }

    if (action == MQTT_ACTION_ON) {
      Serial.print("[MQTT] CMD on, reason=");
      Serial.println(reason);
      relayOn();
    } else if (action == MQTT_ACTION_OFF) {
      Serial.print("[MQTT] CMD off, reason=");
      Serial.println(reason);
      relayOff();
    } else {
      Serial.print("[MQTT] Unknown action: ");
      Serial.println(actionRaw);
      return;
    }

    mqttPublishState(source, reqIdValue.found ? reqIdValue.value : String(""), reqIdValue.quoted);
    return;
  }

  if (topicStr == mqttTopicPing) {
    JsonValue reqIdValue = findJsonValue(payloadStr, "req_id");
    if (!reqIdValue.found) {
      Serial.println("[MQTT] Missing req_id in ping payload");
      return;
    }
    Serial.print("[MQTT] Ping received req_id=");
    Serial.println(reqIdValue.value);
    mqttPublishPong(reqIdValue.value, reqIdValue.quoted);
  }
}

static void mqttEnsureConnected() {
  if (mqttClient.connected()) {
    return;
  }
  if (millis() - mqttReconnectTimer < 5000) {
    return;
  }
  mqttReconnectTimer = millis();

  Serial.print("[MQTT] Connecting to ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  String clientId = deviceId;
  bool connected = false;
  if (strlen(MQTT_USER) > 0) {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (connected) {
    Serial.println("[MQTT] Connected");
    mqttClient.subscribe(mqttTopicCmd.c_str());
    mqttClient.subscribe(mqttTopicPing.c_str());
    mqttPublishHello();
  } else {
    Serial.print("[MQTT] Connect failed, rc=");
    Serial.println(mqttClient.state());
  }
}

void mqttSetup(const String& deviceIdInput) {
  deviceId = deviceIdInput;
  Serial.print("[MQTT] Setup deviceId=");
  Serial.println(deviceId);
  buildMqttTopics();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.print("[MQTT] Broker=");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
}

void mqttLoop() {
  mqttEnsureConnected();
  mqttClient.loop();

  if (millis() - heartbeatTimer >= HEARTBEAT_INTERVAL) {
    Serial.println("[MQTT] Heartbeat");
    mqttPublishState("manual", "", false);
    heartbeatTimer = millis();
  }
}

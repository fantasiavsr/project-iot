#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>  // import WiFi library
#include <PubSubClient.h> // Import mqtt library

const char *ssid = "nekoma";
const char *password = "matahariku";
const char *mqtt_server = "test.mosquitto.org"; // this is our broker

WiFiClient espClient;
PubSubClient client(espClient);

LiquidCrystal_I2C lcd(0x27, 20, 4);
#define echoPin D6
#define triggerPin D7

void setup_wifi()
{ // setting up wifi connection

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);        // select "wifi station" mode (client mode)
  WiFi.begin(ssid, password); // initiate connection

  while (WiFi.status() != WL_CONNECTED)
  { // wait for connection (retry)
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println(""); // when connected
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == "esp32/output")
  {
    Serial.print("Changing output to ");
    if (messageTemp == "on")
    {
      Serial.println("on");
      digitalWrite(D5, HIGH);
    }
    else if (messageTemp == "off")
    {
      Serial.println("off");
      digitalWrite(D5, LOW);
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client"))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // water level sensor connected to A0
  pinMode(A0, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }

  client.loop();

  //test send simple "hello world" message
  client.publish("esp32/input", "hello world");

  long duration, distance; // Duration used to calculate distance

  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  Serial.print("Distance: ");
  Serial.println(distance);

  // water level sensor
  int water_level = analogRead(A0);
  Serial.print("Water Level: ");
  Serial.println(water_level);

  lcd.setCursor(0, 0);
  lcd.print("Distance: ");
  lcd.print(distance);
  lcd.print(" cm");
  lcd.setCursor(0, 1);
  lcd.print("Water Level: ");
  lcd.print(water_level);
  lcd.print(" cm");

  // connected to
  lcd.setCursor(0, 2);
  lcd.print("Connected to: ");
  lcd.print(ssid);

  delay(2000);
  lcd.clear();
}
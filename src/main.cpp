#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>  // import WiFi library
#include <PubSubClient.h> // Import mqtt library
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// https://github.com/mobizt/ESP_SSLClient
#include <ESP_SSLClient.h>

#include <Ethernet.h>

// For NTP time client
#include "MB_NTP.h"

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 1. Define the API Key */
#define API_KEY "AIzaSyC4_Hq_zzMzdtpZMySC3PX0Gu7sFhT1rsI"

/* 2. Define the RTDB URL */
#define DATABASE_URL "eisp8266-default-rtdb.asia-southeast1.firebasedatabase.app" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 3. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "admin@gmail.com"
#define USER_PASSWORD "matahariku"

/* 4. Defined the Ethernet module connection */
#define WIZNET_RESET_PIN 5 // Connect W5500 Reset pin to GPIO 5 (D1) of ESP8266
#define WIZNET_CS_PIN 4    // Connect W5500 CS pin to GPIO 4 (D2) of ESP8266
#define WIZNET_MISO_PIN 12 // Connect W5500 MISO pin to GPIO 12 (D6) of ESP8266
#define WIZNET_MOSI_PIN 13 // Connect W5500 MOSI pin to GPIO 13 (D7) of ESP8266
#define WIZNET_SCLK_PIN 14 // Connect W5500 SCLK pin to GPIO 14 (D5) of ESP8266

/* 5. Define MAC */
uint8_t Eth_MAC[] = {0x02, 0xF0, 0x0D, 0xBE, 0xEF, 0x01};

/* 6. Define IP (Optional) */
IPAddress Eth_IP(192, 168, 1, 104);

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

int count = 0;

// Define the basic client
// The network interface devices that can be used to handle SSL data should
// have large memory buffer up to 1k - 2k or more, otherwise the SSL/TLS handshake
// will fail.
EthernetClient basic_client;

// This is the wrapper client that utilized the basic client for io and
// provides the mean for the data encryption and decryption before sending to or after read from the io.
// The most probable failures are related to the basic client itself that may not provide the buffer
// that large enough for SSL data.
// The SSL client can do nothing for this case, you should increase the basic client buffer memory.
ESP_SSLClient ssl_client;

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
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println();
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

void ResetEthernet()
{
  Serial.println("Resetting WIZnet W5500 Ethernet Board...  ");
  pinMode(WIZNET_RESET_PIN, OUTPUT);
  digitalWrite(WIZNET_RESET_PIN, HIGH);
  delay(200);
  digitalWrite(WIZNET_RESET_PIN, LOW);
  delay(50);
  digitalWrite(WIZNET_RESET_PIN, HIGH);
  delay(200);
}

void networkConnection()
{
  Ethernet.init(WIZNET_CS_PIN);

  ResetEthernet();

  Serial.println("Starting Ethernet connection...");
  Ethernet.begin(Eth_MAC);

  unsigned long to = millis();

  while (Ethernet.linkStatus() == LinkOFF || millis() - to < 2000)
  {
    delay(100);
  }

  if (Ethernet.linkStatus() == LinkON)
  {
    Serial.print("Connected with IP ");
    Serial.println(Ethernet.localIP());
  }
  else
  {
    Serial.println("Can't connect");
  }
}

// Define the callback function to handle server status acknowledgement
void networkStatusRequestCallback()
{
  // Set the network status
  fbdo.setNetworkStatus(Ethernet.linkStatus() == LinkON);
}

void setup()
{
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  networkConnection();

  Serial_Printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Initialize Firebase */
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(A0, INPUT);
}

void loop()
{

  if (!client.connected())
  {
    reconnect();
  }

  client.loop();

  // test send simple "hello world" message
  //  client.publish("esp32/input", "hello world");

  // Ultrasonic Sensor
  long duration, distance;
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  lcd.setCursor(0, 1);
  lcd.print("Distance: ");
  lcd.print(distance);
  lcd.print(" cm");

  // water level sensor
  int water_level = analogRead(A0);
  Serial.print("Water Level: ");
  Serial.println(water_level);

  lcd.setCursor(0, 2);
  lcd.print("Water Level: ");
  lcd.print(water_level);

  // send data to firebase RTDB
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();

    // Serial_Printf("Set bool... %s\n", Firebase.RTDB.setBool(&fbdo, F("/test/bool"), count % 2 == 0) ? "ok" : fbdo.errorReason().c_str());

    // Save distance
    if (Firebase.RTDB.setDouble(&fbdo, "/test/distance", distance))
    {
      Serial.println("Distance saved successfully.");
    }
    else
    {
      Serial.print("Distance save failed, reason: ");
      Serial.println(fbdo.errorReason());
    }

    // Save water level
    if (Firebase.RTDB.setDouble(&fbdo, "/test/water_level", water_level))
    {
      Serial.println("Water level saved successfully.");
    }
    else
    {
      Serial.print("Water level save failed, reason: ");
      Serial.println(fbdo.errorReason());
    }

    count++;
  }

  // connected to
  // lcd.setCursor(0, 2);
  // lcd.print("Connected to: ");
  // lcd.print(ssid);

  delay(2000);
  //lcd.clear();
}
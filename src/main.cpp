#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <secret.h>

#define PWM_Pin   25
#define PWM1_Ch    0      // [0 - 15]
#define PWM1_Res   8
#define PWM1_Freq  1000

//const int BUILTIN_LED = 2;
const int LED = 2;

// Publish topic and Subscribe topic
const char* outTopic = "ceilingLight/demo";       // Publish topic
const char* inTopic = "ceilingLight/cmd";         // Subscribe topic

WiFiClient espClient;
PubSubClient client(espClient);

// Private variables
char msg[256];
unsigned long currectTime = 0;
unsigned long lastTime = 0;
int count = 0;
char lstatus[4];
bool status = 0;    // LED is OFF

uint32_t pwmVal = 0;    // PWM duty cycle value
char txBuffer[256];

// Prototype functions
void pub_response(unsigned int state);
String getValue(byte* data, unsigned int length, char separator, int index);

// WiFi setup function
void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connect to ");
  Serial.println(ssid);

  WiFi.begin(ssid,password);
  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// MQTT callback function
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.println("----------------------------------------------------");
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // payload
  Serial.print("payload : ");
  for(unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // bye array to string
  String payload_in = "";
  for(unsigned int i = 0; i< length; i++)
  {
    payload_in += (char)payload[i];
  }
  //Serial.println(payload_in);

  // topic: ceilingLight/cmd
  // payload: switch/ON       -> LED turn ON
  //          switch/OFF      -> LED turn OFF

  // split with '/'
  int idx1, idx2;                                           // index to string
  idx1 = payload_in.indexOf('/');                           // find location of first
  String device = payload_in.substring(0, idx1);            // capture first data string
  //idx2 = payload_in.indexOf('/', idx1+1);                   // find location of second
  //String cmd = payload_in.substring(idx1 + 1, idx2 + 1);    // capture second data string
  String cmd = payload_in.substring(idx1 + 1, length );    // capture second data string
  Serial.print("Device : ");
  Serial.println(device);
  Serial.print("Command : ");
  Serial.println(cmd);
  Serial.println();

  // parse command
  if(device == "switch")
  {
    if(cmd == "ON")
    {
      digitalWrite(LED, HIGH);
      pub_response(1);              // publish update data
      Serial.println("LED is HIGH");
    }
    else
    {
      digitalWrite(LED, LOW);
      pub_response(0);              // publish udate data
      Serial.println("LED is LOW");
    }
    Serial.println("----------------------------------------------------");
    Serial.println();
  }

  // Demo set PWM value
  if(device == "setPWM")
  {
    pwmVal = cmd.toInt();             // convert ASCII to integer
    if(pwmVal > 255) pwmVal = 255;    // set limit
    pwmVal = 255 - pwmVal;            // convert for hardware active low
    snprintf(txBuffer, 256, "Set PWM value: %lu\r\n", pwmVal);
    Serial.print(txBuffer);
    snprintf(txBuffer, 256, "Brightness: %d%%\r\n", (uint16_t)(pwmVal*100/255));
    Serial.print(txBuffer);
    ledcWrite(PWM1_Ch, pwmVal);        // set PWM value
  }
}

// MQTT reconnect function
void reconnect()
{
  count ++;
  // Loop until we're connected
  while(!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Create random client ID
    String clientID = "ESP8266client-";
    clientID += String(random(0xffff), HEX);

    //client.setServer(mqtt_server, mqtt_port);

    // Attempt to connected
    if(client.connect(clientID.c_str(), mqtt_username, mqtt_password))
    {
      Serial.print("Connected");
      client.subscribe(inTopic);        // subscribe topic : cmd/sn0001
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

  Serial.print("Count: ");
  Serial.println(count);
  if(count >= 10) {
      ESP.restart();
  }
}

// Setup function
void setup() {
  // put your setup code here, to run once:
  pinMode(LED, OUTPUT);                       // Built-in LED GPIO2
  ledcAttachPin(PWM_Pin, PWM1_Ch);            // PWM pin, PWM channel
  ledcSetup(PWM1_Ch, PWM1_Freq, PWM1_Res);    // PWM channel, PWM frequency, PWM resolution
  Serial.begin(115200);                       // Baudrate config 115200, n, 1
  setup_wifi();         // WiFi setup
  client.setServer(mqtt_server, mqtt_port);   // MQTT setup
  client.setCallback(callback);               // Set MQTT callback

  digitalWrite(LED, LOW);                     // Turn off LED

}

// Infinity loop
void loop() {
  // put your main code here, to run repeatedly:
  if(!client.connected())
  {
    reconnect();
  }
  client.loop();

  unsigned long currectTime = millis();
  if(currectTime - lastTime >= 60000)
  {
    lastTime = currectTime;   // timestamp

    snprintf(msg, 64, "Hello from ESP32");

    Serial.print("-> Publish message: ");
    Serial.print(msg);
    Serial.print(" -t ");
    Serial.println(outTopic);
    client.publish(outTopic, msg); // publish topic : ceilingLight/demo
  }

}

// MQTT publish response function
void pub_response(unsigned int state)
{
  status = state;

  if(status){
    snprintf(lstatus, 4, "ON");
  }else{
    snprintf(lstatus, 4, "OFF");
  }

  snprintf(msg, 100, "{\"lstatus\":\"%s\"}", lstatus);
  Serial.print("Publish message: ");
  Serial.print(msg);
  Serial.print(" -t ");
  Serial.println(outTopic);
  client.publish(outTopic, msg); // publish topic : ceilingLight/demo
}


String getValue(byte* data, unsigned int length, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = length - 1;
  String myStr = "";

  for(int i = 0; i <= maxIndex && found <= index; i++)
  {
    if(data[i] == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? myStr.substring(strIndex[0], strIndex[1]) : "" ;
}

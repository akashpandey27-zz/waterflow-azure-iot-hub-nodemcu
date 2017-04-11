/* Libraries for Azure IoT hub  */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h> 


/*Variables for internet and Azure IoT hub */
const char* ssid = "CloudThat_IoT_Lab2_2.4GHz";   // WiFi SSID
const char* password = "ghjklasdf1*&";            // WiFi Password
String azure_mqtt_server = "azure-devices.net";   // Azure IoT Hub URL
String IoTHub = "waterflow-iothub";               // IoTHub Name
String device_id = "waterflow-sensor-001";        // Device ID

String inTopic = "devices/"+device_id+"/messages/devicebound/#";
String outTopic = "devices/"+device_id+"/messages/events/";
String temp1 = IoTHub+"."+azure_mqtt_server;
const char* mqtt_server = temp1.c_str();
String temp2 = IoTHub+"."+azure_mqtt_server+"/"+device_id;
const char* hubuser = temp2.c_str();
const char* hubpass = "SharedAccessSignature sr=waterflow-iothub.azure-devices.net%2Fdevices%2Fwaterflow-sensor-001&sig=mTDZ%2F%2F8bgcrmiYDklrPCYHoChHE5DqOVxryXZokJCR8%3D&se=1523351824";

WiFiClientSecure espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;



byte statusLed    = 13;
byte statusLedPowerOn    = D7;
byte statusLedWaterFlow    = D6;
byte statusLedsendingToCloud    = D5;

byte sensorInterrupt = D2;  // 0 = digital pin 2
byte sensorPin       = 2;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
float calibrationFactor = 6.0;

volatile byte pulseCount;  

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

unsigned long oldTime;

String oldOutputStr;


void setup()
{
  
  // Initialize a serial connection for reporting values to the host
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 8883);
  client.setCallback(callback);
  // Set up the status LED line as an output
  pinMode(statusLed, OUTPUT);
  digitalWrite(statusLed, HIGH);  // We have an active-low LED attached
  
  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);

  pulseCount        = 0;
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0;

  // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
  // Configured to trigger on a FALLING state change (transition from HIGH
  // state to LOW state)
  //attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
   attachInterrupt(digitalPinToInterrupt(sensorInterrupt), pulseCounter, FALLING);

   pinMode(statusLedPowerOn, OUTPUT);
   pinMode(statusLedWaterFlow, OUTPUT);
   pinMode(statusLedsendingToCloud, OUTPUT);
   digitalWrite(statusLedPowerOn, HIGH);
}

/**
 * Main program loop
 */
void loop(){
 digitalWrite(statusLedsendingToCloud, LOW);
 String outputStr =  waterFlow();
  if(outputStr.length() > 0 && oldOutputStr != outputStr){
     Serial.println(outputStr);
        if (!client.connected()) {
          reconnect();
        }
        client.loop();
            
        Serial.print("Publish message: ");
        Serial.println(outputStr.c_str());
        if(client.publish(outTopic.c_str(), outputStr.c_str())){
          digitalWrite(statusLedsendingToCloud, HIGH);
        }
        oldOutputStr = outputStr;
  }
  
}

/*
Insterrupt Service Routine
 */
void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
  digitalWrite(statusLedWaterFlow, HIGH);
}
String waterFlow(){
   if((millis() - oldTime) > 1000)    // Only process counters once per second
  { 
    // Disable the interrupt while calculating flow rate and sending the value to
    // the host
    detachInterrupt(sensorInterrupt);
        
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    
    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    oldTime = millis();
    
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;
    
    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
      
    unsigned int frac;


    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
    // Enable the interrupt again now that we've finished sending output
    digitalWrite(statusLedWaterFlow, LOW);
    //attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
    attachInterrupt(digitalPinToInterrupt(sensorInterrupt), pulseCounter, FALLING);
    return "{\"DeviceID\":\"waterflow-sensor-001\",\"FlowRate\" : "+String(flowRate)+",\"LiquidQuantity\" : "+ String(totalMilliLitres) +"}";
  }  
}


/* IoT hub functions */






void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(device_id.c_str(), hubuser, hubpass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(outTopic.c_str(), "hello world");
      // ... and resubscribe
      client.subscribe(inTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


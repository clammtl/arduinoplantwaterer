/*Automated plant waterer on Arduino MKR1000
Soil sensor and relay to water pump connected to Arduino MKR1000 on A0 and D4 respectively
code to update ThingSpeak in bulk-update modified from  https://www.mathworks.com/help/thingspeak/continuously-collect-data-and-bulk-update-a-thingspeak-channel-using-an-arduino-mkr1000-board-or-an-esp8266-board.html
webserver code modified from https://www.arduino.cc/en/Tutorial/WiFiWebServer
by clammtl 2017 */

#include <SPI.h>
#include <WiFi101.h>
#include <stdio.h>
#include "arduino_secrets.h" 


char jsonBuffer[500] = "["; // Initialize the jsonBuffer to hold sensor data
char jsonBufferP[500] = "["; // Initialize the jsonBuffer to hold pump data

char ssid[] = SECRET_SSID; //  Update your network SSID (name)in arduino_secrets.h
char pass[] = SECRET_PASS; // update your network password in arduino_secrets.h
WiFiClient client; // Initialize the WiFi client library

char server[] = "api.thingspeak.com"; // ThingSpeak Server

/* Set up arduino variables */
int sensorPin = A0; //sensorPin setup to analog 0
int sensorValue;  //collects the value of the sensor
int percent; //converts the value of the sensor to a percentage
int pump = 4; // setup the pump to digital 4


/* Collect data once every 15 seconds and post data to ThingSpeak channel once every 2 minutes */
unsigned long lastConnectionTime = 0; // Track the last connection time
unsigned long lastUpdateTime = 0; // Track the last update time
const unsigned long postingInterval = 120L * 1000L; // Post data every 2 minutes
const unsigned long updateInterval = 15L * 1000L; // Update once every 15 seconds

void setup() {
  Serial.begin(9600);
  
  // Attempt to connect to WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
    delay(10000);  // Wait 10 seconds to connect
  }
  Serial.println("Connected to wifi");
  printWiFiStatus(); // Print WiFi connection information

  // initializes the pump
  pinMode(pump, OUTPUT);

}

void loop() {


  //gets value from soil moisture sensor
  sensorValue = analogRead(sensorPin);
  percent = convertToPercent(sensorValue); 
  Serial.print(percent);
  Serial.println("%");

  
  //activate the pump if humidity is lower than 40%
  if (percent < 40)
  {
    digitalWrite(pump, HIGH);
    delay(3000); // activate the pump for 3 seconds
    digitalWrite(pump, LOW);
    delay(15000); // wait 15 seconds before reading the moisture value again.
  }
  else
  {
    digitalWrite(pump, LOW);
  }
  
  
  // If update time has reached 1 second, then update the jsonBuffer
  if (millis() - lastUpdateTime >=  updateInterval) {
    updatesJson(jsonBuffer);
  }

  

}



// Updates the jsonBuffer with humidity data
void updatesJson(char* jsonBuffer){
  /* JSON format for updates paramater in the API
   *  This examples uses the relative timestamp as it uses the "delta_t". You can also provide the absolute timestamp using the "created_at" parameter
   *  instead of "delta_t".
   *   "[{\"delta_t\":0,\"field1\":-70},{\"delta_t\":3,\"field1\":-66}]"
   */
  // Format the jsonBuffer as noted above

  strcat(jsonBuffer,"{\"delta_t\":");
  unsigned long deltaT = (millis() - lastUpdateTime)/1000;
  size_t lengthT = String(deltaT).length();
  char temp[4]; // initialize char tempp
  sprintf(temp, "%d", percent); //stores percent into string temp
  String(deltaT).toCharArray(temp,lengthT+1); //stores deltaT to temp
  strcat(jsonBuffer,temp); // stores temp to json buffer
  strcat(jsonBuffer,","); 
  strcat(jsonBuffer, "\"field1\":");
  long hum = percent; //initializes hum and stores percent
  lengthT = String(hum).length();
  String(hum).toCharArray(temp,lengthT+1); //adds hum to temp
  strcat(jsonBuffer,temp); // adds temp to json buffer
  strcat(jsonBuffer,"},");
  // If posting interval time has reached 1 minute, update the ThingSpeak channel with data
  if (millis() - lastConnectionTime >=  postingInterval) {
        size_t len = strlen(jsonBuffer);
        jsonBuffer[len-1] = ']';
        httpRequest(jsonBuffer);
  }
  lastUpdateTime = millis(); // Update the last update time
}


// Updates the ThingSpeakchannel with data
void httpRequest(char* jsonBuffer) {
  /* JSON format for data buffer in the API
   *  This examples uses the relative timestamp as it uses the "delta_t". You can also provide the absolute timestamp using the "created_at" parameter
   *  instead of "delta_t".
   *   "{\"write_api_key\":\"YOUR-CHANNEL-WRITEAPIKEY\",\"updates\":[{\"delta_t\":0,\"field1\":-60},{\"delta_t\":15,\"field1\":200},{\"delta_t\":15,\"field1\":-66}]
   */
  // Format the data buffer as noted above
  char data[500] = "{\"write_api_key\":\"ONLXSHEC1HCUP9RY\",\"updates\":";
  strcat(data,jsonBuffer);
  strcat(data,"}");
  
  // Close any connection before sending a new request
  client.stop();
  String data_length = String(strlen(data)+1); //Compute the data buffer length

  //Print the uploaded data to the serial
  Serial.println(data);
  
  // POST data to ThingSpeak
  if (client.connect(server, 80)) {
    client.println("POST /channels/376616/bulk_update.json HTTP/1.1"); // 
    client.println("Host: api.thingspeak.com");
    client.println("User-Agent: mw.doc.bulk-update (Arduino ESP8266)");
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.println("Content-Length: "+data_length);
    client.println();
    client.println(data);
  }
  else {
    Serial.println("Failure: Failed to connect to ThingSpeak");
  }
  delay(250); //Wait to receive the response
  client.parseFloat();
  String resp = String(client.parseInt());
  Serial.println("Response code:"+resp); // Print the response code. 202 indicates that the server has accepted the response
  jsonBuffer[0] = '['; //Reinitialize the jsonBuffer for next batch of data
  jsonBuffer[1] = '\0';
  lastConnectionTime = millis(); //Update the last connection time
}


//function to print the wifi status
void printWiFiStatus() {
  // Print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your device IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

//function to convert the analog sensor value to percentage
int convertToPercent(int value)
{
  int percentValue = 0;
  percentValue = map(value, 0, 1023, 100, 0);
  if(percentValue>100)
    percentValue = 100;
  return percentValue;
}



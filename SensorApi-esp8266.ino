/*
 *  This sketch sends data via HTTP POST requests to an API data service.
 *
 *  The device logs into the api with username & password. Then retrieves a JWT
 *  and then uses the JWT for subsquent posting of data. 
 *
 */

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

const char* ssid     = "";
const char* password = "";
const char* loginEmail = "";
const char* loginPass = "";

const char* host = "";
const int httpPort = 80;
const char* streamId   = "";
const char* url = "/api/v1/streams/";
String privateKey;
bool authorized = false;

struct sAverage {
  int32_t blockSum;
  uint16_t numSamples;
};

struct sAverage sampleAve;

const int analogInPin = A0;  // Analog input pin that the sensor is attached to
int sensorValue = 0;         // value read from sensor

/*******************************************************

 setup

 *******************************************************/
void setup() {

  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  //connect to wifi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  getJWToken(); //get authorization key
}

/*******************************************************

 loop

 *******************************************************/
void loop() {

  delay(1000);

  readSample();

  //only upload every 60 samples (~60 seconds). 
  if (sampleAve.numSamples >= 60) {

    int16_t sensorValue = getAverage(&sampleAve);

    const int BUFFER_SIZE = JSON_OBJECT_SIZE(1);
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject& dataRoot = jsonBuffer.createObject();
    dataRoot["value"] = sensorValue;

    Serial.println("----------Sending Data--------");
    Serial.print("connecting to ");
    Serial.println(host);

    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }

    // We now create a URI for the request
    url += streamId;
    url += "/data";

    Serial.print("Requesting URL: ");
    Serial.println(url);
    dataRoot.prettyPrintTo(Serial);
    //Serial.println(dataStr);

    // This will send the request to the server
    postRequest(&client, url, dataRoot, true);

    while (client.available() == 0)
    {
      if (client.connected() == 0)
      {
        return;
      }
    }

    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
      String line = client.readStringUntil('\r');
      if (line.equals("HTTP/1.1 401 Unauthorized")) {
        authorized = false;
      }
      Serial.print(line);
    }
    if (authorized == false) {
      getJWToken();
    } else {
      Serial.println("-------------------------");
    }
  }
}

/*******************************************************

 getJWToken

 *******************************************************/
void getJWToken() {

  String url = "/auth/login";
  StaticJsonBuffer<400> jsonBuffer; //used to store server response

  //build json object to send data
  const int BUFFER_SIZE = JSON_OBJECT_SIZE(2);
  StaticJsonBuffer<BUFFER_SIZE> jsonRequestBuffer;

  JsonObject& loginRoot = jsonRequestBuffer.createObject();
  loginRoot["email"] = loginEmail;
  loginRoot["password"] = loginPass;

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  Serial.println("----------Retrieving JWT--------");
  Serial.println("Retrieving JWT");
  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  postRequest(&client, url, loginRoot, false);

  //block until data available
  while (client.available() == 0)
  {
    if (client.connected() == 0)
    {
      return;
    }
  }
  Serial.print("response length:");
  Serial.println(client.available());

  client.setTimeout(1000); //in case things get stuck

  //read http header lines
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line.length() == 1) { //empty line means end of headers
      break;
    }
  }

  //read first line of body (should be jwt)
  if (client.available()) {
    String line = client.readStringUntil('\n');
    const char* lineChars = line.c_str();
    JsonObject& root = jsonBuffer.parseObject(lineChars);

    if (!root.success()) {
      Serial.println("-------Parse Json Failed-------");
      return;
    }

    if (root.containsKey("jwt"))
    {
      JsonObject& jwt = root["jwt"];
      const char* tokArray = jwt["token"];
      String token(tokArray); //convert to String
      privateKey = token;
      Serial.println("----------JWT Updated----------");

    } else {
      Serial.println("----------Login Failed----------");
      return;
    }

    authorized = true; //login process complete
  }
}

/*******************************************************

 postRequest

 *******************************************************/
void postRequest(WiFiClient* client, String url, JsonObject& jsonRoot, bool needKey) {
  // This will send the request to the server
  client->print("POST "); // POST /auth/login HTTP/1.1
  client->print(url);
  client->println(" HTTP/1.1");

  client->print("Host: "); //Host: 172.30.1.8
  client->println(host);

  client->println("User-Agent: Esp8266WiFi/0.9");

  if (needKey) {
    client->print("Authorization: Bearer "); //Authorization: Bearer <token>
    client->println(privateKey);
  }

  client->println("Accept: application/json");
  client->println("Content-Type: application/json");
  client->print("Content-Length: ");

  String dataStr;
  jsonRoot.printTo(dataStr);

  client->println(dataStr.length());
  client->println("Connection: close");
  client->println();
  //client->println(dataStr);
  jsonRoot.printTo(*client);
  client->println();
  delay(10);
}
/*******************************************************

 readSample

 *******************************************************/
void readSample() {
  int16_t sensorValue = analogRead(analogInPin);
  addSampleToAverage(&sampleAve, sensorValue);
}
/*******************************************************

 addSampleToAverage

 *******************************************************/
int16_t addSampleToAverage(struct sAverage *ave, int16_t newSample) {
  ave->blockSum += newSample;
  ave->numSamples++;
}
/*******************************************************

  getAverage

 *******************************************************/
int16_t getAverage(struct sAverage *ave) {
  int16_t average = ave->blockSum / ave->numSamples;
  // get ready for the next block
  ave->blockSum = 0; ave->numSamples = 0;
  return average;
}

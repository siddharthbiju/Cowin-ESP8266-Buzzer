static const uint8_t D1   = 5;

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> 
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h> // https://github.com/taranais/NTPClient
#include <WiFiUdp.h>
#include <ArduinoJson.h> // https://arduinojson.org

/* Set these to your desired credentials. */
const char *ssid = "SSID";  //ENTER YOUR WIFI SETTINGS
const char *password = "PASSWORD";


// Enter your cowin required information
const String PIN_CODE = "683542"; //ENTER YOUR LOCALITY PIN CODE
const int MIN_AGE_LIMIT = 18; //ENTER YOUR DESIRED AGE LIMIT
const int DOSE_NUMBER = 1; //ENTER YOUR DESIRED DOSE NUMBER. SHOULD BE 1 OR 2. DEFAULT IS 1.

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


DynamicJsonDocument doc(6144);


// Variables to save date and time
String formattedDate;
String dayStamp;

//Example Link to read data from https://cdn-api.co-vin.in/api/v2/appointment/sessions/public/calendarByPin?pincode=683542&date=01-07-2021
//Web/Server address to read/write from 
const char *host = "cdn-api.co-vin.in";
const int httpsPort = 443;  //HTTPS= 443 and HTTP = 80

//SHA1 finger print of certificate use web browser to view and copy
const char fingerprint[] PROGMEM = "SHA-1 Finger print";
//=======================================================================
//                    Power on setup
//=======================================================================

void setup() {
  pinMode(D1, OUTPUT); //Alarm PIN
  digitalWrite(D1, HIGH); //Turn Alarm OFF
  delay(1000);
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA);        //Only Station No AP, This line hides the viewing of ESP as wifi hotspot
  
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");

  Serial.print("Connecting");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  timeClient.begin();
  timeClient.setTimeOffset(19800);
}

//=======================================================================
//                    Main Program Loop
//=======================================================================
void loop() {
  WiFiClientSecure httpsClient;    //Declare object of class WiFiClient

  Serial.println(host);

  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  httpsClient.setFingerprint(fingerprint);
  httpsClient.setTimeout(15000); // 15 Seconds
  delay(1000);
  
  Serial.print("HTTPS Connecting");
  int r=0; //retry counter
  while((!httpsClient.connect(host, httpsPort)) && (r < 30)){
      delay(100);
      Serial.print(".");
      r++;
  }
  if(r==30) {
    Serial.println("Connection failed");
  }
  else {
    Serial.println("Connected to web");
  }
  
  String ADCData, getData, Link;
  int adcvalue=analogRead(A0);  //Read Analog value of LDR
  ADCData = String(adcvalue);   //String to interger conversion

  String currentDate = getDate();
  Serial.println(currentDate);

  //GET Data
  Link = "/api/v2/appointment/sessions/public/calendarByPin?pincode="+PIN_CODE+"&date=" + currentDate;
  

  Serial.print("requesting URL: ");
  Serial.println(host+Link);

  httpsClient.print(String("GET ") + Link + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +               
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
                  
  while (httpsClient.connected()) {
    String line = httpsClient.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  Serial.println("reply was:");
  Serial.println("==========");
  String line;
  while(httpsClient.available()){        
    line = httpsClient.readStringUntil('\n');  //Read Line by Line
    Serial.println(line); //Print response
  }
  Serial.println("==========");
  Serial.println("closing connection");
  findslots(line);
    
  delay(10000);  //GET Data at every 10 seconds
}


String getDate(){
  timeClient.update();
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  //2021-07-01
  String day = dayStamp.substring(8);
  String month = dayStamp.substring(5,7);
  String year = dayStamp.substring(0,4);
  dayStamp = day+"-"+month+"-"+year;
  return dayStamp;
}

void findslots(String input){
  DeserializationError error = deserializeJson(doc, input);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return; 
  }
  int count_centres = 0;
  int i =0;
  JsonObject centers_0 = doc["centers"][0];
  
  int centre = int(centers_0);
  while(centre!=0){
    count_centres=count_centres+1;
    i=i+1;
    centers_0 = doc["centers"][i];
    centre = int(centers_0);
  }

  Serial.println("Number of centres : "+String(count_centres));
  //Serial.println(count_centres);

  for(i=0;i<count_centres;i++){
    look_for_sessions(doc["centers"][i], String(i+1));
  }
}

void look_for_sessions(JsonObject centre, String centre_number){
  int count_sessions = 0;
  int i = 0;
  JsonObject session = centre["sessions"][0];    
  int session_int = int(session);
  
  while(session_int!=0){
    count_sessions=count_sessions+1;
    i=i+1;
    session = centre["sessions"][i];
    session_int = int(session);
  } 

  Serial.println("Number of sessions in center "+centre_number+" : "+String(count_sessions));
  //Serial.println(count_sessions);

  String dose = "available_capacity_dose1";
  if(DOSE_NUMBER==2){
    dose = "available_capacity_dose2";
  }

  for(i=0;i<count_sessions;i++){
    session = centre["sessions"][i];

    if(session["min_age_limit"]>=MIN_AGE_LIMIT){
      if(session[dose]>0){
        digitalWrite(D1, LOW);  //Turn Alarm ON
        Serial.println("yey! vaccines are available");
        Serial.println(String(centre["name"]));
        Serial.println(String(session["date"]));
        Serial.println(String(session["vaccine"]));
        Serial.println("Total number of vaccines : "+String(session[dose]));
        Serial.println("Fee type : "+String(centre["fee_type"]));
        Serial.println("...................");
        Serial.println("...................");
      }
    }
  }
}

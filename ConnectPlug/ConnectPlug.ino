// ------------------------------------ Initialisation --------------------------------------------------

// ----------------- Librairies
  #include <Arduino.h>
  #include <Hash.h>
  #include <WebSocketsServer.h>
  #include <WebSocketsClient.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266mDNS.h>
  #include <FS.h>
  #include <DHT.h> 
  #include "user_interface.h" //for timer purpose
  //#include <Average.h> //Deprecated
  #include "aes256.h" //Crypto library
  #include <ArduinoJson.h> // Json parser and creator
  #include <Adafruit_NeoPixel.h>  
  #include <stdlib.h> // String manipulator
  #include <ESP8266HTTPUpdateServer.h> //OverTheAir code updater
  #include <WiFiClient.h> // needed for OTA 

  #include <SparkFunTSL2561.h>
  #include <Wire.h>
  #include <time.h>


// ----------------- Serial Debug (comment or uncomment "#define DEBUG")

  #define DEBUG

  #ifdef DEBUG
    #define DBG_CORE(...) printf( __VA_ARGS__ )
    #define DBG_PRINT(x) Serial.print(x)
    #define DBG_PRINTLN(x) Serial.println(x)
  #else
    #define DBG_CORE(...)
    #define DBG_PRINT(x)
    #define DBG_PRINTLN(x)
  #endif

  //uncomment #define DEBUG_WEBSOCKETS(...) Serial.printf( __VA_ARGS__ ) in "WebSockets.h" to debug WebSockets

// ----------------- Bluetooth Serial definition (comment or uncomment)

  #define BT_SERIAL(x) Serial.println(x)
  // #define BT_SERIAL(x)

// ----------------- DHT definition

  #define DHTPIN 12     
  #define DHTTYPE DHT22
  DHT dht(DHTPIN, DHTTYPE);

// ----------------- Lux Sensor definition
  
  SFE_TSL2561 light; //on pin number 4 (SDA) and 5 (SCL) ?

// ----------------- LED NeoPixel definition
  #define PIN 0
  Adafruit_NeoPixel strip = Adafruit_NeoPixel(7, 0, NEO_GRB + NEO_KHZ800);
  #define FADER_STEPS 50
  uint32_t blackCol=strip.Color(0,0,0);

// ----------------- Variables definition
  //Average<int> ave(250); // // Reserve space for n entries in the average bucket.
  aes256_context ctxt; // Crypto init
 
  boolean gain;     // Gain setting, 0 = X1, 1 = X16;
  unsigned int ms;  // Integration ("shutter") time in milliseconds

  uint16_t i=0;
  volatile int rc;
  volatile int gc;
  volatile int bc;
  volatile int newrc;
  volatile int newgc;
  volatile int newbc;
  volatile int oldrc;
  volatile int oldgc;
  volatile int oldbc;
  volatile int command;
  volatile int boucle;
  volatile int wsClientReconnect=1; // mettre sur 1 pour la 1ere connexion ?
  volatile unsigned long lastDebounceTime = 0;
  volatile boolean buttonState;
  volatile unsigned long lastButtonState=0;

  String callBack;
  String wifistatestring[7] = {"Inactive", "No SSID available", "Scan completed", "Connected", "Connection failed", "Connection lost", "Connecting"};
  const char* initSTssid = "NUMERICABLE-3DA4";
  const char* initSTpwd = "BJX57NGKBW"; // Reading file error on STssid
  const char* initAPssid = "LightSwitch";
  const char* initAPpwd = "12345678";
  //const char* homeip = "192.168.0.11"; // host must be live updated and homeip != HomeIP ?
  String APssid;
  String APpwd;
  String STssid;
  String STpwd;
  String WifiLastStatus;
  String InfoFile = "/info.txt";
  String HomeFile = "/home.txt";
  String HomeState = "Not connected";
  String HomeIP;
  boolean WifiActive = false;
  boolean homeClientConnected = false;
  boolean waitingClient=false;
  boolean needUpdate = false;
  os_timer_t myTimer1;
  os_timer_t myTimer2;
  os_timer_t myTimer3;
  os_timer_t myTimer4;
  os_timer_t myTimer5;
  os_timer_t myTimer6;
  #define t3delay 2000
  bool tickUpdate;
  bool tickSensor;
  bool tickCurrent;
  bool tickLux;
  bool tickPattern;
  bool tickWsClient;

  String actuators[10] = {
    "R1",
    "R2",
    "R3",
    "R4",
    "led",
    "color",
    "get",
    "post",
    "ws",
    "sio"
  };

  unsigned long lastDhtRead=0;
  String oldtempDhtValue;
  String oldhumDhtValue;
  String tempDhtValue;
  String humDhtValue;
  float oldCurrentValue;
  float currentValue;
  int val[500] ; // Current raw data. When CPU is free 210 value = 20ms ...
  float oldLumValue;
  float lumValue;

  bool R1Status;
  byte ledStatus[7][3]={{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}};

  const char* host = "switch";
  uint8_t MAC_array[6];
  char MAC_char[18];
  char myIPString[24];
  char mylocalIPString[24];
  char mysubnetMaskString[24];
  char mygatewayIPString[24];
  int chk; // State of the DHT11 sensor (values are more reliable when chk is declared outside the voids)
  int clientTimeout=0;
  bool timeout; // ws timeout repsonse from server

  WiFiClient homeclient;
  String homeurl = "/api/esps";
  int homeport = 8080;

  String conditionalOrders="";
  String directOrders="";

  int timezone = 3;
  int dst = 0;

  File fsUploadFile; // for OTA file update

  // to do: byte oldPixelColor[7][3],long lastCurrentRead
  // long Time ; time.txt ; trouver un api time en unix timestamp

// ----------------- PIN Definition -----------------
  const int buttonPIN =14;
  const int R1PIN = 13;

// ----------------- Servers definition
  ESP8266WebServer server(80);
  ESP8266HTTPUpdateServer httpUpdater; // OTA updater
  WebSocketsServer wsServer = WebSocketsServer(81);
  WebSocketsClient wsClient;
  bool WsClientIsConnected = false; // socket.io client handle connection
  bool WsServerIsConnected = false; // WebSocket server handle connection

//-----------------------------------SPIFFS Files system sub-routines------------------------------------
  String formatBytes(size_t bytes) {
    if (bytes < 1024) {
      return String(bytes) + "B";
    } else if (bytes < (1024 * 1024)) {
      return String(bytes / 1024.0) + "KB";
    } else if (bytes < (1024 * 1024 * 1024)) {
      return String(bytes / 1024.0 / 1024.0) + "MB";
    } else {
      return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
    }
  }

  String getContentType(String filename) {
    if (server.hasArg("download")) return "application/octet-stream";
    else if (filename.endsWith(".htm")) return "text/html";
    else if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".xml")) return "text/xml";
    else if (filename.endsWith(".pdf")) return "application/x-pdf";
    else if (filename.endsWith(".zip")) return "application/x-zip";
    else if (filename.endsWith(".gz")) return "application/x-gzip";
    else if (filename.endsWith(".svg")) return "image/svg+xml";

    return "text/plain";
  }

  bool handleFileRead(String path) {
    //DBG_PRINTLN("handleFileRead: " + path);
    if (path.endsWith("/")) path += "index.htm";
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
      if (SPIFFS.exists(pathWithGz))
        path += ".gz";
      File file = SPIFFS.open(path, "r");
      size_t sent = server.streamFile(file, contentType);
      file.close();
      return true;
    }
    return false;
  }

  void handleFileUpload(){
    if(server.uri() != "/edit") return;
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){
      String filename = upload.filename;
      if(!filename.startsWith("/")) filename = "/"+filename;
      DBG_PRINTLN("handleFileUpload Name: "); DBG_PRINTLN(filename);
      fsUploadFile = SPIFFS.open(filename, "w");
      filename = String();
    } else if(upload.status == UPLOAD_FILE_WRITE){
      //DBG_PRINTLN("handleFileUpload Data: "); DBG_PRINTLN(upload.currentSize);
      if(fsUploadFile)
        fsUploadFile.write(upload.buf, upload.currentSize);
    } else if(upload.status == UPLOAD_FILE_END){
      if(fsUploadFile)
        fsUploadFile.close();
      DBG_PRINTLN("handleFileUpload Size: "); DBG_PRINTLN(upload.totalSize);
    }
  }

  void handleFileDelete(){
    if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
    String path = server.arg(0);
    DBG_PRINTLN("handleFileDelete: " + path);
    if(path == "/")
      return server.send(500, "text/plain", "BAD PATH");
    if(!SPIFFS.exists(path))
      return server.send(404, "text/plain", "FileNotFound");
    SPIFFS.remove(path);
    server.send(200, "text/plain", "File deleted");
    path = String();
  }

  void handleFileCreate(){
    if(server.args() == 0)
      return server.send(500, "text/plain", "BAD ARGS");
    String path = server.arg(0);
    DBG_PRINTLN("handleFileCreate: " + path);
    if(path == "/")
      return server.send(500, "text/plain", "BAD PATH");
    if(SPIFFS.exists(path))
      return server.send(500, "text/plain", "FILE EXISTS");
    File file = SPIFFS.open(path, "w");
    if(file)
      file.close();
    else
      return server.send(500, "text/plain", "CREATE FAILED");
    server.send(200, "text/plain", "File created");
    path = String();
  }

  void handleFileList() { // changer "dir" en "list" dans le html d'upload de fichier
    //if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}

    //String path = server.arg("dir");
    DBG_PRINTLN("handleFileList: ");
    Dir dir = SPIFFS.openDir("/");
    //path = String();

    String output = "[";
    while (dir.next()) {
      File entry = dir.openFile("r");
      if (output != "[") output += ',';
      bool isDir = false;
      output += "{\"type\":\"";
      output += (isDir) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += String(entry.name()).substring(1);
      output += "\"}";
      entry.close();
    }

    output += "]";
    server.send(200, "text/json", output);
  }

//---------------------------------------- Internal sub-routines -----------------------------------------

//------------------------------------------ Input monitoring -------------------------------------------

  void writeRelay(bool Rstate) {    // Deprecated
        R1Status = Rstate;
          if (Rstate) {
            digitalWrite(R1PIN, LOW);
          }
          else {
            digitalWrite(R1PIN, HIGH);
          }
    
    needUpdate = true;
  }

  void energyRead() { // one 50Hz period is + or - 210 samples
    os_timer_disarm(&myTimer3); // disarm all?
    float newAmp = 0;

    for (int i = 0; i < 212; i++) {
      val[i]=analogRead(A0);
    }

    long powerSum=0;
    for (int i = 0; i < 212; i++) {
        powerSum+=abs(val[i]-469);
    }

    newAmp =powerSum/4427+0.0202;

    DBG_PRINT("Current : ");
    DBG_PRINTLN(newAmp);
    newAmp = long(int(newAmp*20)); // Rounds the current at 0.05
    newAmp = newAmp / 20;
    // looks if the value needs to be updated to the "home" server
    if (newAmp != oldCurrentValue){
        needUpdate = true;
        DBG_PRINT("current changed : ");
        DBG_PRINTLN(newAmp);
        oldCurrentValue=newAmp;
        // to do : ajouter la valeur de l'energie consommée entre les 2 derniers relevés à Etotale: 
        // EdernierRelevé = Pintantanée*(lastTime-now()) avec Pint=I*220 et les temps en h --> (millist2-millist1)/(3600*1000)
        // gérer historique de consommation
    }
    currentValue=newAmp;
    os_timer_arm(&myTimer3, t3delay, true); // ?? remplacer 500 par
  }

  
  void dhtRead() {
    //2sec request delay if dht has been readen just before
    if(lastDhtRead+2000<millis()){
        float h = dht.readHumidity();
        float t = dht.readTemperature();
          if (isnan(h) || isnan(t)) {
            DBG_PRINTLN("Failed to read from DHT sensor!");
            tempDhtValue = "null"; // 999 to NaN ?
            humDhtValue ="null";
          } else {
            humDhtValue=String(h);
            tempDhtValue=String(t);
          }
    lastDhtRead=millis();
    if(tempDhtValue!=oldtempDhtValue || humDhtValue!=oldhumDhtValue){
      oldtempDhtValue=tempDhtValue;
      oldhumDhtValue=humDhtValue;
      needUpdate=true;
      DBG_PRINTLN("dht updated");
    }
    }
  }

  void lumRead() {
    double lux;  
    unsigned int data0, data1;
  
    if (light.getData(data0,data1))  {   // getData() returned true, communication was successful
    
      // Resulting lux value
    boolean good;  // True if neither sensor is saturated

    good = light.getLux(gain,ms,data0,data1,lux);   // Perform lux calculation:

    DBG_PRINT(" lux: ");
    DBG_PRINTLN(lux);
    }
    else {
    DBG_PRINTLN(" lux I2C error");  // getData() returned false because of an I2C error, inform the user.
    }
    lumValue=lux;
    tickLux=false;
  }

  void buttonOnChange() {
    boolean reading = digitalRead(buttonPIN);
    if(reading != buttonState) return; // Ignore dupe readings.
    boolean debounce = false;
    
    if((millis() - lastDebounceTime) <= 50) { // debounceDelay=50 (Ignore reads<50ms threshold)
      debounce = true;
    }
    lastDebounceTime = millis();
    if(debounce) return;  

    buttonState = !reading; // inverser la logique du bouton
    DBG_PRINTLN("button: " + String(buttonState));
    DBG_PRINTLN("lapse: " + String(millis()-lastButtonState));
    DBG_PRINTLN("lastButtonState: " + String(millis()-lastButtonState));
    lastButtonState = millis();
    needUpdate=true;
  }

// ------------------------------------------ Output monitoring -----------------------------------------
  void ExecuteOrder(String order, String &cb) { // Deprecated
        StaticJsonBuffer<600> jsonBuffer;
         //String((const char*)payload)

          if (order.length()>360) {
            cb="JSON is too big";
            DBG_PRINTLN(cb);
            return;
          }
          
          JsonObject& objet = jsonBuffer.parseObject(order);
          if (!objet.success()) {
            cb="parseObject() failed";
            DBG_PRINTLN(cb);
            return;
          }
          writeRelay((boolean) objet["R1"]);
          needUpdate=true;
  }

  void SetActuator(int actuator,String value) {
    Serial.println("void SetActuator :");
    Serial.println(actuator);
    Serial.println(value);
    switch (actuator) {
        case 0: //Relay1
          R1Status = boolean(value.toInt());
            if (R1Status) {
              digitalWrite(R1PIN, LOW);
            }
            else {
              digitalWrite(R1PIN, HIGH);
            }
        break;

        case 4: //Led
              if (boolean(value.toInt())) {
                Serial.print("Setting led on");
              }
              else {
                Serial.print("Setting led off");
              }
        break;
    needUpdate = true;
    }
  }

// ------------------------------------ Conditional Orders ----------------------------------------------
  void parseOrders(String orderTemp, String &cb) {
    boolean updateOrders=false;
    // try using DynamicJsonBuffer with esp
    if (orderTemp.length()>360) {
      cb="JSON is too big";
      DBG_PRINTLN(cb);
      return;
    }
    StaticJsonBuffer < 600 > jsonBuffer;
    StaticJsonBuffer < 600 > jsonConditionalOrders;
    JsonObject& objectConditionalOrders = jsonConditionalOrders.createObject();
    StaticJsonBuffer < 600 > jsonDirectOrders;
    JsonObject& objectDirectOrders = jsonDirectOrders .createObject();
    JsonObject& object = jsonBuffer.parseObject(orderTemp);
    if (!object.success()) {
        cb = "parseObject() failed";
        DBG_PRINTLN(cb);
        return;
    }
    cb = "data ok";
    object.prettyPrintTo(Serial);
    for (int i = 0; i < 10; i++) {
    Serial.println("for 0 to 10");
        if (object.containsKey(actuators[i])) {
          updateOrders=true;
          Serial.println("if containsKay");
            if (object[actuators[i]].is < JsonObject& > ()) // if that then root[actuators[i]] is a conditional order
            {
              Serial.println("is JsonObject");
                objectConditionalOrders[actuators[i]]=object[actuators[i]];
            } 
            else 
            {
              Serial.println("is not JsonObject");                
                objectDirectOrders[actuators[i]]=object[actuators[i]];
            }
        }
    }
    // to do
    if (updateOrders) {
      if (conditionalOrders == "" ||  conditionalOrders == "{}") {
        conditionalOrders="";
        objectConditionalOrders.printTo(conditionalOrders);
        WriteTxtData("/cOrders.txt",conditionalOrders);
      } 
      else {
        // Vérifier que les ordres de strTemp ne sont pas incompatibles avec ceux de conditionalOrders ??
        String strTemp;
        conditionalOrders.remove(conditionalOrders.length()-1, 1);
        objectConditionalOrders.printTo(strTemp);
        strTemp.remove(0,1);
        conditionalOrders=conditionalOrders+","+strTemp;
        WriteTxtData("/cOrders.txt",conditionalOrders);
      }
    }
    
    directOrders="";
    objectDirectOrders.printTo(directOrders);
  }

  void executeConditionalOrders(String &ordersTemp, String &cb) {
      boolean updateOrders=false;
      String temp="";
      StaticJsonBuffer <600> jsonTemp;
      JsonObject& object = jsonTemp.parseObject(ordersTemp);
      if (!object.success()) {
          cb = "parseObject() failed";
          return;
      }
      cb = "data ok";

      // si un ordre est unique alors on le supprime de ordres[]
      // on sauvegarde ordres[] en txt

      for (int i = 0; i < 10; i++) {
          if (object.containsKey(actuators[i])) // root[actuators[i]] is valid
          {
              temp=object[actuators[i]]["if"].asString();
              if (temp.length() >0) // root[actuators[i]] is a conditional order
                  {
                      if (ordreIfString(object[actuators[i]]["if"])) {
                          String a = object[actuators[i]]["value"].asString();
                          SetActuator(i,a);
                          int r = object[actuators[i]]["rep"]; // Management of the Repetability of the orders

                            if (r>1) {
                              Serial.println("r>1");
                              object.remove(actuators[i]); // on retire la clé du json
                              JsonObject& tempObject = object.createNestedObject(actuators[i]);
                              tempObject["if"] = temp;
                              tempObject["value"] = a;
                              tempObject["rep"] = r-1;
                              updateOrders = true;
                                  object.prettyPrintTo(Serial);

                            }

                            else if (r==1) {
                              Serial.println("r==1");
                              object.remove(actuators[i]); // une fois que l'ordre à été exécuté ET si il n'y a qu'un ordre....
                              updateOrders = true;
                                  object.prettyPrintTo(Serial);

                            }


                          // ordresNow[] est exécuté une fois puis effacé (chaque type d'ordre a son sous-prog)
                          // ordres[] est sauvegardé dans un fichier texte en cas de reboot de l'esp
                      }

                  }
              temp=object[actuators[i]]["map"].asString();
              if (temp.length() > 0) // root[actuators[i]] is a conditional order
              {

              }
              temp=object[actuators[i]]["and"].asString();
              if (temp.length() > 0) // root[actuators[i]] is a conditional order
              {
                  Serial.println("ordre and detecte");
              }
              temp=object[actuators[i]]["or"].asString();
              if (temp.length() > 0) // root[actuators[i]] is a conditional order
              {
                  Serial.println("ordre or detecte");
              }
              temp=object[actuators[i]]["change"].asString();
              if (temp.length() > 0) // root[actuators[i]] is a conditional order
              {
                  Serial.println("ordre change detecte");
              }
          }
      }
    if (updateOrders == true) {
      ordersTemp="";
      object.printTo(ordersTemp);
      WriteTxtData("/cOrders.txt",ordersTemp);
    } // If a Repetability modification has occured then modify and save the Json
  }

  void executeDirectOrders(String &ordersTemp, String &cb) {
      // si ordersTemp n'est pas vide alors :
      String temp="";
      StaticJsonBuffer <600> jsonTemp;
      JsonObject& object = jsonTemp.parseObject(ordersTemp);
      if (!object.success()) {
          cb = "parseObject() failed";
          return;
      }
      cb = "data ok";

      for (int i = 0; i < 10; i++) {
          if (object.containsKey(actuators[i])) // root[actuators[i]] is valid
          {
              String a = object[actuators[i]].asString();
              SetActuator(i,a);
              needUpdate=true;          
          }
      }
      // delete ordersTemp
  }

  boolean ordreIfString(String ordre) {
      int commaIndex = ordre.indexOf(',');
      int secondCommaIndex = ordre.indexOf(',', commaIndex + 1);
      char capteur = ordre.substring(0, commaIndex).charAt(0);
      String capteurValue;
      char type = ordre.substring(commaIndex + 1, secondCommaIndex).charAt(0);
      String condition = ordre.substring(secondCommaIndex + 1);
      switch (capteur) {
          case 'l':
              capteurValue = lumValue;
          break;
          case 'h':
              capteurValue = humDhtValue;
              break;
          case 't':
              capteurValue = tempDhtValue;
          break;
          case 'b':
              capteurValue = buttonState;
          break;
          case 'c':
              capteurValue = currentValue;
          break;
      }

      switch (type) {
          case '<':
              if (capteurValue < condition) {
                  return 1;
              } else {
                  return 0;
              }
              break;

          case '>':
              if (capteurValue > condition) {
                  return 1;
              } else {
                  return 0;
              }
              break;

          case '=':
              if (capteurValue == condition) {
                  return 1;
              } else {
                  return 0;
              }
              break;

          default:
              return 0;
              break;
      }
  }

//------------------------------------- Internal file handling ------------------------------------------
  void readWifiData(String filename) {
    File f = SPIFFS.open(filename, "r");
    if (!f) {
      DBG_PRINTLN("Wifi parameters file failed to open");
    }
    else {
      APssid = f.readStringUntil('\n');
      APssid = APssid.substring(0, APssid.length() - 1);
      APpwd = f.readStringUntil('\n');
      APpwd = APpwd.substring(0, APpwd.length() - 1);
      STssid = f.readStringUntil('\n');
      STssid = STssid.substring(0, STssid.length() - 1);
      STpwd = f.readStringUntil('\n');
      STpwd = STpwd.substring(0, STpwd.length() - 1);

      f.close();
    }

    if (APssid.length() < 1) { //we can add more security in text verifying
      APssid = initAPssid;
      APpwd = initAPpwd;
    }

    if (STssid.length() < 1) {
      STssid = initSTssid;
      STpwd = initSTpwd;
      DBG_PRINTLN(STssid);
      DBG_PRINTLN(STpwd);
    }
  }

  void WriteWifiData(String filename) {
    File f = SPIFFS.open(filename, "w");
    if (!f) {
      DBG_PRINTLN("file open failed");
    }
    else {
      f.println(APssid);
      f.println(APpwd);
      f.println(STssid);
      f.println(STpwd);
      f.close();
      DBG_PRINTLN("file writed");
    }
  }

  void WriteTxtData(String filename, String data) {
    File f = SPIFFS.open(filename, "w");
    if (!f) {
      DBG_PRINTLN("file open failed");
    }
    else {
      f.println(data);
      f.close();
      DBG_PRINTLN("file writed");
    } //writes serial data to a file
  }

  void readHomeData(String filename) {
    String Homeportstr;
    File f = SPIFFS.open(filename, "r");
    if (!f) {
      DBG_PRINTLN("Home parameters file failed to open");
    }
    else {
      HomeIP = f.readStringUntil('\n');
      HomeIP = HomeIP.substring(0, HomeIP.length() - 1);
      Homeportstr = f.readStringUntil('\n');
      Homeportstr = Homeportstr.substring(0, Homeportstr.length() - 1);
      homeport=Homeportstr.toInt();
      f.close();
    }
  }

  void writeHomeData(String filename) {

    File f = SPIFFS.open(HomeFile, "w");
    if (!f) {
      DBG_PRINTLN("file open failed");
    }
    else {
      f.println(HomeIP);
      f.println(homeport);
      f.close();
      DBG_PRINTLN("file writed");
    }
  }

  void readConditionalOrders(String filename) {
    File f = SPIFFS.open(filename, "r");
    if (!f) {
      DBG_PRINTLN("Conditional orders file failed to open");
    }
    else {
      conditionalOrders = f.readStringUntil('\n');
    }
  }
//----------------------------------------  Encryption --------------------------------------------------
  String privateKey="olivieretflorianolivieretflorian";

  String cryptojson(){
    String encodedjson; // = iotDBjson():
    String message;
    message=iotDBjson();
    
      uint8_t key[] ="olivieretflorianolivieretflorian";

      for (int k=0;k<message.length();k=k+16){
      String sms="" ;
            aes256_init(&ctxt, key);
            uint8_t data[16];
            String stringOne = message.substring(k,k+16);


            char charBuf[17];
            stringOne.toCharArray(charBuf, 17) ;

                for (int i=0;i<sizeof(data);i++){
                data[i]=charBuf[i];
                }

           //complete message with zeros until having 16 characters
                if (message.length()<k+16){
                int diff=k+16-message.length();
                    for (int j=16-diff;j<16;j++){
                      data[j]=00;
                    }
                }

            aes256_encrypt_ecb(&ctxt, data); // must decrypt on receiving data too
            aes256_done(&ctxt);

                for (int i=0;i<sizeof(data);i++){
                  if(String(data[i],HEX).length()==1){
                    sms+="0";
                  }
                  sms += String(data[i],HEX);
                }
                
              encodedjson +=sms;

      }
      DBG_PRINTLN(encodedjson);
      return encodedjson;
  }

  String randomKey() {
    byte randomValue;
    char msg[32];     // Keep in mind SRAM limits
    String strMsg;

    int i;
    
        for(i = 0; i < 32; i++) {
          randomValue = random(0, 62);
          msg[i] = randomValue + 'a';
          if(randomValue > 25) {
              if(randomValue > 35) {
                msg[i] = (randomValue - 36) + 'A';
              }
              else {
                msg[i] = (randomValue - 26) + '0';
              }
          }
        }
        
        strMsg=String(msg);
        strMsg.remove(32);

        return strMsg;
  }

  String encrypt(String message,String keyStr){
    String encodedMsg;
    uint8_t key[32];
    char charBuf[33];
    keyStr.toCharArray(charBuf, 33) ; // todo: pass directly the key in hexadecimal

    uint8_t data[32];
    for (int i=0;i<sizeof(key);i++){
    key[i]=charBuf[i];
    }

      for (int k=0;k<message.length();k=k+16){
      String sms="" ;
            aes256_init(&ctxt, key);
            uint8_t data[16];
            String stringOne = message.substring(k,k+16);


            char charBuf[17];
            stringOne.toCharArray(charBuf, 17) ;

                for (int i=0;i<sizeof(data);i++){
                data[i]=charBuf[i];
                }

           //complete message with zeros until having 16 characters
                if (message.length()<k+16){
                int diff=k+16-message.length();
                    for (int j=16-diff;j<16;j++){
                      data[j]=00;
                    }
                }

            aes256_encrypt_ecb(&ctxt, data);
            aes256_done(&ctxt);

                for (int i=0;i<sizeof(data);i++){
                  if(String(data[i],HEX).length()==1){
                    sms+="0";
                  }
                  sms += String(data[i],HEX);
                }
                
              encodedMsg +=sms;

      }
      DBG_PRINTLN("encoded Msg:");
      DBG_PRINTLN(encodedMsg);
      return encodedMsg;
  }

  String decrypt(String hexMessage,String hexKeyStr){
    String decodedMsg;
    uint8_t key[32];
    char charBuf[33];
    uint8_t data[32];

    hexKeyStr.toCharArray(charBuf, 33) ;
      
    for (int i=0;i<sizeof(key);i++){
    key[i]=charBuf[i];
    }

    for (int k=0;k<hexMessage.length();k=k+32){
      String sms="" ;
            aes256_init(&ctxt, key);
            uint8_t data[16];
            String hexString = hexMessage.substring(k,k+32);

            char text[17];
            int j=0;
            for (int i=0;i< hexString.length();i=i+2){
              char hexLetterBuf[3];
              String hexLetter=hexString.substring(i,i+2);
              hexLetter.toCharArray(hexLetterBuf,3);
              text[j]=char(strtol(hexLetterBuf, 0, 16));
              j++;
            }

            for (int i=0;i<sizeof(data);i++){
            data[i]=text[i];
            }

            aes256_decrypt_ecb(&ctxt, data);
            aes256_done(&ctxt);

                for (int i=0;i<sizeof(data);i++){
                  sms += String(char(data[i]));
                }
                
              decodedMsg +=sms;

      }
      DBG_PRINTLN("decoded Msg:");
      DBG_PRINTLN(decodedMsg);
      return decodedMsg;
      //return hexStringToString(decodedMsg);
  }

  String hexStringToString(String hexString) {
    String textString="";
    for (int i=0;i< hexString.length();i=i+2){
      char hexLetterBuf[3];
      String hexLetter=hexString.substring(i,i+2);
      hexLetter.toCharArray(hexLetterBuf,3);
      textString+=char(strtol(hexLetterBuf, 0, 16)); // Strange but working !
    }
    return textString;
  }

//--------------------------------- String elaboration sub-routines -------------------------------------
  String wifiscan() {
    DBG_PRINTLN("scan start");

    // WiFi.scanNetworks will return the number of networks found
    // WiFi.scanNetworks doesnt work (miss always the best network) on this sdk version issue:#1355 on github
    // fixed : https://github.com/esp8266/Arduino/issues/1355

    // sort an array : http://www.hackshed.co.uk/arduino-sorting-array-integers-with-a-bubble-sort-algorithm/
    int n = WiFi.scanNetworks();
    DBG_PRINTLN("scan done");
    if (n == 0)
      DBG_PRINTLN("no networks found");
    else
    {
      DBG_PRINT(n);
      DBG_PRINTLN(" networks found");

      int rssi[30];
      memset(rssi, 0, sizeof(rssi));
      for (int i = 0; i < n; ++i) {
        rssi[i] = WiFi.RSSI(i);
      }

      int rank[30];
      for (int i = 0; i < 30; ++i) {
        rank[i] = i;
      }

      sort(rssi, n, rank);


      // Print SSID and RSSI in a json for each network found
      String json="[";
      for (int i = 0; i < n; ++i) {
        json += "{\"SSID\":\"" + String(WiFi.SSID(rank[i])) + "\"";
        json += ",\"RSSI\":\"" + String(WiFi.RSSI(rank[i])) + "\"},";
      }
      json = json.substring(0, json.length() - 1);
      json += "]";
      return json;
    }
    //DBG_PRINTLN("");
  }

  String wifistate() {
    // add ssid name (if wifistatus=connecting or connected or not exist?)
    String json = "{\"apstate\":\"" + wifistatestring[WiFi.status()] + "\",{\"apname\":\"" + STssid + "\"}";
    return json;
  }

  String wifiparam() {

    WifiToVars();

    String json = "{\"APip\":\"" + String(myIPString) + "\"";
    json += ",\"APssid\":\"" + APssid + "\"";
          if (WifiActive == true) {
            json += ",\"state\":\"" + wifistatestring[WiFi.status()] + "\"";
            json += ",\"STssid\":\"" + String(WiFi.SSID()) + "\"";
            }
          else {
            json += ",\"state\":\"" + WifiLastStatus + "\"";
            json += ",\"STssid\":\"" + STssid + "\"";
          }
    json += ",\"localip\":\"" + String(mylocalIPString) + "\"";
    json += ",\"netmaskip\":\"" + String(mysubnetMaskString) + "\"";
    json += ",\"gatewayip\":\"" + String(mygatewayIPString) + "\"";
    json += ",\"mac\":\""  + String(MAC_char) +  "\"";
    json += ",\"homeip\":\""  + HomeIP +  "\"";
    json += ",\"homeport\":\""  + String(homeport) +  "\"";
    json += ",\"homestate\":\""  + HomeState +  "\"}";

    return json;
  }

  String macstr() {
    WiFi.macAddress(MAC_array);
    for (int i = 0; i < sizeof(MAC_array); ++i) {
      sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
    }
    MAC_char[17] = (char)0;
    return String(MAC_char);
  }

  String macaddress() {
    String json = "{\"mac address\":\"" + String(MAC_char) +  "\"}";
    return json;
  }

  String dhtjson() {
    String json = "{\"temperature\":\"" + tempDhtValue +  "\",\"humidity\":\"" + humDhtValue +  "\"}";
    return json;
  }

  String currentjson() { //kaka ajouter les maxat et powersum pour debug depuis page web
    String json = "{\"current\":\"" + String(currentValue) +  "\"}";
    return json;
  }

  String iotDBjson() {
    String json = "{\"iotname\":\"" + APssid + "\",";
    json += "\"iotid\":\"" + String(MAC_char) + "\",";
    json += "\"R1\":" + String(R1Status) + ",";
    json += "\"localip\":\"" + String(mylocalIPString) + "\",";
    json += "\"current\":\"" + String(currentValue) + "\",";
    json += "\"luminosity\":\"" + String(lumValue) + "\",";
    json += "\"temperature\":\"" + tempDhtValue + "\",";
    json += "\"humidity\":\"" + humDhtValue + "\",";
    json += "\"button\":\"" + String(buttonState) + "\"}";
    // add time stamp ?
    return json;
  }

    String Randomjson() {
    String json = "{\"iotname\":\"" + APssid + "\",";
    json += "\"iotid\":\"" + String(MAC_char) + "\",";
    json += "\"R1\":" + String(R1Status) + ",";
    json += "\"localip\":\"" + String(mylocalIPString) + "\",";
    json += "\"current\":\"" + String(random(0, 16)) + "\",";
    json += "\"luminosity\":\"" + String(random(0, 100)) + "\",";
    json += "\"temperature\":\"" + String(random(0, 45)) + "\",";
    json += "\"humidity\":\"" + String(random(50, 100)) + "\",";
    json += "\"button\":\"" + String(buttonState) + "\"}";
    // add time stamp ?
    return json;
  }

  String loadjson() {
      String json="[";
      for (int i = 0; i < 500; i++) {
        val[i] = analogRead(A0);
      }
      for (int i = 0; i < 500; i++) {
        json +=String(val[i])+",";
      }
      json=json.substring(0, json.length() - 1);
      json+="]";
      return json;
  }

  String iotDBget() { // the old way of sending GET request
    String DBget = "?iotname=" + APssid;
    DBget += "&iotid=" + String(MAC_char);
    DBget += "&r1=" + String(R1Status);
    DBget += "&localip=" + String(mylocalIPString);
    DBget += "&current=" + String(currentValue);
    DBget += "&luminosity=" + String(lumValue);
    DBget += "&temperature=" + tempDhtValue;
    DBget += "&humidity=" + humDhtValue;
    return DBget;
  }

  String iotDBnewget() { // the new way of sending GET request has this shape : HomeIP/iotname/iotid/r1/r2/r3/r4/localip/current/temperature/humidity
    String DBget = "/" + APssid;
    DBget += "/" + String(MAC_char);
    DBget += "/" + String(R1Status);
    DBget += "/" + String(mylocalIPString);
    DBget += "/" + String(currentValue);
    DBget += "/" + String(lumValue);
    DBget += "/" + tempDhtValue;
    DBget += "/" + humDhtValue;
    DBget += "/";
    return DBget;
  }

// -------------------------------------- Networks connection -------------------------------------------
  void sort(int a[], int size, int r[]) {
    for (int i = 0; i < (size - 1); i++) {
      for (int o = 0; o < (size - (i + 1)); o++) {
        if (a[o] < a[o + 1]) {
          int t = a[o];
          a[o] = a[o + 1];
          a[o + 1] = t;
          t = r[o];
          r[o] = r[o + 1];
          r[o + 1] = t;
        }
      }
    } // used to sort the wifi access point list
  }
      
  void WifiToVars() {
    IPAddress myIP;
    IPAddress mylocalIP;
    IPAddress mysubnetMask;
    IPAddress mygatewayIP;

    myIP = WiFi.softAPIP();
    sprintf(myIPString, "%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
    mylocalIP = WiFi.localIP();
    sprintf(mylocalIPString, "%d.%d.%d.%d", mylocalIP[0], mylocalIP[1], mylocalIP[2], mylocalIP[3]);
    mysubnetMask = WiFi.subnetMask();
    sprintf(mysubnetMaskString, "%d.%d.%d.%d", mysubnetMask[0], mysubnetMask[1], mysubnetMask[2], mysubnetMask[3]);
    mygatewayIP = WiFi.gatewayIP();
    sprintf(mygatewayIPString, "%d.%d.%d.%d", mygatewayIP[0], mygatewayIP[1], mygatewayIP[2], mygatewayIP[3]); //// turns IP chars to strings
  }

  void APconnect(String ssid, String pwd) {
   WiFi.softAP(ssid.c_str(), pwd.c_str());
   needUpdate=true;
  }

  void STconnect(String ssid, String pwd) {
    DBG_PRINT("Connecting to : ");
    DBG_PRINTLN(ssid);
    WiFi.begin(ssid.c_str(), pwd.c_str());
    
    int i = 0;
    int k=1;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      DBG_PRINT(WiFi.status());
      i++;
      if (i > 30) {
        DBG_PRINTLN("");
        DBG_PRINT("Connection failed : ");
        WifiLastStatus = wifistatestring[WiFi.status()];
        DBG_PRINTLN(WifiLastStatus);
        WifiActive = false;
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        uint32_t col=strip.Color(15,0,0);
        for (int k=0; k<strip.numPixels();k++){
          strip.setPixelColor(k, blackCol);
        }
        strip.setPixelColor(0, col);
        strip.show();
        break;
      }
      uint32_t col=strip.Color(15,0,0);
      if (k==1){strip.setPixelColor(6, blackCol);}
      else {strip.setPixelColor(k-1, blackCol);}
      strip.setPixelColor(k, col);
      strip.show();
      k++;
      if(k>6){k=1;}
    }
    DBG_PRINTLN("");

    if (WiFi.status() == WL_CONNECTED) {
      WifiActive = true;
      DBG_PRINT("Connected to : ");
      DBG_PRINTLN(ssid);
      DBG_PRINT("Open http://");
      DBG_PRINTLN(WiFi.localIP());
      uint32_t col=strip.Color(0,15,0);
      for (int k=0; k<strip.numPixels();k++){
        strip.setPixelColor(k, blackCol);
      }
      strip.setPixelColor(0, col);
      strip.show();
      needUpdate=true;

      configTime(timezone * 3600, dst, "pool.ntp.org", "time.nist.gov");
      Serial.println("\nWaiting for time");
      while (!time(nullptr)) {
      Serial.print(".");
      delay(1000);
  }
    }
  }

  void MDNSstart(const char* h, IPAddress IP) {
    if (MDNS.begin(h, IP)) {
      DBG_PRINTLN("MDNS responder started");
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("ws", "tcp", 80);
      DBG_PRINT("Open http://");
      DBG_PRINT(host);
      DBG_PRINTLN(".local");
    }
    else {
      DBG_PRINTLN("MDNS.begin failed");
    }
  }

// ---------------------------------------- Websocket Server events -------------------------------------
  void wsServerEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    DBG_CORE("webSocketEvent(%d, %d, ...)\r\n", num, type);
    switch (type) {
      case WStype_DISCONNECTED:
        DBG_CORE("[WSs] [%u] Disconnected!\r\n", num);
        WsServerIsConnected=false;

        break;
      case WStype_CONNECTED:
        {
          IPAddress ip = wsServer.remoteIP(num);
          DBG_CORE("[WSs] [%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
          // Send the Relays status
          String tempR = iotDBjson();
          size_t lon = strlen(tempR.c_str());
          wsServer.sendTXT(num,tempR.c_str(),lon);
          WsServerIsConnected=true;
          
        }
        break;
      case WStype_TEXT:
        {
        DBG_CORE("[WSs] [%u] get Text: %s\r\n", num, payload);
        //payload type = '["R1":1,"R2":1,"R3":1,"R4":1]"
        String str =(const char*)payload;
        String callBack;
        ExecuteOrder(str,callBack);
      
        // add readcurrent for real time printing
        //else {
        //  DBG_PRINTLN("Unknown command");
        //}


        // kaka send iotDBjson to all connected clients 
        //wsServer.broadcastTXT(payload, length);
        }
        break;
      default:
        DBG_CORE("Invalid WStype [%d]\r\n", type);
        break;
    }
  }

// ---------------------------------------- WebSocket Client events -------------------------------------
  void wsClientEvent(WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:
            DBG_CORE("[WSc] Disconnected!\n");
            WsClientIsConnected = false;
            os_timer_arm(&myTimer6, 2000, false);
            timeout=1;
            break;
        case WStype_CONNECTED:
            {
                DBG_CORE("[WSc] Connected to url: %s\n",  payload);
                WsClientIsConnected = true;
                wsClientReconnect=1;
                os_timer_disarm(&myTimer6);

          // send message to server when Connected
                // socket.io upgrade confirmation message (required)
        wsClient.sendTXT("5");
            }
            break;
        case WStype_TEXT:
            DBG_CORE("[WSc] get text: %s\n", payload);

      // send message to server
      // webSocket.sendTXT("message here");
            break;
        case WStype_BIN:
            DBG_CORE("[WSc] get binary lenght: %u\n", lenght);
            hexdump(payload, lenght);

            // send data to server
            // webSocket.sendBIN(payload, lenght);
            break;
    }
  }

// ---------------------------------------- Timer interrupt ---------------------------------------------
  void timerUpdate(void *pArg) {
        tickUpdate = true;
  }

  void timerDHT(void *pArg) {
        tickSensor = true;
  }

  void timerCurrent(void *pArg) {
        tickCurrent = true;
  }

  void timerLux(void *pArg) {
        tickLux = true;
  }

  void timerPattern(void *pArg) {
        tickPattern = true;
  }

  void timerWsClient(void *pArg) {
        tickWsClient = true;
  }

// -------------------------------------- NeoPixel Patterns ---------------------------------------------

  void fader(int frc,int fgc,int fbc, uint8_t wait) {
  if (i==0){
    oldrc=rc;
    oldgc=gc;
    oldbc=bc;
  }
  if( i<FADER_STEPS) {
  DBG_PRINT(frc);
  DBG_PRINT(",");
    DBG_PRINT(oldrc);
  DBG_PRINT(",");
    DBG_PRINT(FADER_STEPS);
  DBG_PRINT(",");
  rc = (((float) frc- (float) oldrc)/FADER_STEPS)*i+oldrc;
  gc = (((float) fgc-(float) oldgc)/FADER_STEPS)*i+oldgc;
  bc =  (((float) fbc-(float) oldbc)/FADER_STEPS)*i+oldbc;
  DBG_PRINTLN(rc);
  uint32_t col=strip.Color(rc,gc,bc);
  for (int k=0; k<strip.numPixels();k++){
  strip.setPixelColor(k, col);
  }
  strip.show();
  i++;
  os_timer_arm(&myTimer5, wait, 0);
  }
  else { 
  i=0;
  os_timer_disarm(&myTimer5);
  DBG_PRINTLN("pattern ok");
  }

  }

// ----------------------------------------------- Setup ------------------------------------------------
void setup(void) {
// --------------- Pin Initialisation
  delay(100);
  pinMode(R1PIN, OUTPUT);
  pinMode(A0, INPUT);
  pinMode(buttonPIN, INPUT_PULLUP);

  attachInterrupt(buttonPIN, buttonOnChange, CHANGE);

// --------------- Output Initialisation 
  // (Inverted logic depending on the type of the relays)
  digitalWrite(R1PIN, HIGH);

// --------------- Serial Initialisation
  Serial.begin(115200);
  DBG_PRINT("\n");
  randomSeed(analogRead(0)); 

// --------------- Files system initialization
  SPIFFS.begin();
  FSInfo info;
  SPIFFS.info(info);

// ---------------- File system statistics
  DBG_CORE("Total: %u\nUsed: %u\nFree space: %u\nBlock size: %u\nPage size: %u\nMax open files: %u\nMax path len: %u\n",
                info.totalBytes,
                info.usedBytes,
                info.totalBytes - info.usedBytes,
                info.blockSize,
                info.pageSize,
                info.maxOpenFiles,
                info.maxPathLength
               );
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_CORE("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_CORE("\n");
  }

// ------------- Led NeoPixel initialisation

  strip.begin(); // This initializes the NeoPixel library.
  newrc=0;
  newbc=50;
  newgc=200;

// --------------- Connection to server and access point start
  WiFi.mode(WIFI_AP_STA);

  readWifiData(InfoFile);
  readHomeData(HomeFile);
  DBG_PRINTLN(STssid);
  DBG_PRINTLN(STpwd);
  STconnect(STssid, STpwd);
  APconnect(APssid, APpwd);
  MDNSstart(host, WiFi.localIP());

// --------------- Conitional orders file to String

  readConditionalOrders("/cOrders.txt");

// ---------------- Web services begin
  httpUpdater.setup(&server);

  server.begin();
  DBG_PRINTLN("HTTP server started");

  wsServer.begin();
  wsServer.onEvent(wsServerEvent);
  DBG_PRINTLN("webSocket server service started");

  wsClient.beginSocketIO("192.168.0.15", 3000); 
  wsClient.onEvent(wsClientEvent);
  DBG_PRINTLN("webSocket client service started");

  WifiToVars(); // Write the networks IP in their respective variables
  macstr(); // Write the module MAC address in the MAC_char variable

  DBG_PRINT("Module MAC address : ");
  DBG_PRINTLN(String(MAC_char));
  DBG_PRINT("AP IP address: ");
  DBG_PRINTLN(myIPString);

// ---------------- Server.on definition

// -------- Server.on / File system
  server.onNotFound([]() {  if (!handleFileRead(server.uri()))server.send(200, "text/plain", "FileNotFound");});

  server.on("/list", HTTP_GET, handleFileList);

  server.on("/remove", HTTP_GET,[]() {   // deprecated by handleFileDelete 
    SPIFFS.remove("/cOrders.txt");
    server.send(200, "text/json", "cOrders.txt removed");
  });

  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete); // todo: Afficher confirmation de la suppression 
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

// -------- Server.on / Standalone mode
  server.on("/aplist", HTTP_GET, []() {  server.send(200, "text/json", wifiscan());});
  server.on("/apstate", HTTP_GET, []() {  server.send(200, "text/json", wifistate());});
  server.on("/wifiparam", HTTP_GET, []() {  server.send(200, "text/json", wifiparam());});

  server.on("/current", HTTP_GET, []() { //deprecated
  currentRead();
  server.send(200, "text/json", currentjson());
  });

  server.on("/dht", HTTP_GET, []() { //deprecated
  dhtRead();
  server.send(200, "text/json", dhtjson());
  });

  server.on("/hash", HTTP_POST, []() { // to do : reprendre ce modele pour toutes les api + json library
    if (server.args() != 0) {
      if (server.hasArg("plain")) { // add a bug protection if values are not between 0 and 7
            server.send(200, "text/json", sha1(server.arg("plain"))); // What The Fuck ?? "plain" are you kidding ?
      }
    }else { server.send(200, "text/json", "no content");}
  });

  server.on("/encrypt", HTTP_GET, []() {
    String rdmKey=randomKey();
    String m=iotDBjson();
    String h=m+sha1(m);
    String a=encrypt(h,rdmKey);
    String b=encrypt(rdmKey,privateKey);
    String c=a+b;
    server.send(200, "text/json", c);
  });

  server.on("/decrypt", HTTP_POST, []() { 
    if (server.args() != 0) {
      if (server.hasArg("plain")) {
            String postMessage = server.arg("plain");
            String strKey=postMessage.substring(postMessage.length()-64);
            postMessage.remove(postMessage.length()-64);

            String a=decrypt(strKey,privateKey);
            String b=decrypt(postMessage,a);
            String h = b.substring(b.length()-40);
            b.remove(b.length()-40);

            String result="{\"Message crypté\":\""+postMessage+"\",\"Clé cryptée\":\""+strKey+"\",\"Clé décryptée\":\""+a+"\",\"Message décrypté\":\""+b+"\",\"Hash\":\""+h+"\"}";

            server.send(200, "text/json",result);

      }
    }else { server.send(200, "text/json", "je ne vois pas de contenu :)");}
  });

  server.on("/tonetwork", []() { // Chainage des esp : faire une notice du stair + chainer par bluetooth
    String rmessage ="?";
    int stair;
    int i=0;

    if (server.hasArg("stair")) {
      stair=server.arg("stair").toInt();
      stair++;
      }else { stair=1;i=1;}
      
    rmessage += "stair="+String(stair)+"&";

    // on va récupérer le message à déliver dans l'argument message
    // on créé un json [{dataesp1:1},{dataesp2},{dataesp3}]
    // que ce soit le serveur ou la freebox on met un indicateur pour indiquer qu'on passe en mode chainage

    for (i;i<server.args();i++){
      rmessage+= server.argName(i) +"="+ server.arg(i)+"&";
    }

    rmessage=rmessage.substring(0, rmessage.length() - 1);

   // relayDataClient(rmessage);


  server.send(200, "text/json", "{\"data\":\"ok\"}");
  }); 

// -------- Server.on / Home server connection
  server.on("/handshake", HTTP_GET, []() {
    // relier handshake à executeOrder
    boolean change=false;
    if (server.args() != 0) {
      if (server.hasArg("iotname")) {
        if (server.arg("iotname") != APssid) {
          APssid = server.arg("iotname");
          change = true;
        }
      } 
      //      if (server.hasArg("pwd" == true and server.arg("pwd")!=APpwd)){APpwd=server.arg("pwd");

      if (server.hasArg("homeip")) {
        if (server.arg("homeip")!=HomeIP) {
        HomeIP = server.arg("homeip");
        change = true;
        }
      }
      if (server.hasArg("homeport")) {
        if (server.arg("homeport")!=String(homeport)) {
        homeport = (server.arg("homeport")).toInt();
        change = true;
        }
      }
      server.send(200, "text/json", iotDBjson());
      if(change){
      writeHomeData(HomeFile);
      WriteWifiData(InfoFile);
      APconnect(APssid, APpwd);
      }
    }
  });

  server.on("/rgb", HTTP_GET, []() { //deprecated
    if (server.args() != 0) {
      if (server.hasArg("r")) { // add a bug protection if values are not between 0 and 7
          if (server.hasArg("g")) { // add a bug protection if values are not between 0 and 7
              if (server.hasArg("b")) { // add a bug protection if values are not between 0 and 7
                newrc=server.arg("r").toInt();
                newgc=server.arg("g").toInt();
                newbc=server.arg("b").toInt();
                tickPattern=true;
                server.send(200, "text/json", "{\"led\":\"ok\"}");
      }}}
    }
  });

  server.on("/happy", HTTP_POST, []() { // to do : reprendre ce modele pour toutes les api + json library
    if (server.args() != 0) {
      if (server.hasArg("plain")) { // add a bug protection if values are not between 0 and 7
            server.send(200, "text/json", server.arg("plain")); // What The Fuck ?? "plain" are you kidding ?
      }
    }else { server.send(200, "text/json", "no content");}
  });





  server.on("/relay", []() { // Ready for multiple relays. deprecated : we must use "executeOrder"
    if (server.args() != 0) {
      if (server.hasArg("r1")) {
        if (server.arg("r1") == "1") {
          writeRelay(HIGH);
        }
        if (server.arg("r1") == "0") {
          writeRelay(LOW);
        }
      }
    }
    server.send(200, "text/json", iotDBjson()); //do we need to update dht and current before sending iotDBjson ?
  });

  server.on("/iotname", []() { //deprecated : same as handshake
    if (server.args() != 0 && server.arg("name") != "" ) {
      String newAPname = server.arg("name");
      DBG_PRINT("Changing access point name to ");
      DBG_PRINTLN(newAPname);
      if (APssid != newAPname) {
        APssid = newAPname;
        WriteWifiData(InfoFile);
        server.send(200, "text/json", "{\"iotname\":\"" + APssid + "\"}");
        WiFi.softAP(newAPname.c_str(), APpwd.c_str());
      }
      server.send(200, "text/json", iotDBjson());
    }
    else {
      server.send(200, "text/json", iotDBjson());
    }
  });

  server.on("/apconnect", []() { //deprecated : fusion with aplist or apstate
    String ssid = server.arg("ssid");
    String pwd = server.arg("pwd");
    if (ssid != "") {
      if (ssid != STssid || pwd != STpwd) { //futur : if (pwd.length() >8 || pwd == ""){do things}else{"bad password"}
        STssid = ssid;
        STpwd = pwd;
        WriteWifiData(InfoFile);
        server.send(200, "text/json", "{\"apstate\":\"Connecting\",\"apname\":\"" + STssid + "\"}");
        STconnect(STssid, STpwd);
        WifiToVars(); // update wifi IPs
      }
      else {
        if ( WifiActive == true) {
          server.send(200, "text/json", "{\"apstate\":\"" + wifistatestring[WiFi.status()] + "\",\"apname\":\"" + STssid + "\"}");
        }
        else {
          server.send(200, "text/json", "{\"apstate\":\"Not connected\",\"apname\":\"" + STssid + "\"}");
        }
      }
    }
  });

  server.on("/load", HTTP_GET, []() { // for consumption analysis
  server.send(200, "text/json", loadjson());
  });

  server.on("/crypto", HTTP_GET, []() { //deprecated: replaced by encrypt (double encryption)
  server.send(200, "text/json", cryptojson());
  });

// ------------- Timer initialisation
  tickUpdate = false;
  os_timer_setfn(&myTimer1, timerUpdate, NULL);
  os_timer_arm(&myTimer1, 24000, true); //kaka

  tickSensor = false;
  os_timer_setfn(&myTimer2, timerDHT, NULL);
  os_timer_arm(&myTimer2, 2000, true);

  tickCurrent = false; // needed only if current sample rate is different of sensor sample rate
  os_timer_setfn(&myTimer3, timerCurrent, NULL);
  os_timer_arm(&myTimer3, t3delay, true);

  tickPattern = false;
  os_timer_setfn(&myTimer5, timerPattern, NULL);

  tickWsClient = false;
  os_timer_setfn(&myTimer6, timerWsClient, NULL);

// ------------- DHT22 initialisation
  dht.begin();

// ------------- Lux Sensor initialisation
  light.begin();
  unsigned char ID;
  
  if (!light.getID(ID))
  {
    byte error = light.getError();
    DBG_PRINTLN("Error connecting to TSL2561");
  }
  gain = 1;
  unsigned char time = 2;
  light.setTiming(gain,time,ms);
  light.setPowerUp();

// -------------- First home connection
  energyRead();
  //currentRead();
  dhtRead();
  //sendDataClient(); // check if client connected before
}

//---------------------------------------------- Loop ------------------------------------------------
void loop(void) {
wsServer.loop();
server.handleClient(); // comment fusionner avec listenClient() ?
if (wsClientReconnect) {
  //wsClient.loop();
}

// ---------- On timer interrups 
  if (tickWsClient == true) { 
    if (timeout) {
      wsClientReconnect=0;
      timeout=0;
      os_timer_arm(&myTimer6, 30000, true);
      } else {
      wsClientReconnect=1;
      }
    tickWsClient = false;
  }

  if (tickUpdate == true) { 
    // si les valeurs ont changé depuis la dernière fois + gestion du dernier envoi de data (doit etre "ok")
      //DBG_PRINTLN("Tick Update Occurred : 30s");
      tickUpdate = false;
      needUpdate=true;

      if(!waitingClient){ // deprecated
        //sendDataClient();
      }
      clientTimeout++; // deprecated
  }

  if (tickSensor == true) { 
          dhtRead();
          lumRead();
          energyRead();
          DBG_PRINTLN("TickSensor Occurred (2s lapse)");
          tickSensor = false;
   }

  if (tickCurrent == true) { // besoin de relever le courant toutes les 0.5s ?? oui pour app "disjoncteur" sinon non
          //currentRead();
          //DBG_PRINTLN("Tick Current Occurred : 0.5s");
          tickCurrent = false;
   }

  if (tickPattern == true) { 
      fader(newrc, newgc, newbc, 20);
      tickPattern = false;
  }

// ---------- On waiting client
  if(waitingClient){
  //listenClient(); // can't be moved before the timer feature gérer le timeout, activer un mode server distant
  }

// ---------- On need update
  if(needUpdate){
  //sendDataClient(); // can't be moved before the timer feature
  String callBack;
  executeConditionalOrders(conditionalOrders,callBack);     

  if (WsServerIsConnected) {
    String tempR = iotDBjson();
    size_t lon = strlen(tempR.c_str());
    wsServer.broadcastTXT(tempR.c_str(), lon); // kaka broadcaster iotDBjson !
    DBG_PRINTLN("WsServer updated");
  }

  if (WsClientIsConnected) {
    wsClient.sendTXT("42[\"esp\"," + iotDBjson() + "]");
    DBG_PRINTLN("WsClient updated");
  }

  BT_SERIAL(iotDBjson());
  needUpdate =false;
  DBG_PRINTLN("need update : ok");
  os_timer_arm(&myTimer1, 25000, true);
  time_t now = time(nullptr);
  Serial.print("time : ");
  Serial.println(ctime(&now));
  }
  
// ---------- On incoming serial message
  if (Serial.available() > 0) {
    String sdata = Serial.readString();
    // We must verify that sdata is ok
    String callBack;
    //ExecuteOrder(sdata, callBack);
    parseOrders(sdata, callBack);
    Serial.println("conditionalOrders :");
    Serial.println(conditionalOrders);    
    Serial.println("directOrders :");
    Serial.println(directOrders);      

    executeDirectOrders(directOrders,callBack);
    // executeConditionalOrders(conditionalOrders,callBack);     
  
    BT_SERIAL("{\"data\":\""+callBack+"\"}");
    // WriteTxtData("/serial.txt",sdata); // uncomment only if history needed
  }

yield(); //understand and test if it is necessary?!
}

//-------------- Home connection handling as a server to client
  void listenClient() { // il serait mieux de le remplacer par un "server.on"? ne fonctionne pas sur numericable
    if (homeclient.available()){
    String line = homeclient.readStringUntil('\r');
    int a= line.indexOf('"data":');
    String response = line.substring(a+2,a+4);
      if (response=="ok"){
      homeClientConnected=true;
      HomeState="Connected";
      //needUpdate=false;
      
      DBG_PRINTLN("Data received : Home connected");
      waitingClient=false;
      
      }
    homeClientConnected=false;
    //HomeState="Not connected";
    clientTimeout=0;
    }
    if(clientTimeout>3){
    homeClientConnected=false;
    HomeState="Not connected";
    clientTimeout=0;
    waitingClient=false;
    
    
    DBG_PRINTLN("Client timeout : Home disconnected");
    }
  }

  void sendDataClient() { //Equivalent to "home connect"
    if (homeClientConnected==false &&  WifiActive == true){
          if (!homeclient.connect(HomeIP.c_str(), homeport)) {
          DBG_PRINTLN("connection failed to home ip as client");
          homeClientConnected =false;
          //HomeState="Not connected";
          
          return;
          }         
    homeClientConnected =true;
    DBG_PRINTLN(HomeIP);

    DBG_PRINT("Requesting URL: ");
    DBG_PRINTLN(homeurl);
          
    // This will send the request to the server
    homeclient.print(String("GET ") + homeurl + iotDBnewget() +  " HTTP/1.1\r\n" + "Host: " + HomeIP+":" + homeport + "\r\n" + "Connection: close\r\n\r\n");
    //homeclient.print(String("GET ") + "api/esps" +  " HTTP/1.1\r\n" + "Host: " + HomeIP + "\r\n" + "Connection: close\r\n\r\n");

    clientTimeout=0;
        waitingClient=false; //waitingClient=true; if TRUE then the "waitingClient loop" in the void loop takes too much time to operate 
    }
  }

  void relayDataClient(String rsms) {
          if (!homeclient.connect(HomeIP.c_str(), homeport)) {
          DBG_PRINTLN("connection failed to home ip as client");
          homeClientConnected =false;
          //HomeState="Not connected";
          
          return;
          }         
    DBG_PRINTLN(HomeIP);

    DBG_PRINT("Requesting URL: ");
    DBG_PRINTLN(homeurl);
          
    // This will send the request to the server
    homeclient.print(String("GET ") + homeurl + rsms +  " HTTP/1.1\r\n" + "Host: " + HomeIP+":" + homeport + "\r\n" + "Connection: close\r\n\r\n");
      // to send the response to the relayed esp we need to connect ti it as client
      // the last relay must transfor the rmessage into the "new GET method" with slashes
   
    DBG_PRINT("Data relayed : ");
    DBG_PRINTLN(String("GET ") + homeurl + rsms +  " HTTP/1.1\r\n" + "Host: " + HomeIP+":" + homeport + "\r\n" + "Connection: close\r\n\r\n");  
    
    
  }

  void postDataClient() {
 
          if (!homeclient.connect(HomeIP.c_str(), homeport)) {
          DBG_PRINTLN("POST failed to home ip as client");
          homeClientConnected =false;
          //HomeState="Not connected";
          
          return;
          }
    
    
    DBG_PRINTLN(HomeIP);

    DBG_PRINT("Requesting URL: ");
    DBG_PRINTLN(homeurl);

        
    //String cstr=cryptojson(); Content-Type: text/plain
    String cstr = iotDBjson();
    homeclient.print(String("POST ") + homeurl + " HTTP/1.1\r\n" + "Host: " + HomeIP+":" + homeport + "\r\n" + "Content-Type: application/json\r\n" + "Connection: close\r\n" + "Content-Length: " + cstr.length() + "\r\n\r\n" + cstr + "\r\n");
    DBG_PRINTLN("POST sent :");
    DBG_PRINTLN(String("POST ") + homeurl + " HTTP/1.1\r\n" + "Host: " + HomeIP+":" + homeport + "\r\n" + "Content-Type: application/json\r\n" + "Connection: close\r\n" + "Content-Length: " + cstr.length() + "\r\n\r\n" + cstr + "\r\n");
    //
    
  }


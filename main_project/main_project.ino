#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <MD_Parola.h>    //for led matrix led
#include <MD_MAX72xx.h>   //for led matrix led
#include <LedControl.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15
#define Relay_Led_1 D0
#define Relay_Led_2 D1
#define Relay_Led_3 D2
#define Relay_Led_4 D3
#define Alarm 10

bool All_Relay_state = 1 ,Relay1_state = 1 , Relay2_state = 1, Relay3_state = 1, Relay4_state = 1 , Display_state = 0;//if Display_state=0 show time else show word 

unsigned long  timel, curTime[4] ,  lastT;
WiFiClient espClient;
PubSubClient client(espClient);

MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org" , + 3*60*60); //3*60*60 cairo zone

ESP8266WebServer    server(80);

String msgAr = "" ;
String timerSTR;

unsigned long timer1 , timer2 , timer3 , timer4 , mm ;
bool timer1_state = 0, timer2_state = 0, timer3_state = 0, timer4_state = 0 ;

const char* mqtt_Server = "broker.mqtt-dashboard.com";

struct settings {
  char ssid[30];
  char password[30];
} user_wifi = {};

void setup() {
  Serial.begin(115200);

  pinSetup();                               //setup pinModes and its defult state

  EEPROM.begin(sizeof(struct settings) );   //enable reading from flash memory that integrated in Nodemcu
  
  EEPROM.get( 0, user_wifi );               //searching for saved WiFi settings in flash and store it in user_wifi struct

  Display.begin();                          //turn on display
  Display.setIntensity(4);                  //set light intensity for display
  Display.displayClear();                   //clear anything from the previous sessions
   
  WiFi.mode(WIFI_STA);                      // STA to convert mode to connect to WIFI
  WiFi.begin(user_wifi.ssid, user_wifi.password);   // start using stored wifi settings 
  
  unsigned long Timeout = millis();

  Serial.println("Connecting to WiFi.");
  Display.displayScroll("Connecting to WiFi......................", PA_LEFT, PA_SCROLL_LEFT, 20);
  
  while (WiFi.status() != WL_CONNECTED)     //try to connect 
  {  
    Serial.print(".");
    if (Display.displayAnimate()) {
          Display.displayReset();
      }
    
    if (millis() - Timeout > 10000)                       // wifi may be not found or wrong password
    {
      Display.displayClear();
      Display.displayScroll("Connection Failed  ", PA_LEFT, PA_SCROLL_LEFT, 20);
      long long waitForAnimation = millis();
      for (int i = millis(); i - waitForAnimation < 4500 ; i = millis() ){
        Serial.print(i - waitForAnimation);
        if (Display.displayAnimate()) 
        {
          Display.displayReset();
        }
      }
      Serial.println("Connection Failed ");
      WiFi.mode(WIFI_AP);                   // start making access point to enter ssid and password
      WiFi.softAP("SmartPlug", "");
      Serial.print(WiFi.softAPIP());
      Display.displayClear();
      Display.displayScroll("Please Connect To SmartPlug WiFi", PA_LEFT, PA_SCROLL_LEFT, 40);

      waitForAnimation = millis();
      for (int i = 0; WiFi.softAPgetStationNum() < 1 ; i++ ){

        Serial.print(WiFi.softAPgetStationNum());

        if (Display.displayAnimate()) 
        {
          Display.displayReset();
        }
      }
      break;
    }
  }

  Serial.print("http server started");    
  server.on("/",  handleRoot);
  server.begin();

  if (WiFi.status() == WL_CONNECTED)        // فى حالة وجود النت بس هيبدأ يستدعى السيرفرات 
  {
    timeClient.begin();
    client.setServer(mqtt_Server, 1883);
    client.setCallback(callback);
    timel= millis();
    //timer= millis();
  }

  Display.displayClear();

  Display.displayScroll("Please Write ** 192.168.4.1 ** in Your Browser ", PA_LEFT, PA_SCROLL_LEFT, 30);

}

void loop() {

  if(WiFi.status() != WL_CONNECTED)
  {
    
    if (Display.displayAnimate()) 
      {
          Display.displayReset();
      }
    
    server.handleClient();
  }
  else
  {

    if (!client.connected()) 
    {
      reconnect();
    }
    client.loop();

    set_the_timer();

    unsigned long now= millis();

    if(now-timel>2000)
    {
      timel= now;
      
      client.publish("all relay state",String(All_Relay_state).c_str());
      client.publish("relay1 State",String(Relay1_state).c_str());
      client.publish("relay2 State",String(Relay2_state).c_str());
      client.publish("relay3 State",String(Relay3_state).c_str());
      client.publish("relay4 State",String(Relay4_state).c_str());
    }


    if (Display_state == 0 )
    {
      Display.setTextAlignment(PA_CENTER);
      Display.print(liveClock());
      Serial.println(liveClock());
    }
    else {
      if (Display.displayAnimate()) 
      {
        Display.displayAnimate();
          Display.displayReset();
      }
    }
  }
}

void pinSetup(){
  pinMode(Relay_Led_1, OUTPUT);
  pinMode(Relay_Led_2, OUTPUT);
  pinMode(Relay_Led_3, OUTPUT);
  pinMode(Relay_Led_4, OUTPUT);
  pinMode(Alarm , OUTPUT);
  //During Starting all Relays should TURN On
  digitalWrite(Relay_Led_1, HIGH);
  digitalWrite(Relay_Led_2, HIGH);
  digitalWrite(Relay_Led_3, HIGH);
  digitalWrite(Relay_Led_4, HIGH);
  digitalWrite(Alarm, LOW);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  if (strstr(topic, "sub1"))
  {
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '0') {
      digitalWrite(Relay_Led_1, LOW);   // Turn the LED on (Note that LOW is the voltage level
      Relay1_state=0;
    } else {
      digitalWrite(Relay_Led_1, HIGH);  // Turn the LED off by making the voltage HIGH
      Relay1_state=1;
    }    
  }

  else if ( strstr(topic, "sub2"))
  {
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '0') {
      digitalWrite(Relay_Led_2, LOW);   // Turn the LED on (Note that LOW is the voltage level
       Relay2_state=0;
    } else {
      digitalWrite(Relay_Led_2, HIGH);  // Turn the LED off by making the voltage HIGH
       Relay2_state=1;
    }
  }
  else if ( strstr(topic, "sub3"))
  {
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '0') {
      digitalWrite(Relay_Led_3, LOW);   // Turn the LED on (Note that LOW is the voltage level
       Relay3_state=0;
    } else {
      digitalWrite(Relay_Led_3, HIGH);  // Turn the LED off by making the voltage HIGH
       Relay3_state=1;
    }
  }
  else if ( strstr(topic, "sub4"))
  {
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '0') {
      digitalWrite(Relay_Led_4, LOW);   // Turn the LED on (Note that LOW is the voltage level
       Relay4_state=0;
    } else {
      digitalWrite(Relay_Led_4, HIGH);  // Turn the LED off by making the voltage HIGH
       Relay4_state=1;
    }
  }
  else if( strstr(topic, "sub5")){    
     msgAr = "" ;
    for (int i = 0; i < length; i++) {
      msgAr+=(char)payload[i];
      Serial.print((char)payload[i]);
    }
    if ((char)payload[0] == '0')
    {
      Display_state = 0 ;
      msgAr.clear() ;
    }
    else{
      Display.displayClear();
      Display.displayScroll(msgAr.c_str(), PA_LEFT, PA_SCROLL_LEFT, 30);
      Display_state = 1 ;
      //Serial.println(msgAr.c_str());
      Serial.println(msgAr);
    }

  }
  else if ( strstr(topic, "timer1"))
  {
    timerSTR = "" ;
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      timerSTR+=(char)payload[i];
    }
    mm = timerSTR.toInt();
    Serial.println();
    Serial.println(mm);
    if ((char)payload[0] == '0') {
      timer1_state = 0 ;
    } else {
      timer1 = millis() + mm;
      Serial.println(timer1);
      timer1_state = 1 ;
    }
  }
  else if ( strstr(topic, "timer2"))
  {
    timerSTR = "" ;
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      timerSTR+=(char)payload[i];
    }
    mm = timerSTR.toInt();
    Serial.println();
    if ((char)payload[0] == '0') {
      timer2_state = 0 ;
    } else {
      timer2 = millis() + mm;
      timer2_state = 1 ;
    }
  }
  else if ( strstr(topic, "timer3"))
  {
    timerSTR = "" ;
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      timerSTR+=(char)payload[i];
    }
    mm = timerSTR.toInt();
    Serial.println();
    if ((char)payload[0] == '0') {
      timer3_state = 0 ;
    } else {
      timer3 = millis() + mm;
      timer3_state = 1 ;
    }
  }
  else if ( strstr(topic, "timer4"))
  {
    timerSTR = "" ;
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      timerSTR+=(char)payload[i];
    }
    mm = timerSTR.toInt();
    Serial.println();
    if ((char)payload[0] == '0') {
      timer4_state = 0 ;
    } else {
      timer4 = millis() + mm;
      timer4_state = 1 ;
    }
  }
  else
  {
    Serial.println("unsubscribed topic");
  }

}

void reconnect() {
 while (!client.connected()) {
   String clientId="ESP8266Client-";
   clientId +=String(random(0xffff),HEX);
 if (client.connect(clientId.c_str())) 
 {
 Serial.println("MQTT connected");

      client.publish("all relay state","hello World");
      client.publish("relay1 State","hello World");
      client.publish("relay2 State","hello World");
      client.publish("relay3 State","hello World");
      client.publish("relay4 State","hello World");

      client.subscribe("sub1");
      client.subscribe("sub2");
      client.subscribe("sub3");
      client.subscribe("sub4");
      client.subscribe("sub5");
      client.subscribe("timer1");
      client.subscribe("timer2");
      client.subscribe("timer3");
      client.subscribe("timer4");
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handleRoot() {

  if (server.method() == HTTP_POST) {

    strncpy(user_wifi.ssid,     server.arg("ssid").c_str(),     sizeof(user_wifi.ssid) );
    strncpy(user_wifi.password, server.arg("password").c_str(), sizeof(user_wifi.password) );
    user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';
    EEPROM.put(0, user_wifi);
    EEPROM.commit();
    server.send (200,   "text/html",  
  "<!doctype html>\n"
    "<html lang='en'>\n"
    "<head>\n"
    "<meta charset='utf-8'>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
    "<title>Wifi Setup</title>\n"
    "<style>*,::after,::before{box-sizing:border-box;}\n"
    "body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.\n"
    "form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}\n"
    "button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;\n"
    "font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.\n"
    "form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,\n"
    "p{text-align: center}\n"
    "</style> \n"
    "</head> \n"
    "<body>\n"
    "<main class='form-signin'>\n"
    " <h1>Wifi Setup</h1> \n"
    "<br/> <p>Your settings have been saved successfully!<br />\n"
    "Wait For restart the device.</p></main></body></html>" 
  );
    
    delay(1000);
    ESP.restart();

  } else {

      server.send(200,   "text/html", 
      "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>SESB</title>\n"
        "    <style>\n"
        "        *{padding:0;margin:0;box-sizing:border-box;font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,'Open Sans','Helvetica Neue',sans-serif;scroll-behavior:smooth;}\n"
        "        main{width:100%;min-height:300px;}\n"
        "        header{width:100%;height:800px;background-color:rgb(255, 110, 110);}\n"
        "        .webName{color:white;font-size:xxx-large;margin-left:15px;line-height:50px;float:left;}\n"
        "        header ul {list-style:none;padding:0;margin:0;float:right;color:white;margin:auto;}\n"
        "        header ul li { width:150px;height:50px;text-align: center;line-height:50px;float:left;position:relative;}\n"
        "        header ul li a{text-decoration: none;color: white;}\n"
        "        header ul li:hover {background-color:#fb8080d5;}\n"
        "        .dropdown::after {content:'\\25BC';margin-left:2px;font-size:small;}\n"
        "        header .submenu {display:none;position:absolute;top:100%;left:0;width:150px;background-color:#f59c9c;}\n"
        "        .form-control{display:block;width:100%;height:calc(1.5em+0.75rem+2px);border:1px solid #ced4da;}\n"
        "        button{ border:1px solid transparent #007bff;color:#fff;background-color:rgb(255, 110, 110);padding: .5rem 1rem;font-size:1.25rem;line-height:1,5;border-radius:0.3rem;}\n"
        "        .form-signin{width:50%;margin:auto;padding: 15px;}\n"
        "        .form-signin h1{text-align:center;}\n"
        "        header .dropdown:hover .submenu {display:block;}\n"
        "        hr{height: 1px;background-color: #e6e6e6;border: none;}\n"
        "        .intro{color:white;font-size:75px;text-align:center;margin-top:180px;}\n"
        "        .aboutUs{min-height:300px;width:100%;padding-left:180px;padding-top:60px;color:white;background-color:black;position:relative;}\n"
        "        .aboutUs h1{font-size:xx-large;}\n"
        "        .aboutUs p{width: 50%;}\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <main>\n"
        "        <header>\n"
        "            <p class=\"webName\"><strong>SESB</strong></p>\n"
        "            <ul>\n"
        "                <li><a href=\"https://drive.usercontent.google.com/download?id=1VC9dr8K2ZhQW9_8uzAsGgOKcQw6qW4-1&export=download&authuser=0&confirm=t&uuid=da6646a8-668d-418f-a3ec-56a5e4ed18e0&at=APZUnTUosg-oGU5I1GftIf_4Iok4:1693919607625\" target=\"_blank\">Download App</a></li>\n"
        "                <li><a href=\"#about\">About</a></li>\n"
        "                <li class=\"dropdown\"><a href=\"#\">Feature</a>\n"
        "                    <ul class=\"submenu\">\n"
        "                        <li><a href=\"#\">Feature1</a></li>\n"
        "                        <li><a href=\"#\">Feature2</a></li>\n"
        "                        <li><a href=\"#\">Feature3</a></li>\n"
        "                    </ul>\n"
        "                </li>\n"
        "                <li><a href=\"https://192.168.1.1/\" target=\"_blank\">WiFi-Settings</a></li>\n"
        "            </ul>\n"
        "            <div class=\"clear\"></div>\n"
        "            <hr>\n"
        "            <p class=\"intro\">Welcome To <br>\n"
        "                Smart Electric Switch Board <br> Setup</p>\n"
        "        </header>\n"
        "        <div class=\"clear\"></div>\n"
        "        <div class=\"form-signin\">\n"
        "            <form action=\"/\" method=\"post\">\n"
        "                <h1>Connect Smart Plug to your WiFi</h1>\n"
        "                <br>\n"
        "                <div class=\"form-floating\">\n"
        "                    <label>SSID</label><input type=\"text\" class=\"form-control\" name=\"ssid\" >\n"
        "                </div>\n"
        "                <div class=\"form-floating\">\n"
        "                    <br>\n"
        "                    <label>Password</label><input type=\"text\" class=\"form-control\" name=\"password\">\n"
        "                </div>\n"
        "                <br><br>\n"
        "                <button type=\"submit\">Connect</button>\n"
        "            </form>\n"
        "        </div>\n"
        "        <section class=\"aboutUs\" id=\"about\">\n"
        "            <h1>ABOUT US</h1>\n"
        "            <br>\n"
        "            <p>This project is the graduation project of Team 5 of the Robotech Summer Training 2023 at <br> the Faculty of Computing and Information, Ain Shams University\n"
        "            <br><br> Team members : Shady atef  , Ahmed Ibrahim , Ali Ashraf , Abdullah mohamed , Abdelrahman Mohamed , Ali Amgad , Omar Amgad , Ibrahim Mohammed</p>\n"
        "        </section>\n"
        "    </main>\n"
        "</body>\n"
        "</html>" );
  }
}

String liveClock(){
  
  timeClient.update();
  String time = "" ;
  int hours = timeClient.getHours();
  if (hours > 12){
    hours-=12;
  }
  int minutes = timeClient.getMinutes();
  if(hours >= 10){
      if(minutes >= 10){
        time = String(hours) + ":" + String(minutes);
      }
      else
        time = String(hours) + ":" + "0" +String(minutes);
  }
  else 
      {
        if(minutes >= 10){
          time = "0" +String(hours) + ":" + String(minutes);
      }
        else
          time = "0" +String(hours) + ":" + "0" +String(minutes);
      }
  return time;
}

void set_the_timer(){
  if( millis() >= timer1 && timer1_state == 1 ){
    digitalWrite(Relay_Led_1, LOW);
    timer1_state = 0;
    Relay1_state = 0;
  }
  if( millis() >= timer2 && timer2_state == 1 ){
    digitalWrite(Relay_Led_2, LOW);
    timer2_state = 0;
    Relay2_state = 0;
  }
  if( millis() >= timer3 && timer3_state == 1 ){
    digitalWrite(Relay_Led_3, LOW);
    timer3_state = 0;
    Relay3_state = 0;
  }
  if( millis() >= timer4 && timer4_state == 1 ){
    digitalWrite(Relay_Led_4, LOW);
    timer4_state = 0;
    Relay4_state = 0;
  }
}
/*String displayMsg (String newMsg) {
  String Msg = "";
  Msg=newMsg.c_str();
  return Msg;

}*/
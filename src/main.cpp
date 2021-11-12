#include <Arduino.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Ticker.h>
#include <DHtesp.h>
#include <ArduinoJson.h>
#include <U8x8lib.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <Update.h>

#define EEPROM_SIZE 1024
#define WIFI_NOME "Metropole" //rede wifi específica
#define WIFI_SENHA "908070Radio"
#define BROKER_MQTT "10.71.0.2"
#define DEVICE_TYPE "ESP32"
#define TOKEN "ib+r)WKRvHCGjmjGQ0"
#define ORG "n5hyok"
#define PUBLISH_INTERVAL 1000*60*1//intervalo de 1 min para publicar temperatura

uint64_t chipid=ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
uint16_t chip=(uint16_t)(chipid >> 32);
char DEVICE_ID[23];
char an=snprintf(DEVICE_ID, 23, "biit%04X%08X", chip, (uint32_t)chipid); // PEGA IP
String mac;
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000); //Hr do Br
DHTesp dhtSensor;
DynamicJsonDocument doc (1024); //tamanho do doc do json
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(15, 4, 16);

const char* host = "esp3";
char topic1[]= "status";          // topico MQTT
char topic2[]= "mqttemperatura";  // topico MQTT
char topic3[] = "memoria";
char topic4[] = "tempideal";
bool publishNewState = false; 
TaskHandle_t retornoTemp;
IPAddress ip=WiFi.localIP();
int tempAtual=0;
int tempAntiga=0;
bool tasksAtivo = true;
struct tm data; //armazena data 
char data_formatada[64];
char hora_formatada[64];
bool tensaoPin=false;
bool novaTemp=false;
int tIdeal;
int rede;
String comando;
unsigned long previousMillis=0;
unsigned long previousMillis1=0;
const long intervalo=1000*60*1; //1 min
int movimento=0;
//=============
const int dhtPin1=32;
const int pirPin1=33; 
const int con=25;  //vermelha
const int eva=26;
const int sensorTensao=23;
unsigned long tempo=1000*60*5; // verifica movimento a cada 5 min
unsigned long ultimoGatilho = millis()+tempo;
std::string msg2;
std::string msg3;
int vez=0;
////////////////////////////////////////////////////////////////
/* Style */
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Login page */
String loginIndex =
"<form name=loginForm>"
"<h1>ESP32 Login</h1>"
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value=Login></form>"
"<script>"
"function check(form) {"
"if(form.userid.value=='admin' && form.pwd.value=='zaq1xsw2')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>" + style;

/* Server Index Page */
String serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='up' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>" + style;
////////////////////////////////////////////////////////////////
void callback(char* topicc, byte* payload, unsigned int length){
  std::string msg;
  if (topic3){
    for (int i=0;i<length;i++) {
      msg += (char)payload[i];
    }

    msg2= msg.substr(0,1); //retorna o t
    msg3= msg.substr(1,2);  //retorna a temperatura ideal
    if(msg2=="t"){
      Serial.print("---------------------");
      Serial.print(msg2.c_str());
      Serial.print(msg3.c_str());
      Serial.println("---------------------");
      tIdeal = atoi(msg3.c_str());
      Serial.println(tIdeal);
    }
  }
}
void UpdateRemoto() { 
  //upload via web
  if (!MDNS.begin(host)) {
		Serial.println("Error setting up MDNS responder!");
		while (1) {
			delay(1000);
		}
	}
	server.on("/", HTTP_GET, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/html", loginIndex);
	});
	server.on("/serverIndex", HTTP_GET, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/html", serverIndex);
	});
	server.on("/update", HTTP_POST, []() {
		server.sendHeader("Connection", "close");
		server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
		ESP.restart();
	}, []() {
		HTTPUpload& upload = server.upload();
		if (upload.status == UPLOAD_FILE_START) {
			Serial.printf("Update: %s\n", upload.filename.c_str());
			if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {//start with max available size
				Update.printError(Serial);
			}
		} else if (upload.status == UPLOAD_FILE_WRITE) {
			if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
				Update.printError(Serial);
			}
		} else if (upload.status == UPLOAD_FILE_END) {
			if (Update.end(true)) {
				Serial.printf("Update Success: %u\nRebooting++...\n", upload.totalSize);
			} else {
				Update.printError(Serial);
			}
		}
	});
}
void conectaMQTT(){
  //Estabelece conexao c MQTT/WIFI
   if(!client.connected()){
    Serial.println("conectando...");
    if (client.connect("ESP322")){
      Serial.println("CONECTADO! :)");
      client.publish ("teste", "hello word");
      client.subscribe (topic1);   //se inscreve no topico a ser usado
      client.subscribe (topic2);
      client.subscribe (topic3);
      client.subscribe (topic4);
    } else {
      Serial.println("Falha na conexao");
      Serial.print(client.state());
      Serial.print("nova tentativa em 5s...");
      delay (5000);
    }
  }
}
void reconectaMQTT(){
  //reconecta MQTT caso desconecte
  if (!client.connected()){
    conectaMQTT();
  }
  client.loop();
}
void datahora(){
  time_t tt=time(NULL);
  data = *gmtime(&tt);
  strftime(data_formatada, 64, "%w - %d/%m/%y %H:%M:%S", &data);//hora completa
  strftime(hora_formatada, 64, "%H:%M", &data); //hora para o visor
  //Armazena na variável hora, o horário atual.
  int anoEsp = data.tm_year;
  if (anoEsp < 2020) {
    ntp.forceUpdate();
  }	
}
void dadosEEPROM(){
  //DEFINE OS DADOS EMERGENCIAIS DA EPROOM 
  if(EEPROM.read(0) != tIdeal) {
    EEPROM.write(0, tIdeal);  //escreve tempIdeal no dress=0 vindo do mqtt
    Serial.println("ESCREVEU NA EEPROM");
  } 
}
void iniciaWifi(){
  int cont=0;
  WiFi.begin(WIFI_NOME, WIFI_SENHA); 
  while (WiFi.status()!= WL_CONNECTED){//incia wifi ate q funcione
    Serial.print (".");
    delay(1000);
    cont++;
    if(cont==15){  //se n funcionar sai do loop e fica sem rede
      Serial.println("break");
      rede=0; //status da rede
      Serial.println(rede);
      break;
    }
  }  
  if(WiFi.status() == WL_CONNECTED){
    rede=1;
    if (!client.connected()){  //aqui é while, mudei pra teste
      conectaMQTT();
    }
  }
}
void redee(){
  if(rede==1){  //está conectado
    //protocolo online via MQTT
    Serial.println("rede 1");
  } else if(rede==0) { //n esta conectado a rede
    //protocolo offline
    Serial.println("rede 0");
    EEPROM.begin(EEPROM_SIZE);
    
    tIdeal=EEPROM.read(0);

  }
}
void tentaReconexao(){ //roda assincrona no processador 0
  unsigned long currentMillis = millis();
  if (currentMillis-previousMillis<= intervalo) { //a cada 5min tenta reconectar
    Serial.print("*************************");
    Serial.print("TENTA RECONEXAO");
    Serial.println("***********************");
    previousMillis=currentMillis;
    iniciaWifi();
    ntp.forceUpdate();
  }
}
void sensorTemp(void *pvParameters){
  Serial.println ("sensorTemp inicio do LOOP");
  while (1) {//busca temp enquanto estiver ligado
    tempAtual = dhtSensor.getTemperature();
    tasksAtivo=false;
    vTaskSuspend (NULL);
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}
void pegaTemp() {
      if (retornoTemp != NULL) {
    xTaskResumeFromISR (retornoTemp);
  }
}
void publish(){
  if(tempAtual>50){
    tempAtual=tempAntiga;
  }else if (tempAntiga != tempAtual){
    // nova temperatura
    tempAntiga=tempAtual;
  }  
}
void arLiga(void *pvParameters){
  while(1){
    String hora;
    hora= data.tm_hour;
    //liga ar
    digitalWrite(eva, 0);
    Serial.println(tempAtual);
    Serial.println(tIdeal);
    if(tempAtual>=(tIdeal+2)){ //quente
      if(digitalRead(eva)==0){
        digitalWrite(con, 0);
        Serial.println("condensadora ligada");
      } else {
        digitalWrite(con, 0);
        digitalWrite(eva, 0);
        Serial.println("condensadora ligada");
      }	
      StaticJsonDocument<256> doc;
      doc["statusAR"]= "ligado"; 
      doc["condensadora"]=!digitalRead(con);
      doc["evaporadora"]=!digitalRead(eva);
      char buffer[256];
      serializeJson(doc, buffer);
      client.publish(topic2, buffer);
      Serial.println(buffer);	
      	
    } else if(tempAtual<=(tIdeal-2)){ //frio
      digitalWrite(con, 1);
      digitalWrite(eva, 0);
      Serial.println("condensadora desligada");	
      StaticJsonDocument<256> doc;
      doc["statusAR"]="desligado"; 
      doc["condensadora"]=!digitalRead(con);
      doc["evaporadora"]=!digitalRead(eva);
      char buffer[256];
      serializeJson(doc, buffer);
      client.publish(topic2, buffer);

    } else if(tempAtual==tIdeal){
      Serial.println("temp ideal");	
      StaticJsonDocument<256> doc;
      doc["statusAR"]="temp ideal"; 
      doc["condensadora"]=!digitalRead(con);
      doc["evaporadora"]=!digitalRead(eva);
      char buffer[256];
      serializeJson(doc, buffer);
      client.publish(topic2, buffer);
    }
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}
void PinConfig () {
  // config das portas i/o
  pinMode(dhtPin1, INPUT);
	pinMode(pirPin1, INPUT_PULLUP);
  pinMode(sensorTensao, INPUT_PULLUP);
  pinMode(eva, OUTPUT);
  pinMode(con, OUTPUT);
}
void payloadMQTT(){ 
  datahora();
  u8x8.clear();
  time_t tt=time(NULL);
  StaticJsonDocument<256> doc;
  doc["local"] = "AR-Transmissor";
  doc["ip"] = ip.toString();
  doc["mac"] = mac;
  doc["hora"]=tt;
  doc["temperatura"]=tempAtual;
  doc["movimento"]=movimento; 
  doc["evaporadora"]=!(digitalRead(eva));
  doc["condensadora"]=!(digitalRead(con));
  char buffer1[256];
  serializeJson(doc, buffer1);
  client.publish(topic1, buffer1);
  Serial.println(buffer1);
  movimento=0;
}
void IRAM_ATTR mudaStatusPir(){
  ultimoGatilho = millis()+tempo;
  movimento=1;
}
Ticker tickerpin(publish, PUBLISH_INTERVAL);
Ticker tempTicker(pegaTemp, 5000);
void setup(){
  Serial.begin (115200);
  iniciaWifi();
  client.setServer (BROKER_MQTT, 1883);//define mqtt
  client.setCallback(callback); 
  server.begin();
  ntp.begin ();
  ntp.forceUpdate();
  if (!ntp.forceUpdate ()){
      Serial.println ("Erro ao carregar a hora");
      delay (1000);
  } else {
    timeval tv; //define a estrutura da hora 
    tv.tv_sec = ntp.getEpochTime(); //segundos
    settimeofday (&tv, NULL);
  }
  redee();  //define as variaveis
  PinConfig();
  dhtSensor.setup(dhtPin1, DHTesp::DHT11);
  xTaskCreatePinnedToCore (sensorTemp, "sensorTemp", 4000, NULL, 1, &retornoTemp, 0);
  xTaskCreatePinnedToCore (arLiga, "arliga", 10000, NULL, 1, NULL, 0);
  attachInterrupt (digitalPinToInterrupt(pirPin1), mudaStatusPir, RISING);
  tickerpin.start();
  tempTicker.start();
  datahora();
  ip=WiFi.localIP(); //pega ip
  mac=DEVICE_ID;     //pega mac
  UpdateRemoto(); //inicializa update via web
  server.begin(); //servidor web
  dadosEEPROM(); //grava dados na EEPROM
}
void loop(){
  datahora();
  server.handleClient();
  reconectaMQTT();
  if(tempAtual>50){  //prevenção de erros na leitura do sensor
    publishNewState=false;
  }else if(WL_DISCONNECTED || WL_CONNECTION_LOST){
    rede=0;
  }else if(rede==0){
    tentaReconexao();
  }
  unsigned long currentMillis1 = millis();
  if (currentMillis1-previousMillis1 >= intervalo){
    payloadMQTT();
    previousMillis1=currentMillis1;
  } else if(ultimoGatilho>millis() && movimento==1){
    //tem movimento na sala
    payloadMQTT();
  }
  if(vez==0){
    StaticJsonDocument<256> doc;
    doc["local"] = "Transmissor";
    doc["mac"] =  "biitEC15178E0D84";
    doc["etapa"] =  "ligado";
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish("tempideal", buffer);
    Serial.println("mandou");
    vez=1;
  }
  tempTicker.update();
  tickerpin.update();
  delay(500);
}
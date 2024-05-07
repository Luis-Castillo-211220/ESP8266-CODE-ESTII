#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
// #include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "DHTesp.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <WiFiClientSecure.h>
// #include <CTBot.h>

//Red wifi y password
const char* ssid = "";
const char* password = "";


// Cliente HTTP y NTP - Zona horaria
WiFiClientSecure secured_client;
WiFiClient client;
HTTPClient http;
WiFiUDP ntpUDP;
const long timeZone = -6;
const int daylightOffset_sec = 3600;
NTPClient timeClient(ntpUDP, "pool.ntp.org", timeZone, 60000);

// Sensor DHT
int pinDHT = 5;
int dispositivoID = 2; //ASIGNACIÓN DE ID GENERADO PARA ESTE DISPOSITIVO REEMPLAZAR SEGUN EL ID DADO POR LA APP MOVIL
DHTesp dht;

const int sensorZMPT101BPin = A0;
// const float VOLT_CALIBRATION = 0.5;
const float VOLT_CAL = 0.00489;


//BOT DE TELEGRAM - COLOCAR EL ID DEL CHAT CORRESPONDIENTE
const String botToken = "7171664981:AAGuuF_qfxvOkydqXbFhSHcRadN3cRS637Q"; 
const char* chatID = ""; 

void enviarAlerta(String mensaje, String fecha, String hora) {
  if (WiFi.status() == WL_CONNECTED) { // Verificar la conexión WiFi
    HTTPClient http;
    http.begin(client, "http://localhost:3006/api/alertas/create"); // Reemplaza 'localhost'con la URL de tu API
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"fecha\":\"" + fecha + "\",\"mensaje\":\"" + mensaje + "\",\"dispositivo_id\":" + dispositivoID + "}";
    int httpCode = http.POST(jsonPayload); // Enviar la solicitud POST

    if (httpCode > 0) { // Verificar que la solicitud fue exitosa
      String payload = http.getString();
      Serial.println("Respuesta recibida: " + payload);
    } else {
      Serial.println("Error en la solicitud: " + httpCode);
    }
    http.end(); // Cerrar la conexión
  } else {
    Serial.println("WiFi no conectado");
  }
}

void enviarAlertaHumedad(float valorSensor, String fecha, String hora){
  String mensaje = "ALERTA! El sensor de HUMEDAD tiene un valor de " + String(valorSensor) + "%";
  Serial.println(mensaje);
  enviarAlerta(mensaje, fecha, hora);
  // bot.sendMessage(chatID, mensaje);
}

void enviarAlertaTemperatura(float valorSensor, String fecha, String hora){
  String mensaje = "ALERTA! El sensor de TEMPERATURA tiene un valor de " + String(valorSensor) + "°";
  Serial.println(mensaje);
  enviarAlerta(mensaje, fecha, hora);
  // bot.sendMessage(chatID, mensaje);
}

void enviarAlertaVoltaje(float valorSensor, String fecha, String hora){
  String mensaje = "ALERTA! El sensor de VOLTAJE tiene un valor de " + String(valorSensor) + " V";
  Serial.println(mensaje);
  enviarAlerta(mensaje, fecha, hora);
  // bot.sendMessage(chatID, mensaje);
}

void verificarAlerta(float temperatura, float humedad, float voltajeP, String fecha, String hora){
  StaticJsonDocument<300> disposId;
  disposId["dispositivo_id"] = dispositivoID;

  Serial.println("ver:" + String(humedad) + " - " + String(temperatura) + " - " + String(voltajeP));

  String requestBody;
  serializeJson(disposId, requestBody);

  String baseUri = "http://localhost:3006/api/confAlertas/getByDispId/"; // Reemplaza 'localhost'con la URL de tu API
  String uri = baseUri + String(dispositivoID);

  http.begin(client, uri);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.GET();

  if(httpCode == HTTP_CODE_OK){
    String payload = http.getString();
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload);

    for (JsonObject config : doc.as<JsonArray>()) {
      String tipoSensor = config["tipo_sensor"];
      float rangoMax = config["rango_max"];
      float rangoMin = config["rango_min"];
      
      Serial.println(tipoSensor);
      Serial.println("data " + String(rangoMax) + " _ " + String(rangoMin));

      if(tipoSensor == "humedad"){
        if(humedad > rangoMax || humedad < rangoMin ){
          // Serial.println("dataH" + String(rangoMax) + " _ " + String(rangoMin))
          enviarAlertaHumedad(humedad, fecha, hora);
        }
      }
      if(tipoSensor == "temperatura"){
        if(temperatura > rangoMax || temperatura < rangoMin ){
          enviarAlertaTemperatura(temperatura, fecha, hora);
        }
      }
      if(tipoSensor == "voltaje"){
        if(voltajeP > rangoMax || voltajeP < rangoMin ){
          enviarAlertaVoltaje(voltajeP, fecha, hora);
        }
      }
    }
  }else{
      Serial.print("Error");
  }
  http.end();
}
void setup() {
  Serial.begin(19200);
  dht.setup(pinDHT, DHTesp::DHT11);

  // Conexión Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");
  Serial.println(WiFi.localIP());

    // Iniciar el cliente NTP
  timeClient.begin();

  timeClient.setTimeOffset(timeZone * 3600);
}

float readACVoltage() {
  int sensorValue = analogRead(sensorZMPT101BPin);
  // Convertir a voltaje teniendo en cuenta el rango del ADC de 1V
  float voltage = sensorValue * VOLT_CAL;
  return voltage;
}

void loop() {
  // timeClient.update();
  // unsigned long epochTime = timeClient.getEpochTime();
  timeClient.update();
  
  // Usar las funciones de TimeLib para obtener fecha y hora
  time_t now = timeClient.getEpochTime();
  setTime(now);
  
  String fecha = String(year()) + "-" + String(month()) + "-" + String(day());
  String hora = String(hour()) + ":" + String(minute()) + ":" + String(second());
  // double voltajeP = random(30, 101) / 10.0;  // Genera un número entre 3.0 y 10.0

  float acVoltage = readACVoltage();
  float voltajeP = acVoltage;
  
  TempAndHumidity data = dht.getTempAndHumidity();
  if (dht.getStatus() == 0) {
    // Crear JSON para enviar
    StaticJsonDocument<300> jsonDocument;
    jsonDocument["temperatura"] = data.temperature;
    jsonDocument["humedad"] = data.humidity;
    jsonDocument["voltaje"] = voltajeP;
    jsonDocument["hora"] = hora;
    jsonDocument["fecha"] = fecha;
    jsonDocument["dispositivo_id"] = dispositivoID;

    Serial.print(data.temperature + data.humidity + voltajeP);

    verificarAlerta(data.temperature, data.humidity, voltajeP, fecha, hora);

    String requestBody;
    serializeJson(jsonDocument, requestBody);

    // Enviar petición POST
    http.begin(client, "http://localhost:3006/api/sensor/create"); // Reemplaza 'localhost'con la URL de tu API
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Código de respuesta: " + String(httpResponseCode));
      Serial.println("Respuesta: " + response);
    } else {
      Serial.println("Error en la solicitud HTTP: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("Error al leer el sensor DHT");
  }
  delay(10000);
}


#include <WiFi.h>
#include <LoRa.h>
#include <SPI.h>
#include <WebServer.h>
#include <ESP32_MySQL.h>

TaskHandle_t TaskDB;
boolean shouldSend;

//Base de datos
#define MYSQL_HOSTNAME "db.pablohacker.cf"
#define MYSQL_PORT 3306
#define MYSQL_USER "cansat"
#define MYSQL_PASS "LinkinSat"
String MYSQL_DATABASE = "cansat";

ESP32_MySQL_Connection conn((Client *)&client);

// Pines LoRa
#define LORA_SS 18
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_FREQ 433E6
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27

// Configuración punto de acceso WiFi
#define WIFI_SSID "LoRaAp"
#define WIFI_PASSW "estonotieneinternet"

// Inicio de los parámetros del servidor web
WebServer server(80);
String loRaIn = "";

// Variables de los sensores
float gpsLat = NAN;       // Latitud GPS
float gpsLon = NAN;       // Longitud GPS
float altitude = NAN;     // Altitud en metros
float temperature = NAN;  // Temperatura en °C (NAN para indicar "Sin datos")
float pressure = NAN;     // Presión en hPa
float rads = NAN;         // Rads (esto depende del sensor que uses)

// Declaración de funciones
/*void handleGet();
String prepareHTML();
void parseLoRaData(String data);
float extractValue(String data, String label);*/

void setup() {
  Serial.begin(115200);

  //Configurar modo dual WiFi
  WiFi.mode(WIFI_AP_STA);

  //Configurar punto de acceso
  IPAddress Ip(192, 168, 1, 69);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);

  WiFi.softAP(WIFI_SSID, WIFI_PASSW);
  Serial.println("Se ha iniciado el servidor HTTP");

  // Mostrar la IP del punto de acceso
  Serial.print("IP del servidor AP: ");
  Serial.println(WiFi.softAPIP());

  //Configurar cliente WiFi

  WiFi.begin("cansat", "estositieneinternet");

  xTaskCreatePinnedToCore(
    TaskDBcode,
    "TaskDB",
    10000,
    NULL,
    1,
    &TaskDB,
    0);

  server.on("/", handleGetMain);
  server.on("/api/data", handleJSONData);
  server.begin();

  // Inicio la placa LoRa en modo receptor
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Fallo en el inicio del LoRa");
  } else {
    Serial.println("LoRa iniciado correctamente");
  }
}

void TaskDBcode(void * pvParameters) {
  Serial.print("Hello from core");
  Serial.println(xPortGetCoreID());

  //Esperamos 10 segundos para conectarse al WiFi
  for (int i = 0; i <= 10; i++) {
    delay(1000);
  }
  
  //Una vez que ya tenemos internet, conectamos y creamos la base de datos inicial (Si no existe)
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Conectado a la red wifi con la ip: ");
    Serial.println(WiFi.localIP());

    if (conn.connectNonBlocking(MYSQL_HOSTNAME, MYSQL_PORT, MYSQL_USER, MYSQL_PASS) != RESULT_FAIL) {
      delay(100);
      runCreateDefault();
      conn.close();
    } else {
      Serial.println("Error al conectarse a la base de datos!");
    }
  } else {
    Serial.println("No se ha podido conectar a la red WiFi dentro del tiempo establecido, la base de datos puede no haberse creado!");
  }

  for(;;){

    if(shouldSend){
      //Serial.println("Sending data...");    
      shouldSend = false;
      if (WiFi.status() == WL_CONNECTED) {
      if (conn.connectNonBlocking(MYSQL_HOSTNAME, MYSQL_PORT, MYSQL_USER, MYSQL_PASS) != RESULT_FAIL) {
        runInsertData();
        conn.close();
      } else {
        Serial.println("Error al conectarse a la base de datos!");
      }
    }
  }

    delay(25);
  }
}



void loop() {
  server.handleClient();

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    //Serial.println(millis());
    loRaIn = "";  // Limpiar el buffer antes de leer
    while (LoRa.available()) {
      loRaIn += (char)LoRa.read();  // Leer correctamente
    }
    parseLoRaData(loRaIn);  // Procesar datos tras recibidos
    //Serial.println(loRaIn);
    Serial.println(String(gpsLat, 6) + "," + String(gpsLon, 6) + "," + String(altitude) + "," + String(temperature) + "," + String(pressure) + "," + String(rads, 6));
    shouldSend = true;
    }
  loRaIn = "";  // Limpiar después de procesar
}


void runCreateDefault() {
  ESP32_MySQL_Query query = ESP32_MySQL_Query(&conn);
  String data = "";
  data = "CREATE TABLE IF NOT EXISTS " + MYSQL_DATABASE + ".temperatura (valor DOUBLE(4,2) NOT NULL, hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al crear la tabla temperatura");
  }
  data = "CREATE TABLE IF NOT EXISTS " + MYSQL_DATABASE + ".altitud (valor DOUBLE(8,2) NOT NULL, hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al crear la tabla altitud");
  }
  data = "CREATE TABLE IF NOT EXISTS " + MYSQL_DATABASE + ".presion (valor DOUBLE(6,2) NOT NULL, hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al crear la tabla presion");
  }
  data = "CREATE TABLE IF NOT EXISTS " + MYSQL_DATABASE + ".radiacion (valor DOUBLE(8,2) NOT NULL, hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al crear la tabla radiacion");
  }
  data = "CREATE TABLE IF NOT EXISTS " + MYSQL_DATABASE + ".gps (latitude DOUBLE(9,6) NOT NULL, longitude DOUBLE(9,6), hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al crear la tabla gps");
  }
}

void runInsertData() {
  ESP32_MySQL_Query query = ESP32_MySQL_Query(&conn);
  String data = "";
  //Inserta temperatura
  data = "INSERT INTO " + MYSQL_DATABASE + ".temperatura(valor) VALUES(" + String(temperature, 2) + ")";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al subir datos temperatura");
  }
  data = "INSERT INTO " + MYSQL_DATABASE + ".presion(valor) VALUES(" + String(pressure, 2) + ")";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al subir datos presion");
  }
  data = "INSERT INTO " + MYSQL_DATABASE + ".radiacion(valor) VALUES(" + String(rads, 2) + ")";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al subir datos radiacion");
  }
  data = "INSERT INTO " + MYSQL_DATABASE + ".altitud(valor) VALUES(" + String(altitude, 2) + ")";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al subir datos altitud");
  }
  data = "INSERT INTO " + MYSQL_DATABASE + ".gps(latitude,longitude) VALUES(" + String(gpsLat, 6) + "," + String(gpsLon, 6) + ")";
  if (!query.execute(data.c_str())) {
    Serial.println("Error al subir datos gps");
  }
}

void parseLoRaData(String data) {
  gpsLat = extractValue(data, "gpsLat:");
  gpsLon = extractValue(data, "gpsLon:");
  altitude = extractValue(data, "altitude:");
  temperature = extractValue(data, "temp:");
  pressure = extractValue(data, "pressure:");
  rads = extractValue(data, "rads:");
}

float extractValue(String data, String label) {
  int startIndex = data.indexOf(label);
  if (startIndex == -1) return NAN;  // No encontrado

  startIndex += label.length();
  int endIndex = data.indexOf(",", startIndex);
  if (endIndex == -1) endIndex = data.length();  // Hasta el final si no hay coma

  String valueString = data.substring(startIndex, endIndex);
  valueString.trim();            // Elimina espacios extra
  return valueString.toFloat();  // Convertir a float
}

void handleGetMain() {
  server.send(200, "text/html", prepareHTML());
}

void handleJSONData() {
  server.send(200, "application/json", prepareJSONData());
}

String prepareHTML() {
  String ptr = "<!DOCTYPE HTML>\n";
  ptr += "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>\n";
  ptr += "body { font-family: Arial, sans-serif; background-color: #000; color: #fff; text-align: center; margin: 0; padding: 20px; }\n";
  ptr += "h1 { font-size: 24px; margin-bottom: 20px; }\n";
  ptr += ".sensor-item { background-color: #222; margin: 10px 0; padding: 15px; border-radius: 8px; text-align: left; }\n";
  ptr += ".sensor-item h3 { margin: 0; font-size: 18px; color: #aaa; }\n";
  ptr += ".sensor-value { font-size: 20px; color: #fff; }\n";
  ptr += "</style>\n";
  ptr += "<script>window.setInterval(()=>{fetch(\"/api/data\").then(e=>e.json()).then(e=>{document.getElementById(\"gps\").innerText=e.gps.lat+\" \"+e.gps.lon,document.getElementById(\"alt\").innerText=e.alt,document.getElementById(\"temp\").innerText=e.temp,document.getElementById(\"press\").innerText=e.press,document.getElementById(\"rads\").innerText=e.rads})},500);</script>\n</head><body>\n";

  ptr += "<h1>Datos de los Sensores</h1>\n";
  ptr += "<div class=\"sensor-item\"><h3>GPS</h3><p id=\"gps\" class=\"sensor-value\">" + (isnan(gpsLat) || isnan(gpsLon) ? "Sin datos" : "Lat: " + String(gpsLat) + ", Lon: " + String(gpsLon)) + "</p></div>\n";
  ptr += "<div class=\"sensor-item\"><h3>Altitud</h3><p id=\"alt\" class=\"sensor-value\">" + (isnan(altitude) ? "Sin datos" : String(altitude) + " m") + "</p></div>\n";
  ptr += "<div class=\"sensor-item\"><h3>Temperatura</h3><p id=\"temp\" class=\"sensor-value\">" + (isnan(temperature) ? "Sin datos" : String(temperature) + " C") + "</p></div>\n";
  ptr += "<div class=\"sensor-item\"><h3>Presion</h3><p id=\"press\" class=\"sensor-value\">" + (isnan(pressure) ? "Sin datos" : String(pressure) + " hPa") + "</p></div>\n";
  ptr += "<div class=\"sensor-item\"><h3>Rads</h3><p id=\"rads\" class=\"sensor-value\">" + (isnan(rads) ? "Sin datos" : String(rads) + " rads") + "</p></div>\n";

  ptr += "</body></html>";
  return ptr;
}

String prepareJSONData() {
  String ptr = "{\n";
  ptr += "  \"gps\": {";
  ptr += "\n    \"lat\": \"" + (isnan(gpsLat) ? "Sin datos" : String(gpsLat, 6)) + "\",\n";
  ptr += "    \"lon\": \"" + (isnan(gpsLon) ? "Sin datos" : String(gpsLon, 6)) + "\"\n";
  ptr += "\n  },\n";
  ptr += "  \"alt\": \"" + (isnan(altitude) ? "Sin datos" : String(altitude) + " m") + "\",\n";
  ptr += "  \"temp\": \"" + (isnan(temperature) ? "Sin datos" : String(temperature) + " C") + "\",\n";
  ptr += "  \"press\": \"" + (isnan(pressure) ? "Sin datos" : String(pressure) + " hPa") + "\",\n";
  ptr += "  \"rads\": \"" + (isnan(rads) ? "Sin datos" : String(rads) + " rads") + "\"\n";
  ptr += "}";

  return ptr;
}

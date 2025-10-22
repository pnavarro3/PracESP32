#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

#define TRIG_PIN 14
#define ECHO_PIN 12
#define LED1 19
#define LED2 18
#define LED3 5
#define LED4 17
#define BUTTON_PIN 26

// Variables
bool enSleep = false;
unsigned long tCerca = 0;
float distanciaActual = 0;
float distanciaMinima = 150;
float aceleracionSimulada = 0;

// WiFi
const char* ssid = "yiyiyi";          
const char* password = "xabicrack";   


WebServer server(80);

// Medir distancia HC-SR04
float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  float distancia = duracion * 0.034 / 2;
  return distancia;
}

// Calcular aceleración simulada (cm/s²) 
float calcularAceleracion(float distancia) {
  float aMax = 100.0; 
  if (distancia < 15) return 0;
  if (distancia > 100) return aMax;
  float a = ((distancia - 15) / (100.0 - 15.0)) * aMax;
  return a;
}

void mostrarLEDSPorAceleracion(float aceleracion) {
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);

  if (aceleracion > 0) digitalWrite(LED1, HIGH);
  if (aceleracion > 25) digitalWrite(LED2, HIGH);
  if (aceleracion > 50) digitalWrite(LED3, HIGH);
  if (aceleracion > 75) digitalWrite(LED4, HIGH);
}

void apagarLeds() {
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}

// Entrar en modo sleep
void entrarEnSleep() {
  Serial.println("Entrando en modo baja energía...");
  apagarLeds();
  enSleep = true;

  // Configurar G26 para despertar
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);

  delay(200); 
  esp_light_sleep_start();

  Serial.println("Despertado por pulsador");
  enSleep = false;
}

// Servidor web: mostrar distancia y aceleración
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP32 HC-SR04</h1>";
  html += "<p>Distancia actual: " + String(distanciaActual,1) + " cm</p>";
  html += "<p>Distancia minima registrada: " + String(distanciaMinima,1) + " cm</p>";
  html += "<p>Aceleracion simulada: " + String(aceleracionSimulada,1) + " cm/s^2</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void conectarWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n No se pudo conectar a WiFi");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  apagarLeds();

  // EEPROM
  EEPROM.begin(512);
  distanciaMinima = EEPROM.readFloat(0);
  if (distanciaMinima == 0 || distanciaMinima == -1) distanciaMinima = 9999;
  Serial.print("Distancia mínima cargada: ");
  Serial.println(distanciaMinima);

  conectarWiFi();

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Servidor web iniciado");
}

// ================= Loop =================
void loop() {
  // Manejo servidor web
  server.handleClient();

  // Comprobar pulsador para sleep
  if (!enSleep && digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // antirrebote
    if (digitalRead(BUTTON_PIN) == LOW) entrarEnSleep();
  }

  distanciaActual = medirDistancia();
  aceleracionSimulada = calcularAceleracion(distanciaActual);

  Serial.print("Distancia: "); Serial.print(distanciaActual);
  Serial.print(" cm | Aceleración: "); Serial.print(aceleracionSimulada);
  Serial.println(" cm/s²");

  // Guardar distancia mínima
  if (distanciaActual < distanciaMinima) {
    distanciaMinima = distanciaActual;
    EEPROM.writeFloat(0, distanciaMinima);
    EEPROM.commit();
    Serial.print("Nueva distancia mínima: "); Serial.println(distanciaMinima);
  }

  if (!enSleep) mostrarLEDSPorAceleracion(aceleracionSimulada);

  // Entrar automáticamente en sleep si <15 cm por 5 s
  if (!enSleep) {
    if (distanciaActual < 15) {
      if (tCerca == 0) tCerca = millis();
      else if (millis() - tCerca >= 5000) {
        entrarEnSleep();
        tCerca = 0;
      }
    } else tCerca = 0;
  }

  delay(200);
}

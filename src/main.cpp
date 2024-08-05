#include <Arduino.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secret.h"
#include <UniversalTelegramBot.h>
#include "esp_adc_cal.h"

#define MAXATTEMPS 10
#define MAXVALUE 1600
#define MAXMESSAGELENGHT 200

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */
#define A1 4

#define BAT_ADC    2
#define MIN_BAT 3500

RTC_DATA_ATTR int alertCount = 0;

float Voltage = 0.0;
uint32_t readADC_Cal(int ADC_Raw);

WiFiClientSecure client;
UniversalTelegramBot bot(BOTTOKEN, client);

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

boolean connectWifi (char *ssid, char *pass){
  WiFi.useStaticBuffers(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  int max = MAXATTEMPS;

  Serial.println("Connecting to WiFi");
  while ((WiFi.status() != WL_CONNECTED) && --max>0) {
    delay(500);
    Serial.print(".");
  }
  if (max == 0)
    return false;

  Serial.println("");
  Serial.println("Connected to WiFi");
  return true;
}

boolean sendAlert(float v) {
  alertCount++;
  // Crear un mensaje de texto
  char text[MAXMESSAGELENGHT];
  snprintf(text, MAXMESSAGELENGHT, "Alerta! El valor del sensor ha superado el umbral. v = %.2f - alerta: %d",v / 1000.0,alertCount);

  bot.sendMessage(CHAT_ID, text);

  return true;
}

void setup() {
  // (Optional)Press reset button
  // on the dev board to see these print statements

  Serial.begin(115200);
  while (!Serial) { }

  // Configuramos el pin A1 como de entrada analógica
  pinMode(A1, INPUT);


  delay (1000);
  Serial.println("Starting...");

}

void loop() {
  // Leeremos periódicamente el valor analógico
  // del pin A1 y lo imprimiremos en el monitor serie
  int sensorValue = analogRead(A1);
  Serial.println(sensorValue);
  Voltage = (readADC_Cal(analogRead(BAT_ADC))) * 2;
  Serial.printf("%.2fV", Voltage / 1000.0);

  // Cuando el valor llegue al configurado MAXVALUE
  // conectaremos con la wifi
  if (sensorValue > MAXVALUE || Voltage < MIN_BAT) {
    if (!connectWifi(ssid, pass)) {
      Serial.println("Failed to connect to WiFi");
      return;
    } else {
      // printWifiStatus();
      // Enviaremos una alerta llamando a la url 
      if (!sendAlert(Voltage)) {
        Serial.println("Failed to send alert");
        return;
      }
    }
  } else {
    alertCount = 0;
  }
  #ifdef DEEPSLEEP
  // Nos echamos a dormir para no gastar energía
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
  #endif
  delay(1000);
}

uint32_t readADC_Cal(int ADC_Raw)
{
    esp_adc_cal_characteristics_t adc_chars;

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    return (esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}



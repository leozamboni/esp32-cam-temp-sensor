#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "esp_camera.h"

const char *ssid = "WIFI_SSID";
const char *password = "WIFI_PASSWORD";

#define ONE_WIRE_BUS 15
#define DEVICE_DISCONNECTED_C -127.0
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

WebServer server(80);

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

float currentTemp = DEVICE_DISCONNECTED_C;

void startCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound())
  {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Erro ao iniciar a câmera: 0x%x\n", err);
    return;
  }
}

void handleRoot()
{
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="pt-BR">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>ESP32-CAM + Temperatura</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        text-align: center;
        margin: 10px;
        background: #f5f5f5;
        color: #222;
      }
      h1 {
        margin-bottom: 5px;
      }
      #temp {
        font-size: 2em;
        color: #d9534f;
      }
      #camera-stream {
        margin-top: 15px;
        max-width: 100%;
        height: auto;
        border-radius: 10px;
        box-shadow: 0 0 8px rgba(0,0,0,0.2);
      }
      footer {
        margin-top: 20px;
        font-size: 0.8em;
        color: #666;
      }
    </style>
    <script>
      function atualizarTemperatura() {
        fetch('/temp')
          .then(response => response.text())
          .then(temp => {
            document.getElementById('temp').innerText = temp + ' °C';
          })
          .catch(() => {
            document.getElementById('temp').innerText = '--';
          });
      }
      setInterval(atualizarTemperatura, 5000);
      window.onload = atualizarTemperatura;
    </script>
  </head>
  <body>
    <h1>Temperatura atual: <span id="temp">--</span></h1>
    <img id="camera-stream" src="/stream" alt="Stream da Câmera" />
    <footer>ESP32-CAM + DS18B20 &copy; 2025</footer>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleStream()
{
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (client.connected())
  {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Falha ao capturar frame");
      break;
    }

    String partHeader = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(fb->len) + "\r\n\r\n";
    server.sendContent(partHeader);
    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");

    esp_camera_fb_return(fb);

    delay(50);
  }

  client.stop(); 
}

void handleTemp()
{
  if (currentTemp == DEVICE_DISCONNECTED_C)
  {
    server.send(200, "text/plain", "Erro");
  }
  else
  {
    server.send(200, "text/plain", String(currentTemp, 2));
  }
}

unsigned long lastTempRequest = 0;
const unsigned long tempInterval = 2000;

void setup()
{
  Serial.begin(115200);
  sensors.begin();

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi conectado! IP: ");
  Serial.println(WiFi.localIP());

  startCamera();

  server.on("/temp", handleTemp);
  server.on("/stream", HTTP_GET, handleStream);
  server.begin();

  Serial.println("Servidor iniciado.");
}

void loop()
{
  unsigned long now = millis();
  if (now - lastTempRequest > tempInterval)
  {
    sensors.requestTemperatures();
    currentTemp = sensors.getTempCByIndex(0);
    lastTempRequest = now;
  }
  server.handleClient();
}

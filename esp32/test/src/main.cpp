#include <Arduino.h>

// ESP32-C3 Super Mini 板载蓝灯在 GPIO8。
// 注意：这板不同批次 LED 方向不一样，如果 HIGH 不亮，把下面两个 HIGH/LOW 对调即可。
#define LED_BUILTIN 8

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  delay(200); // 等 USB 串口就绪
  Serial.println("Hello from ESP32-C3 via PlatformIO!");
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(400);
  digitalWrite(LED_BUILTIN, LOW);
  delay(400);
}

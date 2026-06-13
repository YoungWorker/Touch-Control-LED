#include <Arduino.h>

// ==================== PIN DEFINITIONS ====================
#define TOUCH_SENSOR_PIN D5      // GPIO14 - TTP223 output pin
#define LED_WARM_PIN D1          // GPIO5  - Warm white LED (PWM)
#define LED_COOL_PIN D2          // GPIO4  - Cool white LED (PWM)
#define LED_WARM_COOL_PIN D3     // GPIO0  - Warm-cool mix LED (PWM)

// ==================== STATE VARIABLES ====================
enum ColorMode {
  MODE_OFF = 0,         // 灯灭
  MODE_WARM = 1,        // 暖色光
  MODE_COOL = 2,        // 冷白色光
  MODE_WARM_COOL = 3    // 暖白色光
};

ColorMode currentMode = MODE_OFF;
int brightness = 255;  // 0-255 PWM brightness

unsigned long lastTouchTime = 0;
const unsigned long DEBOUNCE_TIME = 300;  // 防抖延迟 (ms)
const unsigned long TOUCH_TIMEOUT = 2000; // 触摸超时时间 (ms) - 判断是否为新的触摸序列

bool lastTouchState = LOW;
int touchCount = 0;     // 当前触摸序列的计数
unsigned long touchSequenceStartTime = 0;  // 触摸序列开始时间

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n========== ESP8266 Touch Control LED v2.0 ==========");
  Serial.println("控制逻辑：");
  Serial.println("  第1次触摸 → 白光 (暖白色光模式)");
  Serial.println("  第2次触摸 → 暖色光");
  Serial.println("  第3次触摸 → 冷白色光");
  Serial.println("  第4次触摸 → 关灯");
  Serial.println("  2秒无触摸则重新计数");
  Serial.println("=========================================\n");
  
  // 初始化引脚
  pinMode(TOUCH_SENSOR_PIN, INPUT);
  pinMode(LED_WARM_PIN, OUTPUT);
  pinMode(LED_COOL_PIN, OUTPUT);
  pinMode(LED_WARM_COOL_PIN, OUTPUT);
  
  // 设置PWM频率 (1000Hz)
  analogWriteFreq(1000);
  analogWriteRange(255);
  
  // 关闭所有LED
  allLedOff();
  
  Serial.println("Setup complete. Waiting for touch input...");
}

// ==================== MAIN LOOP ====================
void loop() {
  handleTouchInput();
  updateLED();
  delay(10);
}

// ==================== TOUCH HANDLING ====================
void handleTouchInput() {
  bool currentTouchState = digitalRead(TOUCH_SENSOR_PIN);
  unsigned long now = millis();
  
  // 检测触摸从LOW到HIGH的边沿（按下）
  if (currentTouchState == HIGH && lastTouchState == LOW) {
    // 防抖检查
    if (now - lastTouchTime < DEBOUNCE_TIME) {
      lastTouchState = currentTouchState;
      return;
    }
    
    lastTouchTime = now;
    
    // 检查是否需要重置触摸计数
    if (now - touchSequenceStartTime > TOUCH_TIMEOUT) {
      // 超时：开始新的触摸序列
      touchCount = 0;
      touchSequenceStartTime = now;
      Serial.println("\n[新序列开始]");
    }
    
    // 增加触摸计数
    touchCount++;
    Serial.printf("\n[触摸 #%d] 检测到触摸\n", touchCount);
    
    // 根据触摸次数执行动作
    executeTouchCommand(touchCount);
  }
  
  lastTouchState = currentTouchState;
}

// ==================== EXECUTE TOUCH COMMAND ====================
void executeTouchCommand(int count) {
  switch (count) {
    case 1:
      // 第1次触摸：打开白光 (暖白色混合)
      currentMode = MODE_WARM_COOL;
      brightness = 255;
      printStatus();
      Serial.println("  →白光打开 (暖白色光)");
      break;
      
    case 2:
      // 第2次触摸：切换到暖色光
      currentMode = MODE_WARM;
      brightness = 255;
      printStatus();
      Serial.println("  →切换到暖色光");
      break;
      
    case 3:
      // 第3次触摸：切换到冷白色光
      currentMode = MODE_COOL;
      brightness = 255;
      printStatus();
      Serial.println("  →切换到冷白色光");
      break;
      
    case 4:
      // 第4次触摸：关灯
      currentMode = MODE_OFF;
      brightness = 0;
      printStatus();
      Serial.println("  →灯关闭");
      touchCount = 0;  // 重置计数，准备下一个周期
      break;
      
    default:
      // 不应该到达这里
      break;
  }
}

// ==================== LED CONTROL ====================
void updateLED() {
  switch (currentMode) {
    case MODE_OFF:
      // 灯灭：所有LED关闭
      analogWrite(LED_WARM_PIN, 0);
      analogWrite(LED_COOL_PIN, 0);
      analogWrite(LED_WARM_COOL_PIN, 0);
      break;
      
    case MODE_WARM:
      // 暖色光：只开暖色LED
      analogWrite(LED_WARM_PIN, brightness);
      analogWrite(LED_COOL_PIN, 0);
      analogWrite(LED_WARM_COOL_PIN, 0);
      break;
      
    case MODE_COOL:
      // 冷白色光：只开冷白LED
      analogWrite(LED_COOL_PIN, brightness);
      analogWrite(LED_WARM_PIN, 0);
      analogWrite(LED_WARM_COOL_PIN, 0);
      break;
      
    case MODE_WARM_COOL:
      // 暖白色光 (白光)：混合暖+冷
      // 暖色与冷白各占50%，混合LED作为主输出
      analogWrite(LED_WARM_COOL_PIN, brightness);
      analogWrite(LED_WARM_PIN, brightness / 2);
      analogWrite(LED_COOL_PIN, brightness / 2);
      break;
  }
}

void allLedOff() {
  analogWrite(LED_WARM_PIN, 0);
  analogWrite(LED_COOL_PIN, 0);
  analogWrite(LED_WARM_COOL_PIN, 0);
}

// ==================== DEBUG FUNCTIONS ====================
void printStatus() {
  const char* modes[] = {"灯灭", "暖色光", "冷白色光", "暖白色光(白)"};
  int percent = map(brightness, 0, 255, 0, 100);
  Serial.printf("\n[状态] 模式: %s | 亮度: %d%%\n", modes[currentMode], percent);
}

// ==================== 时间显示辅助函数 ====================
void printTime() {
  unsigned long now = millis();
  unsigned long timeSinceLastTouch = now - lastTouchTime;
  unsigned long timeSinceSequenceStart = now - touchSequenceStartTime;
  
  Serial.printf("[时间] 距离上次触摸: %ldms | 序列时长: %ldms | 触摸计数: %d\n",
                timeSinceLastTouch, timeSinceSequenceStart, touchCount);
}

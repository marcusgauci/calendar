
#define BUTTON_PIN 7
#define LED_PIN 9
#define SOIL_PIN 2

int button_state;

bool timerRunning = false;
unsigned long startTime = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);
}

void loop() {
  //button_state = digitalRead(BUTTON_PIN);
  //if (button_state == HIGH) {
  //  digitalWrite(LED_PIN, LOW);
  //} else {
  //  digitalWrite(LED_PIN, HIGH);
  //}
  int buttonState = digitalRead(BUTTON_PIN);
  int sensor_value = analogRead(SOIL_PIN);
  Serial.println(sensor_value);

  if (!timerRunning && buttonState == LOW) {
    digitalWrite(LED_PIN, HIGH);
    startTime = millis();
    timerRunning = true;
  }

  if (timerRunning && (millis() - startTime >= 2000)) {
    digitalWrite(LED_PIN, LOW);
    timerRunning = false;
  }
}


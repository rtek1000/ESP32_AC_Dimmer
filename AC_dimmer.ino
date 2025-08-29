// ESP32 60Hz dimmer

#define TRIAC_pin 4
#define ZCD_pin 15

#define DIMMER_ON_TIMER_MIN 820
#define DIMMER_ON_TIMER_MAX 45

uint32_t millis1 = 0;
uint32_t millis2 = 0;

volatile uint16_t dimm_buff = 0;
bool dimm_up = false;

uint8_t dimm_power = 0;

volatile uint32_t zc_timer_10us = 0;
volatile uint32_t zc_time_us = 0;

volatile uint16_t dimmer_on_timer = 0;
volatile uint8_t dimmer_off_timer = 0;

hw_timer_t *timer_10us = NULL;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE extIntMux = portMUX_INITIALIZER_UNLOCKED;

#define DIMM_MAP_MIN 515
#define DIMM_MAP_MAX 37

const int DimmMap_array[] = {
  515, 445, 426, 413, 402, 392, 384, 377, 370, 363,
  357, 352, 346, 341, 336, 332, 327, 323, 318, 314,
  310, 306, 302, 298, 295, 291, 287, 284, 280, 277,
  274, 270, 267, 264, 261, 257, 254, 251, 248, 245,
  242, 239, 236, 233, 230, 227, 224, 221, 218, 216,
  213, 210, 207, 204, 201, 199, 196, 193, 190, 187,
  184, 182, 179, 176, 173, 170, 168, 165, 162, 159,
  156, 153, 150, 147, 144, 141, 138, 135, 132, 129,
  126, 123, 120, 117, 113, 110, 107, 103, 100, 96,
  92, 89, 85, 81, 76, 72, 67, 62, 56, 49,
  37
};

uint16_t convert_Dimm(uint8_t power_output) {
  uint16_t result = 0;

  if (power_output <= 100) {
    uint16_t value = DimmMap_array[power_output];

    result = map(value, DIMM_MAP_MIN, DIMM_MAP_MAX, DIMMER_ON_TIMER_MIN, DIMMER_ON_TIMER_MAX);

    return result;
  } else {
    return 0;
  }
}

void ARDUINO_ISR_ATTR timer_10us_ISR() {
  portENTER_CRITICAL_ISR(&timerMux);
  zc_timer_10us++;

  if (dimmer_on_timer > 0) {
    dimmer_on_timer--;

    if (dimmer_on_timer == 0) {
      digitalWrite(TRIAC_pin, HIGH);
    }
  } else {
    if (dimmer_off_timer > 0) {
      dimmer_off_timer--;
    } else {
      digitalWrite(TRIAC_pin, LOW);
    }
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR zero_cross_int_ISR() {
  portENTER_CRITICAL_ISR(&extIntMux);
  if (digitalRead(ZCD_pin) == LOW) {
    zc_time_us = zc_timer_10us;
    zc_timer_10us = 0;

    if ((dimm_buff <= DIMMER_ON_TIMER_MIN) && (dimm_buff >= DIMMER_ON_TIMER_MAX)) {
      dimmer_on_timer = dimm_buff;
    } else {
      dimmer_on_timer = 0;
    }

    dimmer_off_timer = 5;
  }
  portEXIT_CRITICAL_ISR(&extIntMux);
}

void setup() {
  // put your setup code here, to run once:
  pinMode(TRIAC_pin, OUTPUT);
  pinMode(ZCD_pin, INPUT_PULLUP);

  digitalWrite(TRIAC_pin, LOW);

  Serial.begin(115200);  // Initialize the serial communication:
  delay(1000);

  attachInterrupt(ZCD_pin, zero_cross_int_ISR, CHANGE);  // CHANGE FALLING RISING

  timer_10us = timerBegin(100000);
  timerAttachInterrupt(timer_10us, &timer_10us_ISR);
  timerAlarm(timer_10us, 1, true, 0);  // 10us

  Serial.println("Test begin");
}

void loop() {
  // put your main code here, to run repeatedly:
  if ((millis() - millis2) >= 10) {
    millis2 = millis();

    // dimm_power is the control variable
    if (dimm_up == true) {
      if (dimm_power > 0) { // 0%
        dimm_power--;
      } else {
        dimm_up = false;
      }
    } else {
      if (dimm_power < 100) { // 100%
        dimm_power++;
      } else {
        dimm_up = true;
      }
    }

    dimm_buff = convert_Dimm(dimm_power); // 0-100% to dimm_us
  }

  if ((millis() - millis1) >= 100) {
    millis1 = millis();

    Serial.print(get_freq(zc_time_us));
    Serial.println(" Hz");
  }
}

float get_freq(uint32_t time_us) {
    float freq = 0;

    if (time_us != 0) {
      freq = 100000;
      freq /= time_us;
      freq /= 2;
    }

    return freq;
}
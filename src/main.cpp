#include <Arduino.h>

#define TRANS_SHORT 0
#define TRANS_LONG 1

static uint8_t rx_started = 0;
static uint8_t rx_finished = 0;
static uint8_t last_sample = 0;
static uint8_t rx_count = 0;
static uint8_t t_count = 0;
static uint32_t data = 0;
static uint8_t data_size = 0;
static uint8_t prev_bit = 1;
static uint8_t prev_short_trans = 1;
static uint8_t error_code = 0;
static uint8_t error_msg = 0;

void send_error() {
  if (!error_code)
    return;

  Serial.print("#");
  Serial.print(error_code);
  Serial.println(error_msg);

  error_code = 0;
  error_msg = 0;
}

void send_hex8(uint8_t data) {
  if (data < 0x10)
    Serial.print("0");
  
  Serial.println(data, HEX);
}

void send_hex16(uint16_t data) {
  if (data < 0x1000)
    Serial.print("0");
  if (data < 0x100)
    Serial.print("0");
  if (data < 0x10)
    Serial.print("0");
  
  Serial.println(data, HEX);
}

void send_data() {
  if (!rx_finished)
    return;

  if (data_size == 8)
    send_hex8(data);
  else
    send_hex16(data);

    rx_finished = 0;
}

void setup() {
  Serial.begin(9600);
  TCCR2A = _BV(WGM21);
  TCCR2B = _BV(CS22); // 1/64 prescaler
  OCR2A = (128 >> 2) - 1; 
  TIMSK2 = _BV(OCIE2A); // Turn on interrupt
  TCNT2 = 0; // Set counter to 0
}

void error(uint8_t code, uint8_t msg = 0) {
  rx_started = 0;
  rx_finished = 0;
  error_code = code;
  error_msg = msg;
}

ISR(TIMER2_COMPA_vect) {
  uint8_t sample = digitalRead(2);
  uint8_t transition = sample != last_sample;
  rx_count++;

  if (rx_finished)
    return;

  if (rx_started) {
    if (rx_count > 12 && sample == 1 && !transition) {
      data_size--;
      if (data_size != 8 && data_size != 16)
        return error(1, data_size);
      
      if (data_size == 8)
        data &= 0xFF;

      rx_started = 0;
      rx_finished = 1;
      last_sample = sample;
      return;
    }

    if (transition) {
      // If time between two transitions is too long or too short then throw an error
      if (rx_count < 2 || rx_count > 7)
        return error(2, rx_count);

      uint8_t tt = rx_count < 5 ? 0 : 1;
      t_count++;
      rx_count = 0;

      if (tt == TRANS_LONG ) {
        if (prev_short_trans)
          return error(3);

        data = (data << 1) | !prev_bit;
        data_size++;
        prev_bit = !prev_bit;
      }
      else {
        if (prev_short_trans) {
          data = (data << 1) | prev_bit;
          data_size++;
          prev_short_trans = 0;
        }
        else
          prev_short_trans = 1;
      }
    }

  }
  else {
    if (sample == 0 && transition) {
      rx_started = 1;
      rx_count = 0;
      t_count = 1;
      data = 0;
      data_size = 0;
      prev_bit = 1;
      prev_short_trans = 1;
    }
  }

  last_sample = sample;
}

void loop() {
  if (rx_finished)
    send_data();
  
  if (error_code)
    send_error();
}
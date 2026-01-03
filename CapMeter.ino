#include <LiquidCrystal_I2C.h>

const int TIMER_OUT_PIN = 2;
const int MUX_S0 = 4;
const int MUX_S1 = 5;
const int MUX_S2 = 6;

LiquidCrystal_I2C lcd(0x27, 16, 2);

const float R1_FIXED = 1000.0;
const float R2_VALUES[] = {
  10.0e6,
  100.0e3,
  10.0e3,
  1.0e3,
  100.0
};
const int NUM_RANGES = sizeof(R2_VALUES) / sizeof(R2_VALUES[0]);

//Min and max acceptable oscillation periods
const long MIN_MEASURABLE_PERIOD_US = 10;
const long MAX_MEASURABLE_PERIOD_US = 1500000;
const long PULSEIN_TIMEOUT_US = 1000000;

//Calibration constants
const float PARASITIC_CAPACITANCE_PF = 33.2; 
const float MICROFARAD_CORRECTION_FACTOR = 0.70;
const float MID_UF_CORRECTION_FACTOR = 1.3;

void setup() {
  Serial.begin(9600);
  Serial.println("Autoranging Capacitance Meter Initializing...");
  lcd.init();
  lcd.backlight();
  lcd.print("Capacitance Meter");
  lcd.setCursor(0, 1);
  lcd.print("Ready...");

  pinMode(TIMER_OUT_PIN, INPUT);
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);

  delay(2000);
  lcd.clear();
}

void setMultiplexerChannel(int channel) {
  digitalWrite(MUX_S0, bitRead(channel, 0));
  digitalWrite(MUX_S1, bitRead(channel, 1));
  digitalWrite(MUX_S2, bitRead(channel, 2));
  delay(10);
}

long measureOscillationPeriod() {
  long highTime = pulseIn(TIMER_OUT_PIN, HIGH, PULSEIN_TIMEOUT_US);
  long lowTime = pulseIn(TIMER_OUT_PIN, LOW, PULSEIN_TIMEOUT_US);

  if (highTime == 0 || lowTime == 0) {
    return 0;
  }
  return highTime + lowTime;
}

void displayResults(float capacitance, long period, float r2Value) {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("P:");
  lcd.print(period);
  lcd.print("us R2:");
  if (r2Value >= 1e6) {
    lcd.print(r2Value / 1e6);
    lcd.print("M");
  } else if (r2Value >= 1e3) {
    lcd.print(r2Value / 1e3);
    lcd.print("k");
  } else {
    lcd.print(r2Value);
  }
  lcd.print("O"); // Using 'O' for the Ohm symbol for simplicity

  lcd.setCursor(0, 1);
  lcd.print("C:");

  if (capacitance < 1e-9) { // Picofarads
    lcd.print(capacitance * 1e12, 2);
    lcd.print("pF");
  } else if (capacitance < 1e-6) { // Nanofarads
    lcd.print(capacitance * 1e9, 2);
    lcd.print("nF");
  } else { // Microfarads
    lcd.print(capacitance * 1e6, 2);
    lcd.print("uF");
  }
}

void loop() {
  long measuredPeriod_us = 0;
  float calculatedCapacitance_F = 0.0;
  bool rangeFound = false;
  int bestRangeIndex = -1;

  for (int i = 0; i < NUM_RANGES; i++) {
    setMultiplexerChannel(i);
    delay(20);
    measuredPeriod_us = measureOscillationPeriod();

    if (measuredPeriod_us >= MIN_MEASURABLE_PERIOD_US && measuredPeriod_us <= MAX_MEASURABLE_PERIOD_US) {
      bestRangeIndex = i;
      rangeFound = true;
      break;
    }
    delay(20);
  }

  if (rangeFound && bestRangeIndex != -1) {
    float selectedR2 = R2_VALUES[bestRangeIndex];
    delay(20);
    measuredPeriod_us = measureOscillationPeriod();

    if (measuredPeriod_us > 0) {
      calculatedCapacitance_F = (float)measuredPeriod_us * 1e-6 / (0.693 * (R1_FIXED + 2 * selectedR2));

      if (bestRangeIndex == 0) {
          float calculated_pF = calculatedCapacitance_F * 1e12;
          calculated_pF -= PARASITIC_CAPACITANCE_PF;
          if (calculated_pF < 0) calculated_pF = 0;
          calculatedCapacitance_F = calculated_pF * 1e-12;
      }
      else if (calculatedCapacitance_F >= 10e-6 && calculatedCapacitance_F <= 30e-6) {
          calculatedCapacitance_F *= MID_UF_CORRECTION_FACTOR;
      }
      else if (bestRangeIndex >= 3) {
          calculatedCapacitance_F *= MICROFARAD_CORRECTION_FACTOR;
      }

      // Display the final results on the LCD
      displayResults(calculatedCapacitance_F, measuredPeriod_us, selectedR2);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Measurement Failed");
      lcd.setCursor(0, 1);
      lcd.print("Check Connections");
    }
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Out of Range");
    lcd.setCursor(0, 1);
    lcd.print("Check Capacitor");
  }

  delay(2000);
}

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define DEFAULT_MODE 0
#define DEFAULT_AVERAGING 6
#define DEFAULT_BAND 3

#define AD8317_INPUT A0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define AVG 0
#define MAX 1
#define MIN 2

#define BTN_SET 10
#define BTN_SELECT 9
#define BTN_GROUND 8

#define CAL_LOW -40
#define CAL_UP -10

#define OLED_RESET -1

#define MAX_ATT 70

#define MODES 3
char *mode_labels[] = { "AVG", "MAX", "MIN" };

#define BANDS 9
char *band_labels[] = { "10M", "20M", "50M", "144", "430", "1G2", "2G3", "5G", "10G" };
uint16_t cal_lower[]= { 600,   600,   600,   600,   600,   600,   600,   600,  600   }; // guessing for now
uint16_t cal_upper[]= { 225,   225,   225,   225,   225,   225,   225,   225,  225   };
double cal_a[] =      { 0,     0,     0,     0,     0,     0,     0,     0,    0     };
double cal_b[] =      { 0,     0,     0,     0,     0,     0,     0,     0,    0     };

#define AVERAGE_MODES 10
uint16_t avg_values[] = { 1, 5, 10, 50, 100, 500, 1000, 2000, 5000, 10000};

#define SELECTIONS_AVAIL 6
enum btn_select{SEL_UNITS, SEL_BAND, SEL_MODE, SEL_AVG, SEL_ATT, SEL_NONE, SEL_SAVE, SEL_CAL_LEVEL};

#define SELECTIONS_CAL 5
int calibration_selections[] = { SEL_BAND, SEL_MODE, SEL_AVG, SEL_CAL_LEVEL, SEL_SAVE };

#define UNITS_AVAIL 2
enum measure_units{DBM, RAW};
char *units_labels[] = { "dBm", "RAW"};

int mode=DEFAULT_MODE, averaging=DEFAULT_AVERAGING, band=DEFAULT_BAND, current_sel=SEL_NONE, units=SEL_UNITS;
int lowest, highest, r, att, display_value, calibration_mode=0, current_sel_cal=SEL_BAND, cal_level=0;
uint32_t cumulative;
char k;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  // power to the OLED @ pin D2
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  delay(100);

  analogReference(EXTERNAL);
  Serial.begin(115200);
  Serial.println();

  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  // ground for buttons
  pinMode(BTN_GROUND, OUTPUT);
  digitalWrite(BTN_GROUND, LOW);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  prepare_calib();

  if(!digitalRead(BTN_SET) || !digitalRead(BTN_SELECT)) {
    calibration_mode = 1;
    units = RAW;
    
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(4);
    display.print("CALIB :)");
    display.display();
    delay(3000);
  }
}

// read values from EEPROM and prepare "a" & "b" coefficients for calibration
void prepare_calib() {  
  for(int i=0; i<BANDS; i++) {
    cal_lower[i] = (EEPROM.read(i*4) << 8) + EEPROM.read(i*4+1);
    cal_upper[i] = (EEPROM.read(i*4+2) << 8) + EEPROM.read(i*4+3);
    cal_a[i] = (float)(CAL_LOW-CAL_UP)/(cal_lower[i]-cal_upper[i]);
    cal_b[i] = -((double)cal_lower[i]*cal_a[i]-CAL_LOW);
  }
}

// return value in dBm incl. calibration
double get_dbm(int readout) {
  return cal_a[band]*readout + cal_b[band] + att;
}

void invert_text_if_selected(uint8_t value) {
  if(current_sel == value) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // invert text color
  }
}

void reset_text_color() {
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // regular white-on-black
}

void display_all(int val) {
  display.clearDisplay();

  // print measured value
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  switch(units) {
    case DBM:
      display.print(get_dbm(val), 1);
      break;
    case RAW:
      display.print(val);
  }

  // print units
  display.setTextSize(2);
  display.setCursor(90, 0);
  invert_text_if_selected(SEL_UNITS);
  display.print(units_labels[units]);
  reset_text_color();

  // print band label
  display.setTextSize(1);
  display.setCursor(110, 16);
  invert_text_if_selected(SEL_BAND);
  display.print(band_labels[band]);
  reset_text_color();

  // print measurement mode
  display.setCursor(0, 24);
  invert_text_if_selected(SEL_MODE);
  display.print(mode_labels[mode]);
  reset_text_color();
  display.print(":");

  // print dataset size
  display.setCursor(25, 24);
  invert_text_if_selected(SEL_AVG);
  if(avg_values[averaging] < 1000) {
    display.print(avg_values[averaging]);
  }
  else {
    display.print(avg_values[averaging]/1000);
    display.print("K");
  }
  reset_text_color();

  if(!calibration_mode) {
    // print attenuator value
    display.setCursor(48, 24);
    display.print("ATT: ");
    invert_text_if_selected(SEL_ATT);
    display.print(att);
    reset_text_color();
  }

  if(calibration_mode) {
    // change calibration level
    display.setCursor(48, 24);
    invert_text_if_selected(SEL_CAL_LEVEL);
    display.print("LV: ");
    display.print(cal_level == 0 ? CAL_LOW : CAL_UP);
    reset_text_color();
  
    // save calibration
    display.setCursor(104, 24);
    invert_text_if_selected(SEL_SAVE);
    display.print("SAVE");
    reset_text_color();
  }

  display.display();
}

void save_calibration() {
  EEPROM.write(band*4 + cal_level*2, (display_value>>8));
  EEPROM.write(band*4 + cal_level*2 + 1, (display_value&0xff));
}

void set_action() {
  if(current_sel == SEL_MODE) mode = (mode+1)%MODES;
  if(current_sel == SEL_BAND) band = (band+1)%BANDS;
  if(current_sel == SEL_AVG) averaging = (averaging+1)%AVERAGE_MODES;
  if(current_sel == SEL_ATT) att = (att + 10)%MAX_ATT;
  if(current_sel == SEL_UNITS) units = (units+1)%UNITS_AVAIL;
  if(current_sel == SEL_CAL_LEVEL) cal_level = (cal_level+1)%2;
  if(current_sel == SEL_SAVE) save_calibration();
}

void select_action() {
  if(calibration_mode) {
    current_sel_cal = (current_sel_cal+1)%SELECTIONS_CAL;
    current_sel = calibration_selections[current_sel_cal];
  }
  else {
    current_sel = (current_sel + 1) % SELECTIONS_AVAIL;    
  }
}

void print_calibration() {
  Serial.println("Calibration...");
  Serial.print("Lower point: ");
  Serial.println(CAL_LOW);
  Serial.print("Upper point: ");
  Serial.println(CAL_UP);
  
  for(uint8_t i = 0; i<BANDS; i++) {
    Serial.print(band_labels[i]);
    Serial.print(" L: ");
    Serial.print(cal_lower[i]);
    Serial.print(" H: ");
    Serial.println(cal_upper[i]);
  }
  
}

void serial_parse() {
  k = Serial.read();
  if(k=='m') mode = (mode+1)%MODES;
  if(k=='b') band = (band+1)%BANDS;
  if(k=='a') averaging = (averaging+1)%AVERAGE_MODES;
  if(k=='t') att = (att + 10)%MAX_ATT;
  if(k=='p') print_calibration();

  // simulation of physical buttons
  if(k==',') {
    select_action();
  }
  if(k=='.') {
    set_action();
  }
}

uint8_t handle_buttons() {
  if(!digitalRead(BTN_SET)) {
    set_action();
    while(!digitalRead(BTN_SET)) {}
    delay(5);
  }
  else if(!digitalRead(BTN_SELECT)) {
    select_action();
    while(!digitalRead(BTN_SELECT)) {}
    delay(5);
  }
  else {
    return 0; // return 0 if no key was pressed
  }

  return 1; // return 1 - key was pressed
}

void loop() {

  cumulative = 0;
  lowest = 1024;
  highest = 0;

  // read ADC samples, capture min&max
  for(int x=0; x<avg_values[averaging]; x++) {
    r = analogRead(AD8317_INPUT);
    cumulative += r;
    if(r < lowest) lowest = r;
    if(r > highest) highest = r;
    if(x&0xff == 0xff && Serial.available()) {
      serial_parse(); // check serial commands every 256 - reduce lag
      display_all(display_value); // update the display
    }
    if(x&0xff == 0xff) {  // check buttons state every 256 adc loop
      if(handle_buttons()){
        display_all(display_value);
      }
    }
  }

  // control over serial port
  if(Serial.available()) {
    serial_parse();
  }

  // check buttons
  handle_buttons();


  switch(mode) {
    case AVG:
      display_value = cumulative/avg_values[averaging];
      break;
    case MAX:
      display_value = lowest;
      break;
    case MIN:
      display_value = highest;
      break;
  }

  display_all(display_value);
}

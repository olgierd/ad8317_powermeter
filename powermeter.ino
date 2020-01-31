#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DEFAULT_MODE 0
#define DEFAULT_AVERAGING 6
#define DEFAULT_BAND 3

#define AD8317_INPUT A0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define AVG 0
#define MAX 1
#define MIN 2

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
enum btn_select{SEL_UNITS, SEL_BAND, SEL_MODE, SEL_AVG, SEL_ATT, SEL_NONE};

int lowest, highest, r, mode=DEFAULT_MODE, averaging=DEFAULT_AVERAGING, band=DEFAULT_BAND, att, current_sel=SEL_NONE, display_value;
uint32_t cumulative;
char k;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  // power to the OLED @ pin D2
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  delay(200);

  analogReference(EXTERNAL);
  Serial.begin(115200);
  Serial.println();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  prepare_calib();
}

// prepare "a" & "b" coefficients for calibration
void prepare_calib() {
  for(int i=0; i<BANDS; i++) {
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
  display.print(get_dbm(val), 1);

  // print units
  display.setTextSize(2);
  display.setCursor(90, 0);
  invert_text_if_selected(SEL_UNITS);
  display.print("dBm");
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

  // print attenuator value
  display.setCursor(48, 24);
  display.print("ATT: ");
  invert_text_if_selected(SEL_ATT);
  display.print(att);
  reset_text_color();

  display.display();
}

void serial_parse() {
  k = Serial.read();
  if(k=='m') mode = (mode+1)%MODES;
  if(k=='b') band = (band+1)%BANDS;
  if(k=='a') averaging = (averaging+1)%AVERAGE_MODES;
  if(k=='t') att = (att + 10)%MAX_ATT;
  if(k==',') {
    // simluation of physical button #1 - "SELECT"
    current_sel = (current_sel + 1) % SELECTIONS_AVAIL;
  }
  if(k=='.') {
    // simluation of physical button #2 - "SET"
    if(current_sel == SEL_MODE) mode = (mode+1)%MODES;
    if(current_sel == SEL_BAND) band = (band+1)%BANDS;
    if(current_sel == SEL_AVG) averaging = (averaging+1)%AVERAGE_MODES;
    if(current_sel == SEL_ATT) att = (att + 10)%MAX_ATT;
  }
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
  }

  // control over serial port
  if(Serial.available()) {
    serial_parse();
  }

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

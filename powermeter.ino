#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DEFAULT_MODE 1
#define DEFAULT_AVERAGING 6
#define DEFAULT_BAND 3

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define AVG 0
#define MAX 1
#define MIN 2

#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#define MAX_ATT 70

#define MODES 3
char *mode_labels[] = { "AVG", "MAX", "MIN" };

#define BANDS 9
char *band_labels[] = { "10M", "20M", "50M", "144", "430", "1G2", "2G3", "5G", "10G" };
uint16_t cal_40dbm[]= { 600,   600,   600,   600,   600,   600,   600,   600,  600   };
uint16_t cal_10dbm[]= { 225,   225,   225,   225,   225,   225,   225,   225,  225   };
double cal_a[] =      { 0,     0,     0,     0,     0,     0,     0,     0,    0     };
double cal_b[] =      { 0,     0,     0,     0,     0,     0,     0,     0,    0     };

#define AVERAGE_MODES 10
uint16_t avg_values[] = { 1, 5, 10, 50, 100, 500, 1000, 2000, 5000, 10000};

int lowest, highest, r, mode=DEFAULT_MODE, averaging=DEFAULT_AVERAGING, band=DEFAULT_BAND, att;
uint32_t cumulative;

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
    cal_a[i] = -30.0/(cal_40dbm[i]-cal_10dbm[i]);
    cal_b[i] = -((double)cal_40dbm[i]*cal_a[i]+40);
  }
}

// return value in dBm incl. calibration
double get_dbm(int readout) {
  return cal_a[band]*readout + cal_b[band] + att;
}

void display_all(int val) {
  display.clearDisplay();

  // print measured value
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(get_dbm(val), 1);

  // print units (todo: mW + Watts)
  display.setTextSize(2);
  display.setCursor(90, 0);
  display.print("dBm");

  // print measurement mode
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(mode_labels[mode]);
  display.print(":");

  // print dataset size
  display.setCursor(25, 24);
  if(avg_values[averaging] < 1000) {
    display.print(avg_values[averaging]);
  }
  else {
    display.print(avg_values[averaging]/1000);
    display.print("K");
  }

  // print attenuator value
  display.setCursor(48, 24);
  display.print("ATT: ");
  display.print(att);

  // print band label
  display.setCursor(110, 16);
  display.print(band_labels[band]);
      
  display.display();
}



void loop() {

  cumulative = 0;
  lowest = 1024;
  highest = 0;
  
  // read ADC samples, capture min&max
  for(int x=0; x<avg_values[averaging]; x++) {
    r = analogRead(A0);
    cumulative += r;
    if(r < lowest) lowest = r;
    if(r > highest) highest = r;
  }

  // control over serial port - TODO: buttons!
  if(Serial.available()) {
    char k = Serial.read();
    if(k=='m') mode = (mode+1)%MODES;
    if(k=='b') band = (band+1)%BANDS;
    if(k=='a') averaging = (averaging+1)%AVERAGE_MODES;
    if(k=='t') att = (att + 10)%MAX_ATT;
  }

  switch(mode) {
    case AVG:
      display_all(cumulative/avg_values[averaging]);
      break;
    case MAX:
      display_all(lowest);
      break;
    case MIN:
      display_all(highest);
      break;
  }
  
  
}
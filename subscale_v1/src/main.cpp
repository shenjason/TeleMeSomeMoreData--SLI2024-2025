#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SGP30.h"
#include <SD.h>
#include <SPI.h>

#define BUZZER 23

Adafruit_SGP30 sgp;

char segment[50];
char segment_f[50];
bool newsegment;
double gpsLat;
double gpsLong;
char gpsLatDir;
char gpsLongDir;
bool fix; bool isMoxData;
int seg_i;
int nofixticks;
int fixticks;
int noparseticks;
float TVOC; float eCO2;

// Forward declarations
void UpdateMox();
void UpdateGPS();
bool parseSegment(char newchar);
bool parseGPSNMEA();
void beep(float seconds);

File dataFile;


void setup() {
  pinMode(BUZZER, OUTPUT);
  Serial.begin(115200);
  Serial3.begin(115200);

  beep(2);

  // Serial3.println("freset");
  // Serial3.println("unmask GLO");
  // Serial3.println("unmask GAL");
  // Serial3.println("unmask BDS");
  // Serial3.println("unmask GPS");
  // Serial3.println("CONFIG COM1 115200 8 n 1");
  // Serial3.println("GPGGA 0.05");
  // Serial3.println("saveconfig");

  if (!sgp.begin()){
    delay(1000);
    beep(0.1);
    while (true) {};
  }

  // if (!SD.begin(BUILTIN_SDCARD)){
  //   delay(1000);
  //   beep(0.1);
  //   delay(300);
  //   beep(0.1);
  //   while (true) {};
  // }

  // dataFile = SD.open("Data.csv", FILE_WRITE);
  // dataFile.print("Latitude(ddeg)");
  // dataFile.print(", ");
  // dataFile.print("Longitude(ddeg)");
  // dataFile.print(", ");
  // dataFile.print("TVOC(ppb\t)");
  // dataFile.print(", ");
  // dataFile.println("eCO2(ppm)");

  // dataFile.close();

  delay(1000);
  beep(0.1);
  delay(300);
  beep(0.1);
  delay(300);
  beep(0.1);

  newsegment = false;
  nofixticks = 0;
  fixticks = 0;
  noparseticks = 0;
  seg_i = 0;
  isMoxData = false;
}

void loop() {
  char read = Serial3.read();
  Serial.print(read);
  UpdateGPS();
  UpdateMox();

  if (nofixticks > 80){
    beep(0.1);
    nofixticks = 0;
  }

  if (fixticks > 200){
    beep(0.1);
    delay(100);
    beep(0.1);
    fixticks = 0;
  }

  if (noparseticks > 80){
    beep(0.5);
    noparseticks = 0;
  }

  if (fix || isMoxData){
    dataFile = SD.open("Data.csv", FILE_WRITE);
    if (fix){
      dataFile.print(gpsLat);
      dataFile.print(", ");
      dataFile.print(gpsLong);
      dataFile.print(", ");
    }else{
      dataFile.print("N/A, N/A, ");
    }

    if (isMoxData){
        dataFile.print(TVOC);
        dataFile.print(", ");
        dataFile.print(eCO2);
    }else{
      dataFile.print("N/A, N/A");
    }

    dataFile.close();
  }

}

void UpdateMox(){
  isMoxData = false;
  if  (!sgp.IAQmeasure()) return;
  isMoxData = true;

  TVOC = sgp.TVOC;
  eCO2 = sgp.eCO2;
}



void UpdateGPS(){
  if (!Serial3.available()) return;
  char read = Serial3.read();
  bool done = parseSegment(read);
  if (done){
    noparseticks = 0;
    Serial.print(segment_f);
    if (parseGPSNMEA()){
      if (!fix){
        nofixticks ++;
        return;
      }
      fixticks ++;
    }else{
      noparseticks ++;
    }
    return;
  }
  noparseticks ++;
  return;
}

bool parseSegment(char newchar){
  if (newchar == '$') {
    seg_i = 0;
    newsegment = true;
    memset(segment, 0, sizeof(segment));
    return false;
  }
  if (!newsegment) return false;
  if (newchar == '\r'){
    segment[seg_i] = '\n';
    strcpy(segment_f, segment);
    newsegment = false;
    return true;
  }
  segment[seg_i] = newchar;
  seg_i++;
  return false;
}

bool parseGPSNMEA(){
  char part[15];
  int j = 0;
  for (int i=0; i<7; i++){
    int k = 0;
    while (segment_f[j] != ',' && segment_f[j] != '*'){
      part[k] = segment_f[j];
      k++;
      j++;
    }

    if (i == 0 && (part[0] != 'G' || part[1] != 'N' || part[2] != 'G' || part[3] != 'G' || part[4] != 'A')) return false;
    if (i == 1 && part[0] == '\0') return false;
    if (i == 6){
      fix = true;
      if (part[0] == '0') fix = false;
    }

    // if (i == 2 && part[0] != '\0'){
    //   gpsLat = atoi(part[0] + part[1]) + (atof(part[2] + part[3] + part[4] + part[5] + part[6]) / 60.0);
    // }
    // if (i == 4 && part[0] != '\0'){
    //   gpsLong = atoi(part[0] + part[1] + part[2]) + (atof(part[3] + part[4] + part[5] + part[6] + part[7]) / 60.0);
    // }
    // if (i == 3 && part[0] != '\0'){
    //   gpsLatDir = part[0];
    // }
    // if (i == 5 && part[0] != '\0'){
    //   gpsLongDir = part[0];
    // }
    memset(part, 0, sizeof(part));
    if (segment_f[j] == '*') break;
    j++;
  }
  return true;
}


void beep(float seconds){
  digitalWrite(BUZZER, HIGH);
  delay(seconds*1000);
  digitalWrite(BUZZER, LOW);
}

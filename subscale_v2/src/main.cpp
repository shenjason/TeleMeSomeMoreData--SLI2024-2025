#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SGP30.h"
#include <SD.h>
#include <SPI.h>

#define BUZZER 23 //Buzzer Output Pin

Adafruit_SGP30 sgp; //Mox Sensor


char segment[300]; char segment_f[300];

// Data Variables
elapsedMillis timeElapsed;
double gpsLat; double gpsLong;//GPS Lattitude and Longitude in Decimal Degrees
char gpsLatDir; char gpsLongDir;//Direction of GPS N/S E/W
bool fix; bool isMoxData;// GPS fix and Mox data availibility
float TVOC; float eCO2; //Mox Sensor Data: TVOC - Total Volatile Organic compounds(ppb\t), eCO2 - Equivalent CO2(ppm)

bool newsegment;
int seg_i;

// Tick Variables for timing
int nofixticks; int fixticks; int noparseticks; int moxTicks;
long prevTime;

// Forward declarations
void UpdateMox();
void UpdateGPS();
bool parseSegment(char newchar);
bool parseGPSNMEA();
void beep(float seconds);

File dataFile; // SD card file


void setup() {
  // Start Serial connection
  Serial.begin(115200);
  Serial3.begin(115200); //GPS Serial Connection
  pinMode(BUZZER, OUTPUT);

  // Initial
  beep(2);

  // GPS Initialization Commands

  // Uncomment next line to reset GPS to default settings
  // Serial3.println("freset");
  // For more commands vist
  // https://drive.google.com/drive/folders/1RP-g-l8X-KwJOpTcQYS-juQpOMaA7P9b
  Serial3.println("unmask GLO");
  Serial3.println("unmask GAL");
  Serial3.println("unmask BDS");
  Serial3.println("unmask GPS");
  Serial3.println("CONFIG COM1 115200 8 n 1"); //Port config
  Serial3.println("GPGGA 0.05"); // Output GPGGA data at 50 hertz
  Serial3.println("saveconfig");


  //Initialize Mox Sensor
  if (!sgp.begin()){
    delay(1000);
    beep(0.1);
    while (true) {};
  }

  //Initialize SD card
  if (!SD.begin(BUILTIN_SDCARD)){
    delay(1000);
    beep(0.1);
    delay(300);
    beep(0.1);
    while (true) {};
  }

  dataFile = SD.open("Data.csv", FILE_WRITE);

  // Write Header: Time Elapsed(miliseconds), Latitude(ddeg), Longitude(ddeg), TVOC(ppb\t), eCO2(ppm)
  dataFile.print("Time Elapsed(miliseconds)");
  dataFile.print(", ");
  dataFile.print("Latitude(ddeg)");
  dataFile.print(", ");
  dataFile.print("Longitude(ddeg)");
  dataFile.print(", ");
  dataFile.print("TVOC(ppb\t)");
  dataFile.print(", ");
  dataFile.println("eCO2(ppm)");

  dataFile.close();

  // 3 quick beeps for initialization confirmation
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
  moxTicks = 0;
  timeElapsed = 0;
  prevTime = 0;
}

void loop() {

  UpdateGPS();

  /* For some reason updating the mox sensor messes up the GPS serial communication and due to limited time
    I was unable to fix it, and since the GPS data is more important I decided to disable the mox sensor
  */
  // UpdateMox();

  if (nofixticks > 80){ // If GPS has nofix
    // Signle short beep
    beep(0.1);
    nofixticks = 0;
  }


  if (fixticks > 200){ // If GPS has fix
    // Double short beep
    beep(0.1);
    delay(100);
    beep(0.1);
    fixticks = 0;
  }

  if (noparseticks > 80){ // If no available or vaild GPS data for a while
    beep(0.5); // Long Beep
    noparseticks = 0;
  }

  if ((fix || isMoxData) && (timeElapsed - prevTime) > 1000){
    prevTime = timeElapsed;
    dataFile = SD.open("Data.csv", FILE_WRITE);
    dataFile.print(timeElapsed);
    dataFile.print(", ");
    if (fix){
      dataFile.print(gpsLat, 7);
      dataFile.print(", ");
      dataFile.print(gpsLong, 7);
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

    dataFile.println();

    dataFile.close();
  }

}

void UpdateMox(){
  isMoxData = false;
  moxTicks++;
  if (moxTicks < 1000) return;
  if  (!sgp.IAQmeasure()) return;
  moxTicks = 0;
  isMoxData = true;

  TVOC = sgp.TVOC;
  eCO2 = sgp.eCO2;
  // Serial.print(TVOC);
  // Serial.print(", ");
  // Serial.print(eCO2);
  // Serial.println();
}



void UpdateGPS(){
  if (Serial3.available() == 0) return;

  char read = Serial3.read();
  // Serial.print(read);
  bool done = parseSegment(read);
  if (done){
    noparseticks = 0;
    // Serial.println(segment_f);
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
    newsegment = false;
    if (seg_i > 100) return false;
    segment[seg_i] = '\n';
    strcpy(segment_f, segment);
    return true;
  }
  segment[seg_i] = newchar;
  seg_i++;

  return false;
}

bool parseGPSNMEA(){
  char* part = strtok(segment_f, ",");
  if (strcmp(part, "GNGGA") != 0) return false;

  int index = 1;
  while (part != NULL){
    part = strtok(NULL, ",");
    if (part == NULL) break;
    // Serial.println(part);
    if (index == 6){
      fix = true;
      if (part[0] == '0') fix = false;
    }
    if (index == 2){
      if (strlen(part) < 8) continue;
      String latMin = ""; String latDeg = "";
      latDeg += part[0]; latDeg += part[1];
      latMin += part[3]; latMin += part[4]; latMin += part[5]; latMin += part[6]; latMin += part[7];

      gpsLat = latDeg.toInt() + (latMin.toFloat() / 60.0);
    }
    if (index == 4){
      if (strlen(part) < 9) continue;
      String longMin = ""; String longDeg = "";
      longDeg += part[0]; longDeg += part[1]; longDeg += part[2];
      longMin += part[4]; longMin += part[5]; longMin += part[6]; longMin += part[7]; longMin += part[8];

      gpsLong = longDeg.toInt() + (longMin.toFloat() / 60.0);
    }
    if (index == 3){
      if (strlen(part) < 1) continue;
      gpsLatDir = part[0];
    }
    if (index == 5){
      if (strlen(part) < 1) continue;
      gpsLongDir = part[0];
    }
    index++;
  }
  Serial.println(gpsLat, 8);
  Serial.println(gpsLong, 8);
  return true;
}


void beep(float seconds){
  digitalWrite(BUZZER, HIGH);
  delay(seconds*1000);
  digitalWrite(BUZZER, LOW);
}

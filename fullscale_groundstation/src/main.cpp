#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GPS.h>
#include <AccelStepper.h>
#include <AccelStepperWithDistance.h>
#include <math.h>
#include <RH_RF95.h>

char header[] = "HEAD";


#define RADIOFREQ 446.5
#define BUFFERMAXSIZE 120

#define STEPANGLE 0.45
#define MAXSPEED 90
#define MAXACCEL 120

float TARGETLAT = 33.64243218830173;
float TARGETLONG = -117.61880958059619;
float ATTENNALAT = 33.64248538632342;
float ATTENNALONG = -117.61828493443528;


#define GPSSerial Serial7

RH_RF95 RFM(24, 25);
AccelStepperWithDistance stepper1(AccelStepper::FULL4WIRE, 2, 3, 4, 5);
AccelStepperWithDistance stepper2(AccelStepper::FULL4WIRE, 9, 10, 11, 12);
Adafruit_GPS GPS(&GPSSerial);


float deltaX;
float deltaY;

float deltaZ;
float initialZ = 0;

bool TrackingMode = false;

float TargetAzimuthAngle = 0;
float TargetAltitudeAngle = 0;

// Forward declarations
void receiveData();
void receiveCommands();
void stepperSetup();
float getCurrentAngle1();
float getCurrentAngle2();
int angleToStep(float angle);
float stepToAngle(int step);
bool UpdateGPS();
float calculateAzimuthAngle(float lat1, float long1, float lat2, float long2);
float calculateAltitudeAngle(float lat1, float long1, float lat2, float long2);
void getDeltas(float lat1, float long1, float lat2, float long2);
float Distance(float lat1, float lon1, float lat2, float lon2);
float ParseBufferFLOAT(uint8_t buffer[], int bufferparseindex);
int ParseBufferINT(uint8_t buffer[], int bufferparseindex);
unsigned int ParseBufferUINT(uint8_t buffer[], int bufferparseindex);
uint8_t ParseBuffer(uint8_t buffer[], int bufferparseindex);

union Convert{
  int i;
  unsigned int ui;
  float f;
  uint8_t b[4];
};

void setup()
{
    Serial.begin(57600);
    pinMode(6, OUTPUT);
    digitalWrite(6, LOW);
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    stepperSetup();

    if (!RFM.init()){
      Serial.println("Radio init failed");
      while (1) {};
    }
    if (!RFM.setFrequency(RADIOFREQ)){
      Serial.println("setFrequency failed");
      while (1) {};
    }

    bool fix = false;
    while (!fix){
      if (UpdateGPS()){
        fix = GPS.fix;
      }
    }
    ATTENNALAT = GPS.latitudeDegrees;
    ATTENNALONG = GPS.longitudeDegrees;

}

void loop()
{
  receiveData();

  if (TrackingMode){
    stepper1.moveToAngle(calculateAltitudeAngle(ATTENNALAT, ATTENNALONG, TARGETLAT, TARGETLONG));
    stepper2.moveToAngle(calculateAzimuthAngle(ATTENNALAT, ATTENNALONG, TARGETLAT, TARGETLONG));
  }else{
    stepper1.moveToAngle(TargetAltitudeAngle);
    stepper2.moveToAngle(TargetAzimuthAngle);
  }


  stepper1.run();
  stepper2.run();
}

void receiveData(){
  if (RFM.available()){
    uint8_t buffer[BUFFERMAXSIZE];
    uint8_t blen = sizeof(buffer);
    if (RFM.recv(buffer, &blen)){
      if ((char)buffer[0] != header[0]) return;
      if ((char)buffer[1] != header[1]) return;
      if ((char)buffer[2] != header[2]) return;
      if ((char)buffer[3] != header[3]) return;

      Serial.print("pn");
      Serial.print(ParseBufferUINT(buffer, 0));
      Serial.print(",et");
      Serial.print(ParseBufferUINT(buffer, 1) / 1000.0, 3);
      Serial.print(",gw");
      Serial.print(ParseBufferFLOAT(buffer, 2));
      Serial.print(",gx");
      Serial.print(ParseBufferFLOAT(buffer, 3));
      Serial.print(",gy");
      Serial.print(ParseBufferFLOAT(buffer, 4));
      Serial.print(",gz");
      Serial.print(ParseBufferFLOAT(buffer, 5));
      Serial.print(",mx");
      Serial.print(ParseBufferFLOAT(buffer, 6));
      Serial.print(",my");
      Serial.print(ParseBufferFLOAT(buffer, 7));
      Serial.print(",mz");
      Serial.print(ParseBufferFLOAT(buffer, 8));
      Serial.print(",ax");
      Serial.print(ParseBufferFLOAT(buffer, 9));
      Serial.print(",ay");
      Serial.print(ParseBufferFLOAT(buffer, 10));
      Serial.print(",az");
      Serial.print(ParseBufferFLOAT(buffer, 11));
      Serial.print(",al");
      float alti = ParseBufferFLOAT(buffer, 12);
      if (initialZ == 0){
        initialZ = alti;
      }
      deltaZ = alti - initialZ;
      Serial.print(alti);
      Serial.print(",pr");
      Serial.print(ParseBufferFLOAT(buffer, 13));
      Serial.print(",hu");
      Serial.print(ParseBufferFLOAT(buffer, 14));
      Serial.print(",tp");
      Serial.print(ParseBufferFLOAT(buffer, 15));
      Serial.print(",la");
      TARGETLAT = ParseBufferFLOAT(buffer, 16);
      Serial.print(TARGETLAT, 7);
      Serial.print(",lo");
      TARGETLONG = ParseBufferFLOAT(buffer, 17);
      Serial.print(TARGETLONG, 7);
      Serial.print(",ga");
      Serial.print(ParseBufferFLOAT(buffer, 18));
      Serial.print(",sn");
      Serial.print(ParseBuffer(buffer, 19));
      Serial.println();
    }
  }
}

void receiveCommands(){
  if (Serial.available()){
    char command = Serial.read();
    if (command == '0'){
      TrackingMode = false;
      TargetAzimuthAngle = 0;
      TargetAltitudeAngle = 0;
    }
    if (command == '1'){
      TrackingMode = true;
    }
    if (command == 'a'){
      TargetAzimuthAngle -= 10;
    }
    if (command == 'd'){
      TargetAzimuthAngle += 10;
    }
    if (command == 's'){
      TargetAltitudeAngle -= 10;
    }
    if (command == 'w'){
      TargetAltitudeAngle += 10;
    }


  }
}

void stepperSetup(){
  stepper1.setStepsPerRotation((int)round(360.0 / STEPANGLE));
  stepper2.setStepsPerRotation((int)round(360.0 / STEPANGLE));
  stepper1.setMaxSpeed(angleToStep(MAXSPEED));
  stepper2.setMaxSpeed(angleToStep(MAXSPEED*2));
  stepper1.setAcceleration(angleToStep(MAXACCEL));
  stepper2.setAcceleration(angleToStep(MAXACCEL*4));
}

float getCurrentAngle1(){
  return stepToAngle(stepper1.currentPosition());
}

float getCurrentAngle2(){
  return stepToAngle(stepper2.currentPosition());
}

int angleToStep(float angle){
  return (int)round(angle / STEPANGLE);
}

float stepToAngle(int step){
  return step * STEPANGLE;
}


bool UpdateGPS(){
  GPS.read();
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))
      return false;
    return true;
  }else{
    return false;
  }
}


float calculateAzimuthAngle(float lat1, float long1, float lat2, float long2){
  getDeltas(lat1, long1, lat2, long2);
  return atan2(deltaX, deltaY) * RAD_TO_DEG;
}

float calculateAltitudeAngle(float lat1, float long1, float lat2, float long2){
  float distance = Distance(lat1, long1, lat2, long2);
  return atan2(distance, deltaZ) * RAD_TO_DEG;
}


void getDeltas(float lat1, float long1, float lat2, float long2){
  int mdx = 1;
  int mdy = 1;
  if (lat2 > lat1)
  {
    mdx = -1;
  }
  if (long2 >long1)
  {
    mdy = -1;
  }

  deltaX = mdx * Distance(lat1, long1, lat1, long2);
  deltaY= mdy * Distance(lat1, long1, lat2, long1);
}

float Distance(float lat1, float lon1, float lat2, float lon2)
{
  float R = 2.093e7f;
  float lat1r = DEG_TO_RAD * lat1;
  float lat2r = DEG_TO_RAD * lat2;
  float dlat = (lat2 - lat1) * DEG_TO_RAD;
  float dlon = (lon2 - lon1) * DEG_TO_RAD;
  float a = ( sin(dlat / 2) * sin(dlat / 2) ) + cos(lat1r) * cos(lat2r) * ( sin(dlon / 2) * sin(dlon / 2) );
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));

  return R * c;
}


float ParseBufferFLOAT(uint8_t buffer[], int bufferparseindex){
  Convert c;
  for (int i=0; i<4; i++){
    int cbufferindex = (bufferparseindex+1) * 4 + i;
    c.b[i] = buffer[cbufferindex];
  }
  return c.f;
}

int ParseBufferINT(uint8_t buffer[], int bufferparseindex){
  Convert c;
  for (int i=0; i<4; i++){
    int cbufferindex = (bufferparseindex+1) * 4 + i;
    c.b[i] = buffer[cbufferindex];
  }
  return c.i;
}

unsigned int ParseBufferUINT(uint8_t buffer[], int bufferparseindex){
  Convert c;
  for (int i=0; i<4; i++){
    int cbufferindex = (bufferparseindex+1) * 4 + i;
    c.b[i] = buffer[cbufferindex];
  }
  return c.ui;
}

uint8_t ParseBuffer(uint8_t buffer[], int bufferparseindex){
  return buffer[(bufferparseindex+1) * 4];
}

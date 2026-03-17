#include <Arduino.h>
#include <Adafruit_GPS.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <Adafruit_BME280.h>
#include <RH_RF95.h>
#include <SD.h>


union Convert{
  int i;
  unsigned int ui;
  float f;
  uint8_t b[4];
};

#define BUZZER 8

#define RADIOFREQ 446.5
#define BUFFERMAXSIZE 82
#define SEALEVELPRESSURE_HPA (1013)


#define GPSSerial Serial7

Adafruit_GPS GPS(&GPSSerial);
Adafruit_BNO055 bno = Adafruit_BNO055(55);
RH_RF95 RFM(24, 25);
Adafruit_BME280 bme;

// Forward declarations
bool UpdateGPS();
bool UpdateBNO();
bool UpdateBME();
void AppendToBufferINT(int data);
void AppendToBufferSTR(char data[], int size);
void AppendToBufferFLOAT(float data);
void AppendToBufferUINT(unsigned int data);
float readTemperature();
void Buzz(int duration, int freq);

File Log;
char header[] = "HEAD";

float RotX = 0;
float RotY = 0;
float RotZ = 0;

float MagX = 0;
float MagY = 0;
float MagZ = 0;

float AccelX = 0;
float AccelY = 0;
float AccelZ = 0;

float Alti = 0;
float Pressure = 0;
float Humidity = 0;
float Temp = 0;
unsigned int packetnum = 0;

uint8_t Buffer[BUFFERMAXSIZE];
int bufferappendindex = 0;
bool addtochecksum = false;
unsigned int checksum = 0;

elapsedMillis sinceBNO;
elapsedMillis ElapasedTime;
elapsedMillis sinceBME;

bool gpsstatus = false;
bool bnostatus = false;
bool bmestatus = false;

void setup() {
  Serial.begin(9600);

  pinMode(A13, INPUT);
  analogReadResolution(12);

  //Radio
  Buzz(1000, 400);
  Serial.println("Radio init started...");

  if (!RFM.init()){
    Buzz(3000, 200);
    Serial.println("Radio init failed");
    while (1) {};
  }
  if (!RFM.setFrequency(RADIOFREQ)){
    Buzz(3000, 200);
    Serial.println("setFrequency failed");
    while (1) {};
  }
  RFM.setModeTx();
  RFM.setTxPower(17);
  // RFM.setSignalBandwidth(500);
  // RFM.setPayloadCRC(true);


  Buzz(2000, 600);
  Serial.println("Radio init done");

  //BME280
  Buzz(1000, 400);
  Serial.println("BME280 init started...");
  if (!bme.begin()){
    Buzz(3000, 200);
    Serial.println("BME280 init failed");
    while (1) {};
  }
  Buzz(2000, 600);
  Serial.println("BME280 init done");

  //GPS

  Buzz(1000, 400);
  Serial.println("GPS init started...");
  GPSSerial.begin(9600);


  Serial.println("Waiting For GPS fix...");
  bool fix = false;
  while (!fix){
    if (UpdateGPS()){
      fix = GPS.fix;
    }
  }
  Buzz(2000, 600);
  Serial.print("GPS Fix done with ");
  Serial.print(GPS.satellites);
  Serial.println(" satellites");

  //SD
  Buzz(1000, 400);
  Serial.println("SD init started...");
  if (!SD.begin(BUILTIN_SDCARD)){
    Buzz(3000, 200);
    Serial.println("SD init failed");
    while (1) {};
  }

  Log = SD.open("Data.csv", FILE_WRITE);
  Log.println("Packet#,Time(ms),RotW,RotX,RotY,RotZ,MagX(tesla),MagY(tesla),MagZ(tesla),AccelX(m/s^2),AccelY(m/s^2),AccelZ(m/s^2),Altitude(m),Pressure(hpa),Humidity(%),Temp(C),Lat(deg),Long(deg),GPSAltitude(cm),Satellites");
  Log.close();

  Buzz(2000, 600);

  Serial.print("SD card init done");

  //BNO
  Buzz(1000, 400);
  Serial.println("BNO init started...");
  if (!bno.begin()){
    Buzz(3000, 200);
    Serial.println("BNO init failed");
    while (1);
  }
  Buzz(2000, 600);
  Serial.println("BNO init done");

  Serial.println("BNO Calibration");

  uint8_t sys, gyro, accel, mg = 0;
  bno.getCalibration(&sys, &gyro, &accel, &mg);
  Serial.print(sys);
  Serial.print(", ");
  Serial.print(gyro);
  Serial.print(", ");
  Serial.print(accel);
  Serial.print(", ");
  Serial.println(mg);
  bno.setExtCrystalUse(true);


  memset(Buffer, 0, sizeof(Buffer)); // Clear Buffer
  bufferappendindex = 0;
  AppendToBufferSTR("AIAA OC SLI FROM KO4MSQ", 23);
  RFM.send(Buffer, sizeof(Buffer));
  RFM.waitPacketSent();

  Serial.println("Init Complete");
  Buzz(3000, 600);


  packetnum = 0;
  bufferappendindex = 0;
  ElapasedTime = 0;
  sinceBNO = 0;
  sinceBME = 0;
  gpsstatus = false;
  bnostatus = false;
  bmestatus = false;
}

void loop() {
  gpsstatus = (UpdateGPS()) || gpsstatus;
  bnostatus = (UpdateBNO()) || bnostatus;
  bmestatus = (UpdateBME()) || bmestatus;
  bool isdata = gpsstatus && bnostatus && bmestatus;


  if (isdata){
    memset(Buffer, 0, sizeof(Buffer)); // Clear Buffer
    Log = SD.open("Data.csv", FILE_WRITE); //Open SD Card
    bufferappendindex = 0;
    checksum = 0;
    packetnum++;

    addtochecksum = false;
    AppendToBufferSTR(header, 4);
    addtochecksum = true;

    AppendToBufferUINT(packetnum);
    AppendToBufferUINT((unsigned int)ElapasedTime);

    Log.print(packetnum);
    Log.print(",");
    Log.print(ElapasedTime);
    Log.print(",");

    //BnoData
    AppendToBufferFLOAT(RotX);
    AppendToBufferFLOAT(RotY);
    AppendToBufferFLOAT(RotZ);

    Log.print(RotX);
    Log.print(",");
    Log.print(RotY);
    Log.print(",");
    Log.print(RotZ);
    Log.print(",");

    AppendToBufferFLOAT(MagX);
    AppendToBufferFLOAT(MagY);
    AppendToBufferFLOAT(MagZ);

    Log.print(MagX);
    Log.print(",");
    Log.print(MagY);
    Log.print(",");
    Log.print(MagZ);
    Log.print(",");

    AppendToBufferFLOAT(AccelX);
    AppendToBufferFLOAT(AccelY);
    AppendToBufferFLOAT(AccelZ);

    Log.print(AccelX);
    Log.print(",");
    Log.print(AccelY);
    Log.print(",");
    Log.print(AccelZ);
    Log.print(",");
    //BMEData
    AppendToBufferFLOAT(Alti);
    AppendToBufferFLOAT(Pressure);
    AppendToBufferFLOAT(Humidity);

    Log.print(Alti);
    Log.print(",");
    Log.print(Pressure);
    Log.print(",");
    Log.print(Humidity);
    Log.print(",");
    //Thermistor
    AppendToBufferFLOAT(Temp);

    Log.print(Temp);
    Log.print(",");
    //GPS
    AppendToBufferFLOAT(GPS.latitudeDegrees);
    AppendToBufferFLOAT(GPS.longitudeDegrees);
    AppendToBufferFLOAT(GPS.altitude);
    Buffer[bufferappendindex] = GPS.satellites;
    bufferappendindex++;
    checksum += GPS.satellites;

    Log.print(GPS.latitudeDegrees, 7);
    Log.print(",");
    Log.print(GPS.longitudeDegrees, 7);
    Log.print(",");
    Log.print(GPS.altitude);
    Log.print(",");
    Log.print(GPS.satellites);
    Log.println();

    Log.close();

    //CheckSum
    // addtochecksum = false;
    // AppendToBufferUINT(checksum);

    RFM.send(Buffer, sizeof(Buffer));
    RFM.waitPacketSent();


    gpsstatus = false;
    bnostatus = false;
    bmestatus = false;
  }

}


bool UpdateGPS(){
  GPSSerial.read();
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))
      return false;
    return true;
  }else{
    return false;
  }
}


bool UpdateBNO(){
  if (sinceBNO < 10) return false;

  sensors_event_t linearAccelData;
  imu::Vector<3> Rotation = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  bno.getEvent(&linearAccelData, Adafruit_BNO055::VECTOR_LINEARACCEL);
  imu::Vector<3> magnetometer = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);

  MagX = magnetometer.x();
  MagY = magnetometer.y();
  MagZ = magnetometer.z();

  RotX = Rotation.x();
  RotY = Rotation.y();
  RotZ = Rotation.z();

  AccelX = linearAccelData.acceleration.x;
  AccelY = linearAccelData.acceleration.y;
  AccelZ = linearAccelData.acceleration.z;

  sinceBNO = 0;
  return true;
}

bool UpdateBME(){
  if (sinceBME < 10) return false;
  Pressure = bme.readPressure();
  Humidity = bme.readHumidity();
  Alti = bme.readAltitude(SEALEVELPRESSURE_HPA);
  Temp = readTemperature();

  sinceBME = 0;
  return true;
}

void AppendToBufferINT(int data){
  Convert u;
  u.i = data;

  for (int i=0; i < 4; i++){
    Buffer[bufferappendindex] = u.b[i];
    if (addtochecksum) checksum += u.b[i];
    bufferappendindex ++;
  }
}

void AppendToBufferSTR(char data[], int size){
  for (int i=0; i < size; i++){
    Buffer[bufferappendindex] =  (uint8_t)data[i];
    if (addtochecksum) checksum += (uint8_t)data[i];
    bufferappendindex ++;
  }
}


void AppendToBufferFLOAT(float data){
  Convert u;
  u.f = data;

  for (int i=0; i < 4; i++){
    Buffer[bufferappendindex] = u.b[i];
    if (addtochecksum) checksum += u.b[i];
    bufferappendindex ++;
  }
}

void AppendToBufferUINT(unsigned int data){
  Convert u;
  u.ui = data;

  for (int i=0; i < 4; i++){
    Buffer[bufferappendindex] = u.b[i];
    if (addtochecksum) checksum += u.b[i];
    bufferappendindex ++;
  }
}

float readTemperature() {
  double R_NTC, log_NTC;
  uint16_t ARead = analogRead(A13);
  R_NTC = 4700*ARead/(4095.0-ARead);
  log_NTC = log(R_NTC/10000);

  // The line below is the Steinhart-Hart equation
  float temperature = 1/(3.354016E-3 + 2.569850E-4*log_NTC + 2.620131E-6*log_NTC*log_NTC + 6.383091E-8*log_NTC*log_NTC*log_NTC)-273.15;

  return temperature;
}


void Buzz(int duration, int freq){
  tone(BUZZER, freq, duration);
  delay(duration + 1000);
}

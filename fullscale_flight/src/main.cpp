#include <Adafruit_GPS.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <Adafruit_BME280.h>
#include <RH_RF95.h>


union Convert{
  int i;
  unsigned int ui;
  float f;
  uint8_t b[4];
};

#define BUZZER 8
#define TO_RADIANS 0.017453293

#define RADIOFREQ 446.5
#define BUFFERMAXSIZE 82
#define SEALEVELPRESSURE_HPA 1013


#define GPSSerial Serial7

/* The Header is a identifier when receiving a data packet 
if the packet doesn't start with header it is discarded */
const char header[] = "AIAA"; // MUST BE 4 CHARACTERS

RH_RF95 RFM(24, 25);

char segment[300]; char segment_f[300];

bool fix;
double gpsLat = 0; double gpsLong = 0;// GPS Lattitude and Longitude in Decimal Degrees
char gpsLatDir; char gpsLongDir;// Direction of GPS N/S E/W

bool newsegment; int seg_i;


Adafruit_BNO055 bno = Adafruit_BNO055(55);


Adafruit_BME280 bme;

float accelX; float accelY; float accelZ; float rotX; float rotY; float rotZ;

float Alti = 0;
float Pressure = 0;
float Humidity = 0;
float Temp = 0;
unsigned int packetnum = 0;

uint8_t Buffer[BUFFERMAXSIZE];
int bufferappendindex = 0;

bool addtochecksum = false; unsigned int checksum = 0;


elapsedMillis sinceBNO; elapsedMillis elapasedTime; elapsedMillis sinceBME;


bool gpsstatus = false; bool bnostatus = false; bool bmestatus = false;


void setup() {
  Serial.begin(9600);

  pinMode(A13, INPUT); analogReadResolution(12);
  
  // Radio
  Serial.println("Radio init started..."); Buzz(1000, 400);

  if (!RFM.init()){
    Serial.println("Radio init failed"); Buzz(3000, 200);
    while (1) {};
  }
  if (!RFM.setFrequency(RADIOFREQ)){
    Serial.println("setFrequency failed"); Buzz(3000, 200);
    while (1) {};
  }

  RFM.setModeTx(); RFM.setTxPower(17);
  Serial.println("Radio init done"); Buzz(2000, 600);
  

  //BME280
  Serial.println("BME280 init started..."); Buzz(1000, 400);
  if (!bme.begin()){
    Serial.println("BME280 init failed"); Buzz(3000, 200);
    while (1) {};
  }
  Serial.println("BME280 init done"); Buzz(2000, 600);

  // GPS Initialization
  Serial.println("GPS init started..."); Buzz(1000, 400);
  GPSSerial.begin(115200);

  // GPS Initialization Commands

  // Uncomment next line to reset GPS to default settings
  // GPSSerial.println("freset");
  // For more commands vist
  // https://drive.google.com/drive/folders/1RP-g-l8X-KwJOpTcQYS-juQpOMaA7P9b
  GPSSerial.println("unmask GLO");
  GPSSerial.println("unmask GAL");
  GPSSerial.println("unmask BDS");
  GPSSerial.println("unmask GPS");
  GPSSerial.println("CONFIG COM1 115200 8 n 1"); //Port config
  GPSSerial.println("GPGGA 0.05"); // Output GPGGA data at 50 hertz 
  GPSSerial.println("saveconfig");

  newsegment = false; seg_i = 0;


  Serial.println("GPS init done"); Buzz(2000, 600);

  while (!fix){
    UpdateGPS(true);
    char read;
    if (RFM.available()) RFM.recv(read, 1);
    if (read == 'k') break;
  }

  Serial.println("GPS fix done"); Buzz(2000, 600);

  // BNO Initialization 
  Serial.println("BNO init started..."); Buzz(1000, 400);
  if (!bno.begin()){
    Serial.println("BNO init failed"); Buzz(3000, 200);
    while (1);
  }
  Serial.println("BNO init done"); Buzz(2000, 600);
  
  bno.setExtCrystalUse(true);


  memset(Buffer, 0, sizeof(Buffer)); bufferappendindex = 0;
  
  AppendToBufferSTR("AIAA OC SLI FROM KO4MSQ", 23);
  RFM.send(Buffer, sizeof(Buffer));
  RFM.waitPacketSent();

  Serial.println("Init Complete"); Buzz(3000, 600);


  packetnum = 0;
  bufferappendindex = 0;
  elapasedTime = 0; sinceBNO = 0; sinceBME = 0;
  
  gpsstatus = false; bnostatus = false; bmestatus = false;
}


void loop() {
  gpsstatus = (UpdateGPS(false)) || gpsstatus;
  bnostatus = (UpdateBNO()) || bnostatus;
  bmestatus = (UpdateBME()) || bmestatus;
  bool isdata = bmestatus && bnostatus && gpsstatus;

  
  if (isdata){
    RFM.waitPacketSent();
    memset(Buffer, 0, sizeof(Buffer)); // Clear Buffer
    bufferappendindex = 0;
    checksum = 0;
    packetnum++;

    addtochecksum = false;
    AppendToBufferSTR(header, 4);
    addtochecksum = true;

    AppendToBufferUINT(packetnum); // Packet number
    AppendToBufferUINT((unsigned int)elapasedTime); // E
    lapased time

    // Rotation
    AppendToBufferFLOAT(rotX);
    AppendToBufferFLOAT(rotY);
    AppendToBufferFLOAT(rotZ);

    // Acceleration
    AppendToBufferFLOAT(accelX);
    AppendToBufferFLOAT(accelY);
    AppendToBufferFLOAT(accelZ);
  
    AppendToBufferFLOAT(Alti); // Altitude
    AppendToBufferFLOAT(Pressure); // Pressure
    AppendToBufferFLOAT(Humidity); // Humidity
    AppendToBufferFLOAT(Temp); // Temperature

    // GPS
    if (!fix){
      AppendToBufferFLOAT(-3425);
      AppendToBufferFLOAT(-3425);
    }else{
      AppendToBufferFLOAT(gpsLat); // Latitude
      AppendToBufferFLOAT(gpsLong); // Longitude
    }
    

    uint8_t vin = readVcc();
    Buffer[bufferappendindex] = vin; // Battery Level
    bufferappendindex++;
    checksum += vin;
    
    RFM.send(Buffer, sizeof(Buffer));

    gpsstatus = false; bnostatus = false; bmestatus = false;
  }
}


bool UpdateGPS(bool debug){
  if (GPSSerial.available() == 0) return false;

  char read = GPSSerial.read();

  bool done = parseSegment(read);
  if (done){
    if (parseGPSNMEA()){
      if (!fix){
        if (debug) Serial.println("No fix");
        return false;
      }
      if (debug){
        Serial.print("GPS Lat: ");
        Serial.print(gpsLat);
        Serial.print("GPS Long: ");
        Serial.println(gpsLong);
      }
      return true;
    }
    if (debug) Serial.println("Parse Failed");
    return false;
  }
  return false;
}


bool UpdateBNO(){
  if (sinceBNO < 10) return false;

  sensors_event_t rotationData;sensors_event_t accelerationData;
 
  // Fetching sensor-fusion rotation and acceleration data
  bno.getEvent(&rotationData);
  bno.getEvent(&accelerationData, Adafruit_BNO055::VECTOR_LINEARACCEL);


  // Store the sensor-fusion euler rotation in rotX, rotY, rotZ
  rotX = rotationData.orientation.x * TO_RADIANS; rotY = rotationData.orientation.y * TO_RADIANS; rotZ = rotationData.orientation.z * TO_RADIANS;
  computeGlobalAcceleration(accelerationData.acceleration.x, accelerationData.acceleration.y, accelerationData.acceleration.z);

  sinceBNO = 0;
  return true;
}


void computeGlobalAcceleration(float rawX, float rawY, float rawZ){
  accelX = rawX; accelY = rawY; accelZ = rawZ;
  float newaccelX = 0; float newaccelY = 0; float newaccelZ = 0;


  //Undo Rotation around X-axis
  newaccelY = cos(rotX) * accelY - sin(rotX) * accelZ;
  newaccelZ = sin(rotX) * accelY + cos(rotX) * accelZ;
  accelY = newaccelY;
  accelZ = newaccelZ;


  //Undo Rotation around Y-axis (Z)
  newaccelX = cos(rotY) * accelX + sin(rotY) * accelZ;
  newaccelZ = -sin(rotY) * accelX + cos(rotY) * accelZ;
  accelX = newaccelX;
  accelZ = newaccelZ;


  //Undo Rotation around Z-axis (Y)
  newaccelX = cos(rotZ) * accelX - sin(rotZ) * accelY;
  newaccelY = sin(rotZ) * accelX + cos(rotZ) * accelY;
  accelX = newaccelX;
  accelY = newaccelY;
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

float readTemperature() {
  double R_NTC, log_NTC;
  uint16_t ARead = analogRead(A13);
  R_NTC = 4700*ARead/(4095.0-ARead);
  log_NTC = log(R_NTC/10000);

  // The line below is the Steinhart-Hart equation
  float temperature = 1/(3.354016E-3 + 2.569850E-4*log_NTC + 2.620131E-6*log_NTC*log_NTC + 6.383091E-8*log_NTC*log_NTC*log_NTC)-273.15;

  return temperature;
}

/*
  GPS Helper Functions
*/

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

    if (index == 6){
      fix = true;
      if (part[0] == '0') fix = false;
    }
    if (index == 2){
      if (strlen(part) < 8) continue;
      String latMin = ""; String latDeg = "";
      latDeg += part[0]; latDeg += part[1];
      latMin += part[2]; latMin += part[3]; latMin += part[4]; latMin += part[5]; latMin += part[6]; latMin += part[7];

      gpsLat = latDeg.toInt() + (latMin.toFloat() / 60.0);
    }
    if (index == 4){
      if (strlen(part) < 9) continue;
      String longMin = ""; String longDeg = "";
      longDeg += part[0]; longDeg += part[1]; longDeg += part[2];
      longMin += part[3]; longMin += part[4]; longMin += part[5]; longMin += part[6]; longMin += part[7]; longMin += part[8];

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
  return true;
}


uint8_t readVcc() { 
  return 0;
}


void Buzz(int duration, int freq){
  tone(BUZZER, freq, duration); delay(duration + 1000);
}

/*
  All functions below are used to translate sensor data into raw byte data
  Used in "loop()" to add to buffer and send
*/

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


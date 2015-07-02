#include "HX711.h"
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include <HashMap.h>

//--setup SD Logging shield
#define LOG_INTERVAL  1000
#define SYNC_INTERVAL 1000
uint32_t syncTime = 0;
uint32_t m = 0;

#define ECHO_TO_SERIAL   1

#define redLEDpin 2
#define greenLEDpin 3

RTC_DS1307 RTC;

const int chipSelect = 10;

//--setup Scale
HX711 scale(A2, A3);

float totalWeight, previousTotalWeight;
float measuredWeight, previousMeasuredWeight, idWeight;
float smallestWeight = 5.4;
float pickedUpThresh = -0.1;
boolean movedForReal = false;
boolean pickedUp = false;
boolean noSingleProducts = false;

byte objectWeight = 0;
byte objectWeightPlus = 1;
byte objectWeightMinus = 2;

long lastDebounceTime = 0;
int debounceDelay = 500;


//float singleProducts[] = {
//  5.6, 16.8, 38.32, 84, 173.6
//};

byte singleProducts[] = {
  1, 3, 7, 9, 20
};

byte singleProducts_count = 5;

HashType<byte, char*> hashRawArray[78];
HashMap<byte, char*> lookup = HashMap<byte, char*>(hashRawArray, 78);

File logfile;

void setup() {
  Serial.begin(9600);
  byte products[] = {
    20, 20, 1, 3, 3, 3, 7, 7, 7, 9, 9, 1, 1
  };
  byte product_count = 13;

  byte sum = 0;
  byte lookup_cnt = 0;

  char subset[78][10];

  for (byte i = 0; i < product_count - 1; i++) {
    for (byte j = i + 1; j < product_count; j++) {
      sum = products[i] + products[j];
      String middle = ", ";
      String subset_tmp = products[i] + middle + products[j];
      subset_tmp.toCharArray(subset[lookup_cnt], 10);
      lookup[lookup_cnt](sum, subset[lookup_cnt]);
      lookup_cnt++;
    }
  }

  //---- Logging setup ----//
  // use debugging LEDs
  pinMode(redLEDpin, OUTPUT);
  pinMode(greenLEDpin, OUTPUT);

  initializeSD();
  connectToRTC();

  logfile.println("millis,stamp,date,time,sensor name,status");
#if ECHO_TO_SERIAL
  Serial.println("millis,stamp,date,time,sensor name,status");
#endif //ECHO_TO_SERIAL

  //lookup.debug();

  //---- Scale setup ---//
  scale.read();
  scale.set_scale(2280.f);
  scale.tare();


}

void loop() {

  //the one function to rule them all.
  detectChange();

}


void detectChange() {

  totalWeight = scale.get_units(5), 1;
  //  Serial.println(totalWeight);

  if (totalWeight > previousTotalWeight + 3.0 ||
      totalWeight < previousTotalWeight - 3.0) {

    lastDebounceTime = millis();

    movedForReal = true;
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    measuredWeight = totalWeight;

    if (movedForReal) {

      idWeight =  measuredWeight - previousMeasuredWeight;
      Serial.print("idWeight: ");
      Serial.println(idWeight);

      pickedUp = pick(idWeight);

      // keep positive
      idWeight = abs(idWeight);
      checkObjects();


      previousMeasuredWeight = measuredWeight;

      movedForReal = false;
    }
  }

  previousTotalWeight = totalWeight;
}

boolean pick(float x) {
  if (x > 0) {
    return true;
  }
  else {
    return false;
  }
}

void checkObjects() {

  //  Serial.println("in checkObjects");
  // compare to the ratio
  objectWeight = byte(idWeight / smallestWeight);
  objectWeightPlus = byte((idWeight / smallestWeight) - 1);
  objectWeightMinus = byte((idWeight / smallestWeight) + 1);


  Serial.print("Ratio'd Object Weight: ");
  Serial.println(objectWeight);
  // Serial.println(lookup.getValueOf(objectWeight));

  //checking for single products


  for (int i = 0; i < singleProducts_count; i++) {

    if (pickedUp) {
      if (objectWeight == singleProducts[i]) {
        Serial.print("Single Product Picked Up: ");
        Serial.println(singleProducts[i]);
        Serial.println();
        noSingleProducts = false;

      }
      else {
        //noSingleProducts = true;

      }
    }

    else {
      if ( objectWeight == singleProducts[i]) {
        Serial.print("Single Product Put Back: ");
        Serial.println(singleProducts[i]);
        Serial.println();
        noSingleProducts = false;

      }

      else {
        //noSingleProducts = true;

      }
    }

  }


  // look up values in table
  if (noSingleProducts && lookup.getValueOf(objectWeight) != NULL) {

    if (pickedUp) {
      //log picked up
//      Serial.println("Picked Up");
//      Serial.print("Object Weight: ");
//      Serial.println(lookup.getValueOf(objectWeight));
//      Serial.println();
      scaleDebug("Picked Up: ", "Object Weight: ", objectWeight);
    }


    else {
//      Serial.println("Put Back");
//      Serial.print("Object Weight: ");
//      Serial.println(lookup.getValueOf(objectWeight));
//      Serial.println();
      scaleDebug("Put Back: ", "Object Weight: ", objectWeight);
      //log put back
    }
    noSingleProducts = false;
  }


  else if (noSingleProducts && lookup.getValueOf(objectWeightPlus) != NULL) {

    if (pickedUp) {
//      Serial.print("Picked Up: ");
//      Serial.println(objectWeightPlus);
//      Serial.print("Object Weight - 1: ");
//      Serial.println(lookup.getValueOf(objectWeightPlus));
//      Serial.println();
      scaleDebug("Picked Up: ", "Object Weight - 1: ", objectWeightPlus);
    }

    else {
//      Serial.print("Put back: ");
//      Serial.println(objectWeightPlus);
//      Serial.print("Object Weight - 1: ");
//      Serial.println(lookup.getValueOf(objectWeightPlus));
//      Serial.println();
      scaleDebug("Put Back: ", "Object Weight - 1: ", objectWeightPlus);
    }
    noSingleProducts = false;
  }
  else if ( noSingleProducts && lookup.getValueOf(objectWeightMinus) != NULL) {
    if (pickedUp) {
//      Serial.print("Picked Up: ");
//      Serial.println(objectWeightMinus);
//      Serial.print("Object Weight + 1: ");
//      Serial.println(lookup.getValueOf(objectWeightMinus));
//      Serial.println();
      scaleDebug("Picked Up: ", "Object Weight + 1: ", objectWeightMinus);
    }

    else {
//      Serial.print("Put Back: ");
//      Serial.println(objectWeightMinus);
//      Serial.print("Object Weight + 1: ");
//      Serial.println(lookup.getValueOf(objectWeightMinus));
//      Serial.println();
      scaleDebug("Put Back: ", "Object Weight + 1: ", objectWeightMinus);
    }
    noSingleProducts = false;

  }
  else if (noSingleProducts) {
    Serial.println("unkown weight found");
    Serial.println();
  }

  noSingleProducts = true;

  //  Serial.println("check objects");
}

void scaleDebug( char *state, char *objType, byte obj){
      
      Serial.println(state);
      Serial.print(objType);
      Serial.println(lookup.getValueOf(obj));
      Serial.println();
      
}

void logData(char *str, char *stat) {

  DateTime now;

  logfile.print(m);           // milliseconds since start
  logfile.print(", ");

#if ECHO_TO_SERIAL
  Serial.print(m);         // milliseconds since start
  Serial.print(", ");
#endif

  // fetch the time
  now = RTC.now();

  // log date
  logfile.print(now.unixtime()); // seconds since 1/1/1970
  logfile.print(", ");
  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(", ");

  //log time
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);


#if ECHO_TO_SERIAL
  Serial.print(now.unixtime()); // seconds since 1/1/1970
  Serial.print(", ");
  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(", ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
#endif //ECHO_TO_SERIAL

  logfile.print(", ");
  logfile.print(str);
  logfile.print(", ");
  logfile.println(stat);

#if ECHO_TO_SERIAL
  Serial.print(", ");
  Serial.print(str);
  Serial.print(", ");
  Serial.println(stat);
#endif ECHO_TO_SERIAL

  digitalWrite(greenLEDpin, LOW);

  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power and takes time
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();

  // blink LED to show we are syncing data to the card & updating FAT!
  digitalWrite(redLEDpin, HIGH);
  logfile.flush();
  digitalWrite(redLEDpin, LOW);
}

void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);

  // red LED indicates error
  digitalWrite(redLEDpin, HIGH);

  while (1);
}

void initializeSD() {

  // initialize the SD card
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    error("Card failed, or not present");
  }
  Serial.println("card initialized.");

  // create a new file
  char filename[] = "3DMP00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[4] = i / 10 + '0';
    filename[5] = i % 10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE);
      break;  // leave the loop!
    }
  }

  if (! logfile) {
    error("couldnt create file");
  }

  Serial.print("Logging to: ");
  Serial.println(filename);
}

void connectToRTC() {
  // connect to RTC
  Wire.begin();
  if (!RTC.begin()) {
    logfile.println("RTC failed");
#if ECHO_TO_SERIAL
    Serial.println("RTC failed");
#endif  //ECHO_TO_SERIAL
  }
}



#include <SPS_Gate.h>
#include <SPS_Display.h>
#include <SPS_Infrared_Sensor.h>
#include <SPS_RFID_Scanner.h>

#define OPEN 1
#define CLOSE 0

#define IR_CAR_1 22
#define IR_CAR_2 23
#define IR_CAR_3 24
#define IR_CAR_4 25
#define IR_CAR_5 26
#define IR_CAR_6 27
#define IR_ENTRY_FRONT 28
#define IR_ENTRY_BACK 29
#define IR_EXIT_FRONT 30
#define IR_EXIT_BACK 31

#define SERVO_ENTER_PIN 9
#define SERVO_EXIT_PIN 8
#define SERVO_DELAY_MS 0

#define RFID_ENTER_SS_PIN 53
#define RFID_ENTER_RST_PIN 5

#define ENTRY_BTN_PIN 17 //Button 2
#define EXIT_BTN_PIN 18 //Button 1

#define LCD_ADDR 0x27
#define LCD_FPS 45

#define LED_PIN 3
#define LIGHT_SENSOR_PIN 4

#define READ_FROM_ESP_PIN 2

#define TOTAL_SLOTS 6

SPS_InfraredSensor infraredSensor(IR_CAR_1, IR_CAR_2, IR_CAR_3, IR_CAR_4, IR_CAR_5, IR_CAR_6, IR_ENTRY_FRONT, IR_ENTRY_BACK, IR_EXIT_FRONT, IR_EXIT_BACK);
SPS_Display display(LCD_ADDR, LCD_FPS);
SPS_Gate entryGate(SERVO_ENTER_PIN, SERVO_DELAY_MS);
//SPS_Gate entryGate(SERVO_ENTER_PIN, SERVO_DELAY_MS, 90);
SPS_Gate exitGate(SERVO_EXIT_PIN, SERVO_DELAY_MS);
//SPS_RFID_Scanner exitScanner(RFID_EXIT_SS_PIN, RFID_ENTER_RST_PIN);
SPS_RFID_Scanner entryScanner(RFID_ENTER_SS_PIN, RFID_ENTER_RST_PIN);

bool hasSlot;
bool hasChangeState;
int slotStates[TOTAL_SLOTS];
int prevSlotStates[TOTAL_SLOTS] = {0, 0, 0, 0, 0, 0};
int currentEntrySwitchState;
int currentExitSwitchState;

bool isEntryFrontSensorDetected;
bool isEntryBackSensorDetected;
bool isExitFrontSensorDetected;
bool isExitBackSensorDetected;

bool isValidCardDetected;

bool currentEntryGateStatus;
bool currentExitGateStatus;
uint8_t currentESPSignalMode;
bool entryMode; //true is MANUAL, false is AUTO
bool exitMode;

unsigned char** validUIDs = new unsigned char*[6];  // 6 rows


void setup() {
  pinMode(ENTRY_BTN_PIN, INPUT_PULLUP);
  pinMode(EXIT_BTN_PIN, INPUT_PULLUP);
  currentEntrySwitchState = digitalRead(ENTRY_BTN_PIN);
  currentExitSwitchState = digitalRead(EXIT_BTN_PIN);
  entryMode = false;
  exitMode = false;

  Serial.begin(9600);
  hasSlot = true;
  currentESPSignalMode = INPUT;
  pinMode(READ_FROM_ESP_PIN, INPUT);

  isEntryFrontSensorDetected = false;
  isEntryBackSensorDetected = false;
  isExitFrontSensorDetected = false;
  isExitBackSensorDetected = false;

  isValidCardDetected = false;

  // Allocate memory for each row and populate the array
  validUIDs[0] = new unsigned char[4]{ 0x6D, 0xE2, 0xD7, 0x21 };  // Thẻ 1
  validUIDs[1] = new unsigned char[4]{ 0x23, 0x0A, 0x54, 0x11 };  // Thẻ 2
  validUIDs[2] = new unsigned char[4]{ 0xE3, 0x9A, 0x66, 0x10 };  // Thẻ 3
  validUIDs[3] = new unsigned char[4]{ 0x43, 0x34, 0x54, 0x10 };  // Thẻ 4
  validUIDs[4] = new unsigned char[4]{ 0x40, 0x1E, 0x4A, 0x12 };  // Thẻ 5
  validUIDs[5] = new unsigned char[4]{ 0x6A, 0xD5, 0x17, 0xA4 };  // Thẻ 6

  infraredSensor.init();
  display.init();
  entryGate.init();
  entryScanner.init(validUIDs, 6);
  exitGate.init();
}

void sendStateToEsp () {
  String message = "STATE:{";
  for (int i = 0; i < TOTAL_SLOTS; i++) {
    message += String(prevSlotStates[i]);
    if (i < TOTAL_SLOTS - 1) {
      message += ","; 
    }
  }
  message += "}";

  Serial.println(message);
}

void printValidCard (int index) {
  String result = "";
  for (int i = 0; i < 4; i++) {
    result += "0x";
    if (validUIDs[index][i] < 0x10) { 
      result += "0";
    }
    result += String(validUIDs[index][i], HEX); 
    result.toUpperCase();

    if (i < 3) {
      result += "-";
    }
  }

  Serial.println("CARD:" + result);
}

void updateEntryGateStatus() {
  if (currentEntryGateStatus == OPEN) {
    // Serial.println("if statement: " + String(isEntryFrontSensorDetected) + String(isEntryBackSensorDetected)); // debug
    if (!isEntryFrontSensorDetected && !isEntryBackSensorDetected) {
      currentEntryGateStatus = CLOSE;
      entryScanner.clearCache();
    }
  } else {
    // Serial.println("else statement: " + String(hasSlot) + String(isValidCardDetected) + String(isEntryFrontSensorDetected)); // debug
    if (hasSlot && isValidCardDetected && isEntryFrontSensorDetected) {
      currentEntryGateStatus = OPEN;
    } else { 
      currentEntryGateStatus = CLOSE;
      entryScanner.clearCache();
    }
  }
}

void updateExitGateStatus() {
  if (currentExitGateStatus == OPEN) {
    if (!isExitFrontSensorDetected && !isExitBackSensorDetected) {
      currentExitGateStatus = CLOSE;
      //exitScanner.clearCache();
    }
  } else {
    if (isExitFrontSensorDetected) {
      currentExitGateStatus = OPEN;
    } else {
      currentExitGateStatus = CLOSE;
      //exitScanner.clearCache();
    }
  }
}

void readSensor() {
  int slotsLeft = TOTAL_SLOTS;
  int sensorValue;
  int prevSlotStateCounter = 0;

  for (int i = 0; i < 6; i++) {
    sensorValue = infraredSensor.isParkingSensorDetected(i) ? 1 : 0;
    if(slotStates[i] != sensorValue){
      prevSlotStates[i] = slotStates[i];
      prevSlotStateCounter++;
    }
    slotStates[i] = sensorValue;
    slotsLeft = slotsLeft - slotStates[i];
  }
    
  hasChangeState = prevSlotStateCounter > 0;

  hasSlot = slotsLeft > 0;
  
  isEntryFrontSensorDetected = infraredSensor.isEntryFrontSensorDetected();
  isEntryBackSensorDetected = infraredSensor.isEntryBackSensorDetected();
  isExitFrontSensorDetected = infraredSensor.isExitFrontSensorDetected();
  isExitBackSensorDetected = infraredSensor.isExitBackSensorDetected();

  int detectedCardIndex  = entryScanner.validateCard();
  if(detectedCardIndex != -1){
    printValidCard(detectedCardIndex);
  }

  if(!entryMode){ //only update in `auto` mode
    updateEntryGateStatus();
  } 
  if(!exitMode) { //only update in `auto` mode
    updateExitGateStatus();
  }
}

void render() {
  display.render(slotStates[0], slotStates[1], slotStates[2], slotStates[3], slotStates[4], slotStates[5]);
}

int readSignnalFromESP () {
  if(currentESPSignalMode != INPUT){
    pinMode(READ_FROM_ESP_PIN, INPUT);
    currentESPSignalMode = INPUT;
  }
  int signal = digitalRead(READ_FROM_ESP_PIN);
  return signal;
}

void resetSignalFromESP () {
  if(currentESPSignalMode != OUTPUT){
    pinMode(READ_FROM_ESP_PIN, OUTPUT);
    currentESPSignalMode = OUTPUT;
  }
  digitalWrite(READ_FROM_ESP_PIN, LOW);
}

void captureEntrySwitchChangeStateEvent(){ // entry toggle event
  int newEntrySwitchSignal = digitalRead(ENTRY_BTN_PIN);
  if(newEntrySwitchSignal != currentEntrySwitchState){
    if (!entryMode){
      currentEntryGateStatus = currentEntryGateStatus == OPEN ? CLOSE : OPEN;
    } 
    entryMode = !entryMode;
    currentEntrySwitchState = newEntrySwitchSignal; // require for toggling
  }
}

void captureExitSwitchChangeStateEvent(){ //exit toggle event
  int newExitSwitchSignal = digitalRead(EXIT_BTN_PIN);
  if(newExitSwitchSignal != currentExitSwitchState){
    if (!exitMode){
      currentExitGateStatus = currentExitGateStatus == OPEN ? CLOSE : OPEN;
    } 
    exitMode = !exitMode;
    currentExitSwitchState = newExitSwitchSignal;
  }
}

void loop() {
  captureEntrySwitchChangeStateEvent();
  captureExitSwitchChangeStateEvent();
  readSensor();
  render();

  if (currentEntryGateStatus == OPEN) {
    // Serial.println("open");
    entryGate.open();
  } else {
    // Serial.println("close");
    entryGate.close();
  }

  if (currentExitGateStatus == OPEN) {
    exitGate.open();
  } else {
    exitGate.close();
  }

  if(readSignnalFromESP() == HIGH){
    Serial.println("Receive card valid signal from esp");
    isValidCardDetected = true;
    resetSignalFromESP();
  }else{
    isValidCardDetected = false;
  }

  if(hasChangeState){
    sendStateToEsp();
  }

  delay(1000);
}

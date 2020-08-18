#include <Stepper.h>
#include <Servo.h>
#include <EEPROM.h>

// 18700 intera corsa di endstop lettere
// todo se perde passi e inizia a suonare , verificare se al prossimo shot si spegne i buzzer
// impostare la home con il NEAR Lettere invece che il FAR Lettere

// eeprom
const int addrLettere = 0;
const int addrNumeri = 1;
const int addrMoving = 2;
int _valLettere;
int _valNumeri;
int _shutdown_on_moving;  // set to 1 during movement
// input pin
const int pin_endstop_lettere_near = A0;
const int pin_endstop_lettere_far = A1;
const int pin_endstop_numeri_near = A2;
const int pin_endstop_numeri_far = A3;
const int pin_goToHome = A4;
const int pin_mute = 23;
const int pin_avanti = 13;
const int pin_indietro = A5;
const int step_next_hole = -2304;
const int pin_sparo = 22;
const int wait_for_sparo = 1000;
// output pin
const int pin_servo = 3;
const int pin_buzzer = 52;
// variables
int avanzamento_lettere_dopo_home = 4;
int servo_low = 50;                     // servo press position
int servo_high = 100;                   // servo release position
// constant
const int numero_lettere_max = 7;       // (Base 0) A-H
const int numero_numeri_max = 11;       // (Base 0)1-12
const int stepsPerRevolution = 2048;
const int step_numeri_near = -100;
const int step_numeri_far = 100;
const int step_lettere_near = -100;
const int step_lettere_far = 100;
const int delta_home_lettere = 2800;
const int divider = 23;                 // during long movement, divide steps to permit check endstop
// objects
Stepper myStepper_lettere(stepsPerRevolution, 8, 10, 9, 11);
Stepper myStepper_numeri(stepsPerRevolution, 4, 6, 5,7);
Servo myservo;  // create servo object to control a servo

enum direzione {
  AVANTI,
  INDIETRO,
  HOME
};


void setup() {
  pinMode(pin_buzzer,OUTPUT);     // set buzzer pin as output
  myStepper_numeri.setSpeed(20);  // set motor speed
  myStepper_lettere.setSpeed(20); // set motor speed
  SetInput();                     // set pin as input
  myservo.attach(pin_servo);      // servo 
  Serial.begin(9600);             // initialize the serial port:
  PrintActualPosition();
  Bip(500,HIGH,LOW);              // bip for initialization finished
  _shutdown_on_moving =  getMoving();
  //setPositionLettere(4);
  //setPositionNumeri(0);
}


void loop() {
  if (analogRead(pin_goToHome) < 100) GoToHome();               // Go to home  **ok
  if (_shutdown_on_moving == 0){
    if (digitalRead(pin_avanti) == LOW) AvanzaHoleLettere();      // Avanti 
    if (digitalRead(pin_indietro) == LOW) IndietroHoleLettere();  // Indietro 
    if (digitalRead(pin_sparo) == LOW) Sparo();                   // Sparo
    if (digitalRead(pin_mute) == LOW) Mute();                     // Mute
  }
  else{
    Serial.println("go to home, shutdown during movement occured");
      Bip(1000,HIGH,LOW);
      Bip(1000,LOW,LOW);
      Bip(1000,HIGH,LOW);
  }
  delay(50);
}


void GoToHome(){
  Bip(100,LOW,LOW);
  MoveStepperLettere(HOME);
  MoveStepperNumeri(HOME);
  DeltaHoleLettere(delta_home_lettere);       // Aggiungo movimento delta per lettere
  for (int i=0; i < avanzamento_lettere_dopo_home; i++){
     AvanzaHoleLettere();                     // Avanzo di n hole lettere
  }
  delay(10);
  _shutdown_on_moving =  getMoving();
}


void Sparo(){
  Serial.println("pre sparo");
  delay(wait_for_sparo);                        // wait & check again to avoid eccidental shots
  if (digitalRead(pin_sparo) == LOW){           // check if sparo is pressed
    Serial.println("sparo");
    AzionaServo();
    while (digitalRead(pin_sparo) == LOW){      // until sparo is pressed , wait
      delay(10);
    }
    delay(100);
    AvanzaHoleLettere();                        // here i've released sparo button
  }
}


void AvanzaHoleNumeri(){
  Serial.println("avanzaHoleNumeri");
  int valLettere = getPositionLettere();
  int valNumeri = getPositionNumeri();
  if (analogRead(pin_endstop_numeri_far) > 100 && valNumeri < numero_numeri_max){
    int ministep = step_next_hole / divider;
    for (int i = 0; i < divider; i++){
      if (analogRead(pin_endstop_numeri_far) > 100){
        myStepper_numeri.step(-ministep);
      }
    }
    SpegniMotori();
    setPositionNumeri(valNumeri +1);
  }
  else
  {
    Serial.println("Numeri max reached");
  }
}


void AvanzaHoleLettere(){
  Bip(100,LOW,LOW);
  int valLettere = getPositionLettere();
  int valNumeri = getPositionNumeri();
  // check endstop reached
  if (valLettere < numero_lettere_max) {
    if (analogRead(pin_endstop_lettere_near) > 100) {
      MoveStepperLettere(AVANTI);
    }
  }
  else{
    Serial.println("Lettere max reached");
    if (valNumeri < numero_numeri_max){
      AvanzaHoleNumeri();
      for (int i = 0; i < numero_lettere_max; i++){
        IndietroHoleLettere();
      }
    }
    else{
      Serial.println("end");
    }
    // muoversi nella prossima riga di numeri e andare all'inizio delle lettere
  }
  SpegniMotori(); 
}


void IndietroHoleLettere(){
  int valLettere = getPositionLettere();
  int valNumeri = getPositionNumeri();
  if (valLettere <= avanzamento_lettere_dopo_home && valNumeri == 0){
    Serial.println("Non permesso");
  }
  else{
    if (valLettere == 0){
      MoveStepperNumeri(INDIETRO);
      for (int i = 0; i < numero_lettere_max; i++){
        MoveStepperLettere(AVANTI);
      }
    }
    else{
      MoveStepperLettere(INDIETRO);
    }
  }
}


void MoveStepperNumeri(direzione dir){
  int valNumeri = getPositionNumeri();
  int ministep = step_next_hole / divider;
  setMoving(true);
  if (dir == INDIETRO){
    for (int i = 0; i < divider; i++){
      if (analogRead(pin_endstop_numeri_near) > 100){
        myStepper_numeri.step(ministep);
      }
    }
    setPositionNumeri(valNumeri -1);
  }
  else if (dir == AVANTI){
    for (int i = 0; i < divider; i++){
      if (analogRead(pin_endstop_numeri_far) > 100){
        myStepper_numeri.step(-ministep);
      }
    }
    setPositionNumeri(valNumeri +1);
  }
  else{  //HOME
    for (int x = 0; x < 20; x++){
      MoveStepperNumeri(INDIETRO);
      delay(10);
    }
    setPositionNumeri(0);
  }
  SpegniMotori();
}


void MoveStepperLettere(direzione dir){
  int valLettere = getPositionLettere();
  int ministep = step_next_hole / divider;
  setMoving(true);
  if (dir == INDIETRO){
    for (int i = 0; i < divider; i++){
      if (analogRead(pin_endstop_lettere_far) > 100){
        myStepper_lettere.step(-ministep);
      }
    }
    setPositionLettere(valLettere-1);
  }
  else if (dir == AVANTI){
    for (int i = 0; i < divider; i++){
      if (analogRead(pin_endstop_lettere_near) > 100){
        myStepper_lettere.step(ministep);
      }
    }
    setPositionLettere(valLettere+1);
  }
  else{ // HOME
    for (int x = 0; x < 20; x++){
      MoveStepperLettere(INDIETRO);
      delay(10);
    }
    setPositionLettere(0);
  }
  SpegniMotori();
}


void setPositionLettere(int lettere){ EEPROM.write(addrLettere, lettere);}

void setPositionNumeri(int numeri){ EEPROM.write(addrNumeri, numeri);}

int getPositionLettere(){return EEPROM.read(addrLettere);}

int getPositionNumeri() { return EEPROM.read(addrNumeri);}

void setMoving(bool stato){ EEPROM.write(addrMoving, stato);}    

bool getMoving(){ return EEPROM.read(addrMoving);}     

void Mute(){
  Bip(100,LOW,LOW);
  PrintActualPosition();
}

void Bip(int duration, bool initial_state, bool final_state){
  digitalWrite(pin_buzzer,initial_state);
  delay(duration);
  digitalWrite(pin_buzzer,final_state);
}

void SetInput(){
  pinMode(pin_endstop_lettere_far,INPUT_PULLUP);
  pinMode(pin_endstop_lettere_near,INPUT_PULLUP);
  pinMode(pin_endstop_numeri_far,INPUT_PULLUP);
  pinMode(pin_endstop_numeri_near,INPUT_PULLUP);
  pinMode(pin_goToHome,INPUT_PULLUP);
  pinMode(pin_mute,INPUT_PULLUP);
  pinMode(pin_avanti,INPUT_PULLUP);
  pinMode(pin_indietro,INPUT_PULLUP);
  pinMode(pin_sparo,INPUT_PULLUP);
}

void SpegniMotori(){
  int motorPins[] = {4, 5, 6, 7, 8, 9, 10, 11};
  for (int p=0; p < 8; p++){
    digitalWrite(motorPins[p],LOW);
  }
  setMoving(false);
}

void DeltaHoleLettere(int step_to_move){
  setMoving(true);
  myStepper_lettere.step(-step_to_move);
  SpegniMotori();
}

void AzionaServo(){
  Serial.println("azionaServo");
  myservo.write(servo_low);
  delay(500);
  myservo.write(servo_high);
  delay(500);
}

void PrintActualPosition(){
  Serial.println("printActualPosition");
  Serial.print("Numeri: ");
  Serial.println(getPositionNumeri());
  Serial.print("Lettere: ");
  Serial.println(getPositionLettere());
  Serial.print("getMoving: ");
  Serial.println(getMoving());
}

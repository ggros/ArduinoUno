/*
Modified by Lauren
version 0.3

Any suggestions are welcome.
E-mail: Lauran.pan@gmail.com

Editor     : Lauren from DFRobot
Date       : 06.01.2012

* Update the library and sketch to compatible with IDE V1.0 and earlier
Display size 84x48
font 6x8: 6 lines of 14 chars
font Big: Only for numbers 0-9 +/-
*/

#include "LCD4884.h"
#include "DFrobot_bmp.h"
#include "DFrobot_chinese.h"
#include "QAC31.h"
#include <TimeLib.h>

//keypad debounce parameter
#define DEBOUNCE_MAX 15
#define DEBOUNCE_ON  10
#define DEBOUNCE_OFF 3 

#define NUM_KEYS 5

#define NUM_MENU_ITEM	5

// joystick number
#define LEFT_KEY 0
#define CENTER_KEY 1
#define DOWN_KEY 2
#define RIGHT_KEY 3
#define UP_KEY 4

// menu starting points

#define MENU_X	10		// 0-83
#define MENU_Y	1		// 0-5


int  adc_key_val[5] ={
  50, 200, 400, 600, 800 };

// debounce counters
byte button_count[NUM_KEYS];
// button status - pressed/released
byte button_status[NUM_KEYS];
// button on flags for user program 
byte button_flag[NUM_KEYS];

// menu definition
char menu_items[NUM_MENU_ITEM][12]={
  "TEMPERATURE",
  "BACKLIGHT",
  "HEURE",
  "VANNE",
  "SMENU"	
};
void (*menu_funcs[NUM_MENU_ITEM])(void) = {
  temperature,
  toogleBacklight,
  horloge,
  vanne,
  smenu
};
char smenu_items_1[NUM_MENU_ITEM][12]={
  "RELAIS",
  "VIDE",
  "VIDE",
  "ABOUT",
  "RETOUR"	
};

void (*smenu_funcs_1[NUM_MENU_ITEM])(void) = {
  relais,
  about,
  about,
  about,
  exit_smenu
};

char current_menu_item;

int ledPin = 13;      // select the pin for the LED
int I1OnPin = 9; //alim mélangeur, positin par défaut inter fermé => actif
int I1Pin = 10; // mélangeur, position par défault Y2 passant => fermeture
int I2Pin = 11; // bruleur, position par défault inter fermé => bruleur actif
int I3Pin = 12; // pompe, position par défaut inter fermé => pompe active

//courbe de chauffe
int TRef = 40;
int autoTemp = 1;
float courbeChauffeB = 26.43;//déplacement
float courbeChauffeA = 1.2857;//pente

//for Srial
String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

void setup()
{
  //on met l'horloge au moins en 2016
  //setTime(int hr,int min,int sec,int day, int month, int yr);
  setTime(0,0,0,1,1,2016);
  
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);
  // declare relais pin as OUTPUT:  
  pinMode(I1OnPin, OUTPUT);
  pinMode(I1Pin, OUTPUT);
  pinMode(I2Pin, OUTPUT);
  pinMode(I3Pin, OUTPUT);
  //initialisation relais
  digitalWrite(I1OnPin,1);//D9
  digitalWrite(I1Pin,0);//D10
  digitalWrite(I2Pin,1);//D11
  digitalWrite(I3Pin,1);//D12
  
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  Serial.println("Setup");
  
  // setup interrupt-driven keypad arrays  
  // reset button arrays
  for(byte i=0; i<NUM_KEYS; i++){
    button_count[i]=0;
    button_status[i]=0;
    button_flag[i]=0;
  }

  // Setup timer2 -- Prescaler/256
  TCCR2A &= ~((1<<WGM21) | (1<<WGM20));
  TCCR2B &= ~(1<<WGM22);
  TCCR2B = (1<<CS22)|(1<<CS21);
  ASSR |=(0<<AS2);

  // Use normal mode  
  TCCR2A =0;  
  //Timer2 Overflow Interrupt Enable  
  TIMSK2 |= (0<<OCIE2A);
  TCNT2=0x6;  // counting starts from 6;  
  TIMSK2 = (1<<TOIE2);
  SREG|=1<<SREG_I;

  //lcd init
  lcd.LCD_init();
  lcd.LCD_clear();
  //lcd.backlight(ON);//Turn on the backlight
  lcd.backlight(OFF); // Turn off the backlight  

  //menu initialization  
  init_MENU(menu_items);
  //init_MENU();
  current_menu_item = 0;  
  
  // reserve 200 bytes for the inputString on Serial  
  inputString.reserve(200);
}
int backLight = 0;//variable for toggle function below
int modeNuitEco = 0;
void menu_run(char items[][12], void (*f[NUM_MENU_ITEM])(void) );
/* loop */
void loop()
{
  //à chaque loop on regarde l'heure.
  verifModeNuit();
  vanne_commande();
  serial_cmd();
  menu_run(menu_items, menu_funcs);
}
int exit_submenu;
void smenu(){
  exit_submenu = 0;
  init_MENU(smenu_items_1);
  while (!exit_submenu){
    menu_run(smenu_items_1, smenu_funcs_1);
  }
}
void exit_smenu(){
  exit_submenu = 1;
}
/*
  Test de l'horloge, de 23h à 7h on ferme la vanne et arret bruleur
  */
void verifModeNuit(){
  //test de l'heure pour le mode nuit
  //attention si on est dans une des sous fonctions ça déclenche pas
  if(hour()>=23 || hour()<7){
  //|| hour() == 7 && minute() < 30){
    if(modeNuitEco == 0){
      //on ferme la vanne
      digitalWrite(I1OnPin,1);//D9
      digitalWrite(I1Pin,1);//D10
//      TRef = 0;//la commande va fermer
      //arret bruleur et pompe.
      digitalWrite(I2Pin,0);
      digitalWrite(I3Pin,0);
      Serial.println("INFO Passage en mode nuit Eco");
    }
    modeNuitEco = 1;
  }
  else{
    if(modeNuitEco == 1){
      //on ouvre la vanne
      digitalWrite(I1OnPin,1);//D9
      digitalWrite(I1Pin,0);//D10
      //dem bruleur et pompe.
      digitalWrite(I2Pin,1);
      digitalWrite(I3Pin,1);
      Serial.println("INFO Passage en mode jour");
    }
    modeNuitEco = 0;
  }
  //,second());
}

void serial_cmd(){
  // print the string when a newline arrives:
  if (stringComplete) {
    Serial.println(inputString); //echo
    
    if(inputString == "CMD T"){
      int T = getT();
      Serial.println(T);
    }
    else if(inputString == "CMD TINT"){
      int T = getTNTC(A2);
      Serial.println(T);
    }
    else if(inputString == "CMD TEXT"){
      int T = getTNTC(A3);
      Serial.println(T);
    }
    else if(inputString == "CMD TREF"){
      Serial.println(TRef);
    }
    else if(inputString.startsWith("CMD BR") && inputString.length() == 7){
      String c = inputString.substring(6,7);
      Serial.println(c);
      digitalWrite(I2Pin,c == "1");
    }
    else if(inputString.startsWith("CMD PP") && inputString.length() == 7){
      String c = inputString.substring(6,7);
      Serial.println(c);
      digitalWrite(I3Pin,c == "1");
    }
    
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
}
/*
   SerialEvent occurs whenever a new data comes in the
  hardware serial RX.  This routine is run between each
  time loop() runs, so using delay inside loop can delay
  response.  Multiple bytes of data may be available.
  */
void serialEvent() {
   while (Serial.available()) {
     // get the new byte:
     char inChar = (char)Serial.read();
     // if the incoming character is a newline, set a flag
     // so the main loop can do something about it:
     if (inChar == '\n') {
       stringComplete = true;      
     }
     else{
       // add it to the inputString:
       inputString += inChar; 
     }
   }
}
/* menu functions */
void init_MENU(char items[][12]){
//void init_MENU(){
  byte i;
  lcd.LCD_clear();
  lcd.LCD_write_string(MENU_X, MENU_Y, items[0], MENU_HIGHLIGHT );
  for (i=1; i<NUM_MENU_ITEM; i++){
    lcd.LCD_write_string(MENU_X, MENU_Y+i, items[i], MENU_NORMAL);
  }
}
void menu_run(char items[][12], void (*f[NUM_MENU_ITEM])(void) ){
  byte i;
  for(i=0; i<NUM_KEYS; i++){
    if(button_flag[i] !=0){

      button_flag[i]=0;  // reset button flag
      switch(i){

      case UP_KEY:
        // current item to normal display
        lcd.LCD_write_string(MENU_X, MENU_Y + current_menu_item, items[current_menu_item], MENU_NORMAL );
        current_menu_item -=1;
        if(current_menu_item <0)  current_menu_item = NUM_MENU_ITEM -1;
        // next item to highlight display
        lcd.LCD_write_string(MENU_X, MENU_Y + current_menu_item, items[current_menu_item], MENU_HIGHLIGHT );
        break;
      case DOWN_KEY:
        // current item to normal display
        lcd.LCD_write_string(MENU_X, MENU_Y + current_menu_item, items[current_menu_item], MENU_NORMAL );
        current_menu_item +=1;
        if(current_menu_item >(NUM_MENU_ITEM-1))  current_menu_item = 0;
        // next item to highlight display
        lcd.LCD_write_string(MENU_X, MENU_Y + current_menu_item, items[current_menu_item], MENU_HIGHLIGHT );
        break;
      case LEFT_KEY:
        init_MENU(items);
        current_menu_item = 0;
        break;   
      case CENTER_KEY:
        lcd.LCD_clear();
        //(*menu_funcs[current_menu_item])();
        (*f[current_menu_item])();
        lcd.LCD_clear();
        init_MENU(items);
        current_menu_item = 0;           
        break;	
      }
    }
  }
}
// waiting for center key press
void waitfor_OKkey(){
  byte i;
  byte key = 0xFF;
  while (key!= CENTER_KEY){
    for(i=0; i<NUM_KEYS; i++){
      if(button_flag[i] !=0){
        button_flag[i]=0;  // reset button flag
        if(i== CENTER_KEY) key=CENTER_KEY;
      }
    }
  }
}
//Sonde Nickel (QAC 21) sortie melangeur
int getT(){
  int sensorPin = A1;    // select the input pin for the potentiometer
  int sensorValue = 0;  // variable to store the value coming from the sensor
  int R1 = 1000;
  float V1 = 0;
  sensorValue = analogRead(sensorPin);// 0 to 1023
  V1 = (1023-sensorValue) * 5.0 /1023.0;
  if(sensorValue == 1023){
    return -100;
  }
  int R2 = 5 * R1 / V1 - R1;
  //approx lineaire.
  float T = (5.0 * R1 / V1 - R1 - 1000)/5.0;
  return (int)T;
}
//Sonde Nickel (QAC 21) sortie melangeur
int getR2Str(char* str_temp){
  int sensorPin = A1;    // select the input pin for the potentiometer
  int sensorValue = 0;  // variable to store the value coming from the sensor
  int R1 = 1000;
  float V1 = 0;
  sensorValue = analogRead(sensorPin);// 0 to 1023
  V1 = (1023-sensorValue) * 5.0 /1023.0;
  Serial.print("V1: ");
  Serial.println(V1);
  Serial.print("Sensor: ");
  Serial.println(sensorValue);
  if(sensorValue == 1023){
    sprintf(str_temp,"ERROR");
    return 0;
  }
  //formule diviseur de tension, R2 est celle qu'on cherche
  //U2 = U x R2 / (R1 + R2)
  //R2 = R1 x U/U1 - R1, U = 5 Volt
  int R2 = 5 * R1 / V1 - R1;
  Serial.println(R2);
  //1000 ohms = 0C
  //1090 = 20C
  //1137 = 30C
  //1185 = 40C
  //1234 = 50C
  //1285 = 60C
  //1500 = 100C
  
  //approx lineaire.
  float T = (5.0 * R1 / V1 - R1 - 1000)/5.0;
  //char str_temp[10];
  /* 4 is mininum width, 2 is precision; float value is copied onto str_temp*/
  dtostrf(T, 4, 2, str_temp);
  //sprintf(str_temp,"%s C", str_temp);
  //sprintf(str_temp,"%f C", T);//%f not implemented
  Serial.println(str_temp);
  Serial.print("T: ");
  Serial.println(T);
  return R2;
}
//Sonde NTC Thermistance
int getR2StrNTC(char* str_temp,int sensorPin){
  Serial.print("sPin: ");
  Serial.println(sensorPin);
  //int sensorPin = A1;    // select the input pin for the potentiometer
  int sensorValue = 0;  // variable to store the value coming from the sensor
  int R1 = 550;
  float V1 = 0;
  sensorValue = analogRead(sensorPin);// 0 to 1023 mesure de V2, entre R2 et GND
  V1 = (1023-sensorValue) * 5.0 /1023.0;
  Serial.print("V1: ");
  Serial.println(V1);
  Serial.print("Sensor: ");
  Serial.println(sensorValue);
  if(sensorValue == 1023){
    sprintf(str_temp,"ERR S");
    return 0;
  }
  int R2 = 5 * R1 / V1 - R1;
  Serial.print("R2: ");
  Serial.println(R2);
  
  //672 ohms = -35C
  //623 = 0C
  //600 = 10C
  //575 = 20C
  
  //approx lineaire? -> très peu précis car normalement c'est une formule à 3 coef
  //déjà qu'on perd en précision avec les 10bit du ADC (0-1023)
  float T = (R2-615.77)/-1.8845;
  /* 4 is mininum width, 2 is precision; float value is copied onto str_temp*/
  dtostrf(T, 4, 2, str_temp);
  //avec la référence
  int t_index;
  //test que c'est dans la plage
  if(R2 < QAC31_R[QAC31_R_SIZE-1] || R2 > QAC31_R[0]){
    sprintf(str_temp,"ERR R");
    Serial.println("Erreur hors plage");
    return 0;
  }
  for(t_index = 0; t_index<QAC31_R_SIZE; t_index++){
    if(QAC31_R[t_index] < R2) break;//valeur dépassée car R diminue avec la temperature
  }
  sprintf(str_temp,"%02d.00", t_index-35-1);
  
  //sprintf(str_temp,"%s C", str_temp);
  //sprintf(str_temp,"%f C", T);//%f not implemented
  Serial.println(str_temp);
  Serial.print("T: ");
  Serial.println(T);
  return R2;
}
//Sonde NTC Thermistance
int getTNTC(int sensorPin){
  //int sensorPin = A1;    // select the input pin for the potentiometer
  int sensorValue = 0;  // variable to store the value coming from the sensor
  int R1 = 550;
  float V1 = 0;
  sensorValue = analogRead(sensorPin);// 0 to 1023 mesure de V2, entre R2 et GND
  V1 = (1023-sensorValue) * 5.0 /1023.0;
  if(sensorValue == 1023){
    return -100;
  }
  int R2 = 5 * R1 / V1 - R1;
  //avec la référence
  int t_index;
  //test que c'est dans la plage
  if(R2 < QAC31_R[QAC31_R_SIZE-1] || R2 > QAC31_R[0]){
    return -100;
  }
  for(t_index = 0; t_index<QAC31_R_SIZE; t_index++){
    if(QAC31_R[t_index] < R2) break;//valeur dépassée car R diminue avec la temperature
  }
  //sprintf(str_temp,"%i", t_index-35-1);  
  return t_index-35-1;
}
void temperature_display(char* strRes, char* strTemp, char* strPin){
  //lcd.LCD_clear();
  lcd.LCD_write_string(78, 2, "C", MENU_NORMAL);
  lcd.LCD_write_string(38, 5, "OK", MENU_HIGHLIGHT );
  lcd.LCD_write_string(0, 0, strRes, MENU_NORMAL);
  lcd.LCD_write_string(84-15, 0, strPin, MENU_NORMAL);
  lcd.LCD_write_string_big(8, 2, strTemp, MENU_NORMAL);//X,Y,str,mode
}
/*
  Affiche en haut la resistance lu
  A droite le sensorPin (15, 16, 17)
  au milieu en gros la temperature
  */
void temperature()
{
  //temperature_init()
  char strTemp[8];
  char strRes[10];
  char strPin[10];
  int sensorPin = A1;//A1, A2, A3 = 15,16,17
  itoa(sensorPin,strPin,10);

  //Sonde Nickel
  int R2 = getR2Str(strTemp);
  //itoa(R2,strRes,10);
  sprintf(strRes,"%0d",R2);
  
  temperature_display(strRes, strTemp, strPin);
  

  //waitfor_OKkey();
  byte i;
  byte key = 0xFF;
  // boucle attente sortie avec CENTER_KEY
  while (key!= CENTER_KEY){
    //test joystick   
    for(i=0; i<NUM_KEYS; i++){
      if(button_flag[i] !=0){
        button_flag[i]=0;  // reset button flag
        if(i == CENTER_KEY) key=CENTER_KEY;
        else if(i == LEFT_KEY){
          Serial.println("LEFT");
          if(sensorPin>A1) sensorPin--;
        }
        else if(i == RIGHT_KEY){
          Serial.println("RIGHT");
          if(sensorPin<A3) sensorPin++;
        }  
        itoa(sensorPin,strPin,10);
        temperature_display(strRes, strTemp, strPin);
      }
    }
    if(millis() % 500 == 0){
      if(sensorPin == A1){
        //Sonde Nickel
        R2 = getR2Str(strTemp);
        //itoa(R2,strRes,10);
        sprintf(strRes,"%d.00",R2);
      }
      else if(sensorPin == A2 || sensorPin == A3){
        //Sonde NTC
        R2 = getR2StrNTC(strTemp, sensorPin);
        //itoa(R2,strRes,10);
        sprintf(strRes,"%d.00",R2);
      }
      temperature_display(strRes, strTemp, strPin);      
    }
    //delay(100);//stop 100ms
  }
}
void toogleBacklight(){
  if(backLight){
    backLight = 0;
    lcd.backlight(OFF); // Turn off the backlight  
  }
  else{
    backLight = 1;
    lcd.backlight(ON);
  }
}
//byte State = 0xFF;//PULLUP by default
byte State = B00000110;
void relais(){
  int currentPin = 13;
  char str3[10];
  //lecture
  Serial.print("I1Pin: ");
  Serial.println(digitalRead(I1Pin));
  bitWrite(State,0,digitalRead(I1OnPin));//D9
  bitWrite(State,1,digitalRead(I1Pin));//D10
  bitWrite(State,2,digitalRead(I2Pin));//D11
  bitWrite(State,3,digitalRead(I3Pin));//D12
  bitWrite(State,4,digitalRead(ledPin));//D13
  
  itoa(currentPin,str3,10);
  lcd.LCD_write_string(84-15, 0, str3, MENU_NORMAL);
  
  byte i;
  byte key = 0xFF;
  while (key!= CENTER_KEY){    
    for(i=0; i<NUM_KEYS; i++){
      if(button_flag[i] !=0){
        button_flag[i]=0;  // reset button flag
        if(i == CENTER_KEY) key=CENTER_KEY;//EXIT
        else if(i == LEFT_KEY){
          Serial.println("LEFT");
          if(currentPin>I1OnPin) currentPin--;
        }
        else if(i == RIGHT_KEY){
          Serial.println("RIGHT");
          if(currentPin<ledPin) currentPin++;
        } 
        else if(i == UP_KEY){
          Serial.println("UP");
          digitalWrite(currentPin, HIGH);
          bitSet(State,currentPin - 9);
        }
        else if(i == DOWN_KEY){
          Serial.println("DOWN");
          digitalWrite(currentPin, LOW);
          bitClear(State,currentPin - 9);
        }
        itoa(currentPin,str3,10);
        lcd.LCD_write_string(84-15, 0, str3, MENU_NORMAL);
        //print state
        int xPos = 0;
        int xCoef = 2*6+2;
        int typeMenu = MENU_NORMAL;
        lcd.LCD_write_string( xPos*xCoef, 2, "ML", MENU_NORMAL);
        if(currentPin - 9 == xPos) typeMenu = MENU_HIGHLIGHT; else typeMenu = MENU_NORMAL;
        if(bitRead(State, xPos)){
           lcd.LCD_write_string( xPos++*xCoef, 3, "ON", typeMenu);
        }
        else{
           lcd.LCD_write_string( xPos++*xCoef, 3, "OF", typeMenu);
        }
        lcd.LCD_write_string( xPos*xCoef, 2, "Y2", MENU_NORMAL);
        if(currentPin - 9 == xPos) typeMenu = MENU_HIGHLIGHT; else typeMenu = MENU_NORMAL;
        if(bitRead(State, xPos)){
           lcd.LCD_write_string( xPos++*xCoef, 3, "ON", typeMenu);
        }
        else{
           lcd.LCD_write_string( xPos++*xCoef, 3, "OF", typeMenu);
        }
        lcd.LCD_write_string( xPos*xCoef, 2, "BR", MENU_NORMAL);
        if(currentPin - 9 == xPos) typeMenu = MENU_HIGHLIGHT; else typeMenu = MENU_NORMAL;
        if(bitRead(State, xPos)){
           lcd.LCD_write_string( xPos++*xCoef, 3, "ON", typeMenu);
        }
        else{
           lcd.LCD_write_string( xPos++*xCoef, 3, "OF", typeMenu);
        }
        lcd.LCD_write_string( xPos*xCoef, 2, "PP", MENU_NORMAL);
        if(currentPin - 9 == xPos) typeMenu = MENU_HIGHLIGHT; else typeMenu = MENU_NORMAL;
        if(bitRead(State, xPos)){
           lcd.LCD_write_string( xPos++*xCoef, 3, "ON", typeMenu);
        }
        else{
           lcd.LCD_write_string( xPos++*xCoef, 3, "OF", typeMenu);
        }
        lcd.LCD_write_string( xPos*xCoef, 2, "13", MENU_NORMAL);
        if(currentPin - 9 == xPos) typeMenu = MENU_HIGHLIGHT; else typeMenu = MENU_NORMAL;
        if(bitRead(State, xPos)){
           lcd.LCD_write_string( xPos++*xCoef, 3, "ON", typeMenu);
        }
        else{
           lcd.LCD_write_string( xPos++*xCoef, 3, "OF", typeMenu);
        }
      }
    }
  }
}
/*
void relaisOn(){
   // turn the ledPin on:        
   digitalWrite(ledPin, HIGH); 
}
void relaisOff(){
  // turn the ledPin off:        
  digitalWrite(ledPin, LOW); 
}
void charmap(){
  char i,j;
  for(i=0; i<5; i++){
    for(j=0; j<14; j++){
      lcd.LCD_set_XY(j*6,i);
      lcd.LCD_write_char(i*14+j+32, MENU_NORMAL);
    }
  }
  lcd.LCD_write_string(38, 5, "OK", MENU_HIGHLIGHT );
  waitfor_OKkey();   
}

void bitmap(){
  lcd.LCD_draw_bmp_pixel(0,0, DFrobot_bmp, 84,24);
  lcd.LCD_write_chinese(6,3, DFrobot_chinese,12,6,0,0);
  lcd.LCD_write_string(38, 5, "OK", MENU_HIGHLIGHT );
  waitfor_OKkey();
}
*/
void horloge(){

  lcd.LCD_write_string( 0, 0, "Horloge", MENU_NORMAL);
  
  char strT[10],strD[10];
  sprintf(strT, "%02d:%02d:%02d", hour(),minute(),second());
  sprintf(strD, "%04d-%02d-%02d", year(),month(),day());
  
  //String str1=String("");
  //String sep = String(":");
  //str1 += hour() + sep + minute() + sep + second();
  //itoa(minute(),str,5);
  //char str[10];
  //str1.toCharArray(str,10);
  lcd.LCD_write_string( 0, 2, strD, MENU_NORMAL);
  lcd.LCD_write_string( 0, 3, strT, MENU_NORMAL);
  
  lcd.LCD_write_string(38, 5,"OK", MENU_HIGHLIGHT );

  //waitfor_OKkey();
  byte i;
  byte key = 0xFF;
  while (key!= CENTER_KEY){
    sprintf(strT, "%02d:%02d:%02d", hour(),minute(),second());
    lcd.LCD_write_string( 0, 3, strT, MENU_NORMAL);
    verifModeNuit();
    if(modeNuitEco){
      lcd.LCD_write_string( 0, 0, "Horloge - Nuit", MENU_NORMAL);
    }
    else{
      lcd.LCD_write_string( 0, 0, "Horloge - Jour", MENU_NORMAL);
    }
    for(i=0; i<NUM_KEYS; i++){
      if(button_flag[i] !=0){
        button_flag[i]=0;  // reset button flag
        switch(i){
          case UP_KEY:
            Serial.println("UP");
            adjustTime(60);// +1 min
            break;
          case DOWN_KEY:
            Serial.println("DOWN");
            adjustTime(-60);
            break;
          case LEFT_KEY:
            Serial.println("LEFT");
            adjustTime(3600);// +1 heure
            break;
          case RIGHT_KEY:
            Serial.println("RIGHT");
            adjustTime(-3600);
            break;    
          case CENTER_KEY:
            Serial.println("CENTER");
            key=CENTER_KEY;           
            break;	
        }
      }
    }
    delay(100);//stop 100ms
  }

}
unsigned long lastCmdMs = 0;
int vanne_commande(){
    int action = 0;
    //temp int et ext
    int Tinterieur = getTNTC(A2);
    int Text = getTNTC(A3);
    
    int TPente = courbeChauffeB + (20-Text) * courbeChauffeA;
    if(autoTemp){
      TRef = TPente;
    }
    //T Nickel, sortie vanne
    int T = getT();
        
    //Commande
    int delta = 2;
    
    //on gère la remise à 0 tout les 50J
    if(millis() > lastCmdMs+10000 || millis() <= lastCmdMs){
      lastCmdMs = millis();
      if(T<TRef-delta){
        action = 1;
        digitalWrite(I1OnPin,1);//D9
        digitalWrite(I1Pin,0);//D10
      }
      else if(T>TRef+delta){
        action = -1;
        digitalWrite(I1OnPin,1);//D9
        digitalWrite(I1Pin,1);//D10
      }
      else{
        action = 0;
        digitalWrite(I1OnPin,0);//D9
      }
      //delay(500);
    }
    else{
      action = 2; //continue
    }
    return action;
}
void vanne(){
  lcd.LCD_write_string( 0, 0, "Vanne", MENU_NORMAL);
  lcd.LCD_write_string(78, 2, "C", MENU_NORMAL);
  lcd.LCD_write_string(38, 5, "OK", MENU_HIGHLIGHT );
  int T,TPente;
  char str[10];  
  byte i;
  byte key = 0xFF;
  while (key!= CENTER_KEY){
    for(i=0; i<NUM_KEYS; i++){
      if(button_flag[i] !=0){
        button_flag[i]=0;  // reset button flag
        switch(i){
          case UP_KEY:
            Serial.println("UP");
            TRef++;
            autoTemp = 0;
            break;
          case DOWN_KEY:
            Serial.println("DOWN");
            TRef--;
            autoTemp = 0;
            break;
          case LEFT_KEY:
            Serial.println("LEFT");
            
            break;
          case RIGHT_KEY:
            Serial.println("RIGHT");
            TRef = TPente;
            autoTemp = 1;
            break;    
          case CENTER_KEY:
            Serial.println("CENTER");
            key=CENTER_KEY;           
            break;	
        }
      }
    }
    //temp int et ext
    int Tint = getTNTC(A2);
    itoa(Tint,str,10);
    lcd.LCD_write_string(8, 1, str, MENU_NORMAL);
    
    int Text = getTNTC(A3);
    TPente = courbeChauffeB + (20-Text) * courbeChauffeA;
    if(autoTemp){
      lcd.LCD_write_string( 45, 2, "AUTO", MENU_NORMAL);
    }
    else{
      lcd.LCD_write_string( 45, 2, "MANU", MENU_NORMAL);
    }
    //itoa(T,str,10);
    sprintf(str,"%02d %02d",Text,TPente);
    lcd.LCD_write_string(30, 1, str, MENU_NORMAL);
    
    //Tref et T Nickel
    itoa(TRef,str,10);
    lcd.LCD_write_string( 30, 2, str, MENU_NORMAL);
    T = getT();
    itoa(T,str,10);
    lcd.LCD_write_string(8, 2, str, MENU_NORMAL);
    
    int action = vanne_commande();
    switch(action){
      case 1:
        lcd.LCD_write_string(8, 3, "Ouverture", MENU_NORMAL);
        lcd.LCD_write_string(0, 3, "G", MENU_NORMAL);
        break;
      case 0:
        lcd.LCD_write_string(8, 3, "Arret    ", MENU_NORMAL);
        lcd.LCD_write_string(0, 3, "G", MENU_NORMAL);
        break;
      case -1:
        lcd.LCD_write_string(8, 3, "Fermeture", MENU_NORMAL);
        lcd.LCD_write_string(0, 3, "G", MENU_NORMAL);
        break;
      case 2://continue
        lcd.LCD_write_string(0, 3, " ", MENU_NORMAL);
        break;
    }
    
    delay(100);//stop 100ms
  }  
}
void about(){

  lcd.LCD_write_string( 0, 1, "SYGMAGYR JUNIOR - RVP 31.3", MENU_NORMAL);
  lcd.LCD_write_string( 0, 3, "gui.gros@gmail.com", MENU_NORMAL);
  lcd.LCD_write_string(38, 5, "OK", MENU_HIGHLIGHT );
  waitfor_OKkey();


}



// The followinging are interrupt-driven keypad reading functions
// which includes DEBOUNCE ON/OFF mechanism, and continuous pressing detection


// Convert ADC value to key number
char get_key(unsigned int input)
{
  char k;

  for (k = 0; k < NUM_KEYS; k++)
  {
    if (input < adc_key_val[k])
    {

      return k;
    }
  }

  if (k >= NUM_KEYS)
    k = -1;     // No valid key pressed

  return k;
}

void update_adc_key(){
  int adc_key_in;
  char key_in;
  byte i;

  adc_key_in = analogRead(0);
  key_in = get_key(adc_key_in);
  for(i=0; i<NUM_KEYS; i++)
  {
    if(key_in==i)  //one key is pressed 
    { 
      if(button_count[i]<DEBOUNCE_MAX)
      {
        button_count[i]++;
        if(button_count[i]>DEBOUNCE_ON)
        {
          if(button_status[i] == 0)
          {
            button_flag[i] = 1;
            button_status[i] = 1; //button debounced to 'pressed' status
          }

        }
      }

    }
    else // no button pressed
    {
      if (button_count[i] >0)
      {  
        button_flag[i] = 0;	
        button_count[i]--;
        if(button_count[i]<DEBOUNCE_OFF){
          button_status[i]=0;   //button debounced to 'released' status
        }
      }
    }

  }
}

// Timer2 interrupt routine -
// 1/(160000000/256/(256-6)) = 4ms interval

ISR(TIMER2_OVF_vect) {  
  TCNT2  = 6;
  update_adc_key();
}





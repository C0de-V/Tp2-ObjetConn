#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

int pinServo        = 26;  
int pinMoteurAvant  = 13;  
int pinMoteurRecul  = 14;  
int pinLedCligno    = 18;  
int pinLedPhares    = 23;  
int pinBoutonCligno = 32;
int pinBoutonKlaxon = 5;
int pinBuzzer       = 4;

LiquidCrystal_I2C lcd(0x27, 16, 2); //Addresse hexadecimal, 16 char par ligne, 2 lignes
Servo monServo;

float vitesse = 0.0;
bool clignotantActif = false;  
bool pharesAllumes = false;    

// Dessins personnalisés
byte dessinFlecheHaut[8] = { B00100, B01110, B10101, B00100, B00100, B00100, B00100, B00000 };
byte dessinFlecheBas[8]  = { B00100, B00100, B00100, B00100, B10101, B01110, B00100, B00000 };
byte dessinAmpoule[8]    = { B01110, B10001, B10001, B10001, B01010, B01110, B01110, B00100 };
byte dessinPhares[8]     = { B11000, B11101, B11110, B11110, B11110, B11101, B11000, B00000 };

int lireCapteur(int canal) {
  byte commande = 0;
  if (canal == 0) commande = 0x84; //Direction Y
  if (canal == 1) commande = 0xC4; //DirectionX
  if (canal == 2) commande = 0x94; //photo resistance

  Wire.beginTransmission(0x48);
  Wire.write(commande);
  Wire.endTransmission();

  Wire.requestFrom(0x48, 1);
  if (Wire.available()) {
    return Wire.read(); 
  }
  return 127; 
}


void tacheMoteur(void *param) {
  while (true) {

    int joyY = lireCapteur(0); 

    if (joyY < 100) { 
      float acceleration = map(joyY, 100, 0, 0, 50) / 100.0;
      vitesse = vitesse + acceleration; 
    } 
    else if (joyY > 150) {
      float acceleration = map(joyY, 150, 255, 0, 30) / 100.0;
      vitesse = vitesse - acceleration; 
    } 
    else {
      if (vitesse > 0) {
        vitesse = vitesse - 0.1;
        if (vitesse < 0) vitesse = 0; 
      } 
      else if (vitesse < 0) {
        vitesse = vitesse + 0.1;
        if (vitesse > 0) vitesse = 0;
      }
    }

    if (vitesse > 100) vitesse = 100;
    if (vitesse < -50) vitesse = -50;

    if (vitesse > 0) {
      int pwm = map((int)vitesse, 0, 100, 100, 255); 
      analogWrite(pinMoteurAvant, pwm);
      analogWrite(pinMoteurRecul, 0);
    } 
    else if (vitesse < 0) {
      int pwm = map((int)abs(vitesse), 0, 50, 100, 127);
      analogWrite(pinMoteurAvant, 0);
      analogWrite(pinMoteurRecul, pwm);
    } 
    else {
      analogWrite(pinMoteurAvant, 0);
      analogWrite(pinMoteurRecul, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(100)); 
  }
}


void tacheDirection(void *param) {
  while (true) {
    int joyX = lireCapteur(1); 


    int angle = map(joyX, 0, 255, 180, 0);
    monServo.write(angle); 

    vTaskDelay(pdMS_TO_TICKS(250)); 
  }
}


void tacheAccessoires(void *param) {
  int tempsCligno = 0;
  int tempsEcran = 0;

  while (true) {
    //Clignotant
    if (digitalRead(pinBoutonCligno) == LOW) {
      clignotantActif = true;
    } else {
      clignotantActif = false;
    }

    if (clignotantActif) {
      tempsCligno = tempsCligno + 50;
      if (tempsCligno < 250) {
        digitalWrite(pinLedCligno, LOW);
      } else if (tempsCligno < 750) {
        digitalWrite(pinLedCligno, HIGH);
      } else {
        tempsCligno = 0; 
      }
    } else {
      digitalWrite(pinLedCligno, LOW);
      tempsCligno = 0;
    }

    //Phares Automatiques
    int lumiere = lireCapteur(2); 
    
    if (lumiere < 10) pharesAllumes = true;
    if (lumiere > 25) pharesAllumes = false;
    
    digitalWrite(pinLedPhares, pharesAllumes ? HIGH : LOW); 

    //Klaxon
    if (digitalRead(pinBoutonKlaxon) == LOW) {
      tone(pinBuzzer, 440);
    } else {
      noTone(pinBuzzer);
    }

    //Écran LCD
    tempsEcran = tempsEcran + 50;
    if (tempsEcran >= 100) {
      tempsEcran = 0;

      lcd.setCursor(0, 0);
      lcd.print("Vitesse: ");
      lcd.print((int)abs(vitesse));
      lcd.print("   "); 

      // Indicateur de direction
      lcd.setCursor(15, 0);
      if (vitesse > 1) {
        lcd.write(0); // Flèche Avant
      } else if (vitesse < -1) {
        lcd.write(1); // Flèche Arrière
      } else {
        lcd.print(" ");
      }

      // Icône des Phares
      lcd.setCursor(14, 1);
      if (pharesAllumes) {
        lcd.write(3);
      } else {
        lcd.print(" ");
      }

      // Icône de l'Ampoule du clignotant
      lcd.setCursor(15, 1);
      if (clignotantActif) {
        lcd.write(2);
      } else {
        lcd.print(" ");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}


void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); 

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, dessinFlecheHaut);
  lcd.createChar(1, dessinFlecheBas);
  lcd.createChar(2, dessinAmpoule);
  lcd.createChar(3, dessinPhares); 

  pinMode(pinMoteurAvant, OUTPUT);
  pinMode(pinMoteurRecul, OUTPUT);
  pinMode(pinLedCligno, OUTPUT);
  pinMode(pinLedPhares, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);


  pinMode(pinBoutonCligno, INPUT_PULLUP);
  pinMode(pinBoutonKlaxon, INPUT_PULLUP);

  monServo.attach(pinServo);


  xTaskCreate(tacheMoteur, "Moteur", 2048, NULL, 1, NULL); //ft, nom, memoire Ram en KO, param entrees, priorite,  Handle si on veut pauser ou delete dans le future
  xTaskCreate(tacheDirection, "Direction", 2048, NULL, 1, NULL);
  xTaskCreate(tacheAccessoires, "Accessoires", 2048, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(NULL); // Détruit la boucle de base pour laisser FreeRTOS gérer
}
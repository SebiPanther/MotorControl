#include <SoftwareSerial.h>
#include <NintendoExtensionCtrl.h>

const int SpeedOverallMax = 1000;
const int SpeedOverallZero = 0;
const int SpeedOverallMin = -1000;
const int SpeedIncreasement = 2;
const int SpeedDecreasementNormal = 2;
const int SpeedDecreasementFast = 5;
const int SpeedDecreasementMax = 10;

const int ReverseMax = 0;
const int SpeedZeroPosMin = 126;
const int SpeedZeroPosMax = 130;
const int ForwardMax = 255;

const int SteerZeroPosMin = 125;
const int SteerZeroPosMax = 129;

Nunchuk nunchuk; //A4/A5
SoftwareSerial SerialBoard(10, 11); //D10/D11

void setup() {
  Serial.begin(115200);
  SerialBoard.begin(115200);
  nunchuk.begin(); //D4/D5
}


boolean readSuccess = false;
boolean firstConnection = true;
int currentSpeed = SpeedOverallZero;
void loop()
{
  
  //Wurde bei der letzten Runde die Nunchuck noch nicht Verbunden?
  if(!readSuccess)
  {
    //Dann verbinde
    readSuccess = nunchuk.connect(); //Connect controller
    firstConnection = true;
    
  }
  
  if(readSuccess)
  {
    //Wenn Nunchuck verbunden ist versuche Daten zu lesen
    readSuccess = nunchuk.update(); // Get new data from the controller
    
    if(readSuccess)
    {
      //Wenn Daten Auslesen funktioniert hat, lese die Werte ein
      int joyY = 127; //Vor/Zurück
      int joyX = 127; //Links/Rechts
      boolean zButton = false; //Tot Mann Schalter

      joyY = nunchuk.joyY();
      joyX = nunchuk.joyX();
      zButton = nunchuk.buttonZ();
      
      //Wenn wir im ersten Connect sind (firstConnection == true) muss Joystick zentiert sein und Totmannschalter gedrückt)
      if(firstConnection &&
        zButton &&
        joyY >= SpeedZeroPosMin &&
        joyY <= SpeedZeroPosMax &&
        joyX >= SteerZeroPosMin &&
        joyX <= SteerZeroPosMax)
      {
        //Todmann Schalter gedrück, Joystick in der mitte - ab jetzt darf gesteuert werden
        firstConnection = false;
      }

      //Nur wenn gesteuert werden darf
      if(!firstConnection &&
        zButton)
      {
        //Soll langsam gebremst werden? (Joystick mittig)
        if(joyY >= SpeedZeroPosMin &&
          joyY <= SpeedZeroPosMax)
        {
          //Fahren wir vorwärts?
          if(currentSpeed > SpeedOverallZero)
          {
            //stehen wir schon?
            if(currentSpeed - SpeedDecreasementNormal <= SpeedOverallZero)
            {
              currentSpeed = SpeedOverallZero;
            }
            else
            {
              currentSpeed -= SpeedDecreasementNormal;
            }
          }
          else
          {
            //Fahren Rückwärts
            //stehen wir schon?
            if(currentSpeed + SpeedDecreasementNormal > SpeedOverallZero)
            {
              currentSpeed = SpeedOverallZero;
            }
            else
            {
              currentSpeed += SpeedDecreasementNormal;
            }
          }
        }
        else
        {
          //Ist der Joystick leicht nach vorne gedrückt und wir fahren Rückwärts
          if(joyY > SpeedZeroPosMax &&
            joyY < ForwardMax &&
            currentSpeed < SpeedOverallZero)
          {
            //Minimum schon erreicht?
            if(currentSpeed + SpeedDecreasementFast >= SpeedOverallZero)
            {
              currentSpeed = SpeedOverallZero;
              firstConnection = true;
            }
            else
            {
              currentSpeed += SpeedDecreasementFast;
            }
          }
          else
          {
            //Ist der Joystick leicht nach hinten gedrückt und wir fahren vorwärts
            if(joyY < SpeedZeroPosMin &&
              joyY > ReverseMax &&
              currentSpeed > SpeedOverallZero)
            {
              //Minimum schon erreicht?
              if(currentSpeed - SpeedDecreasementFast <= SpeedOverallZero)
              {
                currentSpeed = SpeedOverallZero;
                firstConnection = true;
              }
              else
              {
                currentSpeed -= SpeedDecreasementFast;
              }
            }
            else
            {
              //Ist Joystick ganz nach vorne gedrück?
              if(joyY >= ForwardMax)
              {
                //Fahren wir vorwärts?
                if(currentSpeed >= SpeedOverallZero)
                {
                  //Maximum schon erreicht?
                  if(currentSpeed + SpeedIncreasement > SpeedOverallMax)
                  {
                    currentSpeed = SpeedOverallMax;
                  }
                  else
                  {
                    currentSpeed += SpeedIncreasement;
                  }
                }
                else
                {
                  //Wenn wir rückswärts fahren
                  //Stop schon erreicht?
                  if(currentSpeed + SpeedDecreasementMax >= SpeedOverallZero)
                  {
                    currentSpeed = SpeedOverallZero;
                    firstConnection = true;
                  }
                  else
                  {
                    currentSpeed += SpeedDecreasementMax;
                  }
                }
              }
              else
              {
                //Ist Joystick ganz nach hinten gedrück?
                if(joyY <= ReverseMax)
                {
                  //Fahren wir rückwärts?
                  if(currentSpeed <= SpeedOverallZero)
                  {
                    //Minimum schon erreicht?
                    if(currentSpeed - SpeedIncreasement < SpeedOverallMin)
                    {
                      currentSpeed = SpeedOverallMin;
                    }
                    else
                    {
                      currentSpeed -= SpeedIncreasement;
                    }
                  }
                  else
                  {
                    //Wir fahren vorwärts
                    //Stop schon erreich?
                    if(currentSpeed - SpeedDecreasementMax <= SpeedOverallZero)
                    {
                      currentSpeed = SpeedOverallZero;
                      firstConnection = true;
                    }
                    else
                    {
                      currentSpeed -= SpeedDecreasementMax;
                    }
                  }
                }
              }
            }
          }
        }
      }
      else
      {
        //Totmannschalter nicht gedrückt, Controller ab, irgendwas stimmt nicht: Bremsen!
        currentSpeed = SpeedOverallZero;
      }
    }
  }

  Serial.println(currentSpeed);
  delay(10);
}

//W2MMD Satellite Antenna Manager
/* Objective: switch 70cm, 2m antennas appropriately depending upon:
 *  a) user selection of a satellite in PST Rotator software via frequency sent via CAT command
 *  b) user selection of buttons "Mode", "CAT off" and "RX only" from controller box */

// Pins for relay output
#define RelayPin2 47 //49
#define RelayPin70 49 //47
#define PreampRelayPin 45
// Relay pin low = SDR radio

// Pins for button controllers
#define btnModeChange 4
#define btnFreqControl 5
#define btnRxOnly 3

// Pins for LEDs
#define LEDYaesu70cm 11 //green
#define LEDYaesu2Meter 13 //red
#define LEDSDR70cm 12 //yellow
#define LEDSDR2meter 10 //blue
#define LEDMode 9 //white
#define LEDFreqControlDisabled 8 // yellow
#define LEDRxOnly 7 // red

#define Debug 1 // debug mode

//Pin for TXD from 847
#define TXD847 A0 

int OpMode; //0 = vu, 1 = uv, 2= receive only
int LastOpMode = 0; // Hold the last op mode to leep from repeating the same op mode
int SerialInt = 0; //incoming ASCII value
int LastChar = 0;//Last character sent
int FreqControlEnabled = 1;

// CAT Message
byte byteSerialCAT; // incoming chars from PstRotator CAT commands
byte bCATelements[5] = {0x00,0x00,0x00,0x00,0x00};
String sDebugCAT = "";
int iCounter = 0;

void setup() {
  pinMode (RelayPin2, OUTPUT);
  pinMode (RelayPin70, OUTPUT);
  pinMode (PreampRelayPin, OUTPUT);

  pinMode (LEDYaesu70cm, OUTPUT);
  pinMode (LEDYaesu2Meter, OUTPUT);
  pinMode (LEDSDR70cm, OUTPUT);
  pinMode (LEDSDR2meter, OUTPUT);
  pinMode (LEDMode, OUTPUT);
  pinMode (LEDFreqControlDisabled,OUTPUT);
  pinMode (LEDRxOnly, OUTPUT);

  pinMode (btnModeChange, INPUT_PULLUP);
  pinMode (btnFreqControl, INPUT_PULLUP);
  pinMode (btnRxOnly, INPUT_PULLUP);

  Serial.begin(9600);
  Serial1.begin (57600,SERIAL_8N1);

  // Flash all LEDS
  digitalWrite (LEDYaesu70cm, HIGH);
  digitalWrite (LEDYaesu2Meter, HIGH);
  digitalWrite (LEDSDR70cm, HIGH);
  digitalWrite (LEDSDR2meter, HIGH);
  digitalWrite (LEDMode, HIGH);
  digitalWrite (LEDFreqControlDisabled, HIGH);
  digitalWrite (LEDRxOnly, HIGH);
  delay (2000);
  digitalWrite (LEDYaesu70cm, LOW);
  digitalWrite (LEDYaesu2Meter, LOW);
  digitalWrite (LEDSDR70cm, LOW);
  digitalWrite (LEDSDR2meter, LOW);
  digitalWrite (LEDMode, LOW);
  digitalWrite (LEDFreqControlDisabled, LOW);
  digitalWrite (LEDRxOnly, LOW);
  OpMode = 2; // Start out in receive-only 
  
}

void CheckButtons()
{
  // Mode button 
  digitalWrite (LEDMode,LOW); //momentary
  if (digitalRead (btnModeChange) == LOW)
     {
     digitalWrite (LEDMode,HIGH);
     OpMode = abs(OpMode - 1); // toggle OpMode
     // If we manually use the mode button, assume we don't want CAT On
     FreqControlEnabled=0; 
     digitalWrite (LEDFreqControlDisabled,HIGH);   
     delay(200);
     }

  // CAT off button (frequency control)
  if (digitalRead (btnFreqControl) == LOW)
    {
      if (FreqControlEnabled == 1) // if currently enabled, toggle to disabled
      {
        FreqControlEnabled=0; // toggle to disabled
        digitalWrite (LEDFreqControlDisabled,HIGH);
      }
      else
      {
        FreqControlEnabled=1; // toggle to enabled
        digitalWrite (LEDFreqControlDisabled,LOW); //Freq control LED off
      } 
      delay(100);
    }  

  // RX Only button
  if (digitalRead (btnRxOnly) == LOW)
    {
      OpMode = 2;
      FreqControlEnabled=0; // Disable freq control - otherwise CAT will override
      digitalWrite (LEDFreqControlDisabled,HIGH);   
    }
  //delay (500); // give time to release button
  }

void SwitchAntennas ()
{
  // turn off LEDs, then turn on what we need depending upon relay states
  digitalWrite (LEDYaesu70cm,LOW);
  digitalWrite (LEDSDR70cm, LOW);  
  digitalWrite (LEDYaesu2Meter,LOW);
  digitalWrite (LEDSDR2meter, LOW); 
  digitalWrite (LEDRxOnly, LOW);

  if (OpMode == 0) // mode vu
    {
    digitalWrite (RelayPin2, HIGH); //Transmit on 2 meters
    digitalWrite (RelayPin70, LOW); // Receive on 70 CM
    digitalWrite (PreampRelayPin, LOW); //Preamp on
    digitalWrite (LEDSDR70cm, HIGH);  
    digitalWrite (LEDYaesu2Meter,HIGH);
    if (Debug==1 && OpMode != LastOpMode) Serial.println ("Mode is vu");
    }
  if (OpMode == 1)// mode uv
  {
    digitalWrite (RelayPin2, LOW); //Receive on 2 meters
    digitalWrite (RelayPin70, HIGH); // Transmit on 70 CM
    digitalWrite (PreampRelayPin, HIGH); //Preamp off
    digitalWrite (LEDYaesu70cm,HIGH);
    digitalWrite (LEDSDR2meter, HIGH); 
    if (Debug==1 && OpMode != LastOpMode) Serial.println ("Mode is uv");
  }
  if (OpMode == 2)// receive only
  {
    digitalWrite (RelayPin2, HIGH); //Receive on 2 meters
    digitalWrite (RelayPin70, HIGH); // Receive on 70 CM
    digitalWrite (PreampRelayPin, LOW); //Preamp on
    digitalWrite (LEDSDR70cm, HIGH);  
    digitalWrite (LEDSDR2meter, HIGH);  
    digitalWrite (LEDRxOnly, HIGH);
    if (Debug==1 && OpMode != LastOpMode) Serial.println ("Mode is RX-Only");
  }
  LastOpMode = OpMode;
  delay (100);
}

// CAT command is five bytes. Upon start-up, make sure we get synchronized w/PSTRotator
// PSTR sends status info, 0xF7, 0xE7, 0x03 constantly, so look for one of those to get in sync w/byte stream
void BuildCAT ()
{
  iCounter = 0;
  do {
    if (Serial1.available() > 0)
    {
      byteSerialCAT = Serial1.read();
      // It's possible for the 5 byte CAT command to be out of sequence.  Check to see if a typically transmitted OpCode 
      // is in the wrong position.  Reset the counter and build the next command correctly.
      if (iCounter < 4 && (byteSerialCAT == 0xE7 || byteSerialCAT == 0xF7 ))      
        { 
          if (Debug == 0) 
          {
              Serial.print("OOS: position ");Serial.print(iCounter);Serial.print(". ByteVal ");Serial.println(byteSerialCAT, HEX);Serial.println("-Partial CAT array-");
              for (int j=0; j<iCounter; j++)
              {  
              Serial.println(bCATelements[j],HEX);
              }
              Serial.println(byteSerialCAT,HEX);
          }    
          iCounter = 0; 
         }    
      else
      // In-sync
         {
         bCATelements[iCounter] = byteSerialCAT;
         iCounter++; 
         }
    }
  } while (iCounter <5);
 
  // Build a debug message
  sDebugCAT = "";
  char cFormattedByte[8];
  for (int i=0; i<5; i++)
  {
    //Serial.println(bCATelements[i],HEX);
    sprintf(cFormattedByte, "%2X", bCATelements[i]);
    sDebugCAT = sDebugCAT + " " + String(cFormattedByte);
  }
//  if (bCATelements[4] == 0xE7 || bCATelements[4] == 0xF7 || bCATelements[4] == 0x03  || bCATelements[4] == 0x07)
//    {
//      // do nothing; i.e. suppress standard status messages from cluttering display
//    }
//    else 
//    {
      Serial.println(sDebugCAT);
//    }

  // Set the OpMode based on the CAT command
  if (bCATelements[4] == 0x01) //CAT: Opcode 0x01 "Set Main VFO"
  {
    if (bCATelements[0] == 0x13 || bCATelements[0] == 0x14) // 2 meters
    {
      OpMode = 0; // set VU
    }
    else if (bCATelements[0] == 0x43) // 70 cm
    {
      OpMode = 1; // set UV
    }
    else if (bCATelements[0] == 0x28) // 10 meters
    {
      OpMode = 0; // set VU
    }
  }
  else if (bCATelements[4] == 0x07 || bCATelements[4] == 0x03 || bCATelements[4] == 0x00) 
  {
    /* do nothing: 
     *  0x07-Operating mode Set to Main VFO
     *  0x03-Frequency & Mode status Read Main VFO
     *  0x00-CAT On status message
      */
  }
  SwitchAntennas();
}

void loop()
{
  if (FreqControlEnabled==1) // Determine which CAT command is sent and what OpMode to set, if any
     { BuildCAT (); }
  CheckButtons ();
  SwitchAntennas();
   if (Debug==1) Serial.println ("Looping");
}

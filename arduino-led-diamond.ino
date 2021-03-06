// V2.0a
// This version is now supporting access the four separate led strips separately allowing for more control.
// Added a rest implementation but not quite done yet.
// Add state support using bits to control combination of the three switches (Allows for 9 states).

#include "FastLED.h"

//#include <Dhcp.h>
//#include <Dns.h>
//#include <EthernetUdp.h>

#include <string.h>
#include <SPI.h>
#include <Ethernet.h>

#include <aREST.h>

// Create aREST instance
aREST rest = aREST();

// Ethernet server
EthernetServer server(80);

// Declare functions to be exposed to the API
int ledControl(String command);

//******************************************************************************************
//******************************************************************************************
// SmartThings Library for Arduino Ethernet W5100 Shield
//******************************************************************************************
#include <SmartThingsEthernetW5100.h>    //Library to provide API to the SmartThings Ethernet W5100 Shield

//******************************************************************************************
// ST_Anything Library
//******************************************************************************************
#include <Constants.h>       //Constants.h is designed to be modified by the end user to adjust behavior of the ST_Anything library
#include <Device.h>          //Generic Device Class, inherited by Sensor and Executor classes
#include <Executor.h>        //Generic Executor Class, typically receives data from ST Cloud (e.g. Switch)
#include <Everything.h>      //Master Brain of ST_Anything library that ties everything together and performs ST Shield communications
//#include <Sensor.h>          //Generic Sensor Class, typically provides data to ST Cloud (e.g. Temperature, Motion, etc...)

//#include <IS_Contact.h>      //Implements an Interrupt Sensor (IS) to monitor the status of a digital input pin

//Implements an Interrupt Sensor (IS) and Executor to monitor the status of a digital input pin and control a digital output pin
//#include <IS_Button.h>       //Implements an Interrupt Sensor (IS) to monitor the status of a digital input pin for button presses
#include <EX_Switch.h>       //Implements an Executor (EX) via a digital output to a relay

//"RESERVED" pins for W5100 Ethernet Shield - best to avoid
#define PIN_4_RESERVED            4   //reserved by W5100 Shield on both UNO and MEGA
#define PIN_1O_RESERVED           10  //reserved by W5100 Shield on both UNO and MEGA
#define PIN_11_RESERVED           11  //reserved by W5100 Shield on UNO
#define PIN_12_RESERVED           12  //reserved by W5100 Shield on UNO
#define PIN_13_RESERVED           13  //reserved by W5100 Shield on UNO
#define PIN_50_RESERVED          50  //reserved by W5500 Shield on MEGA
#define PIN_51_RESERVED          51  //reserved by W5500 Shield on MEGA
#define PIN_52_RESERVED          52  //reserved by W5500 Shield on MEGA
#define PIN_53_RESERVED          53  //reserved by W5500 Shield on MEGA

#define PIN_SWITCH_1              15  //SmartThings Capability "Switch"
#define PIN_SWITCH_2              16 //SmartThings Capability "Switch"
#define PIN_SWITCH_3              17  //SmartThings Capability "Switch"

#define PIN_CONTACT_1             26  //SmartThings Capability "Contact Sensor"

//******************************************************************************************
//W5100 Ethernet Shield Information ce:4d:c6:02:47:4b
//******************************************************************************************

byte mac[] = {0x06, 0x4d, 0xc6, 0x02, 0x47, 0x4b}; //MAC address, leave first octet 0x06, change others to be unique //  <---You must edit this line!
//IPAddress ip(192, 168, 1, 133);               //Arduino device IP Address                   //  <---You must edit this line!
//IPAddress gateway(192, 168, 1, 1);            //router gateway                              //  <---You must edit this line!
//IPAddress subnet(255, 255, 255, 0);           //LAN sceubnet mask                             //  <---You must edit this line!
//IPAddress dnsserver(192, 168, 1, 1);          //DNS server                                  //  <---You must edit this line!
const unsigned int serverPort = 8090;         // port to run the http server on

/// Smartthings hub information
IPAddress hubIp(192, 168, 1, 51);            // smartthings hub ip                         //  <---You must edit this line!
const unsigned int hubPort = 39500;           // smartthings hub port


// How many leds are in the strip?
#define NUM_LEDS_PER_STRIP 21
#define NUM_STRIPS 4

// Data pin that led data will be written out over
#define DATA_PIN1 5
#define DATA_PIN2 8
#define DATA_PIN3 7
#define DATA_PIN4 6

// This is an array of leds.  One item for each led in your strip.

CRGB leds[NUM_STRIPS][NUM_LEDS_PER_STRIP];



//define a max set of colors that pattern generator functions can use.
//default to a fixed size of 4 colors if more are needed can increase.
CRGB colorsToUse[4];

//number of colors set in the colorsToUser array.
int colorCount = 1;



byte state = 0;

byte SWITCH1 = 1; //001
byte SWITCH2 = 2; //010
byte SWITCH3 = 4; //100



typedef void (*FuncPtr)(CRGB colors[], int count, int mode);  //typedef 'return type' (*FuncPtr)('arguments')
//FuncPtr drawPatterns[]={&chasingToCenterWithTail, &chasingToCenterThreeColorTail, &cylonEffect};
FuncPtr curDrawPattern = 0;


// This function sets up the leds and tells the controller about them
void setup() {


  // Function to be exposed
  rest.function("led", ledControl);
  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("008");
  rest.set_name("diamond_controller");

  server.begin();

  //******************************************************************************************
  //  FASTLED setup
  //******************************************************************************************
  // sanity check delay - allows reprogramming if accidently blowing power w/leds
  delay(1000);



  FastLED.addLeds<TM1803, DATA_PIN1, RGB>(leds[0], NUM_LEDS_PER_STRIP);
  FastLED.addLeds<TM1803, DATA_PIN2, RGB>(leds[1], NUM_LEDS_PER_STRIP);
  FastLED.addLeds<TM1803, DATA_PIN3, RGB>(leds[2], NUM_LEDS_PER_STRIP);
  FastLED.addLeds<TM1803, DATA_PIN4, RGB>(leds[3], NUM_LEDS_PER_STRIP);

  //FastLED.setBrightness(84);


  //******************************************************************************************
  //  ST_Anything setup
  //Declare each Device that is attached to the Arduino
  //  Notes: - For each device, there is typically a corresponding "tile" defined in your
  //           SmartThings Device Hanlder Groovy code, except when using new COMPOSITE Device Handler
  //         - For details on each device's constructor arguments below, please refer to the
  //           corresponding header (.h) and program (.cpp) files.
  //         - The name assigned to each device (1st argument below) must match the Groovy
  //           Device Handler names.  (Note: "temphumid" below is the exception to this rule
  //           as the DHT sensors produce both "temperature" and "humidity".  Data from that
  //           particular sensor is sent to the ST Hub in two separate updates, one for
  //           "temperature" and one for "humidity")
  //         - The new Composite Device Handler is comprised of a Parent DH and various Child
  //           DH's.  The names used below MUST not be changed for the Automatic Creation of
  //           child devices to work properly.  Simply increment the number by +1 for each duplicate
  //           device (e.g. contact1, contact2, contact3, etc...)  You can rename the Child Devices
  //           to match your specific use case in the ST Phone Application.
  //******************************************************************************************
  //Executors
  static st::EX_Switch              executor1(F("switch1"), PIN_SWITCH_1, LOW, true);
  static st::EX_Switch              executor2(F("switch2"), PIN_SWITCH_2, LOW, true);
  static st::EX_Switch              executor3(F("switch3"), PIN_SWITCH_3, LOW, true);

  //static st::IS_Contact             sensor11(F("contact1"), PIN_CONTACT_1, LOW, true, 500);

  //*****************************************************************************
  //  Configure debug print output from each main class
  //*****************************************************************************
  st::Everything::debug = true;
  st::Executor::debug = true;
  st::Device::debug = true;
  //st::PollingSensor::debug=true;
  //  st::InterruptSensor::debug = true;

  //*****************************************************************************
  //Initialize the "Everything" Class
  //*****************************************************************************

  //Initialize the optional local callback routine (safe to comment out if not desired)
  st::Everything::callOnMsgSend = callback;

  //Create the SmartThings EthernetW5100 Communications Object
  //STATIC IP Assignment - Recommended
  //st::Everything::SmartThing = new st::SmartThingsEthernetW5100(mac, ip, gateway, subnet, dnsserver, serverPort, hubIp, hubPort, st::receiveSmartString);

  //DHCP IP Assigment - Must set your router's DHCP server to provice a static IP address for this device's MAC address
  st::Everything::SmartThing = new st::SmartThingsEthernetW5100(mac, serverPort, hubIp, hubPort, st::receiveSmartString);

  //Run the Everything class' init() routine which establishes Ethernet communications with the SmartThings Hub
  st::Everything::init();

  //*****************************************************************************
  //Add each sensor to the "Everything" Class
  //*****************************************************************************

  //*****************************************************************************
  //Add each executor to the "Everything" Class
  //*****************************************************************************
  st::Everything::addExecutor(&executor1);
  st::Everything::addExecutor(&executor2);
  st::Everything::addExecutor(&executor3);
  //*****************************************************************************
  //Initialize each of the devices which were added to the Everything Class
  //*****************************************************************************
  st::Everything::initDevices();

}

void fadeall() {
  for (int x = 0; x < NUM_STRIPS; x++ ) {
    for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
      leds[x][i].nscale8(250);
      //leds[i] = CRGB::Black;
    }
  }
}

void alltoblack() {
  alltoColor(CRGB::Black);
}

void alltoColor(CRGB color) {

  Serial.print("Setting all leds to color " );
  Serial.println(color);
  for (int x = 0; x < NUM_STRIPS; x++ ) {
    for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
      //leds[i].nscale8(250);
      leds[x][i] = color;
    }
  }
}



int loopCount = -1;

byte REDCHASING = SWITCH1;
byte REDWAVING  = SWITCH1 | SWITCH3;
byte BLUECHASING = SWITCH2;
byte CYCLON      = SWITCH3;
byte FIRECHASING = SWITCH1 | SWITCH2;
byte REDSPLIT = SWITCH2 | SWITCH3;
byte PURPLETAILS = SWITCH1 | SWITCH2 | SWITCH3;

byte lastState = -1;

void loop() {



  // listen for incoming clients
  EthernetClient client = server.available();

  rest.handle(client);

  //*****************************************************************************
  //Execute the Everything run method which takes care of "Everything"
  //*****************************************************************************
  st::Everything::run();

  // curdrawPattern = drawPattern[1] // assign somewhere else

  // invoke the previous setup drawing pattern

  if (loopCount > 0) {
    loopCount = loopCount - 1;
  } else if (loopCount == 0) {
    curDrawPattern = 0;
    loopCount = -1;
    state = 0;
    alltoblack();
    FastLED.show();
  }

  if (curDrawPattern != 0 ) {
    curDrawPattern(colorsToUse, colorCount, 0);
  }

}


void handleStateChange(byte newState) {
  state = newState;

  // whenever a state changes reset everything back to black
  // Will also handle 0 state.
  curDrawPattern = 0;
  alltoblack();
  colorCount = 0;
  FastLED.show();
    

  if ( state == REDCHASING) {
    int colorOffset = 0;
    colorsToUse[0] = CHSV(colorOffset, 255, 255);
    colorsToUse[1] = CHSV(colorOffset, 255, 55);
    colorCount = 2;
    curDrawPattern = &chasingToCenterWithTail;
  }  else if ( state == BLUECHASING) {
    int colorOffset = 120;
    colorsToUse[0] = CHSV(colorOffset, 255, 255);
    colorsToUse[1] = CHSV(colorOffset, 255, 155);
    colorCount = 2;
    curDrawPattern = &chasingToCenterWithTail;
    Serial.println("Turnin on blue chasing lights");
  } else if (state == CYCLON) {
    curDrawPattern = &cylonEffect;
  } else if (state == FIRECHASING) {
    //// lightning yellow is 215
    //// yellow 225
    int colorOffset = 225;
    colorsToUse[0] = CHSV(255, 255, 255);
    colorsToUse[1] = CHSV(255, 255, 155); // 155 before
    colorsToUse[2] = CHSV(colorOffset, 255, 200);
    colorsToUse[3] = CHSV(colorOffset, 255, 150);
    colorCount = 4;
    curDrawPattern = &chasingToCenterWithTail;
  } else if ( state == REDWAVING) {
    int colorOffset = 0;
    colorsToUse[0] = CHSV(colorOffset, 255, 55);
    colorsToUse[1] = CHSV(colorOffset, 255, 255);
    colorsToUse[2] = CHSV(colorOffset, 255, 55);
    colorCount = 3;
    curDrawPattern = &wavingToCenterWithTail;
  } else if ( state == REDSPLIT) {
    int colorOffset = 0;
    colorsToUse[0] = CHSV(colorOffset, 255, 55);
    colorsToUse[1] = CHSV(colorOffset, 255, 255);
    colorsToUse[2] = CHSV(colorOffset, 255, 55);
    colorCount = 3;
    curDrawPattern = &splitToCenterTail;
  } else if ( state == PURPLETAILS) {
    int colorOffset = 70;
    colorsToUse[0] = CHSV(120, 255, 255);
    colorsToUse[1] = CHSV(colorOffset, 255, 155);
    colorsToUse[2] = CHSV(colorOffset, 255, 55);
    colorCount = 3;
    curDrawPattern = &tailsGoingOut;
  }


}


// Custom function accessible by the API
// /led?state=001&loopCount=1
int ledControl(String command) {
  Serial.println("rest call :" + command);

  command.replace("%20", " ");

  int sepIndex = command.indexOf('&');

  String cmd = command.substring(0, sepIndex);

  byte newState = toByteState(cmd);


  String secPart = command.substring(sepIndex + 1);
  int eqIndex = secPart.indexOf('=');
  loopCount = secPart.substring(eqIndex + 1).toInt();

  Serial.println("Got cmd :" + cmd);
  Serial.print("Got count :");
  Serial.println(loopCount);

  handleStateChange(newState);

  //callback(cmd);
  //colorsToUse[0] = CHSV(colorOffset, 255, 255);
  //colorsToUse[0] = 0x command

  return 1;

}

byte toByteState(String str) {
  byte result = 0;
  for (int i = 0; i < str.length(); i++ ) {
    byte b = str.substring(i, i + 1).toInt();
    result = result << 1;
    result = result | b;
  }
  return result;
}

//wil move a dot along with the first color as the main one and the next ones as trailing colors.
// Can take as many colors as you want
void chasingToCenterWithTail(CRGB colors[], int count, int mode) {

  //int colorCount = count;

  //Serial.print("chasingToCenterWith Tail :");
  //Serial.println(count);


  //want to allow series of colors to run off the end.
  // at the very end past zero we dont set the color but still need to set the last color as black again.
  for (int i = NUM_LEDS_PER_STRIP - 1; i >= 0 - count; i--) {

    for (int x = 0; x < NUM_STRIPS; x++ ) {
      // Turn our current led on to white, then show the leds
      //leds[i] = CRGB::Green;
      //leds[i] = CRGB::Yellow;
      for (int ci = 0; ci < count && (i + ci) < NUM_LEDS_PER_STRIP; ci++) {
        if (i + ci >= 0) {
          leds[x][i + ci] = colors[ci];

          //FastLED.show();
          //delay(214);
        }


        //         Serial.print("looping over leds ");
        //         Serial.print(x);
        //         Serial.print(",");
        //         Serial.print(i);
        //         Serial.print(",");
        //         Serial.println(ci);

      }
      // lightning yellow is 215
      // yellow 225

      //only need to set a single one to black since we shift by one color offset each time. Even with trailing colors.
      int ce = i + count;
      if (ce >= 0 && ce < NUM_LEDS_PER_STRIP ) {
        leds[x][ce] =  CRGB::Black;
      }
    }



    //fadeall();
    // Wait a little bit

    delay(14);
    // Show the leds (only one of which is set to white, from above)
    FastLED.show();

  }

}


//wil move a dot along with the first color as the main one and the next ones as trailing colors.
// Can take as many colors as you want
void wavingToCenterWithTail(CRGB colors[], int count, int mode) {

  //int colorCount = count;

  //Serial.print("wavingToCenterWith Tail :");
  //Serial.println(count);


  //want to allow series of colors to run off the end.
  // at the very end past zero we dont set the color but still need to set the last color as black again.
  for (int i = NUM_LEDS_PER_STRIP - 1; i >= 0 - count; i--) {

    for (int x = 0; x < NUM_STRIPS; x++ ) {
      // Turn our current led on to white, then show the leds
      //leds[i] = CRGB::Green;
      //leds[i] = CRGB::Yellow;
      for (int ci = 0; ci < count && (i + ci) < NUM_LEDS_PER_STRIP; ci++) {
        if (i + ci >= 0) {
          leds[x][i + ci] = colors[ci];

          //FastLED.show();
          //delay(214);
        }
      }

      //only need to set a single one to black since we shift by one color offset each time. Even with trailing colors.
      int ce = i + count;
      if (ce >= 0 && ce < NUM_LEDS_PER_STRIP ) {
        leds[x][ce] =  CRGB::Purple;
      }

      delay(7);
      // Show the leds (only one of which is set to white, from above)
      FastLED.show();
    }
  }

}


void splitToCenterTail(CRGB colors[], int count, int mode) {
  int colorCount = sizeOfCRGB(colors);


  alltoColor(CHSV(225, 255, 200));


 
    int i = NUM_LEDS_PER_STRIP - 1;
    int e = 0 ;
for (int x = 0; x < NUM_STRIPS; x++ ) {
    while ( e < i && (e < NUM_LEDS_PER_STRIP / 4 || i > NUM_LEDS_PER_STRIP / 4) ) {
 
      // Turn our current led on to white, then show the leds
      //leds[i] = CRGB::Green;
      //leds[i] = CRGB::Yellow;


      for (int ci = 0; ci < count && (i + ci) >= NUM_LEDS_PER_STRIP / 4; ci++) {
        leds[x][i + ci] = colors[ci];
      }

      for (int ci = 0; ci < count && (e - ci) >= 0; ci++) {
        leds[x][e - ci] = colors[ci];
      }
      // lightning yellow is 215
      // yellow 225


      // Turn our current led back to black for the next loop around
      //for (int ci = 0; ci < colorCount && (i + ci) >= NUM_LEDS_PER_STRIP/4; ci++) {
      //   leds[x][i + ci] = CRGB::Black;
      //   leds[x][e - ci] = CRGB::Black;
      //}
 
      // Show the leds (only one of which is set to white, from above)
      FastLED.show();
 
      //fadeall();
      // Wait a little bit
      delay(14);


      i--;
      e++;

    }


    while (e > 0 &&  i < NUM_LEDS_PER_STRIP)  {
 
      // Turn our current led on to white, then show the leds
      //leds[i] = CRGB::Green;
      //leds[i] = CRGB::Yellow;

      //for (int ci = 0; ci < count && (i - ci) < NUM_LEDS_PER_STRIP; ci++) {
      leds[x][i] = CHSV(225, 255, 200);
      //}

      //for (int ci = 0; ci < count && (e + ci) >= NUM_LEDS_PER_STRIP/4; ci++) {
      leds[x][e] = CHSV(225, 255, 200);
      //}

      // lightning yellow is 215
      // yellow 225


      // Turn our current led back to black for the next loop around
      //for (int ci = 0; ci < colorCount && (i + ci) >= NUM_LEDS_PER_STRIP/4; ci++) {
      //   leds[x][i + ci] = CRGB::Black;
      //   leds[x][e - ci] = CRGB::Black;
      //}
       
      // Show the leds (only one of which is set to white, from above)
      FastLED.show();

      //fadeall();
      // Wait a little bit
      delay(14);


      i++;
      e--;

    }
}
  
}


void tailsGoingOut(CRGB colors[], int count, int mode) {

 for (int i = NUM_LEDS_PER_STRIP - 1; i >= 0 - count; i--) {

    for (int x = 0; x < NUM_STRIPS; x++ ) {
      // Turn our current led on to white, then show the leds
      //leds[i] = CRGB::Green;
      //leds[i] = CRGB::Yellow;
      for (int ci = 0; ci < count && (i + ci) < NUM_LEDS_PER_STRIP; ci++) {
        if (i + ci >= 0) {
          leds[x][i + ci] = colors[ci];

          //FastLED.show();
          //delay(214);
        }

      }
      // lightning yellow is 215
      // yellow 225

      //only need to set a single one to black since we shift by one color offset each time. Even with trailing colors.
      int ce = i + count;
      if (ce >= 0 && ce < NUM_LEDS_PER_STRIP ) {
        leds[x][ce] =  colors[count-1];
      }

      Serial.print("First Loop :" );
      Serial.println(i);
      
    }



    //fadeall();
    // Wait a little bit

    delay(17);
    // Show the leds (only one of which is set to white, from above)
    FastLED.show();

 }

 delay(14);
 
 for (int i = 0 - count; i <  NUM_LEDS_PER_STRIP; i++) {

    for (int x = 0; x < NUM_STRIPS; x++ ) {
      // Turn our current led on to white, then show the leds
      //leds[i] = CRGB::Green;
      //leds[i] = CRGB::Yellow;
      for (int ci = 0; ci < count && (i + ci) < NUM_LEDS_PER_STRIP; ci++) {
        if (i + ci < NUM_LEDS_PER_STRIP && i + ci >= 0) {
          leds[x][i + ci] = colors[ci];

          //FastLED.show();
          //delay(214);
        }

      }
      // lightning yellow is 215
      // yellow 225

      //only need to set a single one to black since we shift by one color offset each time. Even with trailing colors.
      int ce = i -1;
      if (ce >= 0 && ce < NUM_LEDS_PER_STRIP ) {
        leds[x][ce] =  CRGB::Black;
      }

      Serial.print("Second Loop :" );
      Serial.println(i);
    }



    //fadeall();
    // Wait a little bit

    delay(17);
    // Show the leds (only one of which is set to white, from above)
    FastLED.show();

 }
    
 
  
}




//******************************************************************************************
//st::Everything::callOnMsgSend() optional callback routine.  This is a sniffer to monitor
//    data being sent to ST.  This allows a user to act on data changes locally within the
//    Arduino sktech withotu having to rely on the ST Cloud for time-critical tasks.
//******************************************************************************************
void callback(const String &msg)
{

  byte newState = state;

  // lightning yellow is 215
  // yellow 225
  //the light pattern to use.
  int colorOffset = 0;

  Serial.println("callback :" + msg);

  if (msg.indexOf("on") > -1) {
    if (msg.indexOf("switch1") > -1) {
      newState = newState | SWITCH1;
    } else if (msg.indexOf("switch2") > -1 ) {
      newState = newState | SWITCH2;
    } else if (msg.indexOf("switch3") > -1) {
      newState = newState | SWITCH3;
    }
  } else if (msg.indexOf("off") > -1) {
    if (msg.indexOf("switch1") > -1) {
      newState = newState & ~SWITCH1;
    } else if (msg.indexOf("switch2") > -1) {
      newState = newState & ~SWITCH2;
    } else if (msg.indexOf("switch3") > -1) {
      newState = newState & ~SWITCH3;
    }
  }

  Serial.print("Changing state from ");
  Serial.print(state);
  Serial.print(" to ");
  Serial.println(newState);

  handleStateChange(newState);

  //st::receiveSmartString("switch1 off");
  //st::receiveSmartString("switch2 off");

  //Uncomment if it weould be desirable to using this function
  //Serial.print(F("ST_Anything_Miltiples Callback: Sniffed data = "));
  //Serial.println(msg);

  //TODO:  Add local logic here to take action when a device's value/state is changed

  //Masquerade as the ThingShield to send data to the Arduino, as if from the ST Cloud (uncomment and edit following line(s) as you see fit)
  //st::receiveSmartString("Put your command here!");  //use same strings that the Device Handler would send

  //st::receiveSmartString("Put your command here!");  //use same strings that the Device Handler would send

}


void cylonEffect(CRGB colors[], int count,  int mode) {
  static uint8_t hue = 0;
  Serial.print("x");

  for (int x = 0; x < NUM_STRIPS; x++ ) {
    // First slide the led in one direction
    for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
      // Set the i'th led to red
      leds[x][i] = CHSV(hue++, 255, 255);
      // Show the leds
      FastLED.show();
      // now that we've shown the leds, reset the i'th led to black
      // leds[i] = CRGB::Black;
      if (mode == 1) {
        alltoblack();
      } else {
        fadeall();
      }
      // Wait a little bit before we loop around and do it again

    }
    Serial.print("x");
    delay(10);
  }


  for (int x = 0; x < NUM_STRIPS; x++ ) {
    // Now go in the other direction.
    for (int i = NUM_LEDS_PER_STRIP; i >= 0; i--) {
      // Set the i'th led to red
      leds[x][i] = CHSV(hue++, 255, 255);
      // Show the leds
      FastLED.show();
      // now that we've shown the leds, reset the i'th led to black
      // leds[i] = CRGB::Black;
      fadeall();


    }
    // Wait a little bit before we loop around and do it again
    delay(10);
  }




}

int sizeOfInt(int a[]) {
  return (sizeof(a) / sizeof(int));
}

int sizeOfCRGB(CRGB colors[]) {
  return (sizeof(colors) / sizeof(CRGB));
}

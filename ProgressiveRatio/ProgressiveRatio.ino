/*****************************************************************
Jim Lindblom @ SparkFun Electronics
Original Creation Date: July 3, 2014

This sketch uses an Arduino Uno to POST sensor readings to 
SparkFun's data logging streams (http://data.sparkfun.com). A post
will be initiated whenever pin 3 is connected to ground.

Before uploading this sketch, there are a number of global vars
that need adjusting:
1. Ethernet Stuff: Fill in your desired MAC and a static IP, even
   if you're planning on having DCHP fill your IP in for you.
   The static IP is only used as a fallback, if DHCP doesn't work.
2. Phant Stuff: Fill in your data stream's public, private, and 
data keys before uploading!

Hardware Hookup:
  * These components are connected to the Arduino's I/O pins:
    * D3 - Active-low momentary button (pulled high internally)
    * A0 - Photoresistor (which is combined with a 10k resistor
           to form a voltage divider output to the Arduino).
    * D5 - SPDT switch to select either 5V or 0V to this pin.
  * A CC3000 Shield sitting comfortable on top of your Arduino.

Development environment specifics:
    IDE: Arduino 1.0.5
    Hardware Platform: RedBoard & PoEthernet Shield

This code is beerware; if you see me (or any other SparkFun 
employee) at the local, and you've found our code helpful, please 
buy us a round!

Much of this code is largely based on David Mellis' WebClient
example in the Ethernet library.

Distributed as-is; no warranty is given.
*****************************************************************/
#include <SPI.h> // Required to use Ethernet
#include <Ethernet.h> // The Ethernet library includes the client

#define Sbegin(a) (Serial.begin(a))
#define Sprintln(a) (Serial.println(a))
#define Sprint(a) (Serial.print(a))

//#define Sbegin(a)
//#define Sprintln(a)
//#define Sprint(a)


///////////////////////
// Ethernet Settings //
///////////////////////
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0xAA, 0x7D };
//char server[] = "10.35.76.195";
//IPAddress ip(10,35,76,195);

char server[] = "10.35.76.99";
IPAddress ip(10,35,76,99);

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

//////////////////////
// Input Pins, Misc //
//////////////////////
// constants won't change. They're used here to 
// set pin numbers:
const int boardID = 1;      // board ID-- has to change per board
//const int reward_duration = 250; // reward duration in milliseconds

const int buttonPin = 3;    // control of the button to start the ratio/reset
const int lickPin = 8;      // input that signifies licking
const int ledPin =  9;      // the number of the LED pin
const int rewardPin = 5;    // output of ensure
const int grnPin = 2;

int init_time = -1;         // time of the program initialization

int experiment_id = 1;      // experiment ID
int pulled_values = 0;      // number of values pulled from the server
String var_name = "";       // string to use for storing input
String var_val = "";        // string to use for storing input
int on_name = 1;            // are we reading in the name or the value?

float scalar_mult = -1;
float scalar_add = -1;
int timeout = -1;
int reward_duration = 250; // reward duration in milliseconds

int lickstep = 0;
int licks_this_step = 0;
int licks_required_step = 1;

int mr_led_blinky = -1;
int mode = 0;                // 0-init, 1-setup, 2-recording
int last_lick = -1;
bool activelyLicking = LOW;
int buttonDown = 0;

void setup() {
  Sbegin(115200);

  // initialize all of the pins
  pinMode(ledPin, OUTPUT); 
  pinMode(grnPin, OUTPUT); 
  pinMode(rewardPin, OUTPUT);  
  pinMode(buttonPin,INPUT); // initialize the button
  pinMode(lickPin, INPUT); // initialize the lick pin as an input
  digitalWrite(grnPin, HIGH);

  // Set Up Ethernet:
  setupEthernet();

  Sprintln(F("=========== Ready to Stream ==========="));
  Sprintln(F("Press the button (D3) to send an update"));
  digitalWrite(grnPin, LOW);
}

void loop() {
  if (pulled_values < 5) {getData();}
  else {
    
    // If the trigger pin (3) goes low, send the data.
    if (digitalRead(buttonPin) && buttonDown == 0) {
      buttonDown = 1;
      mode = (mode + 1)%3;
      Sprint("mode");
      Sprintln(mode);
      init_time = millis();
      if (mode == 0) {
        digitalWrite(ledPin, HIGH);
        digitalWrite(grnPin, LOW);
        pulled_values = 0;
        getData();
      }
      else if (mode == 1) {
        digitalWrite(ledPin, LOW);
        digitalWrite(grnPin, HIGH);
        delay(150);
        digitalWrite(grnPin, LOW);
      }
      else if (mode == 2) {
        digitalWrite(ledPin, HIGH);
//        digitalWrite(grnPin, LOW);
      }
      
      Sprint("Beginning mode ");
      Sprintln(mode);
      delay(20);
    }
    else if (!digitalRead(buttonPin) && buttonDown == 1) {
      buttonDown = 0;
      delay(20);
    }
    
    if (mode == 2) {
      bool sendlick = isLick(digitalRead(lickPin));
      showlick();
      if (sendlick) {lick();}
    }
    else if (mode == 1) {
      isLick(digitalRead(lickPin));
      showlick();
    }
    else {
      if (mr_led_blinky < 0) {
        mr_led_blinky = millis();
      }
      if (((millis() - mr_led_blinky)/1000)%2 == 0) {
        digitalWrite(ledPin, HIGH);
      }
      else {
        digitalWrite(ledPin, LOW);
      }
    }
  }
}

bool isLick(bool rd) {
  if (!rd && activelyLicking) {
    activelyLicking = LOW;
  }
  
  if (rd && !activelyLicking && millis() - last_lick > 75) {
    last_lick = millis();
    activelyLicking = HIGH;
    return HIGH;
  }
  else {
    return LOW;
  }
}

void lick() {
  Sprintln("Licked!");
  licks_this_step += 1;
  if (licks_this_step >= licks_required_step) {
    reward();
    postData(licks_required_step);
    lickstep += 1;
    licks_this_step = 0;
    licks_required_step = progressiveRatio();
  }
  else {postData(-1);}
}

void showlick() {
  if (activelyLicking || millis() - last_lick < 75) {
    digitalWrite(grnPin, HIGH);
  }
   else {
     digitalWrite(grnPin, LOW);
   }
}

void reward() {
    digitalWrite(rewardPin, HIGH);
    digitalWrite(ledPin, LOW);
    delay(reward_duration);
    Sprint("Reward given, ");
    Sprint("required lick #:");
    Sprintln(licks_required_step);
    digitalWrite(rewardPin, LOW);
    digitalWrite(ledPin, HIGH);
    delay(timeout);
}

//find out how many licks need this time
int progressiveRatio() {
  return (int) round(scalar_add*pow(lickstep, scalar_mult) + 1.0);
}

void getData() {
  Sprintln("Getting data...");
  Sprintln();
  
  if (client.connect(server, 80)) {
    client.print("GET /cgi-bin/pull.py?bid=");
    client.print(boardID);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close");
    client.println();
  }
  else {Sprintln(F("Initial connection failed"));} 
    
  while (client.connected() && pulled_values < 5) {
    if (client.available() && pulled_values < 5) {
      char c = client.read();
      if (c == ',' || c == '\n') {
        on_name = 1;
        processVariableData(var_name, var_val);
        var_name = "";
        var_val = "";
      }
      else if (c == '=') {
        on_name = 0;
      }
      else {
        if (on_name == 1) {
          var_name += c;
        }
        else {
          var_val += c;
        }
      }
    }      
  }
  
  if (var_name.length() > 0 && var_val.length() > 0) {
    on_name = 1;
    processVariableData(var_name, var_val);
    var_name = "";
    var_val = "";
  }
  
  Sprintln();
  client.stop();
}

void processVariableData(String varname, String varval) {
  if (varname.equals("eid")) {
    experiment_id = varval.toInt();
    Sprint("EID: ");
    Sprintln(experiment_id);
    pulled_values += 1;
  }
  else if (varname.equals("A")) {
    scalar_mult = varval.toFloat();
    Sprint("A: ");
    Sprintln(scalar_mult);
    pulled_values += 1;
  }
  else if (varname.equals("B")) {
    scalar_add = varval.toFloat();
    Sprint("B: ");
    Sprintln(scalar_add);
    pulled_values += 1;
  }
  else if (varname.equals("timeout")) {
    timeout = varval.toInt();
    Sprint("Timeout: ");
    Sprintln(timeout);
    pulled_values += 1;
  }
  else if (varname.equals("rewardduration")) {
    reward_duration = varval.toInt();
    Sprint("Reward Duration: ");
    Sprintln(reward_duration);
    pulled_values += 1;
  }
}

void postData(int reward) {
  // Make a TCP connection to remote host
  if (client.connect(server, 80)) {
    // Post the data! Request should look a little something like:
    // GET /input/publicKey?private_key=privateKey&light=1024&switch=0&name=Jim HTTP/1.1\n
    // Host: data.sparkfun.com\n
    // Connection: close\n
    // \n
    
    client.print("GET /cgi-bin/push.py?eid=");
    client.print(experiment_id);
    client.print("&lick=");
    client.print(millis() - init_time);
    
    if (reward > -1) {
      client.print("&reward=");
      client.print(reward);
    }

    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close");
    client.println();
  }
  else {Sprintln(F("Connection failed"));} 

  // Check for a response from the server, and route it
  // out the serial port.
/*  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.print(c);
    }      
  }*/
  
  Sprintln();
  client.stop();
}

void setupEthernet() {
  Sprintln("Setting up Ethernet...");
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Sprintln(F("Failed to configure Ethernet using DHCP"));
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  
  Sprint("My IP address: ");
  Sprintln(Ethernet.localIP());
  // give the Ethernet shield a second to initialize:
  delay(1000);
}


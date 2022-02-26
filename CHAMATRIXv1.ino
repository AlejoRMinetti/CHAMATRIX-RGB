#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>

#include <FastLED.h>

#define LED_PIN    D7
#define BRIGHTNESSMAX  255
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

const uint8_t kMatrixWidth  = 18;
const uint8_t kMatrixHeight = 18;
const bool    kMatrixSerpentineLayout = true;

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)

// The leds
CRGB leds[kMatrixWidth * kMatrixHeight];

// The 16 bit version of our coordinates
static uint16_t x;
static uint16_t y;
static uint16_t z;

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward.  Try
// 1 for a very slow moving effect, or 60 for something that ends up looking like
// water.
uint16_t speed = 1; // speed is set dynamically once we've started up

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise iwll be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
uint16_t scale = 60; // scale is set dynamically once we've started up

// This is the array that we keep our computed noise values in
uint8_t noise[MAX_DIMENSION][MAX_DIMENSION];

CRGBPalette16 currentPalette( PartyColors_p );
uint8_t       colorLoop = 1;

//brillo
#define TIMEDIMMER  100
uint8_t brillo=0;

const byte led = BUILTIN_LED;

/////////// wifi & web

ESP8266WiFiMulti wifiMulti;      // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

WiFiUDP UDP;                     // Create an instance of the WiFiUDP class to send and receive

const char *OTAName = "CHAMATRIX"; // A name and a password for the OTA service
const char *OTAPassword = "rgb";

IPAddress timeServerIP;          // time.nist.gov NTP server address
const char* NTPServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message

byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

/*__________________________________________________________SETUP__________________________________________________________*/

void setup() {
  // blinker led
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(led, 0); // led ON
  
  // Start the Serial communication to send messages to the computer
  Serial.begin(115200);         
  delay(10);

  startWiFi(); // Try to connect to some given access points. Then wait for a connection

  startOTA();  // Start the OTA service

  startUDP(); // for get NTP time

  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
  
  Serial.println("\r\nSending NTP request ...");
  sendNTPpacket(timeServerIP);  

  ///////////////// MATRIX LED setup
  LEDS.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds,NUM_LEDS);
  LEDS.setBrightness(brillo);

  // Initialize our coordinates to some random values
  x = random16();
  y = random16();
  z = random16();

  // blink finish setup
  digitalWrite(led, !digitalRead(led));  // Change the state of the LED
  delay(200);
  digitalWrite(led, !digitalRead(led));  // Change the state of the LED
  delay(200);
  digitalWrite(led, !digitalRead(led));  // Change the state of the LED
  delay(200);
  digitalWrite(led, 1); // led OFF

}

/*__________________________________________________________LOOP__________________________________________________________*/

unsigned long intervalNTP = 60000; // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;
uint32_t OffSetTimeUNIX = -3600*3; //compensar para hora local

unsigned long prevActualTime = 0;

int segs = 11, mins = 0, horas = 0;

void loop() {
  ArduinoOTA.handle();   // listen for OTA events

  ///////////// Get time
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // If a minute has passed since last NTP request
    prevNTP = currentMillis;
    Serial.println("\r\nSending NTP request ...");
    sendNTPpacket(timeServerIP);               // Send an NTP request
  }

  uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
  if (time) {                                  // If a new timestamp has been received
    timeUNIX = time + OffSetTimeUNIX;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
  } else if ((currentMillis - lastNTPResponse) > 3600000) {
    Serial.println("More than 1 hour since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse)/1000;
  if (actualTime != prevActualTime && timeUNIX != 0) { // If a second has passed since last print
    prevActualTime = actualTime;
    horas = getHours(actualTime);
    mins = getMinutes(actualTime);
    segs = getSeconds(actualTime);
    Serial.printf("\rUTC time:\t%d:%d:%d   \n", horas, mins, segs);
  }  

  ////////////// Fondos animados MATRIX RGB

	// Periodically choose a new palette, speed, and scale
	ChangePaletteAndSettingsPeriodically();
	// generate noise data
	fillnoise8();
	// convert the noise data to colors in the LED array
	// using the current palette
	mapNoiseToLEDsUsingPalette();
	//hace que sea un encendido gradual
	brilloControl();

  // sobreescribir sobre fondo
  //mostrarX();
  mostrarClock();

	LEDS.show(); //refresh matrix
}

////////////// Reloj binario

void mostrarBit(int digito,int xShow){
  for(int i = 0; i < 4; i++){
    if( bitRead(digito,i) )
      leds[XY(xShow,1+i*2)] = CRGB::Blue;
  }
}

const unsigned char numToMatrix[10][5] = { { B010, B101, B101, B101, B010 },//0
{ B010, B010, B010, B110, B010 },//1
{ B111, B100, B111, B001, B111 },//2
{ B111, B001, B111, B001, B111 },//3
{ B001, B001, B111, B101, B101 },//4
{ B111, B001, B111, B100, B111 },//5
{ B111, B101, B111, B100, B110 },//6
{ B010, B010, B011, B001, B111 },//7
{ B111, B101, B111, B101, B111 },//8
{ B011, B001, B111, B101, B111 } };//9


void mostrarNum(int digito,int xShow){
  
  for(int j=0; j<5; j++){
    for(int i=0; i<3; i++){
      if( bitRead( numToMatrix[digito][j] , i) )
        leds[XY(xShow-i,12+j)] = CRGB::Blue;
    }
  } 
}

void mostrarClock(){
  //segundos
  int digito = segs % 10;
  uint8_t xShow = 16;
  mostrarBit(digito,xShow);
  //puntos separadores de HH:MM
  if( bitRead(digito,0) ){
    leds[XY(15-6,15)] = CRGB::Blue;
    leds[XY(15-6,13)] = CRGB::Blue;
  }
  //decena de segundos
  digito = segs / 10;
  xShow = 14;
  mostrarBit(digito,xShow);

  //minutos
  digito = mins % 10;
  xShow = 12;
  mostrarBit(digito,xShow-2);
  mostrarNum(digito,17);
  digito = mins / 10;
  xShow = 10;
  mostrarBit(digito,xShow-2);
  mostrarNum(digito,13);

  //horas
  digito = horas % 10;
  xShow = 8;
  mostrarBit(digito,xShow-4);
  mostrarNum(digito,13-6);
  digito = horas / 10;
  xShow = 6;
  mostrarBit(digito,xShow-4);
  mostrarNum(digito,3);

}

/////////////////////// rotando una X

bool estadoX = true;
// Rotar X
void rotarX(){
	if(estadoX){
    estadoX = false;
	}else{
    estadoX = true;
  }
}

//mostrar X
void mostrarX(){

	if(estadoX){
	//probando x cercana a 0,0
	  leds[XY(1,1)] = CRGB::Blue;
	  leds[XY(2,2)] = CRGB::Blue;
	  leds[XY(3,3)] = CRGB::Blue;
	  leds[XY(3,1)] = CRGB::Blue;
	  leds[XY(1,3)] = CRGB::Blue;
	}else{
	  //probando -|- cercana a 0,0
	  leds[XY(2,1)] = CRGB::Blue;
	  leds[XY(2,2)] = CRGB::Blue;
	  leds[XY(2,3)] = CRGB::Blue;
	  leds[XY(1,2)] = CRGB::Blue;
	  leds[XY(3,2)] = CRGB::Blue;
  }
}

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi() { // Try to connect to some given access points. Then wait for a connection
  wifiMulti.addAP("ssid1", "passwd1");
  wifiMulti.addAP("ssid2", "passwd2");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());             // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  Serial.println("\r\n");
}

void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages on port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();
}

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode

  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}

//////// Control general del brillo: va aumentado gradualmente

void brilloControl(){
	  if( brillo < BRIGHTNESSMAX ){
		  int segundos = millis()/100 ;
		  brillo = map(segundos, 0,TIMEDIMMER*10, 0,BRIGHTNESSMAX);
		  LEDS.setBrightness(brillo);
	  }else if( brillo == BRIGHTNESSMAX ){
		  // no se hace nada una vez llegado al brillo maximo
	  }else{
		  brillo = BRIGHTNESSMAX;
		  LEDS.setBrightness(brillo);
	  }
}

///////////// Funciones para fondos de la MATRIX RGB

unsigned long previousTime = millis();
const unsigned long interval = 300;
// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  // If we're runing at a low "speed", some 8-bit artifacts become visible
  // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
  // The amount of data smoothing we're doing depends on "speed".
  uint8_t dataSmoothing = 0;
  if( speed < 50) {
    dataSmoothing = 200 - (speed * 4);
  }

  for(int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    for(int j = 0; j < MAX_DIMENSION; j++) {
      int joffset = scale * j;

      uint8_t data = inoise8(x + ioffset,y + joffset,z);

      // The range of the inoise8 function is roughly 16-238.
      // These two operations expand those values out to roughly 0..255
      // You can comment them out if you want the raw noise data.
      data = qsub8(data,16);
      data = qadd8(data,scale8(data,39));

      if( dataSmoothing ) {
        uint8_t olddata = noise[i][j];
        uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
        data = newdata;
      }

      noise[i][j] = data;
    }
  }

  z += speed;

  // apply slow drift to X and Y, just for visual variation.
  x += speed / 8;
  y -= speed / 16;
}

void mapNoiseToLEDsUsingPalette(){
  static uint8_t ihue=0;

  for(int i = 0; i < kMatrixWidth; i++) {
    for(int j = 0; j < kMatrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.

      uint8_t index = noise[j][i];
      uint8_t bri =   noise[i][j];

      // if this palette is a 'loop', add a slowly-changing base value
      if( colorLoop) {
        index += ihue;
      }

      // brighten up, as the color palette itself often contains the
      // light/dark dynamic range desired
      if( bri > 127 ) {
        bri = 255;
      } else {
        bri = dim8_raw( bri * 2);
      }

      CRGB color = ColorFromPalette( currentPalette, index, bri);
      leds[XY(i,j)] = color;
    }
  }

  ihue+=1;
}

//se define tiempo de encendido en segundos
#define TIMEDIMMER 30


// There are several different palettes of colors demonstrated here.
//
// FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
// OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.
//
// Additionally, you can manually define your own color palettes, or you can write
// code that creates color palettes on the fly.

// 1 = 5 sec per palette
// 2 = 10 sec per palette
// etc
#define HOLD_PALETTES_X_TIMES_AS_LONG 2

void ChangePaletteAndSettingsPeriodically(){

  uint8_t secondHand = ((millis() / 1000) / HOLD_PALETTES_X_TIMES_AS_LONG) % 60;
  static uint8_t lastSecond = 99;

  if( lastSecond != secondHand) {
    lastSecond = secondHand;
    if( secondHand ==  0)  { currentPalette = RainbowColors_p;         speed = 10; scale = 30; colorLoop = 1; }
    if( secondHand ==  5)  { SetupPurpleAndGreenPalette();             speed = 10; scale = 50; colorLoop = 1; }
    if( secondHand == 10)  { SetupBlackAndWhiteStripedPalette();       speed = 10; scale = 30; colorLoop = 1; }
    if( secondHand == 15)  { currentPalette = ForestColors_p;          speed =  2; scale =120; colorLoop = 0; }
    if( secondHand == 20)  { currentPalette = CloudColors_p;           speed =  1; scale = 40; colorLoop = 0; }
    if( secondHand == 25)  { currentPalette = LavaColors_p;            speed =  4; scale = 50; colorLoop = 0; }
    if( secondHand == 30)  { currentPalette = OceanColors_p;           speed = 5; scale = 90; colorLoop = 0; }
    if( secondHand == 35)  { currentPalette = PartyColors_p;           speed = 10; scale = 30; colorLoop = 1; }
    if( secondHand == 40)  { SetupRandomPalette();                     speed = 10; scale = 20; colorLoop = 1; }
    if( secondHand == 45)  { SetupRandomPalette();                     speed = 25; scale = 50; colorLoop = 1; }
    if( secondHand == 50)  { SetupRandomPalette();                     speed = 40; scale = 90; colorLoop = 1; }
    if( secondHand == 55)  { currentPalette = RainbowStripeColors_p;   speed = 10; scale = 20; colorLoop = 1; }
  }
}

// This function generates a random palette that's a gradient
// between four different colors.  The first is a dim hue, the second is
// a bright hue, the third is a bright pastel, and the last is
// another bright hue.  This gives some visual bright/dark variation
// which is more interesting than just a gradient of different hues.
void SetupRandomPalette(){
  currentPalette = CRGBPalette16(
                      CHSV( random8(), 255, 32),
                      CHSV( random8(), 255, 255),
                      CHSV( random8(), 128, 255),
                      CHSV( random8(), 255, 255));
}

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette(){
  // 'black out' all 16 palette entries...
  fill_solid( currentPalette, 16, CRGB::Black);
  // and set every fourth one to white.
  currentPalette[0] = CRGB::White;
  currentPalette[4] = CRGB::White;
  currentPalette[8] = CRGB::White;
  currentPalette[12] = CRGB::White;

}

// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette(){
  CRGB purple = CHSV( HUE_PURPLE, 255, 255);
  CRGB green  = CHSV( HUE_GREEN, 255, 255);
  CRGB black  = CRGB::Black;

  currentPalette = CRGBPalette16(
    green,  green,  black,  black,
    purple, purple, black,  black,
    green,  green,  black,  black,
    purple, purple, black,  black );
}

//
// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
//
uint16_t XY( uint8_t x, uint8_t y){
  uint16_t i;
  if( kMatrixSerpentineLayout == false) {
    i = (y * kMatrixWidth) + x;
  }
  if( kMatrixSerpentineLayout == true) {
    if( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (kMatrixWidth - 1) - x;
      i = (y * kMatrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * kMatrixWidth) + x;
    }
  }
  return i;
}



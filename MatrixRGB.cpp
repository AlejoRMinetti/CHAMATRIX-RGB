#include "MatrixRGB.h"

#include <FastLED.h>

#define LED_PIN     D7
#define BRIGHTNESSMAX  255
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)

//brillo inicial
#define ONDIMMER  100

const uint8_t kMatrixWidth  = 18;
const uint8_t kMatrixHeight = 18;
const bool    kMatrixSerpentineLayout = true;

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

// The leds
CRGB leds[kMatrixWidth * kMatrixHeight];

// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
uint16_t XY( uint8_t x, uint8_t y);

MatrixRGB::MatrixRGB()
    :brillo(0), MatrixON(true)
{}

void MatrixRGB::begin()
{
    LEDS.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds,NUM_LEDS);
    LEDS.setBrightness(brillo);
      // Initialize our coordinates to some random values
    x = random16();
    y = random16();
    z = random16();
}

//refresh cambios
void MatrixRGB::refresh(){
    LEDS.show(); //refresh matrix
}

////////////// Reloj binario

void mostrarBit(int digito,int xShow);
void mostrarNum(int digito,int xShow);

CRGB clockColor;

//set color clock: red, green, and blue
void MatrixRGB::setColorClock(int red, int green, int blue){
  clockColor = CRGB( red, green, blue);
}

// display clocc
void MatrixRGB::mostrarClock(){
  //segundos
  int digito = segs % 10;
  uint8_t xShow = 16;
  mostrarBit(digito,xShow);
  mostrarBit(digito,xShow-1);
  //puntos separadores de HH:MM
  if( bitRead(digito,0) ){
    leds[XY(15-6,15)] = clockColor;
    leds[XY(15-6,13)] = clockColor;
  }
  //decena de segundos
  digito = segs / 10;
  xShow = 13;
  mostrarBit(digito,xShow);
  mostrarBit(digito,xShow-1);

  //minutos
  digito = mins % 10;
  xShow = 12;
  mostrarBit(digito,xShow-3);
  mostrarNum(digito,17);
  digito = mins / 10;
  xShow = 10;
  mostrarBit(digito,xShow-3);
  mostrarNum(digito,13);

  //horas
  digito = horas % 10;
  xShow = 8;
  mostrarBit(digito,xShow-5);
  mostrarNum(digito,13-6);
  digito = horas / 10;
  xShow = 6;
  mostrarBit(digito,xShow-5);
  mostrarNum(digito,3);

}

const unsigned char numToMatrix[10][5] = { 
    { B010, B101, B101, B101, B010 },//0
    { B010, B010, B010, B110, B010 },//1
    { B111, B100, B111, B001, B111 },//2
    { B111, B001, B111, B001, B111 },//3
    { B001, B001, B111, B101, B101 },//4
    { B111, B001, B111, B100, B111 },//5
    { B111, B101, B111, B100, B110 },//6
    { B010, B010, B010, B001, B111 },//7
    { B111, B101, B111, B101, B111 },//8
    { B011, B001, B111, B101, B111 } //9
};

void mostrarBit(int digito,int xShow){
  for(int i = 0; i < 4; i++){
    if( bitRead(digito,i) )
      leds[XY(xShow,1+i*2)] = clockColor;
  }
}

void mostrarNum(int digito,int xShow){
  
  for(int j=0; j<5; j++){
    for(int i=0; i<3; i++){
      if( bitRead( numToMatrix[digito][j] , i) )
        leds[XY(xShow-i,12+j)] = clockColor;
    }
  } 
}

//////// Control general del brillo: va aumentado gradualmente

void MatrixRGB::brilloControl(){
    if( MatrixON == false ){
        LEDS.setBrightness(0);
        return;
    }

	  if( brillo < BRIGHTNESSMAX ){
		  int segundos = millis()/100 ;
		  brillo = map(segundos, 0,ONDIMMER*10, 0,BRIGHTNESSMAX);
		  LEDS.setBrightness(brillo);
	  }else{
		  brillo = BRIGHTNESSMAX;
		  LEDS.setBrightness(brillo);
	  }
}

///////////// Funciones para fondos de la MATRIX RGB

void mapNoiseToLEDsUsingPalette();
void fillnoise8();
void SetupRandomPalette();
void SetupBlackAndWhiteStripedPalette();
void SetupPurpleAndGreenPalette();
void ChangePaletteAndSettingsPeriodically();
// update fondo para mostrar luego de refresh
void MatrixRGB::fondoUpdate(){
	// Periodically choose a new palette, speed, and scale
	ChangePaletteAndSettingsPeriodically();
	// generate noise data
	fillnoise8();
	// convert the noise data to colors in the LED array
	// using the current palette
	mapNoiseToLEDsUsingPalette();
	//hace que sea un encendido gradual
	brilloControl();
}

//////////////////// local funtions para el fondo

#define HOLD_PALETTES_X_TIMES_AS_LONG 2
unsigned long previousTime = millis();
const unsigned long interval = 300;

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

// Mark's xy coordinate mapping code.  See the XYMatrix for more information on it.
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
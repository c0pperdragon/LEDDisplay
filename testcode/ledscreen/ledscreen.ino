#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "outshifter.pio.h"
#include "image.h"

#define SCREENBUFFER_LEN        (64 * 64 * 20 / 2)   // 1/2 int32 pro pixel anstatt 12 bit, da der DMA nur 8/16/32 Bit übertragen kann
#define SHIFTBLOCK_LEN          (64 * 10 / 2)        // 1/2 int32 * number of shifts

#define ROW_TIMING           33 // microseconds per output block
#define ONTIME_BIT3          32
#define ONTIME_BIT2          16
#define ONTIME_BIT1          8
#define ONTIME_BIT0          4

#define PIN_CLK 0
#define PIN_LAT 1
#define PIN_E 2
#define PIN_OE 3
#define PIN_RGB 4  //Pins GPIO4 - GPIO15

uint32_t  screenbuffer[SCREENBUFFER_LEN];

PIO pioout;       //pioblock for outputshifter
int sm0;          //statemachine for outputshifter
int out_dma_chan; //dma for outputshifter


void clearScreen() {
  memset( screenbuffer, 0, sizeof(screenbuffer) );
}

void writePixel(int x, int y, int r, int g, int b) {
  uint32_t shift;
  uint32_t offs;

  if (x<0 || y<0 || x>=320 || y>=256) { return; }

  if ( y < 32 ) {
    offs = (y-0)*4*SHIFTBLOCK_LEN + x/2;
    shift = 9 + 16*(x%2);
  } else if ( y < 64 ) {
    offs = (y-32)*4*SHIFTBLOCK_LEN + x/2;
    shift = 6 + 16*(x%2);
  } else if ( y < 96 ) {
    offs = (y-64)*4*SHIFTBLOCK_LEN + SHIFTBLOCK_LEN/2 + x/2;
    shift = 9 + 16*(x%2);
  } else if ( y < 128 ) {
    offs = (y-96)*4*SHIFTBLOCK_LEN + SHIFTBLOCK_LEN/2 + x/2;
    shift = 6 + 16*(x%2);
  } else if ( y < 160 ) {
    offs = (y-128)*4*SHIFTBLOCK_LEN + x/2;
    shift = 3 + 16*(x%2);
  } else if ( y < 192 ) {
    offs = (y-160)*4*SHIFTBLOCK_LEN + x/2;
    shift = 0 + 16*(x%2);
  } else if ( y < 224 ) {
    offs = (y-192)*4*SHIFTBLOCK_LEN + SHIFTBLOCK_LEN/2 + x/2;
    shift = 3 + 16*(x%2);
  } else if ( y < 256 ) {
    offs = (y-224)*4*SHIFTBLOCK_LEN + SHIFTBLOCK_LEN/2 + x/2;
    shift = 0 + 16*(x%2);
  } else return;

  for (int i=0; i<4; i++) {
    screenbuffer[offs] |= ( 
      (((r>>i) & 0x01) << (shift+0)) |
      (((g>>i) & 0x01) << (shift+1)) |
      (((b>>i) & 0x01) << (shift+2)) 
    );
    offs += SHIFTBLOCK_LEN;
  }
}

void drawDemoImage() {
  clearScreen();


  int imgoffs = 0;  
  for (int y=0; y<256;y++) {
    for (int x=0; x<320;x++) {

      uint32_t rgb = image[imgoffs];      
      imgoffs++;

      int r = ((rgb & 0x00F00000) >> 20);
      int g = ((rgb & 0x0000F000) >> 12);
      int b = ((rgb & 0x000000F0) >> 4);

      writePixel(x,y, r,g,b);
    }
  }

/*
  for (int y=0; y<256;y++) {
    for (int x=0; x<320; x++) {
      
      int r = x/16;
      int g = y/16;
      int b = 0;
      if (x>=256) {
        r = 0;
        g = 0;
        b = y/16;
      } 
      writePixel(x,y, r,g,b);
    }
  }
  */
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite( LED_BUILTIN, HIGH);

  pinMode(PIN_OE, OUTPUT);
  pinMode(PIN_LAT, OUTPUT);
  pinMode(PIN_E, OUTPUT);

  // set control pins to idle states
  digitalWrite(PIN_OE, HIGH);
  digitalWrite(PIN_LAT, LOW);
  digitalWrite(PIN_E, LOW);

  //Prepare screenbuffer with Demoimage from image.h
  drawDemoImage();
  
  //Setup pio
  pioout = pio0;
  uint offset = pio_add_program(pioout, &outshifter_program);
  sm0 = pio_claim_unused_sm(pioout, true);
  outshifter_program_init(pioout, sm0, offset, PIN_RGB, PIN_CLK);

  //Setup dma
  out_dma_chan = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(out_dma_chan);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_read_increment(&c, true);
  channel_config_set_dreq(&c, DREQ_PIO0_TX0);

  dma_channel_configure(
    out_dma_chan,
    &c,
    &pioout->txf[0],    // Write address (only need to set this once)
    NULL,               // Don't provide a read address yet
    SHIFTBLOCK_LEN,
    false               // Don't start yet
  );

}

void loop() {  
  uint64_t stopTime = time_us_64();
  uint64_t startTime = stopTime;
  int row;
  int colorbit;

  // initialize the row counter for correct startup in state 31
  digitalWrite(PIN_E, LOW);
  digitalWrite(PIN_OE, LOW);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_OE, HIGH);
  for (row=0; row<15; row++)
  {
    digitalWrite(PIN_E, LOW);
    digitalWrite(PIN_E, HIGH);
  }
 
  // display picture forever
  for (;;)
  for (row=0; row<32; row++) 
  for (colorbit=3; colorbit>=0; colorbit--) {
      //Start dma to output the current line of pixels
      dma_channel_set_read_addr(out_dma_chan, screenbuffer + SHIFTBLOCK_LEN*(colorbit+row*4), true);

      // finish displaying the previous pixels
      while ( time_us_64() < stopTime );  
      digitalWrite(PIN_OE, HIGH);
      digitalWrite(PIN_LAT, LOW);

      // progress row selectors before showing most significant bit of this row
      if (colorbit==3) {
        if (row==0) {
          digitalWrite(PIN_E, LOW);
          digitalWrite(PIN_E, HIGH);
          digitalWrite(PIN_E, LOW);
        } else if (row==16) {
          digitalWrite(PIN_E, HIGH);
        } else if (row<16) {
          digitalWrite(PIN_E, HIGH);
          digitalWrite(PIN_E, LOW);
        } else {
          digitalWrite(PIN_E, LOW);
          digitalWrite(PIN_E, HIGH);
        }
      }

      // calculate duration of visibility
      startTime = startTime + ROW_TIMING;
      switch (colorbit) {
      case 0:  stopTime = startTime + ONTIME_BIT0; break;
      case 1:  stopTime = startTime + ONTIME_BIT1; break;
      case 2:  stopTime = startTime + ONTIME_BIT2; break;
      case 3:  stopTime = startTime + ONTIME_BIT3; break;
      }

      // Start displaying the new pixels 
      while ( time_us_64() < startTime );  
      digitalWrite(PIN_LAT, HIGH);  
      digitalWrite(PIN_OE, LOW);
  }
}


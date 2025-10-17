#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#define PICO_NO_HARDWARE 0
#include "outshifter.pio.h"
#include "image.h"

#define SCANLINE_LEN            ((320/8) * 4)       // scanline data in int32 units
#define SCREENBUFFER_LEN        (256*SCANLINE_LEN)   // whole screen

#define SCANLINE_TIMING         65 
#define ONTIME_BIT3             16
#define ONTIME_BIT2             33
#define ONTIME_BIT1             48
#define ONTIME_BIT0             62
#define OFFTIME_BIT3            (ONTIME_BIT3+15)
#define OFFTIME_BIT2            (ONTIME_BIT2+8)
#define OFFTIME_BIT1            (ONTIME_BIT1+4)
#define OFFTIME_BIT0            (ONTIME_BIT0+2)

#define PIN_CLK 0
#define PIN_LAT 1
#define PIN_E 2
#define PIN_OE 3
#define PIN_RGB1 13  //Pins GPIO13 - GPIO15
#define PIN_RGB2 10  //Pins GPI10 - GPIO12
#define PIN_RGB3 7   //Pins GPI7 - GPIO9
#define PIN_RGB4 4   //Pins GPIO4 - GPIO6

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

  shift = 28 - (x % 8) * 4;
  offs = x/8 + y*SCANLINE_LEN;

  for (int i=3; i>=0; i--) {
    screenbuffer[offs] |= ( 
      (((r>>i) & 0x01) << (shift+0)) |
      (((g>>i) & 0x01) << (shift+1)) |
      (((b>>i) & 0x01) << (shift+2)) 
    );
    offs += (SCANLINE_LEN/4);
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

static inline void outshifter_program_init(PIO pio, uint sm, uint pins, uint clkpin, uint latchpin )
{
  int offset = pio_add_program(pio, &outshifter_program);
 	pio_sm_config c = outshifter_program_get_default_config(offset);
  pio_gpio_init(pio, clkpin);
  pio_gpio_init(pio, latchpin);
  for (int i=0;i<3;i++) pio_gpio_init(pio, pins+i);
  pio_sm_set_consecutive_pindirs(pio, sm, clkpin, 2, true);	
	pio_sm_set_consecutive_pindirs(pio, sm, pins, 3, true);	
	sm_config_set_out_pins(&c, pins, 3); // rgb1, rgb2
  sm_config_set_sideset_pins(&c, clkpin); // clkpin, latch
	sm_config_set_clkdiv(&c, 1.0);
  sm_config_set_in_shift(&c, false, false, 0);
  sm_config_set_out_shift(&c, false, false, 32);
	pio_sm_init(pio, sm, offset, &c);
	pio_sm_set_enabled(pio, sm, true);
}

static inline void waituntil(uint64_t t)
{
      while ( time_us_64() < t );  
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite( LED_BUILTIN, HIGH);

  pinMode(PIN_OE, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(17, INPUT);

  // set control pins to idle states
  digitalWrite(PIN_OE, HIGH);
  digitalWrite(PIN_E, LOW);
  digitalWrite(16, LOW);

  //Prepare screenbuffer with Demoimage from image.h
  drawDemoImage();
  
  //Setup pio
  pioout = pio0;
  sm0 = pio_claim_unused_sm(pioout, true);
  outshifter_program_init(pioout, sm0, PIN_RGB4, PIN_CLK, PIN_LAT);

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
    SCANLINE_LEN,
    false               // Don't start yet
  );
  
  noInterrupts();
}

void loop() {  
  uint64_t startTime = time_us_64() + 1;
  int row;

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
  for (row=0; row<32; row++) {
      waituntil(startTime);

      //Start dma to output the current line of pixels
      dma_channel_set_read_addr(out_dma_chan, screenbuffer+SCANLINE_LEN*(70+row), true);
      // Trigger interrupt to continue state machines
      digitalWrite( 16, HIGH);
      digitalWrite( 16, LOW);
//      hw_set_bits(&(pioout->irq_force), 0xff);

      // progress row selectors before showing most significant bit of this row
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

      // turn total light on/off for all 4 bits
      waituntil(startTime + ONTIME_BIT3);
      digitalWrite(PIN_OE, LOW);
      waituntil(startTime + OFFTIME_BIT3);
      digitalWrite(PIN_OE, HIGH);

      waituntil(startTime + ONTIME_BIT2);
      digitalWrite(PIN_OE, LOW);
      waituntil(startTime + OFFTIME_BIT2);
      digitalWrite(PIN_OE, HIGH);

      waituntil(startTime + ONTIME_BIT1);
      digitalWrite(PIN_OE, LOW);
      waituntil(startTime + OFFTIME_BIT1);
      digitalWrite(PIN_OE, HIGH);

      waituntil(startTime + ONTIME_BIT0);
      digitalWrite(PIN_OE, LOW);
      waituntil(startTime + OFFTIME_BIT0);
      digitalWrite(PIN_OE, HIGH);

      // compute timing for next line
      startTime = startTime + SCANLINE_TIMING;
  }
}

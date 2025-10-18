#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#define PICO_NO_HARDWARE 0
#include "outshifter.pio.h"
#include "lightswitcher.pio.h"
#include "image.h"

#define PIN_CLK 0
#define PIN_LAT 1
#define PIN_E 2
#define PIN_OE 3
#define PIN_RGB 4   //Pins GPIO4 - GPIO15

#define SCREENBUFFER_LEN        ((320/2)*4*32)   // screen buffer size in uint32
#define SCANLINE_TIMING         65

PIO pioout;       //pioblock for outputshifter
int sm0,sm1;      //statemachines for outputshifter
int out_dma;      //dma for outputshifter

uint32_t  screenbuffer[SCREENBUFFER_LEN];

void clearScreen() {
  memset( screenbuffer, 0, sizeof(screenbuffer) );
}

void writePixel(int x, int y, int r, int g, int b) {
  uint32_t shift;
  uint32_t offs;

  if (x<0 || y<0 || x>=320 || y>=128) { return; }

  shift = ((x % 2) ? 25 : 9) - (y/32)*3;
  offs = x/2 + (y%32)*(SCREENBUFFER_LEN/32);

  for (int i=3; i>=0; i--) {
    screenbuffer[offs] |= ( 
      (((r>>i) & 0x01) << (shift+0)) |
      (((g>>i) & 0x01) << (shift+1)) |
      (((b>>i) & 0x01) << (shift+2)) 
    );
    offs += (SCREENBUFFER_LEN/32)/4;
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

      writePixel(x,y-70, r,g,b);
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
  pinMode(PIN_OE, OUTPUT);
  pinMode(PIN_E, OUTPUT);

  // initialize the row counter for correct startup in state 31
  digitalWrite(PIN_E, LOW);
  digitalWrite(PIN_OE, LOW);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_OE, HIGH);
  for (int row=0; row<15; row++)
  {
    digitalWrite(PIN_E, LOW);
    digitalWrite(PIN_E, HIGH);
  }
  // set control pins to idle states
  digitalWrite(PIN_OE, HIGH);
  digitalWrite(PIN_E, LOW);
  digitalWrite( LED_BUILTIN, HIGH);

  //Setup pio block and state machines
  pioout = pio0;
  pio_gpio_init(pioout, PIN_CLK);
  pio_gpio_init(pioout, PIN_LAT);
  pio_gpio_init(pioout, PIN_OE);
  for (int i=0;i<12;i++) { pio_gpio_init(pioout, PIN_RGB+i); }
  
  sm0 = pio_claim_unused_sm(pioout, true);
  {
    int o = pio_add_program(pioout, &outshifter_program);
    pio_sm_config c = outshifter_program_get_default_config(o);
    sm_config_set_out_pins(&c, PIN_RGB, 12);
    sm_config_set_sideset_pins(&c, PIN_CLK);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_out_shift(&c, false, false, 32);
    pio_sm_set_consecutive_pindirs(pioout, sm0, PIN_CLK, 2, true);
	  pio_sm_set_consecutive_pindirs(pioout, sm0, PIN_RGB, 12, true);	
    pio_sm_init(pioout, sm0, o, &c);
	  pio_sm_set_enabled(pioout, sm0, true);
  }
  sm1 = pio_claim_unused_sm(pioout, true);
  {
    int o = pio_add_program(pioout, &lightswitcher_program);
    pio_sm_config c = lightswitcher_program_get_default_config(o);
    sm_config_set_sideset_pins(&c, PIN_OE);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_out_shift(&c, false, false, 0);
    pio_sm_set_consecutive_pindirs(pioout, sm1, PIN_OE, 1, true);
    pio_sm_init(pioout, sm1, o, &c);
	  pio_sm_set_enabled(pioout, sm1, true);
  }
  //Setup dma channel
  out_dma = dma_claim_unused_channel(true);
  {
    dma_channel_config c = dma_channel_get_default_config(out_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0);
    dma_channel_configure(
      out_dma,
      &c,
      &pioout->txf[0],     // Write address (only need to set this once)
      NULL,                // Don't provide a read address yet
      SCREENBUFFER_LEN/32, // always write 1 of 32 lines in each segment
      false                // Don't start yet
    );
  }

  //Prepare screenbuffer with Demoimage from image.h
  drawDemoImage();

//  noInterrupts();
}

void loop() {  
  uint64_t startTime = time_us_64() + 1;
  int row;
 
  // display picture forever
  for (;;)
  for (row=0; row<32; row++) {
      // start of all row processing 
      while(time_us_64() < startTime);
      // Start dma to output the current line of pixels
      dma_channel_set_read_addr(out_dma, screenbuffer+(SCREENBUFFER_LEN/32)*row, true);
      // in case previous data was still visible, wait for this
      while(time_us_64() < startTime+8);
      // progress row selectors before most significant bit of this row is shown
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
      // compute timing for next line
      startTime = startTime + SCANLINE_TIMING;
  }
}

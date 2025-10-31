// Needs to run with 200Mhz to have necessary performance.
// Use highest optimization level, tool

#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#define PICO_NO_HARDWARE 0
#include "outshifter.pio.h"
#include "lightswitcher.pio.h"
#include "pixelreader.pio.h"
#include "image.h"

#define PIN_CLK 0
#define PIN_LAT 1
#define PIN_E 2
#define PIN_OE 3
#define PIN_RGB 4   //Pins GPIO4 - GPIO15

#define PIN_DATA 16 // Pins GIPO16 - GPIO22
#define PIN_DEBUG 28

#define SCREENBUFFER_LEN        ((320/2)*4*32)   // screen buffer size in uint32

PIO pioout;       //pioblock for outputshifter
int out_dma;      //dma for outputshifter
PIO pioin;        //pioblock for data reader
int in_dma;      //dma for pixel data receiver

int currentTotalLine = 0;   // line number of the original whole screen
uint32_t screenbuffer[SCREENBUFFER_LEN];
uint32_t readlinebuffer[160];

void distributeLineData(int line, uint32_t* data)
{
  int shift = 9 - (line/32) * 3;
  uint32_t use_mask = 0x00070007u<<shift;
  uint32_t cut_mask = ~use_mask;
  uint32_t* outbuf3 = screenbuffer + (line%32) * (SCREENBUFFER_LEN/32);
  uint32_t* outbuf2 = outbuf3 + 160;
  uint32_t* outbuf1 = outbuf2 + 160;
  uint32_t* outbuf0 = outbuf1 + 160;
  int i=160;
  do  // hand-crafted code to mimic cortex m0 assembly
  {
    uint32_t d = *data;
    uint32_t x,y; 
    data++;
    x = *outbuf3;        // bit 3
    x = x & cut_mask;
    y = d>>9;
    y = y<<shift;
    y = y & use_mask;
    x = x | y;
    *outbuf3 = x;
    outbuf3++;
    x = *outbuf2;        // bit 2
    x = x & cut_mask;
    y = d>>6;
    y = y<<shift;
    y = y & use_mask;
    x = x | y;
    *outbuf2 = x;
    outbuf2++;
    x = *outbuf1;        // bit 1
    x = x & cut_mask;
    y = d>>3;
    y = y<<shift;
    y = y & use_mask;
    x = x | y;
    *outbuf1 = x;
    outbuf1++;
    x = *outbuf0;        // bit 0
    x = x & cut_mask;
    y = y<<shift;
    y = y & use_mask;
    x = x | y;
    *outbuf0 = x;
    outbuf0++;
    i--;
  } 
  while (i);
}

// extract demo picture into screen buffer
void drawDemoImage() 
{
  int imgoffs=320*70;
  memset( screenbuffer, 0x47c3a230, sizeof(screenbuffer) );  // fill with some nonsense

  for (int y=0; y<128;y++) 
  {
    uint32_t linebuffer[160];
    uint32_t d = 0;
    for (int x=0; x<320; x++) 
    {
      uint32_t rgb = image[imgoffs++];      
      d = d | ( ((rgb>>20)&1) << 0);    // R0
      d = d | ( ((rgb>>12)&1) << 1);    // G0
      d = d | ( ((rgb>>4 )&1) << 2);    // B0
      d = d | ( ((rgb>>21)&1) << 3);    // R1
      d = d | ( ((rgb>>13)&1) << 4);    // G1
      d = d | ( ((rgb>>5 )&1) << 5);    // B1
      d = d | ( ((rgb>>22)&1) << 6);    // R2
      d = d | ( ((rgb>>14)&1) << 7);    // G2
      d = d | ( ((rgb>>6 )&1) << 8);    // B2
      d = d | ( ((rgb>>23)&1) << 9);    // R3
      d = d | ( ((rgb>>15)&1) << 10);   // G3
      d = d | ( ((rgb>>7 )&1) << 11);   // B3
      if ((x%2)==0) { d = d << 16; }
      else 
      {
        linebuffer[x/2]=d;
        d=0;
      }
    }
    distributeLineData(y, linebuffer);
  }
}


void lineFinishInterrupt()
{
    if (digitalRead(PIN_DATA)==LOW)
    {
      currentTotalLine=0;
    }
    else
    {
      currentTotalLine++;
    }

    // which rows to show next to have minimum delay, but avoid access conflict
    int row = ((currentTotalLine+29)%32);

    // Start dma to output the pixels of the row 
    dma_channel_set_read_addr(out_dma, screenbuffer+(SCREENBUFFER_LEN/32)*row, true);

    // prepare dma to read the next line
    dma_channel_abort(in_dma);
    dma_channel_set_write_addr(in_dma, readlinebuffer, true);

    // progress row selectors before most significant bit of this row is shown
    if (row==0) 
    {
        digitalWrite(PIN_E, LOW);
        digitalWrite(PIN_E, HIGH);
        digitalWrite(PIN_E, LOW);
    } 
    else if (row==16) 
    {
        digitalWrite(PIN_E, HIGH);
    }
    else if (row<16) 
    {
        digitalWrite(PIN_E, HIGH);
        digitalWrite(PIN_E, LOW);
    } 
    else 
    {
        digitalWrite(PIN_E, LOW);
        digitalWrite(PIN_E, HIGH);
    }

    // decode current line - I hope the CPU will stay ahead the DMA for the next line comming right behind
    int top = 64;
    if (currentTotalLine>=top && currentTotalLine<top+128)
    {
      distributeLineData(currentTotalLine-top, readlinebuffer);
    }
}

void setup() 
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_OE, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_DEBUG, OUTPUT);

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
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(PIN_DEBUG, LOW);

  //Setup pio block and state machines for driving the LEDs
  pioout = pio0;
  pio_gpio_init(pioout, PIN_CLK);
  pio_gpio_init(pioout, PIN_LAT);
  pio_gpio_init(pioout, PIN_OE);
  for (int i=0;i<12;i++) { pio_gpio_init(pioout, PIN_RGB+i); }  
  {
    int sm = pio_claim_unused_sm(pioout, true);
    int o = pio_add_program(pioout, &outshifter_program);
    pio_sm_config c = outshifter_program_get_default_config(o);
    sm_config_set_out_pins(&c, PIN_RGB, 12);
    sm_config_set_sideset_pins(&c, PIN_CLK);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_out_shift(&c, false, false, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    pio_sm_set_consecutive_pindirs(pioout, sm, PIN_CLK, 2, true);
	  pio_sm_set_consecutive_pindirs(pioout, sm, PIN_RGB, 12, true);	
    pio_sm_init(pioout, sm, o, &c);
	  pio_sm_set_enabled(pioout, sm, true);
  }
  {
    int sm = pio_claim_unused_sm(pioout, true);
    int o = pio_add_program(pioout, &lightswitcher_program);
    pio_sm_config c = lightswitcher_program_get_default_config(o);
    sm_config_set_sideset_pins(&c, PIN_OE);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_out_shift(&c, false, false, 0);
    pio_sm_set_consecutive_pindirs(pioout, sm, PIN_OE, 1, true);
    pio_sm_init(pioout, sm, o, &c);
	  pio_sm_set_enabled(pioout, sm, true);
  }
  //Setup dma channel for driving LEDs
  out_dma = dma_claim_unused_channel(true);
  {
    dma_channel_config c = dma_channel_get_default_config(out_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0);
    dma_channel_configure
    (
      out_dma,
      &c,
      &pioout->txf[0],     // Write address (only need to set this once)
      NULL,                // Don't provide a read address yet
      SCREENBUFFER_LEN/32, // always write 1 of 32 lines in each segment
      false                // Don't start yet
    );
  }

  //Setup pio block and state machines for reading input data
  pioin = pio1;
//  pio_gpio_init(pioin, PIN_DEBUG);
  for (int i=0;i<7;i++) { pio_gpio_init(pioin, PIN_DATA+i); }  
  {
    int sm = pio_claim_unused_sm(pioin, true);
    int o = pio_add_program(pioin, &pixelreader_program);
    pio_sm_config c = pixelreader_program_get_default_config(o);
    sm_config_set_in_pins(&c, PIN_DATA);
//    sm_config_set_sideset_pins(&c, PIN_DEBUG);
    sm_config_set_clkdiv(&c, 1.0);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_jmp_pin(&c, PIN_DATA+6);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_set_consecutive_pindirs(pioin, sm, PIN_DATA, 7, false);
//    pio_sm_set_consecutive_pindirs(pioin, sm, PIN_DEBUG, 1, true);
    pio_sm_init(pioin, sm, o, &c);
	  pio_sm_set_enabled(pioin, sm, true);
  }
  //Setup dma channel for receiving bit data
  in_dma = dma_claim_unused_channel(true);
  {
    dma_channel_config c = dma_channel_get_default_config(in_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_PIO1_RX0);
    dma_channel_configure
    (
      in_dma,
      &c,
      readlinebuffer,      
      &pioin->rxf[0],      // Read address (only need to set this once)
      160,                 // always write 320 pixels (16 bit per pixel)
      true                 // Start immediately, waiting for data
    );
  }

  // set up interrupt for incomming scanlines of data
  irq_set_exclusive_handler(PIO_IRQ_NUM(pioin, 0), lineFinishInterrupt);
  irq_set_enabled(PIO_IRQ_NUM(pioin, 0), true);
  pio_set_irq0_source_enabled(pioin, pis_interrupt0, true);
}


void loop() 
{  
}


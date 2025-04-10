/*
MIT License

Copyright (c) 2025 RHLamb

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#define MYDHTPIN MYGPIO_IO_PIN
#include "esp_timer.h"
static int dhtintcnt=0;
static uint8_t dhttimes[100];
static void dht_handler(void *arg)
{
  static int64_t tlas=0;
  int64_t tus=esp_timer_get_time();
  int xx=(int)(tus-tlas);
  tlas = tus;
  dhttimes[dhtintcnt] = (uint8_t)xx;
  if(++dhtintcnt >= 100) dhtintcnt = 99;
}
static int read_dht11(int tmo)
{
  static uint8_t init=1;
  if(init) {
    init = 0;
    gpio_install_isr_service(0);  // if not done before
    gpio_set_direction(MYDHTPIN,GPIO_MODE_INPUT);
    gpio_set_pull_mode(MYDHTPIN,GPIO_PULLUP_ONLY);
    gpio_set_intr_type(MYDHTPIN,GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(MYDHTPIN,(gpio_isr_t)dht_handler,NULL);
  }
  // start:18ms 0 then pull high wait for input
  gpio_set_direction(MYDHTPIN,GPIO_MODE_OUTPUT);
  gpio_set_level(MYDHTPIN,0);    
  rdelay(18);
  gpio_set_level(MYDHTPIN,1);
  // 40us before result
  dhtintcnt = 0;
  gpio_set_direction(MYDHTPIN,GPIO_MODE_INPUT);
  gpio_set_pull_mode(MYDHTPIN,GPIO_PULLUP_ONLY);  
  while(dhtintcnt < 85 && tmo-- > 0) vTaskDelay(1);
  if(tmo <= 0) {
    printf("timed out\n");
    return -1;
  }
  /* 
   * dhttimes[i] = 
   * i 2  3   4  5   6  7
   *  82 86  54 23  54 23  54 70  54 23  54 23  54 70  54 70  54 23  54 23  54 24  53 24  54 23  54 23  54 23  54 24  53 25 
   *  54 24  53 24  54 23  54 70  53 71  53 24  53 24  54 23  54 23  54 24  53 24  54 23  54 23  54 23  54 24  53 25 
   *  54 24  53 24  54 70  53 70  54 70  54 70  54 70  53 21  57
   */    
  int i,j,dht[20];
  memset(dht,0,sizeof(dht));
  j = 0;
  for(i=0;i<min(dhtintcnt,85);i++) {
    if( (i >= 5)  // skip start transitions
	&& (i&1) // skip 50us start bit per data bit
	&& (dhttimes[i] < 40 || dhttimes[i] > 60) ) { // data is 28us(0) or 70us(1)
      dht[j/8] <<= 1;
      if(dhttimes[i] > 50) dht[j/8] |= 1;
      j++;
    }
  }
  if( dht[4] != ((dht[0]+dht[1]+dht[2]+dht[3])&0xFF) ) {
    printf("bad checksum\n");
    return -1;
  }
  //F = ((C*9.0)/5.0) + 32.0;
  printf("H:%d.%d%% T:%d.%dC cnt=%d n=%d(%d)\n",dht[0],dht[1],dht[2],dht[3],dhtintcnt,j/8,j);
  return 0;
}
static int read_dht11cmd(int argc,char *argv[])
{
  read_dht11(100);
  return 0;
}

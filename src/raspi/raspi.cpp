// raspi.cpp
//
// Routines for implementing Arduino-LIMC on Raspberry Pi
// using BCM2835 library for GPIO
// This code has been grabbed from excellent RadioHead Library

#ifdef RASPBERRY_PI
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include "raspi.h"

//Initialize the values for sanity
static uint64_t epochMilli ;
static uint64_t epochMicro ;

void SPIClass::begin() {
  initialiseEpoch();
  
  if (!bcm2835_spi_begin()) {
    printf( "bcm2835_spi_begin() failed. Are you running as root??\n");
  }
  
  // LMIC Library code control CS line
  bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);  
}

void SPIClass::end() {
  //End the SPI
  bcm2835_spi_end();
}

void SPIClass::beginTransaction(SPISettings settings) {
  //Set SPI clock divider
  bcm2835_spi_setClockDivider(settings.divider);
  //Set the SPI bit Order
  bcm2835_spi_setBitOrder(settings.bitOrder);
  //Set SPI data mode
  bcm2835_spi_setDataMode(settings.dataMode);
}

void SPIClass::endTransaction() {
  uint8_t cs = lmic_pins.nss;
  // This one was really tricky and spent some time to find
  // it. When SPI transaction is done bcm2835 can setup CE0/CE1
  // pins as ALT0 function which may cause chip unselected or
  // selected depending on chip. And if there are more than 1,
  // then it can also interfere with other chip communication so 
  // what we do here is to ensure ou CE0 and CE1 are output HIGH so 
  // no other interference is happening if other chip are connected
  bcm2835_gpio_fsel ( 7, BCM2835_GPIO_FSEL_OUTP );
  bcm2835_gpio_fsel ( 8, BCM2835_GPIO_FSEL_OUTP );
  bcm2835_gpio_write( 7, HIGH );
  bcm2835_gpio_write( 8, HIGH );

  // CS line as output
  if ( cs!=7 && cs!=8) {
    bcm2835_gpio_fsel( cs, BCM2835_GPIO_FSEL_OUTP );
    bcm2835_gpio_write( cs, HIGH);
  }
}
  
byte SPIClass::transfer(byte _data) {
  byte data;
  data= bcm2835_spi_transfer((uint8_t)_data);
  return data;
}
 
void pinMode(unsigned char pin, unsigned char mode) {
  if (pin == LMIC_UNUSED_PIN) {
    return;
  }
  
  if (mode == OUTPUT) {
    bcm2835_gpio_fsel(pin,BCM2835_GPIO_FSEL_OUTP);
  } else {
    bcm2835_gpio_fsel(pin,BCM2835_GPIO_FSEL_INPT);
  }
}

void digitalWrite(unsigned char pin, unsigned char value) {
  if (pin == LMIC_UNUSED_PIN) {
    return;
  }
  bcm2835_gpio_write(pin,value);
}

unsigned char digitalRead(unsigned char pin) {
  if (pin == LMIC_UNUSED_PIN) {
    return 0;
  }
  return bcm2835_gpio_lev(pin);
}

//Initialize a timestamp for millis/micros calculation
// Grabbed from WiringPi
void initialiseEpoch() {
  struct timeval tv ;
  gettimeofday (&tv, NULL) ;
  epochMilli = (uint64_t)tv.tv_sec * (uint64_t)1000    + (uint64_t)(tv.tv_usec / 1000) ;
  epochMicro = (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)(tv.tv_usec) ;
  pinMode(lmic_pins.nss, OUTPUT);
  digitalWrite(lmic_pins.nss, HIGH);
}

unsigned int millis() {
  struct timeval tv ;
  uint64_t now ;
  gettimeofday (&tv, NULL) ;
  now  = (uint64_t)tv.tv_sec * (uint64_t)1000 + (uint64_t)(tv.tv_usec / 1000) ;
  return (uint32_t)(now - epochMilli) ;
}

unsigned int micros() {
  struct timeval tv ;
  uint64_t now ;
  gettimeofday (&tv, NULL) ;
  now  = (uint64_t)tv.tv_sec * (uint64_t)1000000 + (uint64_t)tv.tv_usec ;
  return (uint32_t)(now - epochMicro) ;
}

bool getDevEuiFromMac(uint8_t * pdeveui) {
  struct ifaddrs *ifaddr=NULL;
  struct ifaddrs *ifa = NULL;
  int family = 0;
  int i = 0;
  bool gotit = false;

  // get linked list of the network interfaces
  if (getifaddrs(&ifaddr) == -1) {
    return false;
  } else {
    // Loop thru interfaces list
    for ( ifa=ifaddr; ifa!=NULL; ifa=ifa->ifa_next) {
      // Ethernet
      if ( (ifa->ifa_addr) && (ifa->ifa_addr->sa_family==AF_PACKET) ) {
        // Not loopback interface
        if (! (ifa->ifa_flags & IFF_LOOPBACK)) {
          char fname[128];
          int fd;
          int up=0;
          struct sockaddr_ll *s = (struct sockaddr_ll*)ifa->ifa_addr;
         
          // Get interface status 
          // Interface can be up with no cable connected and to be sure
          // It's up, active and connected we need to get operstate
          // 
          // if up + cable    if up + NO cable    if down + cable
          // =============    ==========          ==================
          // carrier:1        carrier:0           carrier:Invalid
          // dormant:0        dormant:0           dormant:Invalid
          // operstate:up     operstate:down      operstate :own
          sprintf(fname, "/sys/class/net/%s/operstate", ifa->ifa_name);
          if ( (fd = open( fname, O_RDONLY)) > 0 ){
            char buf[2];
            if ( read(fd, buf, 2) > 0 ) {
              // only first active interface 
              if ( buf[0]=='u' && buf[1]=='p' ) {
                printf("DEVEUI[8]={");
                for (int i=0; i<s->sll_halen; i++) {
                  printf(" 0x%02x,", (s->sll_addr[i]));
                  *pdeveui++ = s->sll_addr[i];
                }
                printf(" 0x00, 0x00 }; // %s\n", ifa->ifa_name);
                // DevEUI has 8 bytes, MAC only 6 add 2 zero
                *pdeveui++ = 0x00;
                *pdeveui = 0x00;
                gotit = true;
                close(fd);
                break;
              }
            }
            close(fd);
          } 
        }
      }
    }
    // Free our Linked list
    freeifaddrs(ifaddr);
  }
  // just in case of error put deveui to 12345678
  if (!gotit) {
    for (i=1; i<=8; i++) {
      *pdeveui++=i;
    }
  }
  return gotit;
}

void SerialSimulator::begin(int baud) {
  // No implementation neccesary - Serial emulation on Linux = standard console
  // Initialize a timestamp for millis calculation - we do this here as well in case SPI
  // isn't used for some reason
  initialiseEpoch();
}

size_t SerialSimulator::println(void) {
  fprintf(stdout, "\n");
}

size_t SerialSimulator::println(const char* s) {
  fprintf( stdout, "%s\n",s);
}

size_t SerialSimulator::print(const char* s) {
  fprintf( stdout, "%s",s);
}

size_t SerialSimulator::println(u2_t n) {
  fprintf(stdout, "%d\n", n);
}

size_t SerialSimulator::print(ostime_t n) {
  fprintf(stdout, "%d\n", n);
}

size_t SerialSimulator::print(unsigned int n, int base) {
  if (base == DEC)
    fprintf(stdout, "%d", n);
  else if (base == HEX)
    fprintf(stdout, "%02x", n);
  else if (base == OCT)
    fprintf(stdout, "%o", n);
  // TODO: BIN
}

size_t SerialSimulator::print(char ch) {
  fprintf(stdout, "%c", ch);
}

size_t SerialSimulator::println(char ch) {
  fprintf(stdout, "%c\n", ch);
}

size_t SerialSimulator::print(unsigned char ch, int base) {
  return print((unsigned int)ch, base);
}

size_t SerialSimulator::println(unsigned char ch, int base) {
  print((unsigned int)ch, base);
  fprintf( stdout, "\n");
}

size_t SerialSimulator::write(char ch) {
  fprintf( stdout, "%c", ch);
}

size_t SerialSimulator::write(unsigned char* s, size_t len) {
  for (int i=0; i<len; i++) {
    fprintf(stdout, "%c", s[i]);
  }
}

void SerialSimulator::flush(void) {
  fflush(stdout);
}

#endif // RASPBERRY_PI

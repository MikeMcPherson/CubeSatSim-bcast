// Sends CubeSatSim telemetry encoded as CW (Morse Code) using AO-7 format
//
// Portions Copyright (c) 2018 Brandenburg Tech, LLC
// All right reserved.
//
// THIS SOFTWARE IS PROVIDED BY BRANDENBURG TECH, LLC AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BRANDENBURT TECH, LLC
// AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <axradio/axradioinit_p.h>
#include <axradio/axradiomode_p.h>
//#include <axradio/axradiorx_p.h>
#include <axradio/axradiotx_p.h>
#include <generated/configtx.h>
//#include <pthread.h>
//#include <semaphore.h>
#include <spi/ax5043spi_p.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <wiringPiI2C.h>
#include <stdlib.h>

#include "status.h"
#include "ax5043.h"
#include "ax25.h"

#define MAX_MESSAGE_LENGTH (197)

#define VBATT 15
#define ADC5 17
#define ADC6 18
#define ADC7 19
#define ADC8 20
#define TIME 8
#define UCTEMP 30

extern uint8_t axradio_rxbuffer[];
void *transmit(void *arg);
int get_message(uint8_t *buffer, int avail);
int lower_digit(int number);
int upper_digit(int number);
int encode_digit(uint8_t *msg, int number);
void config_cw();
int encode_tlm(uint8_t *buffer, int channel, int val1, int val2, int val3, int val4, int avail);
int encode_header(uint8_t *buffer, int avail);
int add_dash(uint8_t *msg, int number); 
int add_dot(uint8_t *msg, int number); 
int add_space(uint8_t *msg);

ax5043_conf_t hax5043;
ax25_conf_t hax25;

static void init_rf();
void config_x25();
void trans_x25();


int main(void)
{
    uint8_t retVal;

    // Configure SPI bus to AX5043
    setSpiChannel(SPI_CHANNEL);
    setSpiSpeed(SPI_SPEED);
    initializeSpi();

    // Initialize the AX5043
    retVal = axradio_init();
    if (retVal == AXRADIO_ERR_NOCHIP) {
        fprintf(stderr, "ERROR: No AX5043 RF chip found\n");
        exit(EXIT_FAILURE);
    }
    if (retVal != AXRADIO_ERR_NOERROR) {
        fprintf(stderr, "ERROR: Unable to initialize AX5043\n");
        exit(EXIT_FAILURE);
    }

    printf("INFO: Found and initialized AX5043\n");

    retVal = mode_tx();
    if (retVal != AXRADIO_ERR_NOERROR) {
         fprintf(stderr, "ERROR: Unable to enter TX mode\n");
         exit(EXIT_FAILURE);
    }
   // Read current from I2C bus
    int devId = 0x40; // +X Panel current
    int i2cDevice = wiringPiI2CSetup (devId) ;
    printf("\n\nCurrent setup result: %d\n", i2cDevice);
    printf("Read: %d\n", wiringPiI2CRead(i2cDevice)) ;

    int result = wiringPiI2CWriteReg16(i2cDevice, 0x05, 4096);
    printf("Write result: %d\n", result);
    
    float current = 0.05 * wiringPiI2CReadReg16(i2cDevice, 0x04);
    printf("Current: %f\n\n\n", current);
   
    // Read temperature from I2C bus
    devId = 0x48; // temp 
    i2cDevice = wiringPiI2CSetup (devId);
    printf("\n\nTemp setup result: %d\n", i2cDevice);
    int tempValue = wiringPiI2CRead(i2cDevice); 
    printf("Read: %x\n", tempValue);
    uint8_t upper = tempValue >> 8;
    uint8_t lower = tempValue && 0xff;
    printf("upper: %x lower: %x\n", upper, lower);  

    config_cw();

    // allocate space for the buffer
    static uint8_t packet[MAX_MESSAGE_LENGTH + 1];
     
    int channel; // AO-7 telemetry format has 6 channels, 4 sub channels in each
    int msg_length;

    while(1) {  // loop forever
	
        for (channel = 0; channel < 7; channel++) {

        if (channel == 0) {  // start with telemetry header "hi hi" plus a few chars to help CW decoding software sync
            msg_length = encode_header(&packet[0], MAX_MESSAGE_LENGTH + 1);
       
            printf("\nINFO: Sending TLM header\n");

        } else {
    
      FILE* file = popen("mpcmd show data 2>&1", "r");
    
      char cmdbuffer[1000];
      fgets(cmdbuffer, 1000, file);
      pclose(file);
      printf("buffer is :%s\n", cmdbuffer);
  
      char mopower[64][14];

      char * data;
      int i = 0;
      data = strtok (cmdbuffer," ");
      while (data != NULL)
      {
        strcpy(mopower[i], data);
        printf ("mopwer[%d]=%s\n",i,mopower[i]);
        data = strtok (NULL, " ");
        i++;
      }
        printf("Battery voltage = %s ADC5 = %s ADC6 = %s ADC7 = %s ADC8 %s \n", 
	  mopower[VBATT],mopower[ADC5],mopower[ADC6],mopower[ADC7],mopower[ADC8]);
        float vbat;
        vbat = strtof(mopower[VBATT], NULL);
        printf(" vbat: %f \n", vbat);
        int tlm_3a = (int)((vbat * 10) - 65.5);
    	
        printf("TLM 3A = %d \n", tlm_3a);

       // Read current from I2C bus
        int devId = 0x40; // +X Panel current
        int i2cDevice = wiringPiI2CSetup (devId) ;
        printf("\n\nI2C result: %d\n", i2cDevice);
        printf("Read: %d\n", wiringPiI2CRead(i2cDevice)) ;

        int result = wiringPiI2CWriteReg16(i2cDevice, 0x05, 4096);
        printf("Write result: %d\n", result);
    
        int currentValue = wiringPiI2CReadReg16(i2cDevice, 0x04);
        printf("Current: %d\n\n\n", currentValue);
        int tlm_1b = (int) (98.5 - currentValue/400);
        printf("TLM 1B = %d \n\n", tlm_1b);

         msg_length = encode_tlm(&packet[0], channel, // add a channel with dummy data to buffer
		 tlm_3a, tlm_1b, channel+2, channel+3,
			 (MAX_MESSAGE_LENGTH + 1));
       
            printf("\nINFO: Sending TLM channel %d \n", channel);
        }
        printf("DEBUG: msg_length = %d\n", msg_length);

        retVal = transmit_packet(&remoteaddr_tx, packet, (uint16_t)(msg_length)); // send telemetry
        if (retVal != AXRADIO_ERR_NOERROR) {
            fprintf(stderr, "ERROR: Unable to transmit a packet\n");
            exit(EXIT_FAILURE);
	}
//	sleep(1);

	usleep(200000);
        }
    }
}
// Encodes telemetry header (channel 0) into buffer
//
int encode_header(uint8_t *buffer, int avail) {

    int count = 0;
    count += add_space(&buffer[count]);
    count += add_space(&buffer[count]);

    count += add_dash(&buffer[count], 1);		// c 
    count += add_dot(&buffer[count], 1);		
    count += add_dash(&buffer[count], 1);		 
    count += add_dot(&buffer[count], 1);		
    count += add_space(&buffer[count]);
    
    count += add_dash(&buffer[count], 2);		// q 
    count += add_dot(&buffer[count], 1);		 
    count += add_dash(&buffer[count], 1);		
    count += add_space(&buffer[count]);
    
    count += add_space(&buffer[count]);

    count += add_dot(&buffer[count], 4);		// h
    count += add_space(&buffer[count]);
    
    count += add_dot(&buffer[count], 2);		// i
    count += add_space(&buffer[count]);
    
    count += add_space(&buffer[count]);
    
    count += add_dot(&buffer[count], 4);		// h
    count += add_space(&buffer[count]);
    
    count += add_dot(&buffer[count], 2);		// i
    count += add_space(&buffer[count]);

    count += add_space(&buffer[count]);
    count += add_space(&buffer[count]);

    return count;
}

// Encodes one channel of telemetry into buffer
//
int encode_tlm(uint8_t *buffer, int channel, int val1, int val2, int val3, int val4, int avail) {

    int count = 0;

    count += add_space(&buffer[count]);
    count += add_space(&buffer[count]);
    count += add_space(&buffer[count]);
    count += add_space(&buffer[count]);
 
    count += encode_digit(&buffer[count], channel);		// for channel 1, encodes 1aa
    count += encode_digit(&buffer[count], upper_digit(val1));
    count += encode_digit(&buffer[count], lower_digit(val1));

    count += add_space(&buffer[count]);

    count += encode_digit(&buffer[count], channel);		// for channel 1, encodes 1bb
    count += encode_digit(&buffer[count], upper_digit(val2));
    count += encode_digit(&buffer[count], lower_digit(val2));

    count += add_space(&buffer[count]);

    count += encode_digit(&buffer[count], channel);		// for channel 1, encodes 1cc
    count += encode_digit(&buffer[count], upper_digit(val3));
    count += encode_digit(&buffer[count], lower_digit(val3));

    count += add_space(&buffer[count]);

    count += encode_digit(&buffer[count], channel);		// for channel 1, encodes 1dd
    count += encode_digit(&buffer[count], upper_digit(val4));
    count += encode_digit(&buffer[count], lower_digit(val4));

    count += add_space(&buffer[count]);

    //printf("DEBUG count: %d avail: %d \n", count, avail);
    if (count > avail) {		// make sure not too long
       buffer[avail-1] = 0;
       count = avail-1;
       printf("DEBUG count > avail!\n");
    }
return count;
}
//  Encodes a single digit of telemetry into buffer
//
int encode_digit(uint8_t *buffer, int digit) {
	int count = 0;
	switch(digit)
	{
		case 0:
			count += add_dash(&buffer[count], 5);		// 0
			count += add_space(&buffer[count]);

			break;
		case 1:
			count += add_dot(&buffer[count], 1);		// 1
			count += add_dash(&buffer[count], 4);		
			count += add_space(&buffer[count]);
    
			break;
		case 2:
			count += add_dot(&buffer[count], 2);		// 2
			count += add_dash(&buffer[count], 3);		
			count += add_space(&buffer[count]);

			break;
		case 3:
			count += add_dot(&buffer[count], 3);		// 3
			count += add_dash(&buffer[count], 2);		
			count += add_space(&buffer[count]);
			
			break;
		case 4:
			count += add_dot(&buffer[count], 4);		// 4
			count += add_dash(&buffer[count], 1);		
			count += add_space(&buffer[count]);
			
			break;
		case 5:
			count += add_dot(&buffer[count], 5);		// 5
			count += add_space(&buffer[count]);
			
			break;
		case 6:
			count += add_dash(&buffer[count], 1);		// 6
			count += add_dot(&buffer[count], 4);		
			count += add_space(&buffer[count]);
			
			break;
		case 7:

			count += add_dash(&buffer[count], 2);		// 7
			count += add_dot(&buffer[count], 3);		
			count += add_space(&buffer[count]);

			break;
		case 8:
			count += add_dash(&buffer[count], 3);		// 8
			count += add_dot(&buffer[count], 2);		
			count += add_space(&buffer[count]);

			break;
		case 9:
			count += add_dash(&buffer[count], 4);		// 9
			count += add_dot(&buffer[count], 1);		
			count += add_space(&buffer[count]);
		
			break;
		default:
			printf("ERROR: Not a digit!\n");
			return 0;
	}
	return count;
}
//  Returns lower digit of a number which must be less than 99
//
int lower_digit(int number) {

	int digit = 0;

	if (number < 100) 
		digit = number - ((int)(number/10) * 10);
	else
		printf("ERROR: Not a digit in lower_digit!\n");

	return digit;
}
// Returns upper digit of a number which must be less than 99
//
int upper_digit(int number) {

	int digit = 0;

	if (number < 100) 
		digit = (int)(number/10);
	else
		printf("ERROR: Not a digit in upper_digit!\n");

	return digit;
}
//  Configure radio to send CW which is ASK
//
void config_cw() {

        printf("Register write to clear framing and crc\n");
	ax5043WriteReg(0x12,0);

        printf("Register write to disable fec\n");
	ax5043WriteReg(0x18,0);

        printf("Register write \n");
	ax5043WriteReg(0x165,0);

	ax5043WriteReg(0x166,0);
	ax5043WriteReg(0x167,0x50); // 0x25); // 0x50); // 0x08); // 0x20);
	
	ax5043WriteReg(0x161,0);
	ax5043WriteReg(0x162,0x20);
	
	long txRate;
	txRate = ax5043ReadReg(0x167) + 256 * ax5043ReadReg(0x166) + 65536 * ax5043ReadReg(0x165);
	printf("Tx Rate %x %x %x \n", ax5043ReadReg(0x165), ax5043ReadReg(0x166), ax5043ReadReg(0x167)); 
	long fskDev;
	fskDev = ax5043ReadReg(0x163) + 256 * ax5043ReadReg(0x162) + 65536 * ax5043ReadReg(0x161);

	ax5043WriteReg(0x37,(uint8_t)((ax5043ReadReg(0x37) + 4)));  // Increase FREQA

	printf("Tx Rate: %ld FSK Dev: %ld \n", txRate, fskDev);
	
	ax5043WriteReg(0x10,0);	// ASK

	printf("Modulation: %x \n", (int)ax5043ReadReg(0x10));
	printf("Frequency A: 0x%x %x %x %x \n",(int)ax5043ReadReg(0x34),(int)ax5043ReadReg(0x35),(int)ax5043ReadReg(0x36),(int)ax5043ReadReg(0x37));

/*
        int x;
	for (x = 0; x < 0x20; x++)
        {
                printf("Register %x contents: %x\n",x,(int)ax5043ReadReg(x));
        }

        printf("Register Dump complete");
*/    
	return;
}
// Adds a Morse space to the buffer
//
int add_space(uint8_t *msg) {
    	msg[0] = 0x00;  // a space is 8 bits
	return 1;	
}
// Adds a Morse dash to the buffer
//
int add_dash(uint8_t *msg, int number) {
	int j;
	int counter = 0;

	for (j=0; j < number; j++) { // a dot is 4 bits, so a dash is 12 bits
     		msg[counter++] = 0xff;
		msg[counter++] = 0x0f;
	}
	return counter;	
}
// Adds a Morse dot to the buffer
//
int add_dot(uint8_t *msg, int number) {
	int counter = 0;
	int j;
	for (j=0; j < number; j++) {  // a dot is 4 bits
     		msg[counter++] = 0x0f;
	}
	return counter;	
}

int x25_main(void) {
    setSpiChannel(SPI_CHANNEL);
    setSpiSpeed(SPI_SPEED);
    initializeSpi();

    int ret;
    uint8_t data[1024];
    // 0x03 is a UI frame
    // 0x0F is no Level 3 protocol
    // rest is dummy CubeSatSim telemetry in AO-7 format 	
    const char *str = "\x03\x0fhi hi 101 102 103 104 202 203 204 205 303 304 305 306 404 405 406 407 408 505 506 507 508 606 607 608 609\n";

    /* Infinite loop */
    for (;;) {
        sleep(2);
    	
	// send X.25 packet

    	init_rf();

    	ax25_init(&hax25, (uint8_t *) "CQ", '2', (uint8_t *) "DX", '2',
    		AX25_PREAMBLE_LEN,
   		 AX25_POSTAMBLE_LEN);

        
	printf("INFO: Transmitting X.25 packet\n");

        memcpy(data, str, strnlen(str, 256));
        ret = ax25_tx_frame(&hax25, &hax5043, data, strnlen(str, 256));
        if (ret) {
            fprintf(stderr,
                    "ERROR: Failed to transmit AX.25 frame with error code %d\n",
                    ret);
            exit(EXIT_FAILURE);
        }
        ax5043_wait_for_transmit();
        if (ret) {
            fprintf(stderr,
                    "ERROR: Failed to transmit entire AX.25 frame with error code %d\n",
                    ret);
            exit(EXIT_FAILURE);
        }

    }

    return 0;
}

static void init_rf() {
    int ret;
    ret = ax5043_init(&hax5043, XTAL_FREQ_HZ, VCO_INTERNAL);
    if (ret != PQWS_SUCCESS) {
        fprintf(stderr,
                "ERROR: Failed to initialize AX5043 with error code %d\n", ret);
        exit(EXIT_FAILURE);
    }
}
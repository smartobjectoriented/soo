#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART

//At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
int uart0_filestream = -1;

//************************************
//************************************
//********** UART TX STRING **********
//************************************
//************************************
void uart_tx_string_cs (char* tx_string, int len)
{
	int i;
	int cs = 0;

    for(i = 4; i < 4 + tx_string[1] - 1; i++) {
        cs += tx_string[i];
    }

    cs = (cs % 256) + 1;
    tx_string[len - 2] = cs;

	printf("cs is %02hhx\n", cs);


	printf("Writing: ");
	for(i = 0; i < len; i++) {
		printf("%02x ", tx_string[i]);
	}
	printf("\n");

	if (uart0_filestream != -1){
		// printf("writing %s\n", tx_string);
		write(uart0_filestream, tx_string, len);
	} else {
		printf("FAILED writing on uart bus\n");
	}		//Filestream, bytes to write, number of bytes to write
}

void uart_tx_string(char* tx_string, int len)
{
	int i;

	printf("tx[%d]: ", len);
	for(i = 0; i < len; i++) {
		printf("%02x ", tx_string[i]);
	}
	printf("\n");

	if (uart0_filestream != -1){
		// printf("writing %s\n", tx_string);
		write(uart0_filestream, tx_string, len);
	} else {
		printf("FAILED writing on uart bus\n");
	}		//Filestream, bytes to write, number of bytes to write
}

void send_ack(void) {

	char ack[1] = {0xe5};
	printf("send ack\n");
	write(uart0_filestream, ack, 1);
}

void close_blind(void) {

	char prop_read_req[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08, 0x01, 0x40, 0x10, 0x01, 0xA9, 0x16};
	char prop_write_req[15] = {0x68, 0x09, 0x09, 0x68, 0x73, 0xF6, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x00, 0xB7, 0x16};
	char prop_read_req4[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x00, 0x01, 0x38, 0x10, 0x01, 0x99, 0x16};
	char down_blind[18] = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x01, 0x01, 0x00, 0x81, 0xAC, 0x16};
	unsigned char rx_buffer[256];
	int step = 0;
	int i;
	
	// flush uart
	int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);
	memset(rx_buffer, 0, 256);

	uart_tx_string(prop_read_req, 14);

	while(1) {
		
		memset(rx_buffer, 0, 256);
		rx_length = read(uart0_filestream, (void*)rx_buffer, 255);
		
		if(rx_length > 0) {

			rx_buffer[rx_length] = '\0';

			printf("rx[%d]: ", rx_length);
			for(i = 0; i < rx_length; i++) {

				printf("%02hhx ", rx_buffer[i]);
			}
			printf("\n");

			if(rx_buffer[rx_length - 1] == 0x16) {
				send_ack();
				
				if(step == 0) {

					uart_tx_string(prop_write_req, 15);
					step++;
				} else if (step == 1) {

					uart_tx_string(prop_read_req4, 15);
					step++;
				}
			} else if(rx_buffer[0] == 0xe5) {
				if(step == 2) {

					printf("___________________DOWN BLIND\n");
					uart_tx_string(down_blind, 18);
					step++;

				} else if(step == 3) {

					break;
				}
			}

		}
	}
}

void stop_blind(void) {

	char prop_read_req[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08, 0x01, 0x40, 0x10, 0x01, 0xA9, 0x16};
	char prop_write_req[15] = {0x68, 0x09, 0x09, 0x68, 0x73, 0xF6, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x00, 0xB7, 0x16};
	char prop_read_req4[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x00, 0x01, 0x38, 0x10, 0x01, 0x99, 0x16};
	char stop_blind_cmd[18] = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x02, 0x01, 0x00, 0x80, 0xAC, 0x16};
	unsigned char rx_buffer[256];
	int step = 0;
	int i;

	// flush uart
	int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);
	memset(rx_buffer, 0, 256);
	
	
	uart_tx_string(prop_read_req, 14);

	while(1) {
		
		memset(rx_buffer, 0, 256);
		rx_length = read(uart0_filestream, (void*)rx_buffer, 255);
		
		if(rx_length > 0) {

			rx_buffer[rx_length] = '\0';

			printf("rx[%d]: ", rx_length);
			for(i = 0; i < rx_length; i++) {

				printf("%02hhx ", rx_buffer[i]);
			}
			printf("\n");

			if(rx_buffer[rx_length - 1] == 0x16) {
				send_ack();
				
				if(step == 0) {

					uart_tx_string(prop_write_req, 15);
					step++;
				} else if (step == 1) {

					uart_tx_string(prop_read_req4, 15);
					step++;
				}
			} else if(rx_buffer[0] == 0xe5) {
				if(step == 2) {

					printf("___________________STOP BLIND\n");
					uart_tx_string(stop_blind_cmd, 18);
					step++;

				} else if(step == 3) {

					break;
				}
			}

		}
	}
}

void up_blind(void) {

	char prop_read_req[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08, 0x01, 0x40, 0x10, 0x01, 0xA9, 0x16};
	char prop_write_req[15] = {0x68, 0x09, 0x09, 0x68, 0x73, 0xF6, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x00, 0xB7, 0x16};
	char prop_read_req4[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x00, 0x01, 0x38, 0x10, 0x01, 0x99, 0x16};
	char up_blind_cmd[18]   = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x01, 0x01, 0x00, 0x80, 0xAB, 0x16};
	unsigned char rx_buffer[256];
	int step = 0;
	int i;
	// flush uart
	int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);
	memset(rx_buffer, 0, 256);
	
	
	uart_tx_string(prop_read_req, 14);

	while(1) {
		
		memset(rx_buffer, 0, 256);
		rx_length = read(uart0_filestream, (void*)rx_buffer, 255);
		
		if(rx_length > 0) {

			rx_buffer[rx_length] = '\0';

			printf("rx[%d]: ", rx_length);
			for(i = 0; i < rx_length; i++) {

				printf("%02hhx ", rx_buffer[i]);
			}
			printf("\n");

			if(rx_buffer[rx_length - 1] == 0x16) {
				send_ack();
				
				if(step == 0) {

					uart_tx_string(prop_write_req, 15);
					step++;
				} else if (step == 1) {

					uart_tx_string(prop_read_req4, 15);
					step++;
				}
			} else if(rx_buffer[0] == 0xe5) {
				if(step == 2) {

					printf("___________________UP BLIND\n");
					uart_tx_string(up_blind_cmd, 18);
					step++;

				} else if(step == 3) {

					break;
				}
			}

		}
	}
}

int main(int argc, char *argv[]) {

    //-------------------------
	//----- SETUP USART 0 -----
	//-------------------------
	
	
	//OPEN THE UART
	//The flags (defined in fcntl.h):
	//	Access modes (use 1 of these):
	//		O_RDONLY - Open for reading only.
	//		O_RDWR - Open for reading and writing.
	//		O_WRONLY - Open for writing only.
	//
	//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
	//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
	//											immediately with a failure status if the output can't be written immediately.
	//
	//	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
	uart0_filestream = open("/dev/ttyAMA3", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	// uart0_filestream = open("/dev/serial0", O_RDWR | O_NOCTTY | O_SYNC);		//Open in non blocking read/write mode
	if (uart0_filestream == -1)
	{
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
	}
	
	//CONFIGURE THE UART
	//The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)
	struct termios options;
	tcgetattr(uart0_filestream, &options);
	cfsetispeed(&options, 19200);
	cfsetospeed(&options, 19200);
	// cfmakeraw(&options);
	
	// options.c_cflag = B19200 | CS8 | CREAD | PARENB;		//<Set baud rate
	// options.c_cflag &= ~(PARODD | CSTOPB | CSIZE);
	// options.c_cflag = B19200 | CS8 | CREAD | CLOCAL | PARENB;		//<Set baud rate
	// options.c_cflag = B19200 | CS8 | CREAD | CLOCAL | PARENB;		//<Set baud rate
	// options.c_cflag &= ~PARODD; // parity even		
	// options.c_cflag &= ~CSTOPB;	// 1 stop bit
	
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	options.c_cflag &= ~(CSIZE | CRTSCTS);
	options.c_cflag |= (CS8 | CREAD | PARENB);
	
	// options.c_cflag |= (FLUSHO);
	// options.c_cflag |= (CLOCAL | CREAD);
	// options.c_cflag |= (PARENB | CS8);
	// options.c_lflag &= ~(ICANON);
	tcsetattr(uart0_filestream, TCSANOW, &options);

	// uart_tx_string("salut", 5);


    char reset[4] = {0x10, 0x40, 0x40, 0x16};

	char pei_req[8] = {0x68, 0x02, 0x02, 0x68, 0x73, 0xA7, 0x1A, 0x16};

	char prop_read_req[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08, 0x01, 0x40, 0x10, 0x01, 0xA9, 0x16};

	char prop_write_req[15] = {0x68, 0x09, 0x09, 0x68, 0x73, 0xF6, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x00, 0xB7, 0x16};

	char prop_read_req2[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x08,  0x01, 0x34, 0x10, 0x01, 0x9D, 0x16};

	char prop_read_req3[14] = {0x68, 0x08, 0x08, 0x68, 0x73, 0xFC, 0x00, 0x08, 0x01, 0x33, 0x10, 0x01, 0xBC, 0x16};

	char prop_read_req4[14] = {0x68, 0x08, 0x08, 0x68, 0x53, 0xFC, 0x00, 0x00, 0x01, 0x38, 0x10, 0x01, 0x99, 0x16};


	char down_blind[18] = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x01, 0x01, 0x00, 0x81, 0xAC, 0x16};
	// char up_blind[18]   = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x01, 0x01, 0x00, 0x80, 0xAB, 0x16};
	char stop_blind_cmd[18] = {0x68, 0x0C, 0x0C, 0x68, 0x73, 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x09, 0x02, 0x01, 0x00, 0x80, 0xAC, 0x16};

	uart_tx_string(reset, 4);
	int sended = 0;
	int i;

	printf("Listening...\n");
    while(1) {
        //----- CHECK FOR ANY RX BYTES -----
        if (uart0_filestream != -1)
        {
            // Read up to 255 characters from the port if they are there
            unsigned char rx_buffer[256];
            int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
            if (rx_length < 0)
            {
                //An error occured (will occur if there are no bytes)
            }
            else if (rx_length == 0)
            {
                //No data waiting
            }
            else
            {
                //Bytes received
                rx_buffer[rx_length] = '\0';

				printf("rx[%d]: ", rx_length);
				for(i = 0; i < rx_length; i++) {

                	printf("%02hhx ", rx_buffer[i]);
				}
				printf("\n");

				if(rx_buffer[0] == 0xe5) {

					// uart_tx_string_cs(get_dp_val, 14);
					// uart_tx_string_cs(to_send, 13);
					if (sended == 0) {

						uart_tx_string(pei_req, 8);
						sended++;
					}
					// if(sended == 10) {

					// 	printf("___________________DOWN BLIND\n");
					// 	uart_tx_string(down_blind, 18);
					// 	sended++;
					// }

				} else if(rx_buffer[rx_length - 1] == 0x16) {
					//send ACK
					send_ack();

					// delay(500);
					// usleep(500*1000);

					// if(sended == 1) {
					// 	close_blind();
					// 	sended++;
					// }
					break;
					// if(sended == 1) {

					// 	uart_tx_string(prop_read_req, 14);
					// 	sended++;
					// } else if(sended == 2) {

					// 	uart_tx_string(prop_write_req, 15);
					// 	sended++;
					// } else if(sended == 3) {
					// 	uart_tx_string(prop_read_req4, 14);
					// 	sended = 10;
					// }else if(sended == 10) {
						
					// 	// usleep(5000*1000);
					// 	printf("___________________DOWN BLIND\n");
					// 	uart_tx_string(down_blind, 18);
					// 	sended++;
					// }else if(sended == 12) {

					// 	usleep(5000*1000);
					// 	printf("___________________STOP BLIND\n");
					// 	uart_tx_string(stop_blind, 18);
					// 	sended++;
					// }else if(sended == 11) {

					// 	usleep(5000*1000);
					// 	printf("___________________UP BLIND\n");
					// 	uart_tx_string(up_blind, 18);
					// 	sended = 10;
					// }
				}
            }
        }
    }
	close_blind();

	usleep(2000*1000);
	
	stop_blind();

	usleep(2000*1000);

	up_blind();
}



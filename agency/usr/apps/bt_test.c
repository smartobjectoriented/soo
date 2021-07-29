#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int running = 1;

const char* moblieEntities = "<mobile-entities>\
<mobile-entity spid=\"cdaa1894-a859-4db4-b31b-db0d7133dba4\">\
<name>SOO.Lorem</name>\
<description>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Phasellus aliquam erat ut posuere congue.\
Nulla malesuada metus tincidunt, pharetra magna ut, bibendum dui. Curabitur id pulvinar urna. Ut faucibus nulla ex,\
vitae molestie leo faucibus non. Nunc feugiat, felis ut semper elementum, nisi augue tristique augue, sit amet porttitor\
ipsum purus non augue. Suspendisse potenti. Pellentesque sed eros posuere, blandit risus sed, condimentum nibh. Nam eget dolor\
a nisl mattis mattis. Maecenas eget imperdiet libero. Cras mauris arcu, porttitor ac mattis in, ultrices eu ex. Aliquam rutrum \
auctor ultrices. Sed nec libero posuere, aliquet urna convallis, venenatis elit. Fusce eu scelerisque diam. Integer sed ex cursus,\
egestas dui eu, tristique odio. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.\
Curabitur ullamcorper magna quis gravida tincidunt.\
</description>\
</mobile-entity>\
<mobile-entity spid=\"03967b81-6996-499f-8b91-e684a5a108c8\">\
<name>SOO.Lorem 2</name>\
<description>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Phasellus aliquam erat ut posuere congue.\
Nulla malesuada metus tincidunt, pharetra magna ut, bibendum dui. Curabitur id pulvinar urna. Ut faucibus nulla ex, \
vitae molestie leo faucibus non. Nunc feugiat, felis ut semper elementum, nisi augue tristique augue, sit amet porttitor \
ipsum purus non augue. Suspendisse potenti. Pellentesque sed eros posuere, blandit risus sed, condimentum nibh. Nam eget dolor \
a nisl mattis mattis. Maecenas eget imperdiet libero. Cras mauris arcu, porttitor ac mattis in, ultrices eu ex. Aliquam rutrum \
auctor ultrices. Sed nec libero posuere, aliquet urna convallis, venenatis elit. Fusce eu scelerisque diam. Integer sed ex cursus, \
egestas dui eu, tristique odio. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. \
Curabitur ullamcorper magna quis gravida tincidunt.\
</description>\
</mobile-entity>\
<mobile-entity>\
<name>SOO.ShouldFail</name>\
<description>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Phasellus aliquam erat ut posuere congue.\
Nulla malesuada metus tincidunt, pharetra magna ut, bibendum dui. Curabitur id pulvinar urna. Ut faucibus nulla ex, \
vitae molestie leo faucibus non. Nunc feugiat, felis ut semper elementum, nisi augue tristique augue, sit amet porttitor \
ipsum purus non augue. Suspendisse potenti. Pellentesque sed eros posuere, blandit risus sed, condimentum nibh. Nam eget dolor \
a nisl mattis mattis. Maecenas eget imperdiet libero. Cras mauris arcu, porttitor ac mattis in, ultrices eu ex. Aliquam rutrum \
auctor ultrices. Sed nec libero posuere, aliquet urna convallis, venenatis elit. Fusce eu scelerisque diam. Integer sed ex cursus, \
egestas dui eu, tristique odio. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. \
Curabitur ullamcorper magna quis gravida tincidunt.\
</description>\
</mobile-entity>\
</mobile-entities>";

void sigterm_handler(int signal) {
	running = 0;
	exit(0);
}

/**
 * RFCOMM Server which receive a file of any size from the tablet
 */
void *receive_thread(void *dummy) {
	
	struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
	char *buf;
	// char size_buf[4] = {0};
  	char get_mobile_entities_message [100] = {0};
	int s, client, bytes_read, total_bytes = 0;
	socklen_t opt = sizeof(rem_addr);
	uint32_t size;
	const uint8_t END_SYMBOL = 0x0A;

	/* allocate socket */
	s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

	/* bind socket to port 1 of the first available */
	/* local bluetooth adapter */
	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = *BDADDR_ANY;
	loc_addr.rc_channel = (uint8_t) 1;

	printf("Binding RFCOMM socket...\n");
	bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
	printf("Done!\n");

	/* put socket into listening mode */
	listen(s, 1);


	while(running) {
		/* accept one connection */
		printf("RFCOMM: Now accepting client...\n");
		client = accept(s, (struct sockaddr *)&rem_addr, &opt);
		printf("Client accepted!\n");
		// /* Read the size which is sent on 4B */
		// bytes_read = read(client, size_buf, sizeof(size_buf));
		// if (bytes_read != sizeof(size_buf)) {
		// 	printf("Couldn't read the 4 size bytes!\n");
		// 	continue;
		// }

		// size = *((uint32_t *)size_buf);

		// printf("Allocating a buffer of %u bytes:\n", size);

		// /* Allocate the receive buffer with the corresponding size */
		// buf = malloc(size * sizeof(char));
		// if (!buf) {
		// 	fprintf(stderr, "%s Error allocating the Me buffer!\n", __FILE__);
		// 	continue;
		// }

		// printf("Now receiving a buffer (%uB):\n", size);
		// /* Read the ME */
		// do {
		// 	bytes_read = read(client, (char *)(buf+total_bytes), 8192);
		// 	total_bytes += bytes_read;
		// 	printf("\r%d/%u B", total_bytes, size);
		// } while (bytes_read != -1 && total_bytes != size);

		// printf("\n\n");
		// /* Send the end symbol to the client so it can close its socket */

		read(client, get_mobile_entities_message, sizeof(get_mobile_entities_message));
		if (get_mobile_entities_message[0] != 0x01) {
			printf("wrong message : %x instead of %x!\n", get_mobile_entities_message[0], 0x01);
			continue;
		}

		int beginPos = 0;
		size_t payloadSize = 1008;
		char tmpBuf[payloadSize-1];
		char payload[payloadSize];

		printf("message size: %d\n", strlen(moblieEntities));
		do {
			memset(tmpBuf, '\0', payloadSize-1);
			memset(payload, '\0', payloadSize);
			strncpy(tmpBuf, moblieEntities + beginPos, payloadSize-1);

			if(strlen(tmpBuf) < payloadSize-1) {
				payload[0] = 0x02; // 0000 0010
			} else {
				payload[0] = 0x82; // 1000 0010
			}
			strcat(payload, tmpBuf);

			printf("sending char from %d to %d!\n", beginPos, beginPos + payloadSize - 1);
			print_hex(payload);
			write(client, payload, payloadSize);

			beginPos += payloadSize-1;
		} while (beginPos < strlen(moblieEntities));

		printf("Waiting 2s before closing socket...\n");
		sleep(2);

		write(client, &END_SYMBOL, 1);


		printf("Waiting 2s before closing socket...\n");
		sleep(2);

		close(client);
		
		free(buf);
		total_bytes = 0;
	}
	
	close(s);
}

void print_hex(const char *s)
{
  while(*s)
    printf("%02x", (unsigned int) *s++);
  printf("\n");
}

int main(int argc, char *argv[]) {
	pthread_t receive_th;
	/* Create the thread which will handle the ME receive from the tablet */
	pthread_create(&receive_th, NULL, receive_thread, NULL);

	signal(SIGINT, sigterm_handler);
	while(running);
	return 0;
}

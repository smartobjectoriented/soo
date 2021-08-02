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

#include <roxml.h>

int running = 1;

 

const char* moblieEntities = "<mobile-entities>\
<mobile-entity spid=\"0020000000000003\">\
<name>SOO.heat</name>\
<description>SOO.heat permet de gérer le termostat des radiateurs.</description>\
</moblie-entity>\
<mobile-entity spid=\"0020000000000002\">\
<name>SOO.outdoor</name>\
<description>\
SOO.outdoor permet de récupérer des informations météorologique \
telle que la luminosité ambiante ou la température externe. \
</description>\
</moblie-entity>\
<mobile-entity spid=\"0020000000000001\">\
<name>SOO.blind</name>\
<description>SOO.blind permet de gérer la position des stores.</description>\
</moblie-entity>\
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
</moblie-entity>\
</mobile-entities>";

const char* sooOutdoor = "<model spid=\"0020000000000002\">\
<name>SOO.outdoor</name>\
<description>SOO.outdoor permet de récupérer les informations d'une station météorologique.</description>\
<layout>\
    <row>\
        <col>\
            <label for=\"temp-per-day-graph\">Luminosité selon l'heure de la journée</label>\
            <graph id=\"temp-per-day-graph\" type=\"line\">

            </graph>
        </col>\
    </row>\
    <row>\
        <col>\
            <label for=\"summary-graph\">Luminosité selon l'heure de la journée</label>\
            <graph id=\"summary-graph\" type=\"table\">
            
            </graph>
        </col>\
    </row>\
</layout>\
</model>";

const char* sooBlind = "<model spid=\"0020000000000001\">\
<name>SOO.blind</name>\
<description>SOO.blind permet de gérer la position des stores.</description>\
<layout>\
    <row>\
        <col><label for=\"up\">Position des stores</label></col>\
        <col><button id=\"up\" lockable=\"true\" lockable-after=\"2\">Monter</button></col>\
        <col><button id=\"down\" lockable=\"true\" lockable-after=\"2\">Descendre</button></col>\
        <col><slider id=\"slider\" max=\"5\" step=\"1\" orientation=\"vertical\">1</slider></col>\
    </row>\
    <row>\
        <col><label for=\"if-lux\">Condition 1</label></col>\
    </row>\
    <row>\
        <col><text>Si la luminosité externe est plus petite que </text><number id=\"if-lux\" value=\"500\"/><text>lux</text></col>\
    </row>\
    <row>\
        <col><text>Alors </text><dropdown id=\"then-lux\"><option value=\"up\">Monter</option><option value=\"down\" default=\"true\">Descendre</option></dropdown></col>\
    </row>\
    <row>\
        <col><text>Sur </text><dropdown id=\"on-lux\"><option value=\"nord\">SOO.outdoor Nord</option><option value=\"south\">SOO.outdoor Sud</option></dropdown></col>\
    </row>\
</layout>\
</model>";

const char* sooHeat = "<model spid=\"0020000000000003\">\
<name>SOO.heat</name>\
<description>SOO.heat permet de gérer le termostat des radiateurs.</description>\
<layout>\
    <row>\
        <col><label for=\"current-temp\">Position des stores</label></col>\
        <col><number id=\"current-temp\" step=\"0.5\">22.5</number></col>\
        <col><button id=\"increase-temp\" lockable=\"true\">-0.5°C</button></col>\
        <col><button id=\"decrease-temp\" lockable=\"true\">+0.5°C</button></col>\
    </row>\
    <row>\
        <col><label for=\"if-temp\">Palier 1</label></col>\
    </row>\
    <row>\
        <col><text>Si la température externe est plus petite que </text></col>\
        <col><number id=\"if-temp\" step=\"0.5\">12.0</number></col>\
        <col><button id=\"if-temp-increase\" lockable=\"true\">-0.5°C</button></col>\
        <col><button id=\"if-temp-decrease\" lockable=\"true\">+0.5°C</button></col>\
    </row>\
    <row>\
        <col><text>Alors la température interne vaut </text></col>\
        <col><number id=\"then-temp\" step=\"0.5\">21.5</number></col>\
        <col><button id=\"then-temp-increase\" lockable=\"true\">-0.5°C</button></col>\
        <col><button id=\"then-temp-decrease\" lockable=\"true\">+0.5°C</button></col>\
    </row>\
    <row>\
        <col><text>Sinon la température interne vaut </text></col>\
        <col><number id=\"else-temp\" step=\"0.5\">20.5</number></col>\
        <col><button id=\"else-temp-increase\" lockable=\"true\">-0.5°C</button></col>\
        <col><button id=\"else-temp-decrease\" lockable=\"true\">+0.5°C</button></col>\
    </row>\
</layout>\
</model>";

 

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
        //     printf("Couldn't read the 4 size bytes!\n");
        //     continue;
        // }

 

        // size = *((uint32_t *)size_buf);

 

        // printf("Allocating a buffer of %u bytes:\n", size);

 

        // /* Allocate the receive buffer with the corresponding size */
        // buf = malloc(size * sizeof(char));
        // if (!buf) {
        //     fprintf(stderr, "%s Error allocating the Me buffer!\n", __FILE__);
        //     continue;
        // }

 

        // printf("Now receiving a buffer (%uB):\n", size);
        // /* Read the ME */
        // do {
        //     bytes_read = read(client, (char *)(buf+total_bytes), 8192);
        //     total_bytes += bytes_read;
        //     printf("\r%d/%u B", total_bytes, size);
        // } while (bytes_read != -1 && total_bytes != size);

 

        // printf("\n\n");
        // /* Send the end symbol to the client so it can close its socket */

 

        read(client, get_mobile_entities_message, sizeof(get_mobile_entities_message));
        if (get_mobile_entities_message[0] != 0x01) {
            printf("wrong message : %x instead of %x!\n", get_mobile_entities_message[0], 0x01);
            continue;
        }

 

        int beginPos = 0;
        size_t payloadSize = 8192;
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

 
/* This thread launch the MEs and the TX thread threads and then acts as the vuihandler-RX thread 
   receiving the BT paquet. */
int main(int argc, char *argv[]) {
    pthread_t receive_th;
    pthread_t me1, me2, tx_thread;

    /* Open socket here */


    /* Launch MEs and TX thread here */


    /* Main RX loop here, which should route rx- data and update MEs accordingly*/


    /* Create the thread which will handle the ME receive from the tablet */
    pthread_create(&receive_th, NULL, receive_thread, NULL);
 

    signal(SIGINT, sigterm_handler);

    /* */
    while(running);
    return 0;
}
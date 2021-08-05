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
#include <semaphore.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#include <roxml.h>

int running = 1;

void sigterm_handler(int signal) {
    running = 0;
    exit(0);
}

#define SHARED 1
#define BLOCK_SIZE 1008
#define TYPE_SIZE 1
#define SPID_SIZE 16
#define PAYLOAD_SIZE BLOCK_SIZE - (TYPE_SIZE + SPID_SIZE)
// #define PAYLOAD_SIZE 8192

typedef struct {
	char type;
	char spid[SPID_SIZE];
	char payload[PAYLOAD_SIZE];
} vuihandler_pkt_t;

// typedef struct {
//     char* id;
//     char* action;
//     char* value;
// } event_t;

// typedef struct node {
//     event_t* handler;
//     struct node* next;
// } linked_events_t;

// linked_events_t* soo_outdoor_events;
// linked_events_t* soo_blind_events;
// linked_events_t* soo_heat_events;
// linked_events_t* messages_to_send;
// sem_t soo_outdoor_empty, soo_outdoor_full;
// sem_t soo_blind_empty, soo_blind_full;
// sem_t soo_heat_empty, soo_heat_full;
sem_t sem_mutex;

enum MESSAGE_TYPE {REPLACE, PUSH};

typedef struct {
    int hour;
    int min;
} soo_outdoor_t;

typedef struct {
    double current_heat;
    double if_external_heat;
    double then_internal_heat;
    double else_internal_heat;
} soo_heat_t;

typedef struct {
    int store_position;
    int min_pos;
    int max_pos;
} soo_blind_t;

soo_outdoor_t soo_outdoor = {
    .hour = 10,
    .min = 25
};

soo_heat_t soo_heat = {
    .current_heat = 22.5,
    .if_external_heat = 12.0,
    .then_internal_heat = 21.5,
    .else_internal_heat = 20.5
};

soo_blind_t soo_blind = {
    .store_position = 1,
    .min_pos = 0,
    .max_pos = 5
};

pthread_t soo_outdoor_th;
pthread_t soo_blind_th;
pthread_t soo_heat_th;

int soo_outdoor_thread_running = 0;
int soo_blind_thread_running = 0;
int soo_heat_thread_running = 0;

int min(int x, int y) {
  return (x < y) ? x : y;
}

double rand_helper(double min, double max) {
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

void print_hex(const char *s) {
  while(*s)
    printf("%02x", (unsigned int) *s++);
  printf("\n");
}

void print_hex_n(const char *s, int n) {
    int i;
    for(i = 0; i < n; ++i) {
        printf("%02x", (unsigned int) *s++);
    }
    printf("\n");
}

void print_vuihandler(vuihandler_pkt_t* vuihandler) {
    printf("{\ntype: ");
    int i;
    for(i = 0; i < TYPE_SIZE; ++i) {
        printf("%02x", (unsigned int) vuihandler->type);
    }
    printf(",\n spid: ");
    for(i = 0; i < SPID_SIZE; ++i) {
        printf("%02x", (unsigned int) vuihandler->spid[i]);
    }
    printf(",\n payload: ");
    for(i = 0; i < PAYLOAD_SIZE; ++i) {
        printf("%02x", (unsigned int) vuihandler->payload[i]);
    }
    printf("\n}\n");
}

int compare_arrays(const char* a, const char* b, int n)
{
    int i;
    for (i=0; i<n; ++i)
    {
        if (a[i] != b[i])
        {
            return -1;
        }
    }
    return 0;
}

void hexstring_to_byte(const char* src, char* dest, int n) {
    int i;
    for(i = 0; i < n; ++i) {
        sscanf(src + (2*i), "%2x", dest + i);
    }
}

vuihandler_pkt_t* get_vuihandler_from_bt(const char* message_block) {
    char type[TYPE_SIZE];
    char spid[SPID_SIZE];
    char payload[PAYLOAD_SIZE];

    memcpy(type, message_block, TYPE_SIZE);
    memset(spid, '\0', SPID_SIZE);
    memcpy(spid, message_block + TYPE_SIZE, SPID_SIZE);

    // printf("spid : ");
    // print_hex_n(spid, SPID_SIZE);

    memset(payload, '\0', PAYLOAD_SIZE);
    memcpy(payload, message_block + TYPE_SIZE + SPID_SIZE, PAYLOAD_SIZE);

    // printf("payload : ");
    // print_hex_n(payload, PAYLOAD_SIZE);

    vuihandler_pkt_t* message = (vuihandler_pkt_t*)malloc(sizeof(vuihandler_pkt_t));
    message->type = type[0];
    memcpy(message->spid, spid, SPID_SIZE);
    memcpy(message->payload, payload, PAYLOAD_SIZE);

    print_vuihandler(message);

    return message;
}

const char* generate_soo_list() {
    return "<mobile-entities>\
        <mobile-entity spid=\"00000200000000000000000000000003\">\
            <name>SOO.heat</name>\
            <description>SOO.heat permet de gérer le termostat des radiateurs.</description>\
        </mobile-entity>\
        <mobile-entity spid=\"00000200000000000000000000000002\">\
            <name>SOO.outdoor</name>\
            <description>\
            SOO.outdoor permet de récupérer des informations météorologique \
            telle que la luminosité ambiante ou la température externe. \
            </description>\
        </mobile-entity>\
        <mobile-entity spid=\"00000200000000000000000000000001\">\
            <name>SOO.blind</name>\
            <description>SOO.blind permet de gérer la position des stores.</description>\
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
}

const char* generate_soo_outdoor() {
    return "<model spid=\"00000200000000000000000000000002\">\
    <name>SOO.outdoor</name>\
    <description>SOO.outdoor permet de récupérer les informations d'une station météorologique.</description>\
    <layout>\
        <row>\
            <col>\
                <label for=\"temp-per-day-graph\">Luminosité selon l'heure de la journée</label>\
                <graph id=\"temp-per-day-graph\" type=\"line\">\
                    <axes display-series-name=\"true\">\
                        <axis type=\"datetime\" format=\"hh:mm\">Heure</axis>\
                        <axis type=\"number\">Température</axis>\
                    </axes>\
                    <series id=\"temp-per-day-north-station\" name=\"SOO.outdoor Nord\">\
                        <point>\
                            <item>09:45</item>\
                            <item>15</item>\
                        </point>\
                        <point>\
                            <item>10:10</item>\
                            <item>15.5</item>\
                        </point>\
                        <point>\
                            <item>10:22</item>\
                            <item>15.6</item>\
                        </point>\
                    </series>\
                    <series id=\"temp-per-day-south-station\" name=\"SOO.outdoor Sud\">\
                        <point>\
                            <item>09:42</item>\
                            <item>14</item>\
                        </point>\
                        <point>\
                            <item>09:59</item>\
                            <item>14</item>\
                        </point>\
                        <point>\
                            <item>10:15</item>\
                            <item>14.2</item>\
                        </point>\
                    </series>\
                </graph>\
            </col>\
        </row>\
        <row>\
            <col>\
                <label for=\"summary-graph\">Luminosité selon l'heure de la journée</label>\
                <graph id=\"summary-graph\" type=\"table\">\
                    <axes display-series-name=\"true\">\
                        <axis type=\"number\">Temperature</axis>\
                        <axis type=\"number\">Luminosity</axis>\
                        <axis type=\"datetime\" format=\"hh:mm\">Hour</axis>\
                        <axis type=\"number\">Mean temperature of the past 30 days</axis>\
                    </axes>\
                    <series id=\"summary-north-station\" name=\"SOO.outdoor Nord\">\
                        <point>\
                            <item>15</item>\
                            <item>500</item>\
                            <item>09:45</item>\
                            <item>14.2</item>\
                        </point>\
                        <point>\
                            <item>15.5</item>\
                            <item>510</item>\
                            <item>10:10</item>\
                            <item>14.4</item>\
                        </point>\
                        <point>\
                            <item>15.6</item>\
                            <item>512</item>\
                            <item>10:22</item>\
                            <item>14.6</item>\
                        </point>\
                    </series>\
                    <series id=\"summary-south-station\" name=\"SOO.outdoor Sud\">\
                        <point>\
                            <item>14</item>\
                            <item>490</item>\
                            <item>09:42</item>\
                            <item>13.9</item>\
                        </point>\
                        <point>\
                            <item>14</item>\
                            <item>490</item>\
                            <item>09:59</item>\
                            <item>14</item>\
                        </point>\
                        <point>\
                            <item>14.2</item>\
                            <item>490</item>\
                            <item>10:15</item>\
                            <item>14.2</item>\
                        </point>\
                    </series>\
                </graph>\
            </col>\
        </row>\
    </layout>\
    </model>";
}

const char* generate_soo_blind() {
    return "<model spid=\"00000200000000000000000000000001\">\
    <name>SOO.blind</name>\
    <description>SOO.blind permet de gérer la position des stores.</description>\
    <layout>\
        <row>\
            <col><label for=\"blind-up\">Position des stores</label></col>\
            <col><button id=\"blind-up\" lockable=\"true\" lockable-after=\"2\">Monter</button></col>\
            <col><button id=\"blind-down\" lockable=\"true\" lockable-after=\"2\">Descendre</button></col>\
            <col><slider id=\"blind-slider\" max=\"5\" step=\"1\" orientation=\"vertical\">1</slider></col>\
        </row>\
        <row>\
            <col><label for=\"blind-if-lux\">Condition 1</label></col>\
        </row>\
        <row>\
            <col><text>Si la luminosité externe est plus petite que </text><number id=\"blind-if-lux\" value=\"500\"/><text>lux</text></col>\
        </row>\
        <row>\
            <col><text>Alors </text><dropdown id=\"blind-then-lux\"><option value=\"up\">Monter</option><option value=\"down\" default=\"true\">Descendre</option></dropdown></col>\
        </row>\
        <row>\
            <col><text>Sur </text><dropdown id=\"blind-on-lux\"><option value=\"nord\">SOO.outdoor Nord</option><option value=\"south\">SOO.outdoor Sud</option></dropdown></col>\
        </row>\
    </layout>\
    </model>";
}

const char* generate_soo_heat() {
    return "<model spid=\"00000200000000000000000000000003\">\
    <name>SOO.heat</name>\
    <description>SOO.heat permet de gérer le termostat des radiateurs.</description>\
    <layout>\
        <row>\
            <col><label for=\"heat-current-temp\">Position des stores</label></col>\
            <col><number id=\"heat-current-temp\" step=\"0.5\">22.5</number></col>\
            <col><button id=\"heat-increase-temp\" lockable=\"true\">-0.5°C</button></col>\
            <col><button id=\"heat-decrease-temp\" lockable=\"true\">+0.5°C</button></col>\
        </row>\
        <row>\
            <col><label for=\"heat-if-temp\">Palier 1</label></col>\
        </row>\
        <row>\
            <col><text>Si la température externe est plus petite que </text></col>\
            <col><number id=\"heat-if-temp\" step=\"0.5\">12.0</number></col>\
            <col><button id=\"heat-if-temp-increase\" lockable=\"true\">-0.5°C</button></col>\
            <col><button id=\"heat-if-temp-decrease\" lockable=\"true\">+0.5°C</button></col>\
        </row>\
        <row>\
            <col><text>Alors la température interne vaut </text></col>\
            <col><number id=\"heat-then-temp\" step=\"0.5\">21.5</number></col>\
            <col><button id=\"heat-then-temp-increase\" lockable=\"true\">-0.5°C</button></col>\
            <col><button id=\"heat-then-temp-decrease\" lockable=\"true\">+0.5°C</button></col>\
        </row>\
        <row>\
            <col><text>Sinon la température interne vaut </text></col>\
            <col><number id=\"heat-else-temp\" step=\"0.5\">20.5</number></col>\
            <col><button id=\"heat-else-temp-increase\" lockable=\"true\">-0.5°C</button></col>\
            <col><button id=\"heat-else-temp-decrease\" lockable=\"true\">+0.5°C</button></col>\
        </row>\
    </layout>\
    </model>";
}

// void send_message(int client, vuihandler_pkt_t* message) {
//     int beginPos = 0;
//     char tmpBuf[PAYLOAD_SIZE - 1];
//     char payload[PAYLOAD_SIZE];

//     do {
//         memset(tmpBuf, '\0', PAYLOAD_SIZE - 1);
//         memset(payload, '\0', PAYLOAD_SIZE);
//         strncpy(tmpBuf, message->payload + beginPos, PAYLOAD_SIZE-1);

//         if(strlen(tmpBuf) < PAYLOAD_SIZE - 1) {
//             payload[0] = 0x02; // 0000 0010
//         } else {
//             payload[0] = 0x82; // 1000 0010
//         }
//         strcat(payload, tmpBuf);



//         printf("sending char from %d to %d!\n", beginPos, beginPos + PAYLOAD_SIZE - 1);
//         print_hex(payload);
//         write(client, payload, PAYLOAD_SIZE);
        
//         beginPos += PAYLOAD_SIZE - 1;
//     } while (beginPos < strlen(message->payload));

//     printf("payload \"%s send.\"\n", message->payload);
// }

void send_payload(int client, const char* spid, const char* payload) {
    int beginPos = 0;
    char _spid[SPID_SIZE];
    char tmpBuf[PAYLOAD_SIZE];
    char message_block[BLOCK_SIZE];

    printf("setting _spid...\n");
    if(spid == NULL) {
        memset(_spid, '\0', SPID_SIZE);
    } else {
        memcpy(_spid, spid, SPID_SIZE);
    }

    printf("entering semaphore mutex...\n");
    sem_wait(&sem_mutex);
    do {

        printf("prepare new message...\n");
        memset(tmpBuf, '\0', PAYLOAD_SIZE);
        strncpy(tmpBuf, payload + beginPos, min(PAYLOAD_SIZE - 1, strlen(payload + beginPos)));

        memset(message_block, '\0', BLOCK_SIZE);

        printf("size tmpBuf : %d\n", strlen(tmpBuf));
        printf("min %d %d",PAYLOAD_SIZE - 1, strlen(payload + beginPos));
        if(strlen(tmpBuf) < PAYLOAD_SIZE - 1) {
            message_block[0] = 0x02; // 0000 0010
        } else {
            message_block[0] = 0x82; // 1000 0010
        }
        memcpy(message_block + TYPE_SIZE, spid, SPID_SIZE);
        memcpy(message_block + TYPE_SIZE + SPID_SIZE, tmpBuf, PAYLOAD_SIZE);

        printf("sending payload char from %d to %d!\n", beginPos, beginPos + PAYLOAD_SIZE - 1);
        print_hex_n(message_block, BLOCK_SIZE);
        printf("payload send : %s\n", message_block + TYPE_SIZE + SPID_SIZE);
        write(client, message_block, BLOCK_SIZE);
        
        beginPos += PAYLOAD_SIZE - 1;
    } while (beginPos < strlen(payload));
    sem_post(&sem_mutex);
    printf("exiting semaphore mutex...\n");
}

void* soo_outdoor_thread(void* client_arg) {
    int client = *((int*) client_arg);
    double temp, lum;
    char *buffer, *spid;
    char type[TYPE_SIZE] = {0x02};
    node_t *root, *messages, *msg, *point, *item;

    printf("soo_outdoor_thread started.\n");

    while(soo_outdoor_thread_running){
        //generate random value
        temp = rand_helper(13.9, 16);
        lum = rand_helper(490, 500);

        root = roxml_add_node(NULL, 0, ROXML_ELM_NODE, "xml", NULL);

        /* Adding attributes to xml node */
        roxml_add_node(root, 0, ROXML_ATTR_NODE, "version", "1.0");
        roxml_add_node(root, 0, ROXML_ATTR_NODE, "encoding", "UTF-8");

        /* Adding the messages node */
        messages = roxml_add_node(root, 0, ROXML_ELM_NODE, "messages", NULL);

        /* Adding the message for "temp-per-day-north-station" */
        msg = roxml_add_node(messages, 0, ROXML_ELM_NODE, "message", NULL);
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "to", "temp-per-day-north-station");
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "type", "push");
        point = roxml_add_node(msg, 0, ROXML_ELM_NODE, "point", NULL);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "point", NULL);
        sprintf(buffer, "%.2f", (float)temp);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, buffer);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "point", NULL);
        sprintf(buffer, "%d:%d", soo_outdoor.hour, soo_outdoor.min);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, buffer);
        
        /* Adding the message for "temp-per-day-south-station" */
        msg = roxml_add_node(messages, 0, ROXML_ELM_NODE, "message", NULL);
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "to", "temp-per-day-south-station");
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "type", "push");
        point = roxml_add_node(msg, 0, ROXML_ELM_NODE, "point", NULL);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "point", NULL);
        sprintf(buffer, "%.2f", (float)temp - 0.4f);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, buffer);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "point", NULL);
        sprintf(buffer, "%d:%d", soo_outdoor.hour, soo_outdoor.min);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, buffer);

        // create string
        roxml_commit_changes(root, NULL, &buffer, 1);
        roxml_release(RELEASE_LAST);
        roxml_close(root);

        hexstring_to_byte("00000200000000000000000000000002", spid, SPID_SIZE);

        // wait a minute before sending
        sleep(60);

        // send the new message
        
        send_payload(client, spid, buffer);
        
    }

    printf("soo_outdoor_thread stopped.\n");
}

const char* create_messages(const char* id, const char* value, enum MESSAGE_TYPE type) {
    char * buffer;
    node_t *root, *messages, *msg;

    root = roxml_add_node(NULL, 0, ROXML_ELM_NODE, "xml", NULL);

    /* Adding attributes to xml node */
    roxml_add_node(root, 0, ROXML_ATTR_NODE, "version", "1.0");
    roxml_add_node(root, 0, ROXML_ATTR_NODE, "encoding", "UTF-8");

    /* Adding the messages node */
    messages = roxml_add_node(root, 0, ROXML_ELM_NODE, "messages", NULL);

    /* Adding the message itself */
    msg = roxml_add_node(messages, 0, ROXML_ELM_NODE, "message", NULL);

    roxml_add_node(msg, 0, ROXML_ATTR_NODE, "to", id);

    if (type == PUSH) {
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "type", "push");
    }

    roxml_add_node(msg, 0, ROXML_TXT_NODE, NULL, value);

    roxml_commit_changes(root, NULL, &buffer, 1);

    roxml_release(RELEASE_LAST);
    roxml_close(root);

    return (const char*) buffer;
}

const char* manage_event(int client, vuihandler_pkt_t* message) {
    char *id, *action, *value;
    node_t *root, *xml;
    node_t *events, *event, *from, *action_attr, *value_txt;

    printf("events received: %s \n", message->payload);

    root = roxml_load_buf(message->payload);
    xml =  roxml_get_chld(root, NULL, 0);

    events = roxml_get_chld(xml, NULL, 0);

    // read each events
    int i;
    for(i = 0; i < roxml_get_chld_nb(events); ++i){
        event = roxml_get_chld(events, NULL, i);
        from = roxml_get_attr(event, "from", 0);
        action_attr = roxml_get_attr(event, "action", 0);
        value_txt = roxml_get_txt(event, 0);


        strcpy(id, roxml_get_content(from, NULL, 0, NULL));
        strcpy(action, roxml_get_content(action_attr, NULL, 0, NULL));
        strcpy(value, roxml_get_content(value_txt, NULL, 0, NULL));

        // apply action
        if(strcmp(id,"blind-up") == 0) {
            // TODO: if action is "clickUp" create timer.
            // TODO: else action is "clickDown" stop timer.
        } else if(strcmp(id,"blind-up") == 0) {
            // TODO: if action is "clickUp" create timer.
            // TODO: else action is "clickDown" stop timer.
        } else if(strcmp(id,"blind-slider") == 0) {
            // double number = strtod(value, NULL);
            // send the message
        } else if(strcmp(id,"blind-if-lux") == 0) {
            // TODO:
        } else if(strcmp(id,"blind-then-lux") == 0) {
            // TODO:
        } else if(strcmp(id,"blind-on-lux") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-current-temp") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-increase-temp") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-decrease-temp") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-if-temp") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-if-temp-increase") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-if-temp-decrease") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-then-temp") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-then-temp-increase") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-then-temp-decrease") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-else-temp") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-else-temp-increase") == 0) {
            // TODO:
        } else if(strcmp(id,"heat-else-temp-decrease") == 0) {
            // TODO:
        }
    }

    roxml_release(RELEASE_LAST);
    roxml_close(root);
}

// create single thread per ME
// void *soo_outdoor_thread(void *dummy) {
//     // TODO:
// }

// void *soo_blind_thread(void *dummy) {
//     // TODO:
//     int store_position = 1;
//     const int MIN_POS = 0;
//     const int MAX_POS = 5;
// }

// void *soo_heat_thread(void *dummy) {
//     // TODO:
//     double current_heat = 22.5;
//     double if_external_heat = 12.0;
//     double then_internal_heat = 21.5;
//     double else_internal_heat = 20.5;
//     while(running) {
//     }
// }

/**
 * RFCOMM Server which receive a file of any size from the tablet
 */
void *receive_thread(void *dummy) {
    
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char *buf;
    // char size_buf[4] = {0};
    // char get_mobile_entities_message [100] = {0};
    char message_block[BLOCK_SIZE] = {0};
    int s, client = 0;
    // int bytes_read, total_bytes = 0;
    socklen_t opt = sizeof(rem_addr);
    uint32_t size;
    const uint8_t END_SYMBOL = 0x0A;
    int result = -1;
 

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

        do {
            result = read(client, message_block, sizeof(message_block));

            printf("message received : ");
            print_hex_n(message_block, sizeof(message_block));

            vuihandler_pkt_t* message = get_vuihandler_from_bt(message_block);
            switch (message->type)
            {
            case 0x01: ;
                printf("case 0x01\n");
                printf("sending message...\n");
                const char* payload = generate_soo_list();
                char spid[SPID_SIZE];
                memset(spid, '\0', SPID_SIZE);
                send_payload(client, spid, payload);
                printf("message send\n");
                break;
            case 0x04:
                soo_outdoor_thread_running = 0;
                soo_blind_thread_running = 0;
                soo_heat_thread_running = 0;
                char spid_outdoor[SPID_SIZE];
                char spid_blind[SPID_SIZE];
                char spid_heat[SPID_SIZE];

                hexstring_to_byte("00000200000000000000000000000002", spid_outdoor, SPID_SIZE);
                hexstring_to_byte("00000200000000000000000000000001", spid_blind, SPID_SIZE);
                hexstring_to_byte("00000200000000000000000000000003", spid_heat, SPID_SIZE);

                // Select the ME
                if (compare_arrays(message->spid, spid_outdoor, SPID_SIZE) == 0) {
                    // stop old thread
                    if(soo_outdoor_th != NULL) {
                        pthread_join(&soo_outdoor_th, NULL);
                    }

                    // send new soo.outdoor model
                    printf("sending model...\n");
                    send_payload(client, spid_outdoor, generate_soo_outdoor());
                    printf("model send\n");
                    
                    // start new thread
                    printf("starting soo.outdoor...\n");
                    soo_outdoor_thread_running = 1;
                    pthread_create(&soo_outdoor_th, NULL, soo_outdoor_thread, NULL);
                    printf("soo.outdoor started\n");
                } else if (compare_arrays(message->spid, spid_blind, SPID_SIZE) == 0) {
                    soo_blind_thread_running = 1;

                } else if (compare_arrays(message->spid, spid_heat, SPID_SIZE) == 0) {
                    soo_heat_thread_running = 1;

                } else {
                    printf("unknown spid :");
                    print_hex_n(message->spid, SPID_SIZE);
                }
                break;
            case 0x08:
            case 0x88:
                /* code */
                manage_event(client, message);
                break;
            
            default:
                printf("wrong message : %x instead of (%x, %x, %x or %x)!\n", message->type, 0x01, 0x04, 0x08, 0x88);
                continue;
            }
        } while (result != -1);

        printf("Waiting 2s before closing socket...\n");
        sleep(2);

        printf("Closing socket...\n");
        close(client);
        
        free(buf);
        // total_bytes = 0;
    }
    
    printf("Closing socket...\n");
    close(s);

    return NULL;
}

/* This thread launch the MEs and the TX thread threads and then acts as the vuihandler-RX thread 
   receiving the BT paquet. */
int main(int argc, char *argv[]) {
    pthread_t receive_th;
    // pthread_t me1, me2, tx_thread;
    // pthread_t outdoor_th, blind_th, heat_th;

    // init semaphores
    // sem_init(&soo_outdoor_empty, SHARED, 1);
    // sem_init(&soo_outdoor_full, SHARED, 0);
    // sem_init(&soo_blind_empty, SHARED, 1);
    // sem_init(&soo_blind_full, SHARED, 0);
    // sem_init(&soo_heat_empty, SHARED, 1);
    // sem_init(&soo_heat_full, SHARED, 0);
    sem_init(&sem_mutex, SHARED, 1);

    /* Open socket here */


    /* Launch MEs and TX thread here */
    // pthread_create(&outdoor_th, NULL, soo_outdoor_thread, NULL);
    // pthread_create(&blind_th, NULL, soo_blind_thread, NULL);
    // pthread_create(&heat_th, NULL, soo_heat_thread, NULL);


    /* Main RX loop here, which should route rx- data and update MEs accordingly*/


    /* Create the thread which will handle the ME receive from the tablet */
    pthread_create(&receive_th, NULL, receive_thread, NULL);

    signal(SIGINT, sigterm_handler);

    /* */
    while(running);
    return 0;
}
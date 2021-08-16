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

static const char* SOO_BLIND_SPID   = "00000200000000000000000000000001";
static const char* SOO_OUTDOOR_SPID = "00000200000000000000000000000002";
static const char* SOO_HEAT_SPID    = "00000200000000000000000000000003";

#define HEAT_MODIFIER 0.5f
// #define PAYLOAD_SIZE 8192

typedef struct {
	char type;
	char spid[SPID_SIZE];
	char payload[PAYLOAD_SIZE];
} vuihandler_pkt_t;

sem_t sem_mutex;

enum MESSAGE_TYPE {REPLACE, PUSH};

typedef struct {
    int hour;
    int min;
} soo_outdoor_t;

typedef struct {
    float current_heat;
    float if_external_heat;
    float then_internal_heat;
    float else_internal_heat;
    float min_heat;
    float max_heat;
} soo_heat_t;

typedef struct {
    int current_pos;
    int min_pos;
    int max_pos;
    float if_luminosity;
    char* then_action;
    char* on_me;
} soo_blind_t;

soo_outdoor_t soo_outdoor = {
    .hour = 10,
    .min = 25
};

soo_heat_t soo_heat = {
    .current_heat = 22.5f,
    .if_external_heat = 12.0f,
    .then_internal_heat = 21.5f,
    .else_internal_heat = 20.5f,
    .min_heat = 0.0f,
    .max_heat = 35.0f
};

soo_blind_t soo_blind = {
    .current_pos = 1,
    .min_pos = 0,
    .max_pos = 15,
    .if_luminosity = 500.0f,
    .then_action = "down",
    .on_me = "north"
};

pthread_t soo_outdoor_th;

pthread_t soo_blind_th;
pthread_t soo_blind_interaction_th;
sem_t sem_mutex_blind;

pthread_t soo_heat_th;
pthread_t soo_heat_interaction_th;
sem_t sem_mutex_heat;

int soo_outdoor_thread_running = 0;
int soo_blind_thread_running = 0;
int soo_blind_outdoor_interaction_thread_running = 0;
int soo_heat_thread_running = 0;
int soo_heat_outdoor_interaction_thread_running = 0;

int min(int x, int y) {
  return (x < y) ? x : y;
}

float rand_helper(float min, float max) {
    float range = (max - min); 
    float div = RAND_MAX / range;
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
    char* result = (char *) malloc(8192);
    sprintf(
        result,
        "<mobile-entities>\
        <mobile-entity spid=\"%s\">\
            <name>SOO.heat</name>\
            <description>SOO.heat permet de gérer le termostat des radiateurs.</description>\
        </mobile-entity>\
        <mobile-entity spid=\"%s\">\
            <name>SOO.outdoor</name>\
            <description>SOO.outdoor permet de récupérer des informations météorologique telle que la luminosité ambiante ou la température externe.</description>\
        </mobile-entity>\
        <mobile-entity spid=\"%s\">\
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
        </mobile-entities>",
        SOO_HEAT_SPID,
        SOO_OUTDOOR_SPID,
        SOO_BLIND_SPID
    );

    return result;
}

const char* generate_soo_outdoor() {
    char* result = (char *) malloc(8192);
    sprintf(
        result,
        "<model spid=\"%s\">\
        <name>SOO.outdoor</name>\
        <description>SOO.outdoor permet de récupérer les informations d'une station météorologique.</description>\
        <layout>\
            <row><col><label for=\"temp-per-day-graph\">Luminosité selon l'heure de la journée :</label></col></row>\
            <row>\
                <col offset=\"1\" span=\"10\">\
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
                <col offset=\"1\" span=\"10\">\
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
        </model>",
        SOO_OUTDOOR_SPID
    );

    return result;
}

const char* generate_soo_blind() {
    char* result = (char *) malloc(8192);
    sem_wait(&sem_mutex_blind);
    sprintf(
        result,
        "<model spid=\"%s\">\
        <name>SOO.blind</name>\
        <description>SOO.blind permet de gérer la position des stores.</description>\
        <layout>\
            <row>\
                <col span=\"2\"><label for=\"blind-up\">Position des stores :</label></col>\
                <col span=\"2\"><button id=\"blind-up\" lockable=\"true\" lockable-after=\"1.5\">Monter</button></col>\
                <col span=\"2\"><button id=\"blind-down\" lockable=\"true\" lockable-after=\"1.5\">Descendre</button></col>\
                <col span=\"4\"><slider id=\"blind-slider\" max=\"%i\" min=\"%i\" step=\"1\" orientation=\"vertical\">%i</slider></col>\
            </row>\
            <row>\
                <col id=\"blind-outdoor-interaction\"/>\
            </row>\
        </layout>\
        </model>",
        SOO_BLIND_SPID,
        soo_blind.max_pos,
        soo_blind.min_pos,
        soo_blind.current_pos
    );
    sem_post(&sem_mutex_blind);

    return result;
}

const char* generate_soo_heat() {
    char* result = (char *) malloc(8192);
    sem_wait(&sem_mutex_heat);
    sprintf(
        result,
        "<model spid=\"%s\">\
        <name>SOO.heat</name>\
        <description>SOO.heat permet de gérer le termostat des radiateurs.</description>\
        <layout>\
            <row>\
                <col span=\"2\"><text>Température actuelle :</text></col>\
                <col span=\"2\"><number id=\"heat-current-temp\" step=\"0.5\">%.1f</number><label for=\"heat-current-temp\"> °C</label></col>\
                <col span=\"2\"><button id=\"heat-decrease-temp\" lockable=\"true\">-0.5 °C</button></col>\
                <col span=\"2\"><button id=\"heat-increase-temp\" lockable=\"true\">+0.5 °C</button></col>\
            </row>\
            <row>\
                <col id=\"heat-outdoor-interaction\"/>\
            </row>\
        </layout>\
        </model>",
        SOO_HEAT_SPID,
        soo_heat.current_heat
    );
    sem_post(&sem_mutex_heat);

    return result;
}

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
        strncpy(tmpBuf, payload + beginPos, min(PAYLOAD_SIZE, strlen(payload + beginPos)));

        memset(message_block, '\0', BLOCK_SIZE);

        printf("size tmpBuf : %d\n", strnlen(tmpBuf, PAYLOAD_SIZE));
        printf("min %d %d\n",PAYLOAD_SIZE, strnlen(payload + beginPos, PAYLOAD_SIZE));
        if(strnlen(tmpBuf, PAYLOAD_SIZE) < PAYLOAD_SIZE) {
            message_block[0] = 0x02; // 0000 0010
        } else {
            message_block[0] = 0x82; // 1000 0010
        }
        memcpy(message_block + TYPE_SIZE, spid, SPID_SIZE);
        memcpy(message_block + TYPE_SIZE + SPID_SIZE, tmpBuf, PAYLOAD_SIZE);

        printf("sending payload char from %d to %d!\n", beginPos, beginPos + PAYLOAD_SIZE);
        print_hex_n(message_block, BLOCK_SIZE);
        printf("payload send : %s\n", message_block + TYPE_SIZE + SPID_SIZE);
        write(client, message_block, BLOCK_SIZE);
        
        beginPos += PAYLOAD_SIZE;
    } while (beginPos < strlen(payload));
    sem_post(&sem_mutex);
    printf("exiting semaphore mutex...\n");
}

const char* create_messages(const char* id, const char* value, enum MESSAGE_TYPE type) {
    // char * buffer = (char*) malloc(8192);
    char* buffer;
    node_t *root, *messages, *msg;

    // printf("create root\n");
    root = roxml_add_node(NULL, 0, ROXML_PI_NODE, "xml", "version=\"1.0\" encoding=\"UTF-8\"");

    /* Adding attributes to xml node */
    // roxml_add_node(root, 0, ROXML_ATTR_NODE, "version", "1.0");
    // roxml_add_node(root, 0, ROXML_ATTR_NODE, "encoding", "UTF-8");

    /* Adding the messages node */
    printf("create messages\n");
    messages = roxml_add_node(root, 0, ROXML_ELM_NODE, "messages", NULL);

    /* Adding the message itself */
    // printf("create message\n");
    msg = roxml_add_node(messages, 0, ROXML_ELM_NODE, "message", NULL);

    // printf("add attr 'to'\n");
    roxml_add_node(msg, 0, ROXML_ATTR_NODE, "to", id);

    if (type == PUSH) {
        // printf("add attr type=\"push\"\n");
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "type", "push");
    }

    // printf("add content\n");
    roxml_add_node(msg, 0, ROXML_TXT_NODE, NULL, value);

    // printf("commit changes\n");
    roxml_commit_changes(root, NULL, &buffer, 1);

    // printf("realease root\n");
    roxml_release(RELEASE_LAST);
    roxml_close(root);

    return (const char*) buffer;
}

void* soo_outdoor_thread(void* client_arg) {
    int client = *((int*) client_arg);
    float temp;
    char low_buffer[80];
    char *buffer;
    char spid[SPID_SIZE];
    node_t *root, *messages, *msg, *point, *item;
    
    hexstring_to_byte(SOO_BLIND_SPID, spid, SPID_SIZE);

    printf("soo_outdoor_thread started.\n");
    while(soo_outdoor_thread_running){
        //generate random value
        printf("get random values\n");
        temp = rand_helper(13.9, 16);

        printf("create root\n");
        root = roxml_add_node(NULL, 0, ROXML_PI_NODE, "xml", "version=\"1.0\" encoding=\"UTF-8\"");

        /* Adding attributes to xml node */
        // roxml_add_node(root, 0, ROXML_ATTR_NODE, "version", "1.0");
        // roxml_add_node(root, 0, ROXML_ATTR_NODE, "encoding", "UTF-8");

        /* Adding the messages node */
        printf("create messages\n");
        messages = roxml_add_node(root, 0, ROXML_ELM_NODE, "messages", NULL);

        /* Adding the message for "temp-per-day-north-station" */
        printf("create temp-per-day-north-station\n");
        msg = roxml_add_node(messages, 0, ROXML_ELM_NODE, "message", NULL);
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "to", "temp-per-day-north-station");
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "type", "push");
        point = roxml_add_node(msg, 0, ROXML_ELM_NODE, "point", NULL);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "item", NULL);
        sprintf(low_buffer, "%d:%d", soo_outdoor.hour, soo_outdoor.min);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, low_buffer);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "item", NULL);
        sprintf(low_buffer, "%.2f", temp);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, low_buffer);
        
        /* Adding the message for "temp-per-day-south-station" */
        printf("create temp-per-day-south-station\n");
        msg = roxml_add_node(messages, 0, ROXML_ELM_NODE, "message", NULL);
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "to", "temp-per-day-south-station");
        roxml_add_node(msg, 0, ROXML_ATTR_NODE, "type", "push");
        point = roxml_add_node(msg, 0, ROXML_ELM_NODE, "point", NULL);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "item", NULL);
        sprintf(low_buffer, "%d:%d", soo_outdoor.hour, soo_outdoor.min);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, low_buffer);
        item = roxml_add_node(point, 0, ROXML_ELM_NODE, "item", NULL);
        sprintf(low_buffer, "%.2f", temp - 0.4f);
        roxml_add_node(item, 0, ROXML_TXT_NODE, NULL, low_buffer);

        // create string
        printf("create temp-per-day-south-station\n");
        roxml_commit_changes(root, NULL, &buffer, 1);
        roxml_release(RELEASE_LAST);
        roxml_close(root);

        // wait a 15 seconds before sending
        printf("sleep 15 seconds...\n");
        sleep(15);

        if(!soo_outdoor_thread_running) {
            break;
        }

        // send the new message
        printf("sending... payload\n");
        send_payload(client, spid, buffer);
        printf("payload send.\n");

        // update soo.outdoor
        soo_outdoor.min += 2;
        if(soo_outdoor.min >= 60) {
            soo_outdoor.min = soo_outdoor.min % 60;
            soo_outdoor.hour = (soo_outdoor.hour + 1) % 24;
        }
    }

    printf("soo_outdoor_thread stopped.\n");

    return NULL;
}

void* soo_blind_thread(void* args) {
    int client = *((int*) args);
    int direction = *((int*) args + 1); // 0 = up; 1 = down
    int modified = 0;

    char spid[SPID_SIZE];
    hexstring_to_byte(SOO_BLIND_SPID, spid, SPID_SIZE);
    
    do {
        sem_wait(&sem_mutex_blind);
        if(direction == 0 && soo_blind.current_pos <= soo_blind.max_pos){
            soo_blind.current_pos += 1;
            modified = 1;
        } else if(direction == 1 && soo_blind.current_pos >= soo_blind.min_pos) {
            soo_blind.current_pos -= 1;
            modified = 1;
        }
        
        // send new position
        if(modified){
            char value[80];
            sprintf(value, "%i", soo_blind.current_pos);
            const char* payload = create_messages("blind-slider", value, REPLACE);
            printf("sending payload...\n");
            send_payload(
                client, 
                spid,
                payload
            );
            printf("payload send\n");
            free((char*)payload);
        }
        sem_post(&sem_mutex_blind);
        modified = 0;
        sleep(1);
    } while(soo_blind_thread_running);

    return NULL;
}

void* soo_send_outdoor_interaction_with_blind(void* args) {
    int client = *((int*) args);
    printf("sleep 15 seconds before sending interaction with outdoor...\n");
    sleep(15);
    if(!soo_blind_outdoor_interaction_thread_running) {
        return NULL;
    }
    char* result = (char *) malloc(8192);
    char spid[SPID_SIZE];
    hexstring_to_byte(SOO_BLIND_SPID, spid, SPID_SIZE);

    sem_wait(&sem_mutex_blind);
    sprintf(
        result,
        "<layout>\
        <row>\
            <col><text>Condition 1 :</text></col>\
        </row>\
        <row>\
            <col span=\"6\" offset=\"1\">\
                <label for=\"blind-if-lux\">Si la luminosité externe est plus petite que </label>\
                <number id=\"blind-if-lux\" value=\"%.1f\"/>\
                <label for=\"blind-if-lux\"> lux</label>\
            </col>\
        </row>\
        <row>\
            <col span=\"2\" offset=\"1\"><label for=\"blind-then-lux\">Alors </label></col>\
            <col span=\"3\">\
                <dropdown id=\"blind-then-lux\">\
                    <option value=\"up\" default=\"%s\">Monter</option>\
                    <option value=\"down\" default=\"%s\">Descendre</option>\
                </dropdown>\
            </col>\
        </row>\
        <row>\
            <col span=\"2\" offset=\"1\"><label for=\"blind-else-lux\">Sur </label></col>\
            <col span=\"3\">\
                <dropdown id=\"blind-on-lux\">\
                    <option value=\"north\" default=\"%s\">SOO.outdoor Nord</option>\
                    <option value=\"south\" default=\"%s\">SOO.outdoor Sud</option>\
                </dropdown>\
            </col>\
        </row>\
        </layout>",
        soo_blind.if_luminosity,
        strcmp(soo_blind.then_action, "up") == 0 ? "true" : "false",
        strcmp(soo_blind.then_action, "down") == 0 ? "true" : "false",
        strcmp(soo_blind.on_me, "north") == 0 ? "true" : "false",
        strcmp(soo_blind.on_me, "south") == 0 ? "true" : "false"
    );
    sem_post(&sem_mutex_blind);
    const char* payload = create_messages("blind-outdoor-interaction", result, REPLACE);
    send_payload(client, spid, payload);
    free(payload);
}

void* soo_heat_thread(void* args) {
    int client = *((int*) args);
    int direction = *((int*) args + 1); // 0 = up; 1 = down
    int type_field = *((int*) args + 2); // 0 = current; 1 = if_external; 2 = then_external; 3 = else external;
    int modified = 0;
    const char* payload = NULL;

    char spid[SPID_SIZE];
    hexstring_to_byte(SOO_HEAT_SPID, spid, SPID_SIZE);
    
    do {
        sem_wait(&sem_mutex_heat);
        if(type_field == 0 && direction == 0 && soo_heat.current_heat + HEAT_MODIFIER <= soo_heat.max_heat) {
            // printf("a1\n");
            soo_heat.current_heat += HEAT_MODIFIER;
            modified = 1;
        } else if(type_field == 0 && direction == 1 && soo_heat.current_heat - HEAT_MODIFIER >= soo_heat.min_heat) {
            // printf("a2\n");
            soo_heat.current_heat -= HEAT_MODIFIER;
            modified = 1;
        } else if(type_field == 1 && direction == 0 && soo_heat.if_external_heat + HEAT_MODIFIER <= soo_heat.max_heat) {
            // printf("a3\n");
            soo_heat.if_external_heat += HEAT_MODIFIER;
            modified = 1;
        } else if(type_field == 1 && direction == 1 && soo_heat.if_external_heat - HEAT_MODIFIER >= soo_heat.min_heat) {
            // printf("a4\n");
            soo_heat.if_external_heat -= HEAT_MODIFIER;
            modified = 1;
        } else if(type_field == 2 && direction == 0 && soo_heat.then_internal_heat + HEAT_MODIFIER <= soo_heat.max_heat) {
            // printf("a5\n");
            soo_heat.then_internal_heat += HEAT_MODIFIER;
            modified = 1;
        } else if(type_field == 2 && direction == 1 && soo_heat.then_internal_heat - HEAT_MODIFIER >= soo_heat.min_heat) {
            // printf("a6\n");
            soo_heat.then_internal_heat -= HEAT_MODIFIER;
            modified = 1;
        } else if(type_field == 3 && direction == 0 && soo_heat.else_internal_heat + HEAT_MODIFIER <= soo_heat.max_heat) {
            // printf("a7\n");
            soo_heat.else_internal_heat += HEAT_MODIFIER;
            modified = 1;
        } else if(type_field == 3 && direction == 1 && soo_heat.else_internal_heat - HEAT_MODIFIER >= soo_heat.min_heat) {
            // printf("a8\n");
            soo_heat.else_internal_heat -= HEAT_MODIFIER;
            modified = 1;
        } else {
            printf("no modification in soo.heat");
        }
        
        // send new position
        if(modified){
            char value[80];
            switch (type_field) {
            case 0:
                sprintf(value, "%.1f", soo_heat.current_heat);
                payload = create_messages("heat-current-temp", value, REPLACE);
                break;
            case 1:
                sprintf(value, "%.1f", soo_heat.if_external_heat);
                payload = create_messages("heat-if-temp", value, REPLACE);
                break;
            case 2:
                sprintf(value, "%.1f", soo_heat.then_internal_heat);
                payload = create_messages("heat-then-temp", value, REPLACE);
                break;
            default:
                sprintf(value, "%.1f", soo_heat.else_internal_heat);
                payload = create_messages("heat-else-temp", value, REPLACE);
                break;
            }
            
            printf("sending payload...\n");
            send_payload(
                client, 
                spid,
                payload
            );
            printf("payload send\n");
            free((char*)payload);
        }
        sem_post(&sem_mutex_heat);
        modified = 0;
        sleep(1);
    } while(soo_heat_thread_running);

    
    return NULL;
}

void* soo_send_outdoor_interaction_with_heat(void* args) {
    int client = *((int*) args);
    printf("sleep 15 seconds before sending interaction with outdoor...\n");
    sleep(15);
    if(!soo_heat_outdoor_interaction_thread_running) {
        return NULL;
    }
    char* result = (char *) malloc(8192);
    char spid[SPID_SIZE];
    hexstring_to_byte(SOO_HEAT_SPID, spid, SPID_SIZE);

    sem_wait(&sem_mutex_heat);
    sprintf(
        result,
        "<layout>\
        <row>\
            <col><text>Palier 1 :</text></col>\
        </row>\
        <row>\
            <col span=\"2\"><label for=\"heat-if-temp\">Si la température externe est plus petite que </label></col>\
            <col span=\"2\"><number id=\"heat-if-temp\" step=\"0.5\">%.1f</number><label for=\"heat-if-temp\"> °C</label></col>\
            <col span=\"2\"><button id=\"heat-if-temp-decrease\" lockable=\"true\">-0.5 °C</button></col>\
            <col span=\"2\"><button id=\"heat-if-temp-increase\" lockable=\"true\">+0.5 °C</button></col>\
        </row>\
        <row>\
            <col span=\"2\"><label for=\"heat-then-temp\">Alors la température interne vaut </label></col>\
            <col span=\"2\"><number id=\"heat-then-temp\" step=\"0.5\">%.1f</number><label for=\"heat-then-temp\"> °C</label></col>\
            <col span=\"2\"><button id=\"heat-then-temp-decrease\" lockable=\"true\">-0.5 °C</button></col>\
            <col span=\"2\"><button id=\"heat-then-temp-increase\" lockable=\"true\">+0.5 °C</button></col>\
        </row>\
        <row>\
            <col span=\"2\"><label for=\"heat-else-temp\">Sinon la température interne vaut </label></col>\
            <col span=\"2\"><number id=\"heat-else-temp\" step=\"0.5\">%.1f</number><label for=\"heat-else-temp\"> °C</label></col>\
            <col span=\"2\"><button id=\"heat-else-temp-decrease\" lockable=\"true\">-0.5 °C</button></col>\
            <col span=\"2\"><button id=\"heat-else-temp-increase\" lockable=\"true\">+0.5 °C</button></col>\
        </row>\
        </layout>",
        soo_heat.if_external_heat,
        soo_heat.then_internal_heat,
        soo_heat.else_internal_heat
    );
    sem_post(&sem_mutex_heat);
    const char* payload = create_messages("heat-outdoor-interaction", result, REPLACE);
    send_payload(client, spid, payload);
    free(payload);
}

void manage_event(int client, vuihandler_pkt_t* message) {
    char id[1024], action[1024], value[1024];
    // char *id, *action, *value;
    node_t *root, *events, *event, *from, *action_attr, *value_txt;

    printf("events received: %s \n", message->payload);

    root = roxml_load_buf(message->payload);
    events = roxml_get_chld(root, NULL, 0);
    printf("events loaded\n");

    // read each events
    int i;
    for(i = 0; i < roxml_get_chld_nb(events); ++i){
        printf("treating event nth %i\n", i+1);
        event = roxml_get_chld(events, "event", i+1);
        from = roxml_get_attr(event, "from", 0);
        action_attr = roxml_get_attr(event, "action", 0);
        value_txt = roxml_get_txt(event, 0);
        printf("xml parsed\n");

        memset(id, '\0', sizeof(id));
        memset(action, '\0', sizeof(action));
        memset(value, '\0', sizeof(value));
        roxml_get_content(from, id, sizeof(id), NULL);
        roxml_get_content(action_attr, action, sizeof(action), NULL);
        roxml_get_content(value_txt, value, sizeof(value), NULL);

        // apply action
        // blind-up
        if(strcmp(id,"blind-up") == 0) {
            printf("event from blind-up with %s\n", action);

            int args[2] = {client, 0};
            soo_blind_thread_running = 0;

            // stop old thread
            if(soo_blind_th != NULL) {
                sem_wait(&sem_mutex_blind); // avoid the existing thread to take the mutex
                printf("cancelling old SOO.blind thread\n");
                // pthread_cancel(soo_blind_th);
                sem_post(&sem_mutex_blind);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.blind timer...\n");
                soo_blind_thread_running = 1;
                pthread_create(&soo_blind_th, NULL, soo_blind_thread, &args);
                printf("soo.blind timer started\n");
            }
        } 
        // blind-down
        else if(strcmp(id,"blind-down") == 0) {
            printf("event from blind-down with %s\n", action);
            
            int args[2] = {client, 1};
            soo_blind_thread_running = 0;

            // stop old thread
            if(soo_blind_th != NULL) {
                sem_wait(&sem_mutex_blind);
                printf("cancelling old SOO.blind thread\n");
                // pthread_cancel(soo_blind_th);
                sem_post(&sem_mutex_blind);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.blind timer...\n");
                soo_blind_thread_running = 1;
                pthread_create(&soo_blind_th, NULL, soo_blind_thread, &args);
                printf("soo.blind timer started\n");
            }
        } 
        // blind-slider
        else if(strcmp(id,"blind-slider") == 0) {
            printf("event from blind-slider with %s\n", action);
            // float number = strtod(value, NULL);
            int new_pos = atoi(value);
            if (new_pos <= soo_blind.max_pos && new_pos >= soo_blind.min_pos){
                sem_wait(&sem_mutex_blind);
                soo_blind.current_pos = new_pos;
                sem_post(&sem_mutex_blind);
            }
        } 
        // blind-if-lux
        else if(strcmp(id,"blind-if-lux") == 0) {
            printf("event from blind-if-lux with action %s and value %s\n", action, value);
            float number = strtod(value, NULL);
            sem_wait(&sem_mutex_blind);
            soo_blind.if_luminosity = number;
            sem_post(&sem_mutex_blind);
        } 
        // blind-then-lux
        else if(strcmp(id,"blind-then-lux") == 0) {
            printf("event from blind-then-lux with action %s and value %s\n", action, value);
            sem_wait(&sem_mutex_blind);
            soo_blind.then_action = value;
            sem_post(&sem_mutex_blind);
        } 
        // blind-on-lux
        else if(strcmp(id,"blind-on-lux") == 0) {
            printf("event from blind-on-lux with action %s and value %s\n", action, value);
            sem_wait(&sem_mutex_blind);
            soo_blind.on_me = value;
            sem_post(&sem_mutex_blind);
        } 

        // SOO.heat events
        // heat-current-temp
        else if(strcmp(id,"heat-current-temp") == 0) {
            printf("event from heat-current-temp with action %s and value %s\n", action, value);
            float number = strtod(value, NULL);
            sem_wait(&sem_mutex_heat);
            soo_heat.current_heat = number;
            sem_post(&sem_mutex_heat);
        } 
        // heat-increase-temp
        else if(strcmp(id,"heat-increase-temp") == 0) {
            printf("event from heat-increase-temp with %s\n", action);
            // timer
            int args[3] = {client, 0, 0};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        } 
        // heat-decrease-temp
        else if(strcmp(id,"heat-decrease-temp") == 0) {
            printf("event from heat-decrease-temp with %s\n", action);
            // timer
            int args[3] = {client, 1, 0};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        } 
        // heat-if-temp
        else if(strcmp(id,"heat-if-temp") == 0) {
            printf("event from heat-if-temp with action %s and value %s\n", action, value);
            float number = strtod(value, NULL);
            sem_wait(&sem_mutex_heat);
            soo_heat.if_external_heat = number;
            sem_post(&sem_mutex_heat);
        } 
        // heat-if-temp-increase
        else if(strcmp(id,"heat-if-temp-increase") == 0) {
            printf("event from heat-if-temp-increase with %s\n", action);
            // timer
            int args[3] = {client, 0, 1};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        } 
        // heat-if-temp-decrease
        else if(strcmp(id,"heat-if-temp-decrease") == 0) {
            printf("event from heat-if-temp-decrease with %s\n", action);
            // timer
            int args[3] = {client, 1, 1};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        } 
        // heat-then-temp
        else if(strcmp(id,"heat-then-temp") == 0) {
            printf("event from heat-then-temp with action %s and value %s\n", action, value);
            float number = strtod(value, NULL);
            sem_wait(&sem_mutex_heat);
            soo_heat.then_internal_heat = number;
            sem_post(&sem_mutex_heat);
        } 
        // heat-then-temp-increase
        else if(strcmp(id,"heat-then-temp-increase") == 0) {
            printf("event from heat-then-temp-increase with %s\n", action);
            // timer
            int args[3] = {client, 0, 2};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        }  
        // heat-then-temp-decrease
        else if(strcmp(id,"heat-then-temp-decrease") == 0) {
            printf("event from heat-then-temp-decrease with %s\n", action);
            // timer
            int args[3] = {client, 1, 2};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        }  
        // heat-else-temp
        else if(strcmp(id,"heat-else-temp") == 0) {
            printf("event from heat-else-temp with action %s and value %s\n", action, value);
            float number = strtod(value, NULL);
            sem_wait(&sem_mutex_heat);
            soo_heat.else_internal_heat = number;
            sem_post(&sem_mutex_heat);
        }  
        // heat-else-temp-increase
        else if(strcmp(id,"heat-else-temp-increase") == 0) {
            printf("event from heat-else-temp-increase with %s\n", action);
            /// timer
            int args[3] = {client, 0, 3};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        }  
        // heat-else-temp-decrease
        else if(strcmp(id,"heat-else-temp-decrease") == 0) {
            printf("event from heat-else-temp-decrease with %s\n", action);
            // timer
            int args[3] = {client, 1, 3};
            soo_heat_thread_running = 0;

            // stop old thread
            if(soo_heat_th != NULL) {
                sem_wait(&sem_mutex_heat);
                printf("cancelling old SOO.heat thread\n");
                // pthread_cancel(soo_heat_th);
                sem_post(&sem_mutex_heat);
            }

            // if action is "clickDown" create timer.
            if (strcmp(action, "clickDown") == 0) {
                // start new thread
                printf("starting soo.heat timer...\n");
                soo_heat_thread_running = 1;
                pthread_create(&soo_heat_th, NULL, soo_heat_thread, &args);
                printf("soo.heat timer started\n");
            }
        }  
        // unkwown
        else {
            printf("unknown event from %s\n", id);
        }
    }

    roxml_release(RELEASE_LAST);
    roxml_close(root);
    printf("end event_manager\n");
}

/**
 * RFCOMM Server which receive a file of any size from the tablet
 */
void *receive_thread(void *dummy) {
    
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char message_block[BLOCK_SIZE] = {0};
    int s, client = 0;
    socklen_t opt = sizeof(rem_addr);
    int result = -1;
    const char* payload;
 

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

        do {
            memset(message_block, '\0', sizeof(message_block));
            result = read(client, message_block, sizeof(message_block));
            if(result == -1) {
                break;
            }

            printf("\nmessage received : ");
            // print_hex_n(message_block, sizeof(message_block));

            vuihandler_pkt_t* message = get_vuihandler_from_bt(message_block);
            switch (message->type)
            {
            case 0x01: ;
                printf("case 0x01\n");
                printf("sending message...\n");
                payload = generate_soo_list();
                char spid[SPID_SIZE];
                memset(spid, '\0', SPID_SIZE);
                send_payload(client, spid, payload);
                free((char*)payload);
                printf("message send\n");
                break;
            case 0x04:
                printf("case 0x04\n");
                printf("resetting threads\n");
                soo_outdoor_thread_running = 0;
                soo_blind_thread_running = 0;
                soo_blind_outdoor_interaction_thread_running = 0;
                soo_heat_thread_running = 0;
                soo_heat_outdoor_interaction_thread_running = 0;
                char spid_outdoor[SPID_SIZE];
                char spid_blind[SPID_SIZE];
                char spid_heat[SPID_SIZE];

                hexstring_to_byte(SOO_OUTDOOR_SPID, spid_outdoor, SPID_SIZE);
                hexstring_to_byte(SOO_BLIND_SPID, spid_blind, SPID_SIZE);
                hexstring_to_byte(SOO_HEAT_SPID, spid_heat, SPID_SIZE);

                // Select the ME
                // soo.outdoor
                if (compare_arrays(message->spid, spid_outdoor, SPID_SIZE) == 0) {
                    printf("SOO.outdoor selected\n");
                    // stop old thread
                    if(soo_outdoor_th != NULL) {
                        printf("cancelling old SOO.outdoor thread\n");
                        pthread_cancel(soo_outdoor_th);
                    }

                    // send new soo.outdoor model
                    printf("sending model...\n");
                    payload = generate_soo_outdoor();
                    send_payload(client, spid_outdoor, payload);
                    free((char*)payload);
                    printf("model send\n");
                    
                    // start new thread
                    printf("starting soo.outdoor...\n");
                    soo_outdoor_thread_running = 1;
                    pthread_create(&soo_outdoor_th, NULL, soo_outdoor_thread, &client);
                    printf("soo.outdoor started\n");
                } 
                // soo.blind
                else if (compare_arrays(message->spid, spid_blind, SPID_SIZE) == 0) {
                    printf("SOO.blind selected\n");

                    // send new soo.blind model
                    printf("sending model...\n");
                    payload = generate_soo_blind();
                    send_payload(client, spid_blind, payload);
                    free((char*)payload);
                    printf("model send\n");

                    // start new interaction thread
                    printf("starting soo.blind interaction with soo.outdoor...\n");
                    soo_blind_outdoor_interaction_thread_running = 1;
                    pthread_create(&soo_blind_interaction_th, NULL, soo_send_outdoor_interaction_with_blind, &client);
                    printf("soo.blind interaction started\n");
                    
                } 
                // soo.heat
                else if (compare_arrays(message->spid, spid_heat, SPID_SIZE) == 0) {
                    printf("SOO.heat selected\n");

                    // send new soo.heat model
                    printf("sending model...\n");
                    payload = generate_soo_heat();
                    send_payload(client, spid_heat, payload);
                    free((char*)payload);
                    printf("model send\n");

                    // start new interaction thread
                    printf("starting soo.heat interaction with soo.outdoor...\n");
                    soo_heat_outdoor_interaction_thread_running = 1;
                    pthread_create(&soo_heat_interaction_th, NULL, soo_send_outdoor_interaction_with_heat, &client);
                    printf("soo.heat interaction started\n");
                    
                } else {
                    printf("unknown spid :");
                    print_hex_n(message->spid, SPID_SIZE);
                }
                break;
            case 0x08:
            case 0x88:
                /* code */
                printf("case 0xX8\n");
                manage_event(client, message);
                break;
            
            default:
                printf("wrong message : %x instead of (%x, %x, %x or %x)!\n", message->type, 0x01, 0x04, 0x08, 0x88);
                // continue;
            }
        } while (result != -1);

        printf("Waiting 2s before closing socket...\n");
        sleep(2);

        soo_blind_thread_running = 0;
        soo_outdoor_thread_running = 0;
        soo_heat_outdoor_interaction_thread_running = 0;
        soo_heat_thread_running = 0;
        soo_heat_outdoor_interaction_thread_running = 0;

        printf("Closing socket...\n");
        close(client);
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
    sem_init(&sem_mutex_blind, SHARED, 1);
    sem_init(&sem_mutex_heat, SHARED, 1);

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
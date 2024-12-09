#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Config.h"
#include "MQTTClient.h"

struct tbl {
    char error_msg[Len_Error_Msg];
    char error_code[Len_Error_Code];
    struct tbl *next;
};

struct tbl *head = NULL;
struct tbl *tail = NULL;

// this mqtt token is set as global var to ease up this program
volatile MQTTClient_deliveryToken deliveredtoken;

void insert_first(char *error_code, char *error_msg) {
    struct tbl *lk = (struct tbl *)malloc(sizeof(struct tbl));

    if (lk == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    strcpy(lk->error_msg, error_msg);
    strcpy(lk->error_code, error_code);
    lk->next = head;
    head = lk;

    if (tail == NULL) {
        tail = lk;
    }
}

void insert_next(struct tbl *list, char *error_code, char *error_msg) {
    struct tbl *lk = (struct tbl *)malloc(sizeof(struct tbl));

    if (lk == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    strcpy(lk->error_code, error_code);
    strcpy(lk->error_msg, error_msg);

    lk->next = NULL;
    list->next = lk;
}

void search_list(char *error_code) {
    struct tbl *p = head;
    while (p != NULL) {
        if (strcmp(p->error_code, error_code) == 0) {
            printf("Gevonden! \n\n");
            printf("msg: %s\n", p->error_msg);
            return;
        }
        p = p->next;
    }

    printf("error msg not found.\n");
}

void splitStrings(FILE *file) {
    char line[150];
    char *token, *rest;

    // Lees en controleer de eerste regel
    if (fgets(line, sizeof(line), file)) {
        if (line[0] != '#') {
            printf("%s", line);
        }
    }

    // Lees de rest van het bestand
    while (fgets(line, sizeof(line), file)) {
        if (line[0] != '#') {
            char fields[2][65];
            int fieldCount = 0;

            rest = line;
            while ((token = strtok_r(rest, "\t", &rest)) != NULL && fieldCount < 2) {
                strncpy(fields[fieldCount], token, sizeof(fields[fieldCount]) - 1);
                fields[fieldCount][sizeof(fields[fieldCount]) - 1] = '\0';  // Zorg voor null-terminatie
                fieldCount++;
            }

            if (fieldCount == 2) {
                insert_first(fields[0], fields[1]);
            } else {
                printf("Error: %d velden ipv 2 velden!\n", fieldCount);
            }
        }
    }

    fclose(file);
}

void print_list() {
    struct tbl *p = head;
    printf("+------------+----------------------------------------------------+\n");
    printf("%-10s%-50s\n", "ErrCode", "Err_Text");
    printf("+------------+----------------------------------------------------+\n");
    while (p != NULL) {
        printf(" %-10s%-50s\n", p->error_code, p->error_msg);
        p = p->next;
    }
    printf("+------------+----------------------------------------------------+\n");
    printf("End of error list.\n");
}

// This function is called upon when a message is successfully delivered through mqtt
void delivered(void *context, MQTTClient_deliveryToken dt) {
    
    printf("Message with token value %d delivery confirmed\n", dt);
    printf( "-----------------------------------------------\n" );    
    deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *error_in = (char *)message->payload;
    char derdeDeel[50] = "";  // Buffer om het derde deel op te slaan

    // Print inkomend bericht
    printf("msgarrvd: error_in: <%s>\n", error_in);

    // Maak een kopie van error_in om te splitsen
    char temp[ERR_OUT_LEN];
    strncpy(temp, error_in, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';  // Zorg dat de string correct is afgesloten

    // Gebruik strtok om tokens te splitsen
    char *token = strtok(temp, ";");
    int i = 1;
    while (token != NULL) {
        if (i == 3) {  // Derde token opslaan
            strncpy(derdeDeel, token, sizeof(derdeDeel) - 1);
            derdeDeel[sizeof(derdeDeel) - 1] = '\0';  // Zorg voor correcte afsluiting
        }
        token = strtok(NULL, ";");
        i++;
    }

    // Controleer of het derde deel is gevonden
    if (i <= 3) {
        fprintf(stderr, "Error: Derde deel niet gevonden in error_in\n");
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    // Voeg het derde deel toe aan je linked list
    search_list(derdeDeel);

    // Bouw de error_out string
    char error_out[ERR_OUT_LEN];
    int len = snprintf(error_out, sizeof(error_out), "Error: %s", derdeDeel);
    if (len >= sizeof(error_out)) {
        fprintf(stderr, "Error: Formatted string exceeds buffer size\n");
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    // MQTT-publicatie voorbereiden
    MQTTClient client = (MQTTClient)context;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token_out;

    pubmsg.payload = error_out;
    pubmsg.payloadlen = strlen(error_out);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    // Publiceer het bericht
    int rc = MQTTClient_publishMessage(client, TOPIC_PUB, &pubmsg, &token_out);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
    } else {
        printf("Publishing to topic %s\n", TOPIC_PUB);
    }

    // Valideer dat het bericht succesvol is afgeleverd
    rc = MQTTClient_waitForCompletion(client, token_out, TIME);
    printf("Message with delivery token %d delivered, rc=%d\n", token_out, rc);
    printf("Msg out:\t<%s>\n", error_out);

    // Vrijgeven van de ontvangen berichten
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

// This function is called upon when the connection to the mqtt-broker is lost
void connlost(void *context, char *cause) {
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

int main(int argc, char *argv[]) {
    // Check if the filename is provided
    if (argc < 2) {
        printf("Usage: %s <filename.txt>\n", argv[0]);
        return 1;
    }

    // Open the file in read mode
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        file = fopen(default_txt, "r");
        if (file == NULL) {
            perror("Error opening file");
            return 1;
        }
    }

    //splitStrings(file);
    //print_list();

    // Open MQTT client for listening
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Define the correct call back functions when messages arrive
    MQTTClient_setCallbacks(client, client, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    printf("Subscribing to topic %s for client %s using QoS%d\n\n", TOPIC_SUB, CLIENTID, QOS);
    MQTTClient_subscribe(client, TOPIC_SUB, QOS);

    // Keep the program running to continue receiving and publishing messages
    for(;;) {
        ;
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;
}
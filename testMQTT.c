#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Config.h"
#include "MQTTClient.h"

volatile MQTTClient_deliveryToken delivered_token;

int messageArrivedHandler(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    printf("Message arrived on topic: %s\n", topicName);
    printf("Message: %.*s\n", message->payloadlen, (char*)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;  // Indicate successful processing
}

struct tbl {
    char error_msg[Len_Error_Msg];
    char error_code[Len_Error_Code];
    struct tbl *next;
};

struct tbl *head = NULL;
struct tbl *tail = NULL;

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
            printf("Naam: %s\n", p->error_msg);
            return;
        }
        p = p->next;
    }

    printf("Achternaam niet gevonden.\n");
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

void free_list() {
    struct tbl *p = head;
    struct tbl *next;
    while (p != NULL) {
        next = p->next;
        free(p);
        p = next;
    }
    head = NULL;
    tail = NULL;
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

    splitStrings(file);
    print_list();


    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Set callback to handle incoming messages
    MQTTClient_setCallbacks(client, NULL, NULL, messageArrivedHandler, NULL);

    // Connect to the broker
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        free_list();
        MQTTClient_destroy(&client);
        return 1;
    }
    printf("Connected to the broker.\n");

    // Subscribe to the topic
    MQTTClient_subscribe(client, TOPICRecv, QOS);

    printf("Subscribed to topic: %s\n", TOPICRecv);
    printf("Waiting for messages...\n");

    // Keep the main function running to listen for messages
    while (1) {
        // Infinite loop to keep receiving messages
        // Could add some other conditions to exit if needed
    }

    // Disconnect and clean up
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    free_list();
    return rc;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Config.h"
#include "MQTTClient.h"
#include <time.h>

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

char* search_list(const char *error_code) {
    struct tbl *p = head;
    size_t error_code_len = strlen(error_code);
    char trimmed_error_code[error_code_len + 1];
    strncpy(trimmed_error_code, error_code, error_code_len);
    trimmed_error_code[error_code_len] = '\0';
    trimmed_error_code[strcspn(trimmed_error_code, "\r\n")] = '\0';

    while (p != NULL) {
        size_t list_error_code_len = strlen(p->error_code);
        char trimmed_list_error_code[list_error_code_len + 1];
        strncpy(trimmed_list_error_code, p->error_code, list_error_code_len);
        trimmed_list_error_code[list_error_code_len] = '\0';
        trimmed_list_error_code[strcspn(trimmed_list_error_code, "\r\n")] = '\0';

        if (strcmp(trimmed_list_error_code, trimmed_error_code) == 0) {
            printf("Found!\n\n");
            printf("msg: %s\n", p->error_msg);

            // Allocate memory for the copy of p->error_msg
            char *copy = malloc(strlen(p->error_msg) + 1);
            if (copy == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                return NULL;
            }
            strcpy(copy, p->error_msg);
            return copy;
        }
        p = p->next;
    }
    printf("Error message not found.\n");
    return NULL;
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

// This function is called upon when an incoming message from mqtt is arrived
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *error_in = message->payload;
    char error_out[ERR_OUT_LEN];  // Ensure ERR_OUT_LEN is defined and large enough
    char error_in_copy[ERR_IN_LEN];
    char finalMsg[ERR_OUT_LEN];

    char *token1, *token2, *token3, *token4;
    char *sev = NULL;
    char *Application = NULL;
    char *Error_Code = NULL;
    char *Text = NULL;

    int len;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *time_str = asctime(tm);
    time_str[strcspn(time_str, "\n")] = '\0';

    if (strlen(error_in) < ERR_IN_LEN) {
    strcpy(error_in_copy, error_in);
    }
    else{
        printf("Error_in is too long. Truncating to %d characters.\n", ERR_IN_LEN);
        strncpy(error_in_copy, error_in, ERR_IN_LEN - 1);
        error_in_copy[ERR_IN_LEN - 1] = '\0';  // Ensure null-termination
    }

   

    // Print incoming message
    printf("msgarrvd: error_in: <%s>\n", error_in);

    // Get the first token
    token1 = strtok(error_in, ";");
    if (token1 != NULL) {
        // Get the second token
        sev = token1;
        token2 = strtok(NULL, ";");
        if (token2 != NULL) {
            // Get the third token
            Application = token2;
            token3 = strtok(NULL, ";");
            if (token3 != NULL) {
                Error_Code = token3;
                // Get the fourth token
                token4 = strtok(NULL, ";");
                if (token4 != NULL) {
                    Text = token4;
                }
            }
        }
    }

    char Error_test[ERR_IN_LEN];
    // Now you can use Error_Code and Text
    if (Error_Code != NULL) {
        printf("Error Code: %s\n", Error_Code);
    }
    if (Text != NULL) {
        printf("Text: %s\n", Text);
    }
    
    // Format error out msg
    // Ensure the formatted string does not exceed ERR_OUT_LEN
    char *msgIN = search_list(Error_Code);
    if (msgIN!= NULL) {
        if (strstr(msgIN, "%s")!= NULL) {
            sprintf(finalMsg, msgIN, Text);
            printf("%s\n", finalMsg);
            len = snprintf(error_out, sizeof(error_out), "%s;%s;%s;%s;%s", time_str, sev, Application, Error_Code, finalMsg);
        } else {
            fprintf(stderr, "Error: msgIN does not contain %%s.\n");
            len = snprintf(error_out, sizeof(error_out), "%s;%s;%s;%s;%s", time_str, sev, Application, Error_Code, msgIN);
        }
    } else {
        // Handle case where Error_Code is not found
        fprintf(stderr, "Error: Error_Code not found.\n");
        len = snprintf(error_out, sizeof(error_out), "%s;%s;%s;%s", sev, Application, Error_Code, "Error_Code not found");
    }

    if (len >= sizeof(error_out)) {
        fprintf(stderr, "Error: Formatted string exceeds buffer size\n");
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    printf("msgarrvd: error_out: <%s>\n", error_out);

    // Create a new client to publish the error_out message
    MQTTClient client = (MQTTClient)context;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload = error_out;
    pubmsg.payloadlen = strlen(error_out);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    // Publish the error_out message on PUB TOPIC
    int rc = MQTTClient_publishMessage(client, TOPIC_PUB, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
    } else {
        printf("Publishing to topic %s\n", TOPIC_PUB);
    }

    // Validate that message has been successfully delivered
    rc = MQTTClient_waitForCompletion(client, token, TIME);
    printf("Message with delivery token %d delivered, rc=%d\n", token, rc);
    printf("Msg out:\t<%s>\n", error_out);

    // Close the outgoing message queue
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

    splitStrings(file);
    print_list();

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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define HTTPSERVER_IMPL
#include "httpserver.h"

#include <curl/curl.h>

char* serverTimeoffset[30];

void handle_request(struct http_request_s* request) {
    char responseBody[4096] = "";
    int responseBodyIndex = 0;
    for (int i = 0; i < sizeof(serverTimeoffset) / sizeof(serverTimeoffset[0]); i++) {
        if (serverTimeoffset[i] != NULL) {
            for (int j = 0; j < 100; j++) {
                if (serverTimeoffset[i][j] == '\0') {
                    break;
                }
                responseBody[responseBodyIndex] = serverTimeoffset[i][j];
                responseBodyIndex++;
            }
            responseBody[responseBodyIndex] = '\n';
            responseBodyIndex++;
        }
    }
    responseBody[responseBodyIndex] = '\0';

    struct http_response_s* response = http_response_init();

    http_response_status(response, 200);
    http_response_header(response, "Access-Control-Allow-Origin", "*");
    http_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    http_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
    http_response_header(response, "Content-Type", "text/plain"); // For some reason my browser still downloads it as a file
    http_response_body(response, responseBody, sizeof(responseBody) - 1);
    http_respond(request, response);
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
    size_t total_size = size * nmemb;
    strncat(data, ptr, total_size);
    return total_size;
}

char* serverCodes[30];
int serverCodesIndex = 0;

void findServerInData(const char *data) {
    char stringToSplit[4096];
    for (int i = 0; i < 4096; i++) {
        if (data[i] == ',')
            stringToSplit[i] = ' ';
        else
            stringToSplit[i] = data[i];
    }

    char *serverObject;
    serverObject = strtok(stringToSplit, "{}");
    serverCodesIndex = 0;
    while (serverObject != NULL) {
        char *pos = strstr(serverObject, "\"ServerCode\":\"");
        if (pos != NULL) {
            char serverCode[5];
            printf("Found at position: %s\n", pos);
            for (int j = 0; j < 4; j++) {
                // 14 is size of "ServerCode":"
                if (pos[j + 14] == '\"') {
                    serverCode[j] = 'X';
                    break;
                } else {
                    serverCode[j] = pos[j + 14];
                }
                serverCode[4] = '\0';
            } 
            char shortServerCode[4];
            if (serverCode[3] == 'X') {
                for (int j = 0; j < 3; j++) {
                    shortServerCode[j] = serverCode[j];
                }
                shortServerCode[3] = '\0';
            }
            if (serverCode[3] == 'X') {
                printf("Server codeshort: %s\n", shortServerCode);
                serverCodes[serverCodesIndex] = strdup(shortServerCode);
            } else {
                if (serverCode[2] == 't' && serverCode[3] == '4') { // replace with good code instead of this.
                    char longServerCode[] = "int44";
                    printf("Server code: %s\n", longServerCode);
                    serverCodes[serverCodesIndex] = strdup(longServerCode);
                } else {
                    printf("Server code: %s\n", serverCode);
                    serverCodes[serverCodesIndex] = strdup(serverCode);
                }
            }
            serverCodesIndex++;
        } else {
            printf("Code not found in the data\n");
        }
        serverObject = strtok(NULL, "{}"); // goto next object
    }

    for (int i = 0; i < serverCodesIndex; i++) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            fprintf(stderr, "Failed to initialize curl\n");
            return;
        }
        CURLcode res;
        char data[10] = "";

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

        char url[100] = "https://api1.aws.simrail.eu:8082/api/getTimeZone?serverCode=";
        strncat(url, serverCodes[i], strlen(serverCodes[i]));
        printf("URL: %s\n", url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            char fullTimeOffsetString[100];
            snprintf(fullTimeOffsetString, sizeof(fullTimeOffsetString), "%s:%s", serverCodes[i], data);
            serverTimeoffset[i] = strdup(fullTimeOffsetString);
        }
        curl_easy_cleanup(curl);
    }
}

void getServerTimes() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return;
    }
    CURLcode res;
    char data[4096] = "";

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

    curl_easy_setopt(curl, CURLOPT_URL, "https://panel.simrail.eu:8084/servers-open");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return;
    } else {
        printf("Request successful\n");
    }
    curl_easy_cleanup(curl);

    findServerInData(data);

    return;
}

void* getTimeOffsetThread(void* vargp) {
    while(1) {
        getServerTimes();
        sleep(28800); // Every 8 hours
    }
}

int main() {
    pthread_t thread_id;
    printf("Before Thread\n");
    pthread_create(&thread_id, NULL, getTimeOffsetThread, NULL);
    printf("After Thread\n");

    struct http_server_s* server = http_server_init(8080, handle_request);
    http_server_listen(server);
}
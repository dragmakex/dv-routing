/******************************************************************************
 * File: main.c
 *
 * Part 3: Integration
 *   - Exactly one sending thread, one receiving thread:
 *       SenderThread: every 5s => neighborSendHELLO(), neighborRemoveStale()
 *                     if updatedDV => broadcast DV => dvSent()
 *       ReceiverThread: blocks on recvfrom() => parse => if HELLO => neighborProcessHELLO()
 *                                                   if DV => processDistanceVector()
 *   - main() waits until user hits ENTER, then stops everything.
 *
 * Usage:
 *   ./dv_routing [myIp]
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "neighbor.h"
#include "distance.h"

#define HELLO_INTERVAL_SEC  5

/* We use global g_sock, g_broadcastAddr from neighbor.h */
extern int g_sock;
extern struct sockaddr_in g_broadcastAddr;

/* We use updatedDV from distance.h */
extern int updatedDV;

/* A global flag to keep threads running */
static volatile int g_running = 1;

/******************************************************************************
 * broadcastDV
 *   1) getDistanceVector() 
 *   2) send it to 255.255.255.255:5555
 *   3) dvSent()
 ******************************************************************************/
static void broadcastDV(void) {
    char* dvStr = getDistanceVector();
    if (!dvStr) return;

    if (g_sock < 0) {
        free(dvStr);
        return;
    }

    /* Send the DV to broadcast. */
    ssize_t sent = sendto(g_sock, dvStr, strlen(dvStr), 0,
                          (struct sockaddr*)&g_broadcastAddr,
                          sizeof(g_broadcastAddr));
    if (sent < 0) {
        perror("[ERROR] sendto(DV)");
    } else {
        printf("[INFO] Broadcasted DV: %s\n", dvStr);
        dvSent();  // updatedDV=0
    }
    free(dvStr);
}

/******************************************************************************
 * SenderThread
 *   Wakes up every 5s to send HELLO + removeStale.
 *   If updatedDV=1 => broadcast DV.
 ******************************************************************************/
static void* SenderThread(void* arg) {
    (void) arg;
    while (g_running) {
        neighborSendHELLO();
        neighborRemoveStale();

        /* If the distance table changed => broadcast new DV. */
        if (updatedDV) {
            broadcastDV();
        }

        /* Sleep 5 seconds. */
        for (int i = 0; i < HELLO_INTERVAL_SEC; i++) {
            if (!g_running) break;
            sleep(1);
        }
    }
    return NULL;
}

/******************************************************************************
 * parseMessage
 *   If "ip:HELLO:seq" => neighborProcessHELLO(ip, seq)
 *   If "ip:DV:..."    => processDistanceVector()
 ******************************************************************************/
static void parseMessage(const char* msg) {
    if (!msg) return;

    /* We'll do token parse: ipTok : typeTok : rest... */
    char buf[512];
    strncpy(buf, msg, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* saveptr = NULL;
    char* ipTok   = strtok_r(buf, ":", &saveptr);
    char* typeTok = strtok_r(NULL, ":", &saveptr);

    if (!ipTok || !typeTok) return;

    if (strcmp(typeTok, "HELLO") == 0) {
        char* seqTok = strtok_r(NULL, ":", &saveptr);
        if (!seqTok) return;
        unsigned short seqVal = (unsigned short) atoi(seqTok);
        neighborProcessHELLO(ipTok, seqVal);
    } 
    else if (strcmp(typeTok, "DV") == 0) {
        processDistanceVector((char*)msg); 
    }
}

/******************************************************************************
 * ReceiverThread
 *   Blocks on recvfrom(g_sock).
 *   parse -> neighborProcessHELLO or processDistanceVector
 ******************************************************************************/
static void* ReceiverThread(void* arg) {
    (void)arg;

    struct sockaddr_in fromAddr;
    socklen_t addrLen = sizeof(fromAddr);
    char buffer[512];

    while (g_running) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recvfrom(g_sock, buffer, sizeof(buffer)-1, 0,
                                 (struct sockaddr*)&fromAddr, &addrLen);
        if (bytes < 0) {
            // Possibly interrupted or no data
            usleep(100000); // 0.1s
            continue;
        }

        buffer[bytes] = '\0';
        parseMessage(buffer);
    }

    return NULL;
}

/******************************************************************************
 * main
 ******************************************************************************/
int main(int argc, char* argv[]) {
    const char* myIp = (argc > 1) ? argv[1] : "192.168.1.100";
    printf("[INFO] Starting DV Routing on IP=%s\n", myIp);

    if (neighborInit(myIp) != 0) {
        fprintf(stderr, "[ERROR] neighborInit() failed\n");
        return 1;
    }

    pthread_t sThread, rThread;
    if (pthread_create(&sThread, NULL, SenderThread, NULL) != 0) {
        perror("[ERROR] pthread_create(SenderThread)");
        neighborStop();
        return 1;
    }
    if (pthread_create(&rThread, NULL, ReceiverThread, NULL) != 0) {
        perror("[ERROR] pthread_create(ReceiverThread)");
        g_running = 0;
        neighborStop();
        return 1;
    }

    /* Hit ENTER to stop */
    printf("[INFO] Press ENTER to stop...\n");
    getchar();

    g_running = 0;
    pthread_join(sThread, NULL);
    pthread_join(rThread, NULL);

    neighborStop();
    distanceCleanup();

    printf("[INFO] Exiting.\n");
    return 0;
}

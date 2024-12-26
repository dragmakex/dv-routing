/******************************************************************************
 * File: neighbor.c
 *
 * Implementation of Neighbour Detection (Part 1).
 *   - No internal threads used here; main.c handles sending/receiving in a single
 *     sender thread and single receiver thread.
 *
 *   Provides:
 *     - neighborInit()
 *     - neighborStop()
 *     - neighborSendHELLO()
 *     - neighborProcessHELLO()
 *     - neighborRemoveStale()
 *     - neighborPrintTable()
 *
 * We store neighbor info in a linked list with a 10s stale timeout.
 ******************************************************************************/

#include "neighbor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BROADCAST_PORT       5555
#define BROADCAST_IP         "255.255.255.255"
#define NEIGHBOR_TIMEOUT_SEC 10
#define IP_STR_LEN           32

/* Exposed so main can also broadcast DV. */
int g_sock = -1;
struct sockaddr_in g_broadcastAddr;

static char g_myIP[IP_STR_LEN];
static unsigned short g_helloSeq = 0; // increments each time we send HELLO

typedef struct NeighborNode {
    char ip[IP_STR_LEN];
    unsigned short lastSeq;
    time_t lastHeard;
    struct NeighborNode* next;
} NeighborNode;

static NeighborNode* g_neighborsHead = NULL;

/******************************************************************************
 * Utility: current time
 ******************************************************************************/
static inline time_t nowInSeconds(void) {
    return time(NULL);
}

/******************************************************************************
 * findNeighbor
 ******************************************************************************/
static NeighborNode* findNeighbor(const char* ip) {
    NeighborNode* cur = g_neighborsHead;
    while (cur) {
        if (strcmp(cur->ip, ip) == 0) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/******************************************************************************
 * createNeighbor
 ******************************************************************************/
static NeighborNode* createNeighbor(const char* ip, unsigned short seq) {
    NeighborNode* n = (NeighborNode*) malloc(sizeof(NeighborNode));
    if (!n) {
        fprintf(stderr, "[ERROR] Out of memory creating neighbor.\n");
        return NULL;
    }
    strncpy(n->ip, ip, IP_STR_LEN - 1);
    n->ip[IP_STR_LEN - 1] = '\0';
    n->lastSeq   = seq;
    n->lastHeard = nowInSeconds();
    n->next      = g_neighborsHead;
    g_neighborsHead = n;
    return n;
}

/******************************************************************************
 * neighborInit
 ******************************************************************************/
int neighborInit(const char* myIp) {
    if (!myIp) myIp = "0.0.0.0";
    strncpy(g_myIP, myIp, IP_STR_LEN - 1);
    g_myIP[IP_STR_LEN - 1] = '\0';

    // Create socket
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0) {
        perror("[ERROR] socket()");
        return -1;
    }

    // Enable broadcast
    int broadcastPermission = 1;
    if (setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST,
                   &broadcastPermission, sizeof(broadcastPermission)) < 0) {
        perror("[ERROR] setsockopt(SO_BROADCAST)");
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    // Bind to 5555
    struct sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family      = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port        = htons(BROADCAST_PORT);

    if (bind(g_sock, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
        perror("[ERROR] bind()");
        close(g_sock);
        g_sock = -1;
        return -1;
    }

    // Prepare broadcast address
    memset(&g_broadcastAddr, 0, sizeof(g_broadcastAddr));
    g_broadcastAddr.sin_family      = AF_INET;
    g_broadcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    g_broadcastAddr.sin_port        = htons(BROADCAST_PORT);

    g_neighborsHead = NULL;
    g_helloSeq = 0;

    printf("[INFO] neighborInit OK, myIP=%s, sock=%d\n", g_myIP, g_sock);
    return 0;
}

/******************************************************************************
 * neighborStop
 ******************************************************************************/
void neighborStop(void) {
    if (g_sock >= 0) {
        close(g_sock);
        g_sock = -1;
    }
    // free neighbor list
    while (g_neighborsHead) {
        NeighborNode* tmp = g_neighborsHead;
        g_neighborsHead = tmp->next;
        free(tmp);
    }
}

/******************************************************************************
 * neighborSendHELLO
 ******************************************************************************/
void neighborSendHELLO(void) {
    if (g_sock < 0) return;

    char msg[128];
    snprintf(msg, sizeof(msg), "%s:HELLO:%hu", g_myIP, g_helloSeq);
    g_helloSeq++;

    ssize_t sent = sendto(g_sock, msg, strlen(msg), 0,
                          (struct sockaddr*)&g_broadcastAddr,
                          sizeof(g_broadcastAddr));
    if (sent < 0) {
        perror("[ERROR] sendto(HELLO)");
    } else {
        // Debug
        printf("[DEBUG] Sent HELLO: %s\n", msg);
    }
}

/******************************************************************************
 * neighborProcessHELLO
 ******************************************************************************/
void neighborProcessHELLO(const char* senderIP, unsigned short seq) {
    if (!senderIP) return;
    if (strcmp(senderIP, g_myIP) == 0) {
        // ignore self
        return;
    }

    NeighborNode* nb = findNeighbor(senderIP);
    if (!nb) {
        nb = createNeighbor(senderIP, seq);
        if (nb) {
            printf("[INFO] New neighbor discovered: %s (seq=%u)\n", senderIP, seq);
        }
    } else {
        if (seq > nb->lastSeq) {
            nb->lastSeq = seq;
        }
        nb->lastHeard = nowInSeconds();
    }
}

/******************************************************************************
 * neighborRemoveStale
 ******************************************************************************/
void neighborRemoveStale(void) {
    time_t now = nowInSeconds();
    NeighborNode** ptr = &g_neighborsHead;
    while (*ptr) {
        double diff = difftime(now, (*ptr)->lastHeard);
        if (diff > NEIGHBOR_TIMEOUT_SEC) {
            printf("[INFO] Removing stale neighbor: %s\n", (*ptr)->ip);
            NeighborNode* toDel = *ptr;
            *ptr = toDel->next;
            free(toDel);
        } else {
            ptr = &((*ptr)->next);
        }
    }
}

/******************************************************************************
 * neighborPrintTable
 ******************************************************************************/
void neighborPrintTable(void) {
    printf("--- Neighbor Table ---\n");
    time_t now = nowInSeconds();
    for (NeighborNode* cur = g_neighborsHead; cur; cur = cur->next) {
        double diff = difftime(now, cur->lastHeard);
        printf("  %s (seq=%u, lastHeard=%.0f s ago)\n",
               cur->ip, cur->lastSeq, diff);
    }
    printf("----------------------\n");
}

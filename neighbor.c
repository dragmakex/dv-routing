/******************************************************************************
 * File: neighbor.c
 *
 * Description:
 *   Implementation of neighbor detection with a dynamic linked-list.
 *   Handles:
 *     - Creation of a UDP broadcast socket on port 5555
 *     - Sending HELLO messages every 5 seconds
 *     - Receiving/processing HELLO messages
 *     - Tracking neighbors in a linked list, timing them out after 10s
 *
 *   Public API:
 *     - neighborInit(const char* myIp)
 *     - neighborStop(void)
 *     - neighborPrintTable(void)
 *
 * See neighbor.h for more.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#include "neighbor.h"

/******************************************************************************
 * Defines & Constants
 ******************************************************************************/

#define BROADCAST_PORT       5555
#define BROADCAST_IP         "255.255.255.255"
#define HELLO_INTERVAL_SEC   5
#define NEIGHBOR_TIMEOUT_SEC 10
#define IP_STR_LEN           32

/******************************************************************************
 * Types
 ******************************************************************************/
typedef struct NeighborNode {
    char ip[IP_STR_LEN];            // neighbor's IP in dotted-decimal
    unsigned short lastSeq;         // last sequence number we saw from them
    time_t lastHeard;              // last time we received a HELLO
    struct NeighborNode* next;      // pointer to next in the linked list
} NeighborNode;

/******************************************************************************
 * Global/Static Data
 ******************************************************************************/
static int g_sock = -1;            // Our UDP socket
static struct sockaddr_in g_broadcastAddr;
static char g_myIP[IP_STR_LEN];    // Our own IP address (used for HELLO)
static volatile int g_running = 1; // Controls whether threads should keep running

// Linked list head for neighbors
static NeighborNode* g_neighborsHead = NULL;

// Mutex to protect neighbor linked list
static pthread_mutex_t g_neighborListMutex = PTHREAD_MUTEX_INITIALIZER;

/******************************************************************************
 * Utility: current time in seconds
 ******************************************************************************/
static inline time_t nowInSeconds(void)
{
    return time(NULL);
}

/******************************************************************************
 * findNeighbor: looks up an existing neighbor by IP
 *   returns pointer to the node if found, NULL otherwise
 ******************************************************************************/
static NeighborNode* findNeighbor(const char* ip)
{
    NeighborNode* current = g_neighborsHead;
    while (current != NULL) {
        if (strcmp(current->ip, ip) == 0) {
            return current; // found
        }
        current = current->next;
    }
    return NULL; // not found
}

/******************************************************************************
 * createNeighbor: inserts a new neighbor at the front of the list
 *   returns pointer to the new node, or NULL on failure
 ******************************************************************************/
static NeighborNode* createNeighbor(const char* ip, unsigned short seq)
{
    NeighborNode* newNode = (NeighborNode*) malloc(sizeof(NeighborNode));
    if (!newNode) {
        fprintf(stderr, "[ERROR] Out of memory creating neighbor node.\n");
        return NULL;
    }

    // Initialize
    strncpy(newNode->ip, ip, IP_STR_LEN - 1);
    newNode->ip[IP_STR_LEN - 1] = '\0';
    newNode->lastSeq   = seq;
    newNode->lastHeard = nowInSeconds();
    newNode->next      = g_neighborsHead;

    // Insert at head
    g_neighborsHead = newNode;
    return newNode;
}

/******************************************************************************
 * removeStaleNeighbors: removes neighbors not heard from in > NEIGHBOR_TIMEOUT_SEC
 ******************************************************************************/
static void removeStaleNeighbors(void)
{
    time_t now = nowInSeconds();

    pthread_mutex_lock(&g_neighborListMutex);

    NeighborNode** currPtr = &g_neighborsHead;
    while (*currPtr != NULL) {
        NeighborNode* node = *currPtr;
        double diff = difftime(now, node->lastHeard);
        if (diff > NEIGHBOR_TIMEOUT_SEC) {
            // This neighbor is stale; remove it from the list
            printf("[INFO] Link to neighbor %s expired (> %d s)\n",
                   node->ip, NEIGHBOR_TIMEOUT_SEC);
            *currPtr = node->next;
            free(node);
        } else {
            currPtr = &((*currPtr)->next);
        }
    }

    pthread_mutex_unlock(&g_neighborListMutex);
}

/******************************************************************************
 * HelloSenderThread: sends a HELLO message every HELLO_INTERVAL_SEC
 ******************************************************************************/
static void* HelloSenderThread(void* arg)
{
    (void) arg;  // unused

    unsigned short seq = 0;
    while (g_running) {
        // Build the HELLO message in ASCII:
        //    "<myIP>:HELLO:<sequenceNumber>"
        char message[128];
        snprintf(message, sizeof(message), "%s:HELLO:%hu", g_myIP, seq);

        // Send to broadcast
        ssize_t sent = sendto(g_sock, message, strlen(message), 0,
                              (struct sockaddr*)&g_broadcastAddr,
                              sizeof(g_broadcastAddr));
        if (sent < 0) {
            perror("[ERROR] sendto(HELLO) failed");
        } else {
            // Debug
            printf("[DEBUG] Sent HELLO: %s\n", message);
        }

        seq++;

        // Remove stale neighbors
        removeStaleNeighbors();

        // Sleep
        sleep(HELLO_INTERVAL_SEC);
    }
    return NULL;
}

/******************************************************************************
 * processHELLO: handles a received HELLO message
 ******************************************************************************/
static void processHELLO(const char* senderIP, unsigned short seq)
{
    // Ignore HELLO from ourselves
    if (strcmp(senderIP, g_myIP) == 0) {
        return;
    }

    pthread_mutex_lock(&g_neighborListMutex);

    NeighborNode* neighbor = findNeighbor(senderIP);
    if (!neighbor) {
        // Issaaa a new neighbor
        neighbor = createNeighbor(senderIP, seq);
        if (neighbor) {
            printf("[INFO] New neighbor discovered: %s (seq=%u)\n", senderIP, seq);
        }
    } else {
        // Existing neighbor - update
        if (seq > neighbor->lastSeq) {
            neighbor->lastSeq = seq;
        }
        neighbor->lastHeard = nowInSeconds();
    }

    pthread_mutex_unlock(&g_neighborListMutex);
}

/******************************************************************************
 * ReceiverThread: receives broadcast UDP messages and processes them
 ******************************************************************************/
static void* ReceiverThread(void* arg)
{
    (void) arg; // unused

    struct sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);
    char buffer[256];

    while (g_running) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesReceived = recvfrom(
            g_sock, buffer, sizeof(buffer) - 1, 0,
            (struct sockaddr*)&fromAddr, &fromAddrLen
        );
        if (bytesReceived < 0) {
            // This might happen if g_running = 0 and socket closed
            perror("[ERROR] recvfrom() failed");
            continue;
        }

        buffer[bytesReceived] = '\0'; // Null-terminate

        // Parse HELLO message: "<ipTok>:HELLO:<seqTok>"
        // or ignore if doesn't match
        char* saveptr = NULL;
        char* ipTok   = strtok_r(buffer, ":", &saveptr);
        char* helloTok= strtok_r(NULL,    ":", &saveptr);
        char* seqTok  = strtok_r(NULL,    ":", &saveptr);

        if (!ipTok || !helloTok || !seqTok) {
            continue;
        }

        if (strcmp(helloTok, "HELLO") == 0) {
            unsigned short seqNum = (unsigned short) atoi(seqTok);
            processHELLO(ipTok, seqNum);
        } 
    }
    return NULL;
}

/******************************************************************************
 * neighborInit: public function to initialize neighbor detection
 ******************************************************************************/
int neighborInit(const char* myIp)
{
    // Store our IP
    strncpy(g_myIP, myIp, IP_STR_LEN - 1);
    g_myIP[IP_STR_LEN - 1] = '\0';

    // Create socket
    g_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_sock < 0) {
        perror("[ERROR] socket() failed");
        return -1;
    }

    // Enable broadcast
    int broadcastPermission = 1;
    if (setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST,
                   (void*)&broadcastPermission, sizeof(broadcastPermission)) < 0) {
        perror("[ERROR] setsockopt(SO_BROADCAST) failed");
        close(g_sock);
        return -1;
    }

    // Bind to port 5555 (all local interfaces)
    struct sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family      = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port        = htons(BROADCAST_PORT);

    if (bind(g_sock, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
        perror("[ERROR] bind() failed");
        close(g_sock);
        return -1;
    }

    // Prepare broadcast address
    memset(&g_broadcastAddr, 0, sizeof(g_broadcastAddr));
    g_broadcastAddr.sin_family      = AF_INET;
    g_broadcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    g_broadcastAddr.sin_port        = htons(BROADCAST_PORT);

    // Reset global running flag
    g_running = 1;

    // Start threads
    pthread_t senderThread;
    pthread_t receiverThread;

    int rc = pthread_create(&senderThread, NULL, HelloSenderThread, NULL);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] pthread_create(HelloSenderThread) failed (rc=%d)\n", rc);
        close(g_sock);
        return -1;
    }

    rc = pthread_create(&receiverThread, NULL, ReceiverThread, NULL);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] pthread_create(ReceiverThread) failed (rc=%d)\n", rc);
        g_running = 0;
        close(g_sock);
        return -1;
    }

    // Detach threads (or store them to join later)
    pthread_detach(senderThread);
    pthread_detach(receiverThread);

    return 0; // success
}

/******************************************************************************
 * neighborStop: public function to stop neighbor detection
 ******************************************************************************/
void neighborStop(void)
{
    g_running = 0;

    // A short sleep to give threads time to exit
    usleep(200 * 1000);

    if (g_sock >= 0) {
        close(g_sock);
        g_sock = -1;
    }

    // Cleanup neighbor list
    pthread_mutex_lock(&g_neighborListMutex);
    NeighborNode* current = g_neighborsHead;
    while (current) {
        NeighborNode* temp = current;
        current = current->next;
        free(temp);
    }
    g_neighborsHead = NULL;
    pthread_mutex_unlock(&g_neighborListMutex);
}

/******************************************************************************
 * neighborPrintTable: public function to print the neighbor table
 ******************************************************************************/
void neighborPrintTable(void)
{
    pthread_mutex_lock(&g_neighborListMutex);

    printf("--- Neighbor Table ---\n");
    NeighborNode* current = g_neighborsHead;
    time_t now = nowInSeconds();
    while (current) {
        double diff = difftime(now, current->lastHeard);
        printf("  %s (seq=%u, lastHeard=%.0f s ago)\n",
               current->ip, current->lastSeq, diff);
        current = current->next;
    }
    printf("----------------------\n");

    pthread_mutex_unlock(&g_neighborListMutex);
}


/******************************************************************************
 * File: distance.c
 *
 * Implementation of Part 2: Distance Table & DV.
 * 
 * EXACT functions required:
 *   - char* getDistanceVector()
 *   - processDistanceVector(char* DV)
 *   - dvUpdate()
 *   - dvSent()
 *
 * The DV format is:
 *   "senderIP:DV:(dest1,dist1):(dest2,dist2):...:"
 * We'll store routes in a linked list: (dest, viaNeighbor, distance).
 * If table changes => dvUpdate() => updatedDV=1
 * After broadcasting => dvSent() => updatedDV=0
 ******************************************************************************/

#include "distance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IP_STR_LEN 32

typedef struct Route {
    char destIP[IP_STR_LEN];
    char viaNeighbor[IP_STR_LEN];
    int distance;
    struct Route* next;
} Route;

static Route* g_routes = NULL; /* Head of route list */
int updatedDV = 0;

static char g_myIP[IP_STR_LEN] = "0.0.0.0"; /* store sender IP */

/******************************************************************************
 * Utility: findRoute or create
 ******************************************************************************/
static Route* findRoute(const char* dest, const char* via) {
    for (Route* r = g_routes; r; r = r->next) {
        if (strcmp(r->destIP, dest) == 0 &&
            strcmp(r->viaNeighbor, via) == 0) {
            return r;
        }
    }
    return NULL;
}

static Route* createRoute(const char* dest, const char* via, int dist) {
    Route* r = (Route*) malloc(sizeof(Route));
    if (!r) {
        fprintf(stderr, "[ERROR] Out of memory in createRoute.\n");
        return NULL;
    }
    strncpy(r->destIP, dest, IP_STR_LEN - 1);
    r->destIP[IP_STR_LEN - 1] = '\0';
    strncpy(r->viaNeighbor, via, IP_STR_LEN - 1);
    r->viaNeighbor[IP_STR_LEN - 1] = '\0';
    r->distance = dist;
    r->next = g_routes;
    g_routes = r;
    return r;
}

static int findBestDistance(const char* dest) {
    int best = 999999;
    for (Route* r = g_routes; r; r = r->next) {
        if (strcmp(r->destIP, dest) == 0) {
            if (r->distance < best) {
                best = r->distance;
            }
        }
    }
    return best;
}

/******************************************************************************
 * char* getDistanceVector()
 * 
 * Format:
 *   senderIPAddress:DV:(dest1,dist1):(dest2,dist2):...:
 *
 * We'll store "senderIPAddress" in g_myIP the first time or let the caller set it.
 ******************************************************************************/
char* getDistanceVector(void) {
    /* If g_myIP is "0.0.0.0", we just keep it for demonstration,
       or you can set it from main. */

    /* We'll build in a static buffer. But let's do dynamic to be safe. */
    char* dvBuf = (char*) calloc(1, 2048);
    if (!dvBuf) return NULL;

    /* Start with "myIP:DV:" */
    snprintf(dvBuf, 2047, "%s:DV:", g_myIP);

    /* We'll keep track of unique destinations so we don't duplicate. */
    char usedDest[100][IP_STR_LEN];
    int usedCount = 0;

    for (Route* r = g_routes; r; r = r->next) {
        /* see if we already appended r->destIP */
        int found = 0;
        for (int i = 0; i < usedCount; i++) {
            if (strcmp(usedDest[i], r->destIP) == 0) {
                found = 1;
                break;
            }
        }
        if (found) continue;

        /* find best distance for this dest */
        int bestDist = findBestDistance(r->destIP);
        if (bestDist < 999999) {
            /* Append "(dest,dist):" */
            char tuple[128];
            snprintf(tuple, sizeof(tuple), "(%s,%d):", r->destIP, bestDist);
            strncat(dvBuf, tuple, 2047 - strlen(dvBuf));
        }

        /* Mark dest used */
        strncpy(usedDest[usedCount], r->destIP, IP_STR_LEN - 1);
        usedDest[usedCount][IP_STR_LEN - 1] = '\0';
        usedCount++;
    }

    return dvBuf; 
}

/******************************************************************************
 * processDistanceVector(char* DV)
 * 
 * Format: "senderIP:DV:(dest,dist):(dest2,dist2):...:"
 * 
 * For each (dest,dist), we do dist+1 => store route with via=senderIP
 * If table changes => dvUpdate().
 ******************************************************************************/
void processDistanceVector(char* DV) {
    if (!DV) return;

    /* We'll parse in place */
    char buf[1024];
    strncpy(buf, DV, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* saveptr = NULL;
    char* senderIP = strtok_r(buf, ":", &saveptr);
    if (!senderIP) return;

    /* store senderIP in g_myIP if we haven't set it yet, or skip if logic needed. */
    /* But the assignment states 'senderIPAddress' is the IP of the router that created the DV.
       We will just parse it, won't overwrite tho */

    char* dvMarker = strtok_r(NULL, ":", &saveptr);
    if (!dvMarker) return;
    if (strcmp(dvMarker, "DV") != 0) {
        // not a valid DV
        return;
    }

    int changed = 0;
    while (1) {
        char* tuple = strtok_r(NULL, ":", &saveptr);
        if (!tuple) break;  // no more
        // tuple looks like "(dest,dist)"
        if (tuple[0] != '(') continue;
        char inside[128];
        strncpy(inside, tuple + 1, sizeof(inside)-1);
        inside[sizeof(inside)-1] = '\0';
        // remove trailing ')'
        char* rp = strchr(inside, ')');
        if (rp) *rp = '\0';

        // inside => "destIP,dist"
        char* comma = strchr(inside, ',');
        if (!comma) continue;
        *comma = '\0';
        char* destIP = inside;
        char* distStr= comma+1;
        int distVal   = atoi(distStr);

        // cost to sender is 1 => newDist = distVal+1
        int newDist = distVal + 1;

        // find or create route => (destIP, senderIP)
        Route* r = findRoute(destIP, senderIP);
        if (!r) {
            r = createRoute(destIP, senderIP, newDist);
            if (r) changed = 1;
        } else {
            if (r->distance != newDist) {
                r->distance = newDist;
                changed = 1;
            }
        }
    }

    if (changed) {
        dvUpdate();
    }
}

/******************************************************************************
 * dvUpdate
 *   Called when table changes => updatedDV=1
 ******************************************************************************/
void dvUpdate(void) {
    updatedDV = 1;
    printf("[INFO] dvUpdate() => updatedDV = 1\n");
}

/******************************************************************************
 * dvSent
 *   Called after we broadcast => updatedDV=0
 ******************************************************************************/
void dvSent(void) {
    updatedDV = 0;
    printf("[INFO] dvSent() => updatedDV = 0\n");
}

/******************************************************************************
 * printDistanceTable
 ******************************************************************************/
void printDistanceTable(void) {
    printf("=== Distance Table ===\n");
    for (Route* r = g_routes; r; r = r->next) {
        printf("  dest=%s via=%s dist=%d\n", r->destIP, r->viaNeighbor, r->distance);
    }
    printf("======================\n");
}

/******************************************************************************
 * distanceCleanup
 ******************************************************************************/
void distanceCleanup(void) {
    while (g_routes) {
        Route* tmp = g_routes;
        g_routes = tmp->next;
        free(tmp);
    }
}

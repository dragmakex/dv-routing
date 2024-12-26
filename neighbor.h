/******************************************************************************
 * File: neighbor.h
 *
 * Part 1: Neighbour Detection
 * 
 * Required for:
 *  - neighborInit(const char* myIp)  -> sets up UDP sock 5555, broadcast enabled
 *  - neighborStop()                 -> frees neighbor list, closes sock
 *  - neighborSendHELLO()            -> sends "myIp:HELLO:seq" to broadcast
 *  - neighborProcessHELLO(ip, seq)  -> updates neighbor table
 *  - neighborRemoveStale()          -> removes neighbors with no fresh HELLO in >10s
 *  - neighborPrintTable()           -> debug
 *
 ******************************************************************************/

#ifndef NEIGHBOR_H
#define NEIGHBOR_H

#include <arpa/inet.h>

/* 
 * Global socket & broadcast address:
 *    - g_sock: The UDP socket bound to port 5555
 *    - g_broadcastAddr: The broadcast address (255.255.255.255:5555)
 */
extern int g_sock;
extern struct sockaddr_in g_broadcastAddr;

/**
 * @brief Initialize neighbor detection:
 *   - Creates a UDP socket on port 5555 (g_sock).
 *   - Enable broadcast, bind to INADDR_ANY:5555.
 *   - Prepares g_broadcastAddr for 255.255.255.255:5555.
 * @param myIp  The local IP address in dotted-decimal (for logging/HELLO).
 * @return 0 on success, non-zero on error.
 */
int neighborInit(const char* myIp);

/**
 * @brief Stop neighbor detection (close socket, free neighbor list).
 */
void neighborStop(void);

/**
 * @brief Send a HELLO message in ASCII: "myIp:HELLO:seq".
 */
void neighborSendHELLO(void);

/**
 * @brief Process a received HELLO message: if new neighbor => add it, else refresh
 */
void neighborProcessHELLO(const char* senderIP, unsigned short seq);

/**
 * @brief Remove neighbors that haven't sent HELLO for > 10s.
 */
void neighborRemoveStale(void);

/**
 * @brief Print the current neighbor table (debug).
 */
void neighborPrintTable(void);

#ifdef __cplusplus
}
#endif

#endif /* NEIGHBOR_H */

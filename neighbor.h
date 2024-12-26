/******************************************************************************
 * File: neighbor.h
 *
 * Description:
 *   Header file for neighbor detection.
 *   Provides the public API for:
 *     - Initializing the neighbor detection system
 *     - Stopping it
 *     - Printing the current neighbor table
 ******************************************************************************/

#ifndef NEIGHBOR_H
#define NEIGHBOR_H


/**
 * @brief Initialize neighbor detection on the provided IP.
 *
 * This function:
 *  - Creates a broadcast UDP socket bound to port 5555.
 *  - Spawns threads to send HELLO messages and receive/parse incoming messages.
 *  - Maintains an internal neighbor table that times out neighbors after 10s 
 *    if no fresh HELLO is received.
 *
 * @param myIp  A string containing the local IP address in dotted-decimal format.
 * @return  0 on success; non-zero on error.
 */
int neighborInit(const char* myIp);

/**
 * @brief Stops neighbor detection.
 *
 * This function:
 *  - Signals the threads to stop, closes the socket,
 *  - Cleans up any internal data structures (linked list of neighbors),
 *  - Returns.
 */
void neighborStop(void);

/**
 * @brief Prints the current neighbor table to stdout for debugging.
 */
void neighborPrintTable(void);


#endif

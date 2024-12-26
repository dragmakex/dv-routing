/******************************************************************************
 * File: test_neighbor.c
 *
 * Description:
 *   A simple test harness for neighbor detection.
 *
 * Compile Example:
 *   gcc -pthread -o test_neighbor test_neighbor.c neighbor.c
 *
 * Usage:
 *   ./test_neighbor
 ******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include "neighbor.h"

int main(int argc, char* argv[])
{
    // Use a hard-coded ip address or parse from arguments
    const char* myIp = (argc > 1) ? argv[1] : "192.168.1.100";

    printf("[INFO] Initializing neighbor detection on IP: %s\n", myIp);
    if (neighborInit(myIp) != 0) {
        printf("[ERROR] neighborInit() failed\n");
        return 1;
    }

    // Let it run for 30 seconds, printing neighbor table every 5 seconds
    for (int i = 0; i < 6; i++) {
        sleep(5);
        neighborPrintTable();
    }

    printf("[INFO] Stopping neighbor detection\n");
    neighborStop();

    return 0;
}

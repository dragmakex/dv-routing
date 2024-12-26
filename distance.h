/******************************************************************************
 * File: distance.h
 *
 * Part 2: Distance Table + DV logic
 *
 * Required functions:
 *   - char* getDistanceVector() -> returns DV string
 *   - processDistanceVector(char* DV)
 *   - dvUpdate() -> sets updatedDV to true
 *   - dvSent()   -> sets updatedDV to false
 *
 * DV string format:
 *   senderIPAddress:DV:(dest1,dist1):(dest2,dist2):...:
 ******************************************************************************/

#ifndef DISTANCE_H
#define DISTANCE_H

/**
 * @brief Build a string-encoded distance vector in the format:
 *   "myIP:DV:(dest1,dist1):(dest2,dist2):...:"
 * Caller must free() the returned string.
 */
char* getDistanceVector(void);

/**
 * @brief Parse and process a DV string
 *   "senderIP:DV:(dest,dist):(dest2,dist2):...:"
 * If table changes => dvUpdate().
 */
void processDistanceVector(char* DV);

/**
 * @brief Called whenever the DV is updated => sets updatedDV=true
 */
void dvUpdate(void);

/**
 * @brief Called after we broadcast a DV => sets updatedDV=false
 */
void dvSent(void);

/**
 * @brief Print the distance table (debug).
 */
void printDistanceTable(void);

/**
 * @brief Cleanup the distance table
 */
void distanceCleanup(void);

/* The "updatedDV" flag. main can check if updatedDV==true => broadcast new DV. */
extern int updatedDV;

#ifdef __cplusplus
}
#endif

#endif /* DISTANCE_H */

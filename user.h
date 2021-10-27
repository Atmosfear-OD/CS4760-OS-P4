/*
 * user.h by Pascal Odijk 10/24/2021
 * P4 CMPSCI 4760 Prof. Bhatia
 *
 * This is the header file for user.c, it contains macros and the function prototypes.
 */

#ifndef USER_H
#define USER_H

#define CHANCE_TO_TERMINATE 10
#define CHANCE_TO_EXPIRE 90

void allocateShmem();
void allocateQueue();
void addTime(Time* time, int amount);
void addTimeSpec(Time* time, int sec, int ns);
int findPID(int pid);

#endif


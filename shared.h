/*
 * shared.h by Pascal Odijk 10/27/2021
 * P4 CS4760 Prof. Bhatia
 *
 * These are the macros and strucutres needed for the scheduler.
 */

#ifndef SHARED_H
#define SHARED_H

#define MAX_PROCESSES 19

// Time structure
typedef struct {
        unsigned int seconds, ns;
} Time;

// PCB structure
typedef struct {
        int realtime, queueID, pid, loc_pid;
        Time tCpuTime, tSysTime, tBlockedTime, tWaitTime;         
} PCB;

// Shared memory structure
typedef struct {
        PCB proc[MAX_PROCESSES]; //process table
        Time sysTime; //system clock time
        int childcount;
} Shared;

#endif


/*
 * oss.h by Pascal Odijk 10/19/2021
 * P4 CS4760 Prof. Bhatia
 *
 * This is the header file for oss.c, it contains the function prototypes and macros.
 */

#ifndef OSS_H
#define OSS_H

#define MAX_TIME_BETWEEN_NEW_PROCS_NS 150000
#define MAX_TIME_BETWEEN_NEW_PROCS_SEC 1
#define SCHEDULER_CLOCK_ADD_INC 10000
#define CHANCE_TO_BE_REALTIME  80
#define QUEUE_BASE_TIME  10
#define QUEUE_AMOUNT 3

void signalHandler(int signal);
void helpMessage();
void doFork(int value);
void allocateShmem();
void timerHandler(int sig);
int setupInterrupt();
int setupTimer(int s);
void scheduler();
int findEmptyPCB();
void sweepPCB();
void addTimeSpec(Time* time, int sec, int ns);
void addTime(Time* time, int amount);
int findPID(int pid);
int findLocPID(int pid);
void allocateQueue();
void addTimeLong(Time* time, long amount);
void subTime(Time* time, Time* time2);
void subTimeOutput(Time* time, Time* time2, Time* out);

#endif

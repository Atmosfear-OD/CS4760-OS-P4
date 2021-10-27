/*
 * user.c by Pascal Odijk 10/24/2021
 * P4 CMPSCI 4760 Prof. Bhatia
 *
 * This is the program which oss.c calls when forking a user process.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/msg.h>
#include "queue.h"
#include "shared.h"
#include "string.h"
#include "user.h"

// Globals
Shared* data;
int chanceToTerminate = CHANCE_TO_TERMINATE;
int toChildQueue, toParentQueue, ipcid;
char* filen;

// Message queue buffer structure
struct {
        long mtype;
        char mtext[100];
} msgbuf;

// Main function
int main(int argc, int *argv[]) {
        allocateShmem();
        allocateQueue();

        int pid = getpid();

        chanceToTerminate = (data->proc[findPID(pid)].realtime == 1) ? CHANCE_TO_TERMINATE * 2 : CHANCE_TO_TERMINATE; // If a realtime proccess, increase chance to terminate

        int secstoadd = 0;
        int mstoadd = 0;
        int runningIO = 0;
        Time unblockTime;

        srand(time(NULL) ^ (pid << 16));

        while(1) {
                msgrcv(toChildQueue, &msgbuf, sizeof(msgbuf), pid, 0); 
				
		// Chance to terminate
                if((rand() % 100) <= CHANCE_TO_TERMINATE && runningIO == 0) {
                        msgbuf.mtype = pid;
                        strcpy(msgbuf.mtext, "USED_TERM");
                        msgsnd(toParentQueue, &msgbuf, sizeof(msgbuf), 0);

                        int rngTimeUsed = (rand() % 99) + 1;
                        char* convert[15];
                        sprintf(convert, "%i", rngTimeUsed);

                        msgbuf.mtype = pid;
                        strcpy(msgbuf.mtext, convert);
                        msgsnd(toParentQueue, &msgbuf, sizeof(msgbuf), 0);

                        exit(1);
                }

		// Chance to use all time and expire
                if((rand() % 100) <= CHANCE_TO_EXPIRE) {
                        msgbuf.mtype = pid;
                        strcpy(msgbuf.mtext, "EXPIRED");
                        msgsnd(toParentQueue, &msgbuf, sizeof(msgbuf), 0);
                } else {
                        if(runningIO == 0) {
                                unblockTime.seconds = data->sysTime.seconds;
                                unblockTime.ns = data->sysTime.ns;
                                secstoadd = rand() % 6;
                                mstoadd = (rand() % 1001) * 1000000;
                                runningIO = 1;

                                addTimeSpec(&unblockTime, secstoadd, mstoadd);
                                addTimeSpec(&(data->proc[findPID(pid)].tBlockedTime), secstoadd, mstoadd);

                                int rngTimeUsed = (rand() % 99) + 1;
                                char* convert[15];
                                sprintf(convert, "%i", rngTimeUsed);

                                msgbuf.mtype = pid;
                                strcpy(msgbuf.mtext, "USED_PART");
                                msgsnd(toParentQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT); 

                                msgbuf.mtype = pid;
                                strcpy(msgbuf.mtext, convert);
                                fflush(stdout);
                                msgsnd(toParentQueue, &msgbuf, sizeof(msgbuf), 0);

				// Loop until unblock time
                                while(1) {
                                        if(data->sysTime.seconds >= unblockTime.seconds && data->sysTime.ns >= unblockTime.ns) break;
                                }

                                msgbuf.mtype = pid;
                                strcpy(msgbuf.mtext, "USED_IO_DONE");
                                msgsnd(toParentQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);

                                runningIO = 0;
                        }
                }
        }
}

// Find the PCB with pid and return the position, else return -1
int findPID(int pid) {
        for (int i = 0; i < MAX_PROCESSES; i++)
                if (data->proc[i].pid == pid)
                        return i;
        return -1;
}

// Add time to given time structure
void addTime(Time* time, int amount) {
        int _nano = time->ns + amount;

        while (_nano >= 1000000000) {
                _nano -= 1000000000;
                (time->seconds)++;
        }

        time->ns = _nano; 
}

// Specifically add seconds and nanoseconds to time
void addTimeSpec(Time* time, int sec, int ns) {
        time->seconds += sec;
        addTime(time, ns);
}

// Allocate message queues
void allocateQueue() {
        key_t shmkey = ftok("shmemQ", 766);
        if(shmkey == -1) {
                printf("\n%s: ", filen);
                fflush(stdout);
                perror("oss: Error: Ftok failure");
                return;
        }

        toChildQueue = msgget(shmkey, 0600 | IPC_CREAT);
        if(toChildQueue == -1) {
                printf("\n%s: ", filen);
                fflush(stdout);
                perror("oss: Error: toChildQueue creation failure");
                return;
        }

        shmkey = ftok("shmemQ2", 767);
        if(shmkey == -1) {
                printf("\n%s: ", filen);
                fflush(stdout);
                perror("oss: Error: Ftok failure");
                return;
        }

        toParentQueue = msgget(shmkey, 0600 | IPC_CREAT);
        if(toParentQueue == -1) {
                printf("\n%s: ", filen);
                fflush(stdout);
                perror("oss: Error: toParentQueue creation failure");
                return;
        }
}

// Allocate shared memory
void allocateShmem() {
        key_t shmkey = ftok("shmem", 312);
        if (shmkey == -1) {
                printf("\n%s: ", filen);
                fflush(stdout);
                perror("oss: Error: Ftok failure");
                return;
        }

        ipcid = shmget(shmkey, sizeof(Shared), 0600 | IPC_CREAT);
        if (ipcid == -1) {
                printf("\n%s: ", filen);
                fflush(stdout);
                perror("oss: Error: Failure to get shared memory");
                return;
        }

        data = (Shared*)shmat(ipcid, (void*)0, 0); //attach to shared mem
        if (data == (void*)-1) {
                printf("\n%s: ", filen);
                fflush(stdout);
                perror("oss: Error: Failure to attach to shared memory");
                return;
        }
}

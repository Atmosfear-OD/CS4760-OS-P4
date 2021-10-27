/*
 * oss.c by Pascal Odijk 10/19/2021
 * P4 CS4760 Prof. Bhatia
 *
 * Entry point to the prgram. This is were the program schedules process though a simulated os and forks user processes which exec user.c.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
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
#include "oss.h"

// Globals
static Queue* *active;
static Queue* *expired;
int ipcid;
Shared* data;
int toChildQueue;
int toParentQueue;
int locpidcnt = 0;
FILE* filep;

// Message queue buffer structure
struct {
        long mtype;
        char mtext[100];
} msgbuf;

// Main function
int main(int argc, char* argv[]) {
	signal(SIGINT, signalHandler);
	signal(SIGALRM, signalHandler);

	int opt;
	int s = 3; // Default termination time is 3 seconds
	char *fileName = "outFile.log"; // Default output filename
	
	// Check for opt args
	while((opt = getopt(argc, argv, "hs:l:")) != -1) {
		switch(opt) {
			case 'h':
				helpMessage();
				exit(0);
			case 's':
				if(!isdigit(*optarg) || (s = atoi(optarg)) < 0) {
					perror("oss: Error: Invalid termination time");
					helpMessage();
					exit(1);
				}

				break;
			case 'l':
				if(isdigit(*optarg)) {
					perror("oss: Error: File name must be a string");
					helpMessage();
					exit(1);
				}
				
				fileName = optarg;
				break;
			default:
				perror("oss: Error: Invalid argument");
				helpMessage();
				exit(1);
				break;
		}
	}
	
	// Open output file
	if((filep = fopen(fileName, "w")) == NULL) {
		perror("oss: Error: Failure opening file");
		exit(1);
	}
	
	// Initialize interrupt handler and timer
	if (setupInterrupt() == -1) {
		perror("oss: Error: Failure to setup interrupt handler");
		return 1;
	}

	if (setupTimer(s) == -1) {
		perror("oss: Error: Failure to setup timer");
		return 1;
	}
	
	// Initialize shared memory, message queues, and PCB
	allocateShmem();
	allocateQueue();
	sweepPCB();
	scheduler();

	// Close output file and terminate
	fflush(filep);
	fclose(filep);
	printf("oss: Terminate: Message queues and shared memory are released\n");

	return 0;
}

// Handle ctrl-c and timer end
void signalHandler(int signal) {     
	if(signal == SIGINT) {
                printf("oss: Terminate: CTRL+C encountered\n");
                fflush(stdout);
        } else if(signal == SIGALRM) {
                printf("oss: Terminate: Termination time has elapsed\n");
                fflush(stdout);
        }
        
        for(int i = 0; i < MAX_PROCESSES; i++) {
                if(data->proc[i].pid != -1) {
                        kill(data->proc[i].pid, SIGTERM);
		}
	}
		
	// Close file and deallocate memory and queues
        fflush(stdout);
	fflush(filep);
        fclose(filep);
        shmctl(ipcid, IPC_RMID, NULL);
        msgctl(toChildQueue, IPC_RMID, NULL);
        msgctl(toParentQueue, IPC_RMID, NULL);

        printf("oss: Termination: Killed processes and removed shared memory/message queues\n");

        kill(getpid(), SIGTERM);
}

// Display usage message
void helpMessage() {
	printf("---------- USAGE ----------\n");
	printf("./oss [-h] [-s t] [-l f]\n");
	printf("-h\tDisplays usage message\n");
	printf("-s t\tWhere t indicates max seconds before system terminates (t should be an int greater than or equal to 0)\n");
	printf("-l f\tWhere f specifies a particular name for the log file\n");
	printf("---------------------------\n");
}

// Find average time and overwrite time structure with the averages
void averageTime(Time* time, int count) {
        long timeEpoch = (((long)(time->seconds) * (long)1000000000) + (long)(time->ns))/count;

        Time temp = {0, 0};
        addTimeLong(&temp, timeEpoch);

        time->seconds = temp.seconds;
        time->ns = temp.ns;
}

// Add time to time structure
void addTime(Time* time, int amount) {
        int _nano = time->ns + amount;
        while (_nano >= 1000000000) {
                _nano -= 1000000000;
                (time->seconds)++;
        }
        time->ns = _nano;
}

// Specificall add given seconds and nanoseconds to time structure
void addTimeSpec(Time* time, int sec, int ns) {
	time->seconds += sec;
	addTime(time, ns);
}

// Add time -- but long
void addTimeLong(Time* time, long amount) {
        long _nano = time->ns + amount;
		
        while (_nano >= 1000000000) {
                _nano -= 1000000000;
                (time->seconds)++;
        }
		
        time->ns = (int)_nano;
}

// Subtract time convert back to time structure -- Overwrites first param
void subTime(Time* time, Time* time2) {
        long epochTime1 = (time->seconds * 1000000000) + (time->ns);
        long epochTime2 = (time2->seconds * 1000000000) + (time2->ns);

        long epochTimeDiff = abs(epochTime1 - epochTime2);

        Time temp;
        temp.seconds = 0;
        temp.ns = 0;

        addTimeLong(&temp, epochTimeDiff);

        time->seconds = temp.seconds;
        time->ns = temp.ns;
}

// Subtract time convert back to time structure -- Overwrites third param
void subTimeOutput(Time* time, Time* time2, Time* out) {
        long epochTime1 = (time->seconds * 1000000000) + (time->ns);
        long epochTime2 = (time2->seconds * 1000000000) + (time2->ns);

        long epochTimeDiff = abs(epochTime1 - epochTime2);

        Time temp;
        temp.seconds = 0;
        temp.ns = 0;

        addTimeLong(&temp, epochTimeDiff); 

        out->seconds = temp.seconds;
        out->ns = temp.ns;
}

// Fork to create child process
void doFork(int value) {
        char* forkarg[] = { "./user", NULL };

        execv(forkarg[0], forkarg);
        signalHandler(1);
}

// Allocate shared memory
void allocateShmem() {
        key_t shmkey = ftok("shmem", 312);
        if (shmkey == -1) {
                fflush(stdout);
                perror("Error: Ftok failure");
                return;
        }

        ipcid = shmget(shmkey, sizeof(Shared), 0600 | IPC_CREAT);
        if(ipcid == -1) {
                fflush(stdout);
                perror("oss: Error: Failure to get shared memory");
                return;
        }

        data = (Shared*)shmat(ipcid, (void*)0, 0);
        if(data == (void*)-1) {
                fflush(stdout);
                perror("oss: Error: Failure to attach to shared memory");
                return;       
        }

}

// Allocate message queues
void allocateQueue() {
	key_t shmkey = ftok("shmemQ", 766);
	if(shmkey == -1) {
		fflush(stdout);
		perror("oss: Error: Ftok failure");
		return;
	}

	toChildQueue = msgget(shmkey, 0600 | IPC_CREAT);
	if(toChildQueue == -1) {
		fflush(stdout);
		perror("oss: Error: toChildQueue creation failure");
		return;
	}

	shmkey = ftok("shmemQ2", 767);
	if(shmkey == -1) {
		fflush(stdout);
		perror("oss: Error: Ftok failure");
		return;
	}

	toParentQueue = msgget(shmkey, 0600 | IPC_CREAT);
	if(toParentQueue == -1) {
		fflush(stdout);
		perror("oss: Error: toParentQueue creation failure");
		return;
	}
}	

// Handle the timer hitting limit
void timerHandler(int sig) {
	signalHandler(sig);
}

// Setup interrupt handling
int setupInterrupt() {
        struct sigaction act;
        act.sa_handler = timerHandler;
        act.sa_flags = 0;
        return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

// Setup interrupt handling from the timer
int setupTimer(int s) {
        struct itimerval value;
        value.it_interval.tv_sec = s;
        value.it_interval.tv_usec = 0;
        value.it_value = value.it_interval;
		
        return (setitimer(ITIMER_PROF, &value, NULL));
}

// Find the next empty PCB and return address, else return -1
int findEmptyPCB() {
        for(int i = 0; i < MAX_PROCESSES; i++) {
                if(data->proc[i].pid == -1)
                        return i;
        }

        return -1;
}

// Sets all positions in PCB to initial value of -1
void sweepPCB() {
       
        for(int i = 0; i < MAX_PROCESSES; i++)
                data->proc[i].pid = -1;
}

// Find the PCB with the pid and return address, else return -1
int findPID(int pid) {
        int i;
        for(i = 0; i < MAX_PROCESSES; i++)
                if(data->proc[i].pid == pid)
                        return i;
        return -1;
}

// Find the PCB with local PID and return address, else return -1
int findLocPID(int pid) {
        for(int i = 0; i < MAX_PROCESSES; i++)
                if(data->proc[i].loc_pid == pid)
                        return i;
        return -1;
}

// Create queues
void createQueueSet(struct Queue* arr[]) {
        for(int i = 0; i < 4; i++) {
                arr[i] = createQueue(MAX_PROCESSES);
        }
}

// Swap active and expired queues
void swapActiveAndExpired() {
        Queue* *tmp;
        tmp = active;
        active = expired;
        expired = tmp;
}


// Scheduler
void scheduler() {
        // Variables for scheduling
        int activeProcs = 0;
        int remainingExecs = 100;
        int exitCount = 0;
        int status;
        int activeProcIndex = -1;
        int procRunning = 0;
        int msgsize;

        // Set shared memory timer
        data->sysTime.seconds = 0;
        data->sysTime.ns = 0;

        // Timer variables
        Time nextExec = { 0, 0 };
        Time timesliceEnd = { 0, 0 };
        Time totalCpuTime = { 0, 0 };
        Time totalWaitTime = { 0, 0 };
        Time totalBlockedTime = { 0, 0 };
        Time totalTime = { 0, 0 };
	Time idleTime = { 0, 0 };

	// Queues
	Queue* qset1[QUEUE_AMOUNT];
	Queue* qset2[QUEUE_AMOUNT];
	Queue* queueBlock = createQueue(MAX_PROCESSES);
	createQueueSet(qset1);
	createQueueSet(qset2);
	
	// Keep track of expired and active sets
	active = qset1;
	expired = qset2;
	
	//Create Queue times
	int queueCost0 = (QUEUE_BASE_TIME) * 1000000;
	int queueCost1 = (QUEUE_BASE_TIME / 2) * 1000000;
	int queueCost2 = (QUEUE_BASE_TIME / 3) * 1000000;
	int queueCost3 = (QUEUE_BASE_TIME / 4) * 1000000;

	int pauseSent = 0;

	srand(time(0));

	while(1) {
		addTime(&(data->sysTime), SCHEDULER_CLOCK_ADD_INC);
		addTime(&(idleTime), SCHEDULER_CLOCK_ADD_INC);

		pid_t pid;
		int usertracker = -1;
		
		if(remainingExecs > 0 && activeProcs < MAX_PROCESSES && (data->sysTime.seconds >= nextExec.seconds) && (data->sysTime.ns >= nextExec.ns)) 
		{
			pid = fork();

			if(pid < 0) {
				perror("oss: Error: Fork failure");
				signalHandler(1);
			}

			remainingExecs--; 
			if (pid == 0) {
				doFork(pid);
			}
			
			// Setup the next exec for proccess
			nextExec.seconds = data->sysTime.seconds;
			nextExec.ns = data->sysTime.ns;
			int secstoadd = abs(rand() % (MAX_TIME_BETWEEN_NEW_PROCS_SEC + 1));
			int nstoadd = abs((rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_NS + 1));
			addTimeSpec(&nextExec, secstoadd, nstoadd);

			// Set child process to next empty position in PCB
			int pos = findEmptyPCB();
			if(pos > -1) {
				// Initialize PCB
				data->proc[pos].pid = pid;
				data->proc[pos].realtime = ((rand() % 100) < CHANCE_TO_BE_REALTIME) ? 1 : 0; //Random calculation for assigning realtime or user

				data->proc[pos].tCpuTime.seconds = 0;
				data->proc[pos].tCpuTime.ns = 0;
				data->proc[pos].tBlockedTime.seconds = 0;
				data->proc[pos].tBlockedTime.ns = 0;
				data->proc[pos].tSysTime.seconds = data->sysTime.seconds;
				data->proc[pos].tSysTime.ns = data->sysTime.ns;

				data->proc[pos].loc_pid = ++locpidcnt;

				// Place process in queue based off priority
				if(data->proc[pos].realtime == 1)
				{
					data->proc[pos].queueID = 0; // Realtime
					enqueue(active[0], data->proc[pos].loc_pid);
					fprintf(filep, "OSS: Process created with PID %i [LOCAL PID: %i] placed into queue: 0 at time %i:%i\n",
						data->proc[pos].pid, data->proc[pos].loc_pid, data->sysTime.seconds, data->sysTime.ns);

				}
				else {
					data->proc[pos].queueID = 1; // User
					enqueue(active[1], data->proc[pos].loc_pid);
					fprintf(filep, "OSS: Process created with PID %i [LOCAL PID: %i] placed into queue: 1 at time %i:%i\n",
						data->proc[pos].pid, data->proc[pos].loc_pid, data->sysTime.seconds, data->sysTime.ns);
				}

				activeProcs++;
			} else {
				kill(pid, SIGTERM); // Child failed to find empty PCB position -- terminate it
			}
		}

		// If there is a process in the queue, check if it has sent a message
		if(procRunning == 1) {
			if((msgsize = msgrcv(toParentQueue, &msgbuf, sizeof(msgbuf), data->proc[activeProcIndex].pid, 0)) > -1) {
				if(strcmp(msgbuf.mtext, "USED_TERM") == 0) {
					msgrcv(toParentQueue, &msgbuf, sizeof(msgbuf), data->proc[activeProcIndex].pid, 0);

					int i;
					sscanf(msgbuf.mtext, "%i", &i); // Convert from string to int
					int cost;

					printf("[LOCAL PID: %i] Time Finished: %i:%i, Time Started: %i:%i\n",
						data->proc[activeProcIndex].loc_pid, data->sysTime.seconds, data->sysTime.ns, 
						data->proc[activeProcIndex].tSysTime.seconds, data->proc[activeProcIndex].tSysTime.ns);

					subTimeOutput(&(data->sysTime), &(data->proc[activeProcIndex].tSysTime), &(data->proc[activeProcIndex].tSysTime));

					// Increment time based off what queue the process is in
					switch(data->proc[activeProcIndex].queueID) {
						case 0:
							cost = (int)((double)queueCost0 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Terminated process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n",
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							addTime(&(data->sysTime), cost);
							break;
						case 1:
							cost = (int)((double)queueCost1 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Terminated process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n",
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							addTime(&(data->sysTime), cost);
							break;
						case 2:
							cost = (int)((double)queueCost2 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Terminated process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n",
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							addTime(&(data->sysTime), cost);
							break;
						case 3:
							cost = (int)((double)queueCost3 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Terminated process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n",
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							addTime(&(data->sysTime), cost);
							break;
					}

					procRunning = 0; // Proccess is no longer running
				}
				// Child uses all time and expires
				else if (strcmp(msgbuf.mtext, "EXPIRED") == 0) {
					switch(data->proc[activeProcIndex].queueID) {
						case 0:
							enqueue(active[0], data->proc[findPID(msgbuf.mtype)].loc_pid); // Requeue the proccess into the same queue since it is realtime
							data->proc[findPID(msgbuf.mtype)].queueID = 0;
							addTime(&(data->sysTime), queueCost0);
							fprintf(filep, "\tOSS: Queue shifted [0 -> 0] process with PID: %i [LOCAL PID: %i] after %i ns at time %i:%i\n", 
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, queueCost0, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->proc[activeProcIndex].tCpuTime), queueCost0);
							break;
						case 1:
							if(&(data->proc[activeProcIndex].tCpuTime) >= queueCost1) {
								enqueue(expired[2], data->proc[findPID(msgbuf.mtype)].loc_pid);
								data->proc[findPID(msgbuf.mtype)].queueID = 2;
								fprintf(filep, "\tOSS: Queue shifted [1 -> expired 2] process with PID: %i [LOCAL PID: %i] after %i ns at time %i:%i\n", 
									data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, 
									queueCost2, data->sysTime.seconds, data->sysTime.ns);
								addTime(&(data->sysTime), queueCost2);
								addTime(&(data->proc[activeProcIndex].tCpuTime), queueCost2);
							} else {
								enqueue(expired[1], data->proc[findPID(msgbuf.mtype)].loc_pid);
								data->proc[findPID(msgbuf.mtype)].queueID = 1;
								fprintf(filep, "\tOSS: Queue shifted [1 -> expired 1] process with PID: %i [LOCAL PID: %i] after %i ns at time %i:%i\n", 
									data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, 
									queueCost1, data->sysTime.seconds, data->sysTime.ns);
								addTime(&(data->sysTime), queueCost1);  
								addTime(&(data->proc[activeProcIndex].tCpuTime), queueCost1);
							}
							break;
						case 2:
							if(&(data->proc[activeProcIndex].tCpuTime) >= queueCost2) {
								enqueue(expired[3], data->proc[findPID(msgbuf.mtype)].loc_pid);
								data->proc[findPID(msgbuf.mtype)].queueID = 3;
								fprintf(filep, "\tOSS: Queue shifted [2 -> expired 3] process with PID: %i [LOCAL PID: %i] after %i ns at time %i:%i\n", 
									data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, 
									queueCost3, data->sysTime.seconds, data->sysTime.ns);
								addTime(&(data->sysTime), queueCost3);
								addTime(&(data->proc[activeProcIndex].tCpuTime), queueCost3);
							} else {
								enqueue(expired[2], data->proc[findPID(msgbuf.mtype)].loc_pid);
								data->proc[findPID(msgbuf.mtype)].queueID = 2;				
								fprintf(filep, "\tOSS: Queue shifted [2 -> expired 2] process with PID: %i [LOCAL PID: %i] after %i ns at time %i:%i\n", 
									data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, 
									queueCost2, data->sysTime.seconds, data->sysTime.ns);
								addTime(&(data->sysTime), queueCost2);
								addTime(&(data->proc[activeProcIndex].tCpuTime), queueCost2);
							}
							break;
						case 3:
							enqueue(expired[3], data->proc[findPID(msgbuf.mtype)].loc_pid);
							data->proc[findPID(msgbuf.mtype)].queueID = 3;
							fprintf(filep, "\tOSS: Queue shifted [3 -> expired 3] process with PID: %i [LOCAL PID: %i] after %i ns at time %i:%i\n", 
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, queueCost3, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->sysTime), queueCost3);
							addTime(&(data->proc[activeProcIndex].tCpuTime), queueCost3);
							break;
					}
					
					procRunning = 0; // Proccess is no longer running
				}
				// Process only used part of their time and becomed blocked
				else if (strcmp(msgbuf.mtext, "USED_PART") == 0) {
					msgrcv(toParentQueue, &msgbuf, sizeof(msgbuf), data->proc[activeProcIndex].pid, 0); // Wait for child to send percentage used

					int i;
					sscanf(msgbuf.mtext, "%i", &i); // Convert from string to int
					int cost;

					switch (data->proc[activeProcIndex].queueID) {
						case 0:
							cost = (int)((double)queueCost0 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Blocked process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n", 
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->sysTime), cost);
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							break;
						case 1:
							cost = (int)((double)queueCost1 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Blocked process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n", 
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->sysTime), cost);
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							break;
						case 2:
							cost = (int)((double)queueCost2 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Blocked process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n", 
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->sysTime), cost);
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							break;
						case 3:
							cost = (int)((double)queueCost3 * ((double)i / (double)100));
							fprintf(filep, "\tOSS: Blocked process with PID %i [LOCAL PID: %i] after %i ns at time %i:%i\n\n", 
								data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, cost, data->sysTime.seconds, data->sysTime.ns);
							addTime(&(data->sysTime), cost);  
							addTime(&(data->proc[activeProcIndex].tCpuTime), cost);
							break;
					}

					enqueue(queueBlock, data->proc[findPID(msgbuf.mtype)].loc_pid); // Load the processes into the blocked queue if they have used only part of time
					procRunning = 0; // Proccess is no longer running
				}
			}
		}
		// If blocked queue is empty, wait until a process is ready and assign it to a main queue
		if(isEmpty(queueBlock) == 0) {
			if(procRunning == 0) {
				addTime(&(idleTime), 5000000);
				addTime(&(data->sysTime), 5000000);	// Run until the next process is ready
			}		

			// Loop through blocked processes -- return them to main queques if unblocked, else keep blocked
			for(int t = 0; t < getSize(queueBlock); t++) {
				int blockedProcID = findLocPID(dequeue(queueBlock));
				if((msgsize = msgrcv(toParentQueue, &msgbuf, sizeof(msgbuf), data->proc[blockedProcID].pid, IPC_NOWAIT)) > -1 && strcmp(msgbuf.mtext, "USED_IO_DONE") == 0) {
					if (data->proc[blockedProcID].realtime == 1) {
						enqueue(active[0], data->proc[blockedProcID].loc_pid);
						data->proc[blockedProcID].queueID = 0;
						fprintf(filep, "OSS: Unblocked process with PID %i [LOCAL PID: %i] and sent to queue: 0 at time %i:%i\n", 
							data->proc[blockedProcID].pid, data->proc[blockedProcID].loc_pid, data->sysTime.seconds, data->sysTime.ns);
					} else {
						enqueue(active[1], data->proc[blockedProcID].loc_pid);
						data->proc[blockedProcID].queueID = 1;
						fprintf(filep, "OSS: Unblocked process with PID %i [LOCAL PID: %i] and sent to queue: 1 at time %i:%i\n", 
							data->proc[blockedProcID].pid, data->proc[blockedProcID].loc_pid, data->sysTime.seconds, data->sysTime.ns);
					}

					int schedCost = ((rand() % 9900) + 100);
					addTime(&(data->sysTime), schedCost);

					fprintf(filep, "\tOSS: Scheduler cost to move process with PID %i [LOCAL PID: %i] was %i ns at time %i:%i\n\n", 
						data->proc[blockedProcID].pid, data->proc[blockedProcID].loc_pid, schedCost, data->sysTime.seconds, data->sysTime.ns);
				} else {
					enqueue(queueBlock, data->proc[blockedProcID].loc_pid); // Process is not ready to be unblocked
				}
			}
		}

		// If no process is currently active but one is ready, get it running -- check queue with highest priority first (queue 0 first)
		if((isEmpty(active[0]) == 0 || isEmpty(active[1]) == 0 || isEmpty(active[2]) == 0 || isEmpty(active[3]) == 0) && procRunning == 0) {
			if(isEmpty(active[0]) == 0) {
				activeProcIndex = findLocPID(dequeue(active[0])); // Dequeue proccess
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				fprintf(filep, "OSS: Dispatched process with PID %i [LOCAL PID: %i] to queue: 0 at time %i:%i\n",
					data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, data->sysTime.seconds, data->sysTime.ns);
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			} else if (isEmpty(active[1]) == 0) {
				activeProcIndex = findLocPID(dequeue(active[1]));
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				fprintf(filep, "OSS: Dispatched process with PID %i [LOCAL PID: %i] to queue: 1 at time %i:%i\n",
					data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, data->sysTime.seconds, data->sysTime.ns);
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			} else if (isEmpty(active[2]) == 0) {
				activeProcIndex = findLocPID(dequeue(active[2]));
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				fprintf(filep, "OSS: Dispatched process with PID %i [LOCAL PID: %i] to queue: 2 at time %i:%i\n",
					data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, data->sysTime.seconds, data->sysTime.ns);
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			} else if (isEmpty(active[3]) == 0) {
				activeProcIndex = findLocPID(dequeue(active[3]));
				msgbuf.mtype = data->proc[activeProcIndex].pid;
				strcpy(msgbuf.mtext, "");
				fprintf(filep, "OSS: Dispatched process with PID %i [LOCAL PID: %i] to queue: 3 at time %i:%i\n",
					data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, data->sysTime.seconds, data->sysTime.ns);
				msgsnd(toChildQueue, &msgbuf, sizeof(msgbuf), IPC_NOWAIT);
			}

			int schedCost = ((rand() % 99) + 1) * 100;
			addTime(&(data->sysTime), schedCost);
			fprintf(filep, "\tOSS: Cost to schedule process with PID %i [LOCAL PID: %i] was %i ns at time %i:%i\n",
				data->proc[activeProcIndex].pid, data->proc[activeProcIndex].loc_pid, schedCost, data->sysTime.seconds, data->sysTime.ns);
			procRunning = 1; // There is a proccess running
		}

		if ((isEmpty(active[0]) != 0 || isEmpty(active[1]) != 0 || isEmpty(active[2]) != 0 || isEmpty(active[3]) != 0) && procRunning == 0) {
			swapActiveAndExpired();
		}

		if((pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0) {
			if(WIFEXITED(status)) {
				if (WEXITSTATUS(status) == 1) {
					exitCount++;
					activeProcs--;
					int position = findPID(pid);

					// Calculate final time stats for local pid
					data->proc[position].tWaitTime.seconds = data->proc[position].tSysTime.seconds;
					data->proc[position].tWaitTime.ns = data->proc[position].tSysTime.ns;
					subTime(&(data->proc[position].tWaitTime), &(data->proc[position].tCpuTime));
					subTime(&(data->proc[position].tWaitTime), &(data->proc[position].tBlockedTime));

					printf("~~~~~~~ TIME STATS FOR LOCAL PID: %i ~~~~~~~\n\tCPU Time: %i:%i\n\tWait Time: %i:%i\n\tBlocked Time: %i:%i\n\n",
						data->proc[position].loc_pid, data->proc[position].tCpuTime.seconds, data->proc[position].tCpuTime.ns, data->proc[position].tWaitTime.seconds,
						data->proc[position].tWaitTime.ns, data->proc[position].tBlockedTime.seconds, data->proc[position].tBlockedTime.ns);

					addTimeLong(&(totalCpuTime), (((long)data->proc[position].tCpuTime.seconds * (long)1000000000)) + (long)(data->proc[position].tCpuTime.ns));
					addTimeLong(&(totalWaitTime), (((long)data->proc[position].tWaitTime.seconds) * (long)1000000000) + (long)(data->proc[position].tWaitTime.ns));
					addTimeLong(&(totalBlockedTime), ((long)(data->proc[position].tBlockedTime.seconds) * (long)1000000000) + 
							(long)(data->proc[position].tBlockedTime.ns));

					if(position > -1)
						data->proc[position].pid = -1;
				}
			}
		}

		// Only exit loop when we ran out of execs or we reached the max child count
		if(remainingExecs <= 0 && exitCount >= 100) {
			totalTime.seconds = data->sysTime.seconds;
			totalTime.ns = data->sysTime.ns;
			break;
		}

		fflush(stdout);
	}

	// Print final times
	printf("~~~~~~~ TOTAL TIMES ~~~~~~~\n\tTotal Time: %i:%i\n\tCPU Time: %i:%i\n\tWait Time: %i:%i\n\tBlocked Time: %i:%i\n\tSystem Idle Time %i:%i\n\n",
		 totalTime.seconds, totalTime.ns, totalCpuTime.seconds, totalCpuTime.ns, totalWaitTime.seconds,
		 totalWaitTime.ns, totalBlockedTime.seconds, totalBlockedTime.ns, idleTime.seconds, idleTime.ns);

	// Replace total times with average times
	averageTime(&(totalTime), exitCount);
	averageTime(&(totalCpuTime), exitCount);
	averageTime(&(totalWaitTime), exitCount);
	averageTime(&(totalBlockedTime), exitCount);
	averageTime(&(idleTime), exitCount);

	// Print average times
	printf("~~~~~~~ AVERAGE TIMES ~~~~~~~\n\tTotal Time: %i:%i\n\tCPU Time: %i:%i\n\tWait Time: %i:%i\n\tBlocked Time: %i:%i\n\tSystem Idle Time %i:%i\nn",
		totalTime.seconds, totalTime.ns, totalCpuTime.seconds, totalCpuTime.ns, totalWaitTime.seconds,
		totalWaitTime.ns, totalBlockedTime.seconds, totalBlockedTime.ns, idleTime.seconds, idleTime.ns);

	// Cleanup
	shmctl(ipcid, IPC_RMID, NULL);
	msgctl(toChildQueue, IPC_RMID, NULL);
	msgctl(toParentQueue, IPC_RMID, NULL);
}

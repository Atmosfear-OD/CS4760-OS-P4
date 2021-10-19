/*
 * oss.c by Pascal Odijk 10/19/2021
 * P4 CS4760 Prof. Bhatia
 *
 * Descr....
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include "oss.h"

FILE *filep;

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
	
	// Initialize timer with specified time
	srand(time(NULL));
	timer(s);

	allocate();
	initialize();
	scheduler();



	// Close output file, kill remaining child processes, and release message queues/shared memory
	fclose(filep);
	killProcesses():
	releaseQueues();
	releaseMemory();
	printf("oss: Terminate: Message queues and shared memory are released\n");

	return 0;
}

// Catches timer and ctrl+c for termination
void signalHandler(int sig) {
	if(sig == SIGINT) {
		printf("oss: Terminate: ctrl+c encountered\n");
		fflush(stdout);
	} else if(signal == SIGALRM) {
		printf("oss: Terminate: Termination time has elapsed\n");
		fflush(stdout);
	}

	for(int i = 0 ; i < MAX_PROCESSES ; i++) {
		killProcesses(shared->table[i].userPID, SIGINT);
	}
	
	// Clean up and terminate program
	fclose(filep);
	releaseQueues();
	releaseMemory();

	exit(0);
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

// Terminate timer -- default is 3 seconds
void timer(int sec) {
	struct sigaction act;
	act.sa_handler = &signalHandler;
	act.sa_flags = SA_RESTART;

	if(sigaction(SIGALRM, &act, NULL) == -1) {
		perror("oss: Error: sigaction failure");
		exit(1);
	}

	struct itimerval value;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	
	value.it_value.tv_sec = sec;
	value.it_value.tv_usec = 0;

	if(setitimer(ITIMER_REAL, &value, NULL)) {
		perror("oss: Error: setitimer failure");
		exit(1);
	}
}

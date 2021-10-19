/*
 * oss.h by Pascal Odijk 10/19/2021
 * P4 CS4760 Prof. Bhatia
 *
 * This is the header file for oss.c, it contains the function prototypes and macros.
 */

#ifndef OSS_H
#define OSS_H

#define MAX_PROCESSES 18
#define TIME_QUANTUM 10

void signalHandler(int sig);
void helpMessage();
void timer(int sec);

#endif

/*
 * queue.h by Pascal Odijk 10/27/2021
 * P4 CMPSCI 4760 Prof. Bhatia
 *
 * This file contains the queue helper functions to init/add/remove/etc.
 * This code has been modified from the website: https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

// A structure to represent a queue
typedef struct Queue
{
        int front, rear, size;
        unsigned capacity;
        int* array;
}Queue;

// Create and initialize queue
struct Queue* createQueue(unsigned capacity)
{
        struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
        queue->capacity = capacity;
        queue->front = queue->size = 0;
        queue->rear = capacity - 1;  // This is important, see the enqueue
        queue->array = (int*)malloc(queue->capacity * sizeof(int));
        return queue;
}

// Check if queue is full
int isFull(struct Queue* queue) {
        return (queue->size == queue->capacity);
}

// Check if queue is empty
int isEmpty(struct Queue* queue) {
        return (queue->size == 0);
}

// Add an item to the queue
void enqueue(struct Queue* queue, int item) {
        if (isFull(queue)) {
                return;
	}

        queue->rear = (queue->rear + 1) % queue->capacity;
        queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
}

// Remove function from queue
int dequeue(struct Queue* queue) {
        if (isEmpty(queue))
                return INT_MIN;
        int item = queue->array[queue->front];
        queue->front = (queue->front + 1) % queue->capacity;
        queue->size = queue->size - 1;
        return item;
}

// Return front of queue
int front(struct Queue* queue) {
        if (isEmpty(queue))
                return INT_MIN;
        return queue->array[queue->front];
}

// Return rear of queue
int rear(struct Queue* queue) {
        if (isEmpty(queue))
                return INT_MIN;
        return queue->array[queue->rear];
}

// Return current size of queue
int getSize(struct Queue* queue) {
        return queue->size;
}

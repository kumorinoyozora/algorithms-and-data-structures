#include <stdlib.h>
#include <stdio.h>
#include "queue.h"


struct Queue {
    Passenger *data;
    size_t head, tail, capacity;
};

Queue *createQueue(size_t capacity) {
    Queue *q = (Queue *)malloc(sizeof(Queue));
    if (!q) return NULL;
    q->data = (Passenger *)malloc(capacity * sizeof(Passenger));
    if (!q->data) {
        free(q);
        return NULL;
    }
    q->head = q->tail = 0;
    q->capacity = capacity;
    return q;
}

void destroyQueue(Queue *q) {
    if (q) {
        free(q->data);
        free(q);
    }
}

void enqueue(Queue *q, Passenger info) {
    if (isFull(q)) {
        puts("Overflow");
        return;
    }
    q->data[q->tail] = info;
    q->tail = (q->tail + 1) % q->capacity; // зацикливание
}

void dequeue(Queue *q, Passenger *info) {
    if (isEmpty(q)) {
        puts("Underflow");
        return;
    }
    *info = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
}

int isEmpty(Queue *q) {
    return q->head == q->tail; // можно q->size == 0;
}

int isFull(Queue *q) {
    return (q->tail + 1) % q->capacity == q->head;
}

int qSize(Queue *q) {
    if (q->tail >= q->head) {
        return q->tail - q->head;
    } else {
        return q->capacity - (q->head - q->tail);
    }
}

Passenger *front(Queue *q) {
    if (isEmpty(q)) return NULL;
    return &q->data[q->head];
}

void printQueueState(Queue *q) {
    Queue tempQueue = *q;

    while (!isEmpty(&tempQueue)) {
        Passenger p;
        Passenger *frontP = front(&tempQueue);
        printf("%s ", frontP->id);
        dequeue(&tempQueue, &p);
    }
}

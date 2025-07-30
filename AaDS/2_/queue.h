#ifndef QUEUE_h
#define QUEUE_h

#include <stddef.h>


// определение структур

typedef struct Passenger {
    char id[2<<3]; // идентификатор пассажира
    int ta; // время прибытия пассажира
    int ts; // время обслуживания пассажира
} Passenger;

typedef struct Queue Queue;

Queue *createQueue(size_t capacity); // capacity только для вектора 

void destroyQueue(Queue *q);

void enqueue(Queue *q, Passenger info); 

void dequeue(Queue *q, Passenger *info); 

int isEmpty(Queue *q);

int isFull(Queue *q); 

int qSize(Queue *q);

Passenger *front(Queue *q); // возвращает информацию о первом элементе очереди

void printQueueState(Queue *q);

Queue copyQueue(Queue *q);

#endif // QUEUE_h
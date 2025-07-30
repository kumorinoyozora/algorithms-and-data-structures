#include <stdlib.h>
#include <stdio.h>
#include "queue.h"


typedef struct Node {
    Passenger info;
    struct Node *next;
} Node;

typedef struct Queue {
    Node *head;
    Node *tail;
} Queue;

Queue *createQueue(size_t capacity) {
    (void)capacity;
    Queue *q = (Queue *)malloc(sizeof(Queue));
    if (!q) return NULL;
    q->head = q->tail = NULL;
    return q;
}

void destroyQueue(Queue *q) {
    while (!isEmpty(q)) {
        Passenger info;
        dequeue(q, &info); // зачищаем записи о пассажирах при каждом извлечении с free
    }
    free(q);
}

void enqueue(Queue *q, Passenger info) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (!newNode) return;
    newNode->info = info;
    newNode->next = NULL; // т.к. добавляем в конец
    if (isEmpty(q)) {
        q->head = q->tail = newNode;
    } else {
        q->tail->next = newNode; // заносим в текущий узел указатель на новый
        q->tail = newNode; // смещаем указатель на последнего на новый
    }
}

void dequeue(Queue *q, Passenger *info) {
    if(isEmpty(q)) {
        puts("Underflow");
        return;
    }
    Node *temp = q->head;
    *info = temp->info; // копируем данные из temp, а потом освобождаем его
    q->head = temp->next;
    if (q->head == NULL) {
        q->tail = NULL; // Обнуляем tail, если очередь пуста
    }
    free(temp);
}

int isEmpty(Queue *q) {
    return q->head == NULL;
}


int isFull(Queue *q) {
    return 0; // переполнение невозможно в случае списка
}

int qSize(Queue *q) {
    int size = 0;
    Node *current = q->head;
    while (current) {
        size++;
        current = current->next;
    }
    return size;
}

Passenger *front(Queue *q) {
    if (isEmpty(q)) return NULL;
    return &q->head->info;
}

void printQueueState(Queue *q) {
    Queue tempQueue = copyQueue(q); // создаём копию, чтобы не менять оригинальную при выводе

    while (!isEmpty(&tempQueue)) {
        Passenger p;
        dequeue(&tempQueue, &p);
        printf("%s ", p.id);
    }
    destroyQueue(&tempQueue); // уничтожаем копию во избежании утечек памяти
}

Queue copyQueue(Queue *q) {
    Queue newQueue = {NULL, NULL};
    Node *current = q->head;

    while (current) {
        enqueue(&newQueue, current->info);
        current = current->next;
    }
    return newQueue;
}
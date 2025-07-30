#include <stdio.h>
#include <stdlib.h>
#include "queue.h"


#define PASSENGERS_NUM 10
#define QUEUE_CAPACITY 3

int main() 
{   
    // вводим число стоек в аэропорту 
    int desksNum;
    scanf("%d ", &desksNum);
    
    // создаёи массив очередей по числу стоек
    Queue *desks[desksNum];
    for (int i = 0; i < desksNum; i++) {
        desks[i] = createQueue(QUEUE_CAPACITY);
    }
    
    Passenger passengers[PASSENGERS_NUM];
    int totPass = 0;
    Passenger *p = passengers;

    // читаем всех пассажиров
    while (scanf(" %[^/]/%d/%d", p[totPass].id, &p[totPass].ta, &p[totPass].ts) == 3) {
        totPass++;
        char isOver;
        scanf("%c", &isOver);
        if (isOver == '\n') break;
    }

    /*
     * цикл по времени
     */
    int currentTime = 0;
    int maxTime = 100;
    int isAdded = 0, isDeleted = 0;
    while (currentTime <= maxTime) {
        // на текущем шаге времени добавляем всех пассажиров с соотв. ta
        for (int i = 0; i < totPass; i++) {
            if (passengers[i].ta == currentTime) {
                
                // выбираем стойку для пассажира
                int idx1 = rand() % desksNum;
                int idx2;
                do {
                    idx2 = rand() % desksNum;
                } while (idx1 == idx2); 

                if (qSize(desks[idx1]) > qSize(desks[idx2])) {
                    idx1 = idx2; 
                }

                // обработка переполнения
                if (!isFull(desks[idx1])) {
                    enqueue(desks[idx1], passengers[i]);
                    isAdded = 1;
                }
                else 
                    passengers[i].ta++;
            }
        }

        // на текущем шаге времени убираем всех пассажиров с соотв. ts
        for (int i = 0; i < desksNum; i++) {
            if (!isEmpty(desks[i])) {
                Passenger *frontP = front(desks[i]);
                frontP->ts--;
                if (frontP->ts == 0) {
                    Passenger temp;
                    dequeue(desks[i], &temp);
                    isDeleted = 1;
                } 
            }
        }

        // на текущей итерации по времени выводим статус стоек при условии его изменения (isAdded или isDeleted)
        if (isAdded || isDeleted) {       
            printf("Time %d\n", currentTime);
            puts("-----------------");
            for (int i = 0; i < desksNum; i++) {
                printf("#%d ", i + 1);
                printQueueState(desks[i]);
                putchar('\n');
            }
            putchar('\n');
        } 
        currentTime++;
        isAdded = 0;
        isDeleted = 0;
    }

    // очистка памяти
    for (int i = 0; i < desksNum; i++) {
        destroyQueue(desks[i]);
    }
    
    return 0;
}

/*
 * gcc -o program main.c queue_vector.c - компиляция и создание program.exe
 * ./program - запуск
 */
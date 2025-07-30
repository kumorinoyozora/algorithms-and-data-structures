#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/*
 * Итак, структура файлов следующая: 
 *
 *      table.bin - хранит метаинформацию о таблице (ks1, ks2, csize1/2, msize1/2)
 * также возможно хранение в нём имён других файлов данных, что было бы логично
 * (в рамках настоящего исполнения размеры ks1 и ks2 буду зафиксированы)
 * 
 *      data.bin - хранит объединённую информационную структуру, которая формируется при вводе
 * включает поля Item + Info, а также массивы строк key2 и note
 * 
 *     	ks1_nodes.bin - хранит списки Node'ов, смещения на них лежат в ks1
 * 
 *      ks2.bin - хранит списки KeySpace2, смещения на которые лежат в ks2 
 * 
 *      free_list.bin - хранит список с записями о свободных блоках других списочных файлов
*/

#define MAX_INPUT_LEN 128
#define MAX_NOTE_LEN 128
#define MAX_KEY2_LEN 8
#define KS1_SIZE 512
#define KS2_SIZE 512
#define INVALID_KEY1 ((size_t)-1)
#define INVALID_IDX ((size_t)-1)
#define NULL_OFFSET -1L

enum FILE_TYPE {
    TABLE_FILE,
    DATA_FILE, // сериализованный Item (Item + key2 + info + note)
    KS1_NODES_FILE, // списки - Node1, на которые ссылаются элементы KeySpace1 по offset'ам
    KS2_FILE, // списки - KeySpace2, на которые ссылается массив offset'ов из ks2 
    FREELIST_FILE
};

typedef struct FreeBlock {
    long offset; // смещение (можно использовать -1 как знак отсутствия записи в KeySpace1, к примеру)
    size_t size; // размер блока данных
    struct FreeBlock *next;
    size_t listType; // тип из enum
} FreeBlock;

/* Основные структуры */

typedef struct Item {
    float num1, num2;
    char note[MAX_NOTE_LEN];

    size_t release;
    size_t key1; 
    char key2[MAX_KEY2_LEN]; 
} Item;

typedef struct Node1 {
    size_t release;
    long itemOffset; // "адрес" item'а
    long nextNodeOffset;
} Node1;

typedef struct KeySpace1 {
    size_t key;
    long nodeOffset;
} KeySpace1;

typedef struct KeySpace2 {
    char key[MAX_KEY2_LEN];
    long itemOffset;
    long nextKeySpace2Offset;
} KeySpace2;


/*
 * ks1 есть массив KeySpace1 с указателями на начала списков из Node1
 * ks2 есть массив со смещениями начал списков из KeySpace2
*/
typedef struct Table {
    KeySpace1 ks1[KS1_SIZE];
    long ks2[KS2_SIZE];
    size_t msize1;
    size_t msize2;
    size_t csize1;
    size_t csize2;
} Table;

const char *getFilePath(enum FILE_TYPE type);
FILE *openFile(enum FILE_TYPE type, const char *mode);
int readFromFile(enum FILE_TYPE type, long, void *, size_t);
int writeToFile(enum FILE_TYPE type, const void *, size_t, FreeBlock **, long *);
int deleteFromFile(enum FILE_TYPE type, long offset, size_t size, FreeBlock **freeList);

FreeBlock *loadFreeList(const char *);
int saveFreeList(const char *, FreeBlock *);
void freeFreeList(FreeBlock *);
int appendFreeList(FreeBlock **, long, size_t, enum FILE_TYPE listType);
long findFreeBlock(FreeBlock **, size_t, enum FILE_TYPE);

size_t findKeyIdxKs1(Table *, size_t);
int insertKs1(Table *, FreeBlock **, size_t, long);
int deleteKeyKs1(Table *, FreeBlock **, size_t, int);

size_t hashFunction(Table *, const char *);
size_t findKeyIdxKs2(Table *, const char *);
KeySpace2 createNewKeyKs2(const char *);
int insertKs2(Table *, FreeBlock **, const char *, long);
int deleteKeyKs2(Table *, FreeBlock **, const char *);

Item *findItemByKey1(Table *, size_t, size_t, long *);
Item *findItemByKey2(Table *, const char *, long *);
Item **findItem(Table *, size_t, const char *);
void freeItemArray(Item **);
int deleteItem(Table *, FreeBlock **, size_t, const char *);
int insertItem(Table *, FreeBlock **, size_t, const char *, long);

void createDataFiles();
int saveTable(Table *);
Table *createTable();
Table *initTable(const char *);

void printTable(const Table*);
Item createItem(size_t, const char *, float, float, const char *, FreeBlock **, long *);

void logError(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
}

/* Функции для работы с файлами */

const char *getFilePath(enum FILE_TYPE type) {
    switch (type) {
        case TABLE_FILE: return "table.bin";
        case DATA_FILE: return "data.bin";
        case KS1_NODES_FILE: return "ks1_nodes.bin";
        case KS2_FILE: return "ks2.bin";
        case FREELIST_FILE: return "free_list.bin";
        default: return NULL;
    }
}

FILE *openFile(enum FILE_TYPE type, const char *mode) {
    const char *path = getFilePath(type);
    if (!path) return NULL;
    return fopen(path, mode);
}

int readFromFile(enum FILE_TYPE type, long offset, void *dst, size_t size) {
    FILE *file = openFile(type, "rb");
    if (!file) return -1;

    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    size_t readCount = fread(dst, 1, size, file);
    fclose(file);

    return (readCount == size) ? 0 : -1;
}

int writeToFile(enum FILE_TYPE type, const void *src, size_t size, 
                            FreeBlock **freeList, long *outOffset) {

    long offset = -1L;                      
    if (freeList)
        offset = findFreeBlock(freeList, size, type);

    FILE *file = NULL;

    if (offset == -1L) {
        // нет свободного блока, дописываем в конец
        file = openFile(type, "ab+"); // открываем для добавления
        if (!file) return -1;

        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return -1;
        }
        offset = ftell(file); // получаем текущую позицию (конец файла)
    } else {
        // записываем в свободный блок
        file = openFile(type, "rb+"); // перезапись
        if (!file) return -1;

        if (fseek(file, offset, SEEK_SET) != 0) {
            fclose(file);
            return -1;
        }  
    }

    if (!file) return -1;

    size_t writeCount = fwrite(src, 1, size, file);
    fclose(file);

    if (writeCount != size) return -1;

    if (outOffset) *outOffset = offset; // возвращаем offset записи
    return 0;
}

int deleteFromFile(enum FILE_TYPE type, long offset, size_t size, FreeBlock **freeList) {
    // просто добавляем блок в список свободных
    return appendFreeList(freeList, offset, size, type);
}

/* Менеджер пустого пространства */

FreeBlock *loadFreeList(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        // тут можно добавить вызов создания списка
        return NULL;
    }

    // смотрим, пустой ли файл
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    if (!size) {
        fclose(file);
        return NULL; // пустой список — нормальная ситуация
    }

    FreeBlock *head = NULL;
    FreeBlock *tail = NULL;

    while (1) {
        long offset;
        size_t size;
        size_t listType;
        if (fread(&offset, sizeof(long), 1, file) != 1) break;
        if (fread(&size, sizeof(size_t), 1, file) != 1) break;
        if (fread(&listType, sizeof(size_t), 1, file) != 1) break;

        FreeBlock *block = (FreeBlock *)malloc(sizeof(FreeBlock));
        if (!block) {
            fclose(file);
            return NULL;
        }
        block->offset = offset;
        block->size = size;
        block->next = NULL;
        block->listType = listType;

        if (!head) { // первый элемент
            head = tail = block;
        } else { // все последующие
            tail->next = block; // в текущий->next записываем последний
            tail = block; // смещаем хвост на последний 
        }
    }
    fclose(file);
    return head;
}

int saveFreeList(const char *filename, FreeBlock *head) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Cannot open freelist file for writing");
        return -1;
    }

    FreeBlock *current = head;
    while (current) {
        fwrite(&current->offset, sizeof(long), 1, file);
        fwrite(&current->size, sizeof(size_t), 1, file);
        fwrite(&current->listType, sizeof(int), 1, file);
        current = current->next;
    }

    fclose(file);
    return 0;
}

void freeFreeList(FreeBlock *list) {
    while (list) {
        FreeBlock *tmp = list;
        list = list->next;
        free(tmp);
    }
}

// через двойной указатель добавляем новый свободный блок в начало списка
int appendFreeList(FreeBlock **list, long offset, size_t size, enum FILE_TYPE listType) {
    FreeBlock *newBlock = (FreeBlock *)malloc(sizeof(FreeBlock));
    if (!newBlock) return -1;
    
    newBlock->offset = offset;
    newBlock->size = size;
    newBlock->next = *list;
    newBlock->listType = listType; // указываем, в каком именно файле
    *list = newBlock;
    return 0;
}

long findFreeBlock(FreeBlock **freeList, size_t requiredSize, enum FILE_TYPE requiredListType) {
    FreeBlock *prev = NULL;
    FreeBlock *curr = *freeList;

    while (curr) {
        if (curr->size >= requiredSize && curr->listType == requiredListType) {
            long offset = curr->offset;
            if (prev) {
                prev->next = curr->next;
            } else {
                *freeList = curr->next;
            }
            free(curr);
            return offset;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

/* Функции для работы с основными структурами */

// бинарный поиск в просматриваемой упорядоченной таблице
size_t findKeyIdxKs1(Table *table, size_t key1) {
    if (!table || !table->ks1 || table->csize1 == 0) {
        return INVALID_IDX;
    }

    KeySpace1 *keySpace = table->ks1; 
    size_t start = 0, end = table->csize1 - 1;

    while (start <= end) {
        size_t middle = start + (end - start) / 2; // аналогично (start + end) / 2
        size_t currKey = keySpace[middle].key;
        if (currKey == key1) {
            return middle; // *(k + i) -> (k + i) возвращаем указатель (если возвращаем указатель)
        } else if (currKey > key1) {
            if (middle == 0) break; // иначе underflow в size_t
            end = middle - 1;
        } else {
            start = middle + 1;
        }
    }
    return INVALID_IDX; // если ключ не найден
}

// здесь item уже должен быть добавлен в data.bin (передаётся offset на него)
int insertKs1(Table *table, FreeBlock **freeList, size_t key1, long itemOffset) {

    // создаём новую запись
    Node1 newNode = {
        .release = 0, // по дефолту 0
        .itemOffset = itemOffset, 
        .nextNodeOffset = NULL_OFFSET
    };

    // ищем, есть ли элемент с таким ключом
    size_t foundIdx = findKeyIdxKs1(table, key1);
    if (foundIdx != INVALID_IDX) { // если элемент с таким ключом уже есть
        KeySpace1 *found = &table->ks1[foundIdx];
        long firstNode = found->nodeOffset;
        long lastOffset = firstNode;
        Node1 curr;
        readFromFile(KS1_NODES_FILE, firstNode, &curr, sizeof(Node1));
        while (curr.nextNodeOffset != NULL_OFFSET) {
            // доходим до конца
            lastOffset = curr.nextNodeOffset;
            readFromFile(KS1_NODES_FILE, curr.nextNodeOffset, &curr, sizeof(Node1)); 
        }
        newNode.release = curr.release + 1;
        // записываем новый элемент
        long newNodeOffset;
        writeToFile(KS1_NODES_FILE, &newNode, sizeof(Node1), freeList, &newNodeOffset);

        // обновляем предпоследний элемент
        curr.nextNodeOffset = newNodeOffset; // добавляем в конец offset на новый элемент
        deleteFromFile(KS1_NODES_FILE, lastOffset, sizeof(Node1), freeList);
        writeToFile(KS1_NODES_FILE, &curr, sizeof(Node1), freeList, &newNodeOffset);
    } else { // если элемента с таким ключом нет
        size_t insertIdx = table->csize1; // по умолчанию - в конец 
        for (size_t i = 0; i < table->csize1; i++) {
            if (table->ks1[i].key > key1) { // находим первый наибольший 
                insertIdx = i;
                break;
            }
        }
        // смещаем вперёд для освобождения индекса для вставки
        for (size_t i = table->csize1; i > insertIdx; i--) {
            table->ks1[i] = table->ks1[i - 1];
        }
        table->csize1++;
        KeySpace1 *newEntry = &table->ks1[insertIdx];
        newEntry->key = key1; // приписываем новый ключ после вставки

        long newNodeOffset;
        writeToFile(KS1_NODES_FILE, &newNode, sizeof(Node1), freeList, &newNodeOffset);
        newEntry->nodeOffset = newNodeOffset;
    }

    Item item;
    long updatedItemOffset;
    readFromFile(DATA_FILE, itemOffset, &item, sizeof(Item));
    item.release = newNode.release;

    deleteFromFile(DATA_FILE, itemOffset, sizeof(Item), freeList);
    writeToFile(DATA_FILE, &item, sizeof(Item), freeList, &updatedItemOffset);

    if (updatedItemOffset != itemOffset) {
        fprintf(stderr, "Warning: reused itemOffset unexpectedly changed!\n");
    } // если вылазит эта штука, нужно добавить каскадное обновление

    return 0;
}

/* здесь логика удаления ключа и соответствующего поля
 * логика удаления item будет в главной функции delete для table
 */
int deleteKeyKs1(Table *table, FreeBlock **freeList, size_t key1, int allReleases) {
    size_t idxToDelete = findKeyIdxKs1(table, key1);
    if (idxToDelete == INVALID_IDX) {
        logError("delete error, key not found (ks1)");
        return -1;
    }
    KeySpace1 *keyToDelete = &table->ks1[idxToDelete];
    long currOffset = keyToDelete->nodeOffset;
    Node1 nodeToDelete;

    if (allReleases) {
        while (currOffset != NULL_OFFSET) { // удаляем все версии
            readFromFile(KS1_NODES_FILE, currOffset, &nodeToDelete, sizeof(Node1));
            deleteFromFile(KS1_NODES_FILE, currOffset, sizeof(Node1), freeList);
            currOffset = nodeToDelete.nextNodeOffset;
        }
        for (size_t i = idxToDelete; i < table->csize1 - 1; i++) {
            table->ks1[i] = table->ks1[i + 1];
        }
        table->csize1--;
        printf("Key %u successfully deleted from ks1\n", key1);
    } else { // удаляем только последнюю версию (rollback)
        readFromFile(KS1_NODES_FILE, currOffset, &nodeToDelete, sizeof(Node1));
        if (!nodeToDelete.nextNodeOffset) { // один релиз = полное удаление
            return deleteKeyKs1(table, freeList, key1, 1);
        } 
        // иначе находим и удаляем последний Node1
        long prevOffset = currOffset;
        while (nodeToDelete.nextNodeOffset != NULL_OFFSET) {
            prevOffset = currOffset;
            currOffset = nodeToDelete.nextNodeOffset;
            readFromFile(KS1_NODES_FILE, currOffset, &nodeToDelete, sizeof(Node1));
        }
        deleteFromFile(KS1_NODES_FILE, currOffset, sizeof(Node1), freeList);

        // обновляем next
        Node1 lastNode;

        readFromFile(KS1_NODES_FILE, prevOffset, &lastNode, sizeof(Node1));
        lastNode.nextNodeOffset = NULL_OFFSET;
        deleteFromFile(KS1_NODES_FILE, prevOffset, sizeof(Node1), freeList);
        writeToFile(KS1_NODES_FILE, &lastNode, sizeof(Node1), freeList, NULL);

        printf("Last release of key %u deleted from ks1\n", key1);
    }

    return 0;
}

size_t hashFunction(Table *table, const char *key2) {
    size_t hash = 0;
    while (*key2) {
        hash ^= (unsigned char)(*key2++);
    }
    return hash % table->msize2; // при resize нужно делать rehash
}

// сложность O(1); фактически дублирует хеш-функцию
size_t findKeyIdxKs2(Table *table, const char *key2) {
    size_t idx = hashFunction(table, key2); // находим по хэшу индекс
    return idx; // указатель по индексу может быть на NULL_OFFSET
}

// подфункция функции insertKs2
KeySpace2 createNewKeyKs2(const char *key2) {
    KeySpace2 newKey = {
        .itemOffset = NULL_OFFSET,
        .nextKeySpace2Offset = NULL_OFFSET
    };
    strncpy(newKey.key, (char *)key2, MAX_KEY2_LEN);
    return newKey;
}

/* тут следим за оригинальностью key2 
* решение в случае повтора: замена
* ключи разные, один хэш? -> запись в конец цепочки через next 
*/
int insertKs2(Table *table, FreeBlock **freeList, const char *key2, long itemOffset) {
    
    size_t idx = findKeyIdxKs2(table, key2);
    long currentOffset = table->ks2[idx]; // смещение по индексу
    
    if (currentOffset == NULL_OFFSET) { // указатель на NULL (нет цепочки)
        KeySpace2 newKey = createNewKeyKs2(key2);
        newKey.itemOffset = itemOffset;
        long newKeyOffset;
        writeToFile(KS2_FILE, &newKey, sizeof(KeySpace2), freeList, &newKeyOffset);
        table->ks2[idx] = newKeyOffset; // ставим смещение на первый элемент цепочки
        table->csize2++; // новое ненулевое смещение в массиве
        return 0;
    } else { // указатель не на NULL (цепочка есть)
        long prevElem = currentOffset;
        KeySpace2 current;
        while (currentOffset != NULL_OFFSET) { // идём до последнего элемента
            readFromFile(KS2_FILE, currentOffset, &current, sizeof(KeySpace2));
            if (!strcmp(current.key, key2)) { // если ключ совпал, заменяем информацию
                current.itemOffset = itemOffset;
                deleteFromFile(KS2_FILE, currentOffset, sizeof(KeySpace2), freeList);
                writeToFile(KS2_FILE, &current, sizeof(KeySpace2), freeList, NULL);
                return 0;
            }
            prevElem = currentOffset;
            currentOffset = current.nextKeySpace2Offset;
        } // не нашли такой ключ в цепочке 
        // currentOffset = NULL_OFFSET, prevElem - offset последнего (указывает на current)
        KeySpace2 newKey = createNewKeyKs2(key2);
        newKey.itemOffset = itemOffset;
        long newKeyOffset;
        writeToFile(KS2_FILE, &newKey, sizeof(KeySpace2), freeList, &newKeyOffset);
        
        // добавляем в бывший последний элемент смещение на новый
        current.nextKeySpace2Offset = newKeyOffset;
        deleteFromFile(KS2_FILE, prevElem, sizeof(KeySpace2), freeList);
        writeToFile(KS2_FILE, &current, sizeof(KeySpace2), freeList, NULL);
        return 0;
    }
}

/* здесь логика удаления указателя на ключ и соответствующего поля
* логика удаления item и самого ключа будет в главной функции delete для table
*/
int deleteKeyKs2(Table *table, FreeBlock **freeList, const char *key2) {
    
    size_t idx = findKeyIdxKs2(table, key2);
    long currElem = table->ks2[idx];
    if (currElem == NULL_OFFSET) {
        logError("no keyspace at this index");
        return -1;
    }
    long prevElem = currElem; // для удаления из середины или конца должны знать предыдущий
    KeySpace2 toDelete;
    
    while (currElem != NULL_OFFSET) { // если цепочка существует
        readFromFile(KS2_FILE, currElem, &toDelete, sizeof(KeySpace2));
        if (!strcmp(toDelete.key, key2)) { // ищем по уникальному ключу
            if (currElem == table->ks2[idx]) { // если элемент первый
                table->ks2[idx] = toDelete.nextKeySpace2Offset; // перемещаем смещение на список на следующий элемент
                deleteFromFile(KS2_FILE, currElem, sizeof(KeySpace2), freeList); // удаляем
                printf("Key %s deleted successfully\n", key2);
                if (table->ks2[idx] == NULL_OFFSET) { // если удалили единственный элемент цепочки
                    table->csize2--;
                }
                return 0;
            } else { // если элемент в середине или конце
                // обновляем смещение на следующий в предыдущем
                KeySpace2 beforeToDelete;
                readFromFile(KS2_FILE, prevElem, &beforeToDelete, sizeof(KeySpace2));
                beforeToDelete.nextKeySpace2Offset = toDelete.nextKeySpace2Offset; // перешагиваем удаляемый элемент
                deleteFromFile(KS2_FILE, prevElem, sizeof(KeySpace2), freeList);
                writeToFile(KS2_FILE, &beforeToDelete, sizeof(KeySpace2), freeList, NULL);
                
                // только после обновления на том же смещении предыдущего удаляем нужный
                deleteFromFile(KS2_FILE, currElem, sizeof(KeySpace2), freeList);
                printf("Key %s deleted successfully\n", key2);
                return 0;
            }
        }
        if (toDelete.nextKeySpace2Offset == NULL_OFFSET) break; // чтобы посмотреть последний но не уйти дальше
        prevElem = currElem;
        currElem = toDelete.nextKeySpace2Offset;
    } 
    // если не вошли в цикл или вышли из него без удаления элемента
    logError("the key is not found or does not exist");
    return -1;
}


/* поиск в таблице элемента по любому заданному ключу;
* результатом поиска должна быть копии всех найденных элементов со значениями ключей;
*
* 
* в функциях производится копирование элементов в внутреннюю память
* в логике верхнего уровня нужно обеспечить очистку памяти после завершения вывода
*/
Item *findItemByKey1(Table *table, size_t key1, size_t release, long *outOffset) {
    if (!table || !table->ks1 || table->csize1 == 0) {
        return NULL;
    }
    
    KeySpace1 *keySpace = table->ks1;
    size_t start = 0, end = table->csize1 - 1;
    while (start <= end) {
        size_t middle = start + (end - start) / 2;
        KeySpace1 currElem = keySpace[middle];
        if (currElem.key == key1) { // если нашли ключ
            long currNodeOffset = currElem.nodeOffset;
            Node1 currNode;
            while (currNodeOffset != NULL_OFFSET) { // бежим по списку пока не найдём нужную версию
                readFromFile(KS1_NODES_FILE, currNodeOffset, &currNode, sizeof(Node1));
                if (currNode.release == release) {
                    Item *item = (Item *)malloc(sizeof(Item));
                    if (!item) {
                        logError("memory allocation failed");
                        return NULL;
                    }
                    readFromFile(DATA_FILE, currNode.itemOffset, item, sizeof(Item));
                    *outOffset = currNode.itemOffset;
                    return item;
                }
                currNodeOffset = currNode.nextNodeOffset;
            }
            // если не нашли версию
            logError("release not found for the given key1");
            return NULL;
        } else if (currElem.key > key1) {
            if (middle == 0) break;
            end = middle - 1;
        } else {
            start = middle + 1;
        }
    }
    logError("key1 not found in ks1");
    return NULL; // если ключ не найден
}

Item *findItemByKey2(Table *table, const char *key2, long *outOffset) {
    size_t idx = findKeyIdxKs2(table, key2);
    long currentOffset = table->ks2[idx];
    KeySpace2 current;
    while (currentOffset != NULL_OFFSET) { 
        readFromFile(KS2_FILE, currentOffset, &current, sizeof(KeySpace2));
        if (!strcmp(current.key, key2)) {
            Item *item = (Item *)malloc(sizeof(Item));
            if (!item) {
                logError("memory allocation failed");
                return NULL;
            }
            readFromFile(DATA_FILE, current.itemOffset, item, sizeof(Item));
            if (outOffset) *outOffset = current.itemOffset;
            return item;
        }
        currentOffset = current.nextKeySpace2Offset;
    }
    // цепочки по такому ключу нет
    logError("key2 not found in ks2");
    return NULL;
}

/* поиск в таблице элемента, заданного составным ключом 
* 
* поскольку всякий составной ключ оригинален за счёт оригинальности key2,
* поиск по составному ключу сводится к поиску по второму ключу одного элемента, 
* однако поиск по первому ключу должен обеспечивать вывод нескольких элементов 
* 
*/
Item **findItem(Table *table, size_t key1, const char *key2) {
    if (key2) { // если второй ключ, всегда ищем по нему (см. выше)
        Item *item = findItemByKey2(table, key2, NULL);
        if (!item) return NULL;
        if (key1 != INVALID_KEY1 && item->key1 != key1) { // по сути, эта проверка не нужна
            logError("key1 does not match key2, try key1 = -1");
        } 
        Item **result = (Item **)malloc(2 * sizeof(Item *));
        if (!result) {
            logError("memory allocation failed");
            return NULL;
        }
        result[0] = item;
        result[1] = NULL; // маркер конца массива
        return result;
    } else if (key1 != INVALID_KEY1) { // поиск только по первому
        // можно добавить поиск отдельного релиза
        size_t idx = findKeyIdxKs1(table, key1);
        if (idx == INVALID_IDX) return NULL;
        long nodeOffset = table->ks1[idx].nodeOffset;
        Node1 node;
        size_t releaseNum = 0; // считаем число релизов
        while (nodeOffset != NULL_OFFSET) {
            readFromFile(KS1_NODES_FILE, nodeOffset, &node, sizeof(Node1));
            releaseNum++;
            nodeOffset = node.nextNodeOffset;
        }
        if (releaseNum == 0) {
            logError("releases not found");
            return NULL;
        }
        Item **result = (Item **)malloc((releaseNum + 1) * sizeof(Item *));
        if (!result) {
            logError("memory allocation failed");
            return NULL;
        }
        nodeOffset = table->ks1[idx].nodeOffset;
        for (size_t i = 0; i < releaseNum; i++) {
            readFromFile(KS1_NODES_FILE, nodeOffset, &node, sizeof(Node1));
            Item *item = (Item *)malloc(sizeof(Item));
            if (!item) {
                logError("memory allocation failed");
                return NULL;
            }
            readFromFile(DATA_FILE, node.itemOffset, item, sizeof(Item));
            result[i] = item;
            nodeOffset = node.nextNodeOffset;
        }
        result[releaseNum] = NULL; // маркер конца массива
        return result;
    } else {
        logError("no keys provided for deletion");
        return NULL;
    } 
}

void freeItemArray(Item **array) {
    for (size_t i = 0; array[i]; i++) {
        free(array[i]);
    }
    free(array);
}

/* удаление из таблицы элемента, заданного составным ключом;
 * 
 * также удаляются ВСЕ связанные структуры (note, info, item, Node1)}->Item
*/
int deleteItem(Table *table, FreeBlock **freeList, size_t key1, const char *key2) {
    if (key2) { // наличие key2 -> удаление по key2 или (key1, key2)
        long itemToDeleteOffset;
        Item *itemToDelete = findItemByKey2(table, key2, &itemToDeleteOffset);
        if (!itemToDelete) {
            logError("item with this key2 was not found");
            return -1;
        }
        size_t idx1 = (key1 != INVALID_KEY1) 
            ? findKeyIdxKs1(table, key1)
            : findKeyIdxKs1(table, itemToDelete->key1);
        if (idx1 == INVALID_IDX) {
            logError("item with this key1 was not found");
            return -1;
        }
    
        // ищем запись по ключу с таким item'ом, чтобы удалить нужный релиз
        long nodeToDeleteOffset = table->ks1[idx1].nodeOffset;
        long prevNodeOffset = nodeToDeleteOffset;
        Node1 nodeToDelete;
        while (nodeToDeleteOffset != NULL_OFFSET) {
            readFromFile(KS1_NODES_FILE, nodeToDeleteOffset, &nodeToDelete, sizeof(Node1));
            if (nodeToDelete.itemOffset == itemToDeleteOffset) { // нашли node с смещением на нужный item
                deleteFromFile(DATA_FILE, itemToDeleteOffset, sizeof(Item), freeList);
                int delKey2Res = deleteKeyKs2(table, freeList, key2); // удаляем key2 на удалённый item
                if (delKey2Res == -1) {
                    logError("can`t delete key2 from table, but item was deleted");
                }
                if (nodeToDeleteOffset == prevNodeOffset) { // если первый Node
                    KeySpace1 *currElem = &table->ks1[idx1];
                    currElem->nodeOffset = nodeToDelete.nextNodeOffset;
                    if (currElem->nodeOffset == NULL_OFFSET) { // если первый и единственный 
                        deleteKeyKs1(table, freeList, key1, 1);
                    } 
                } else { // если в середине или конце
                    Node1 prevNode;
                    long prevNodeNewOffset;
                    readFromFile(KS1_NODES_FILE, prevNodeOffset, &prevNode, sizeof(Node1));
                    prevNode.nextNodeOffset = nodeToDelete.nextNodeOffset;
                    deleteFromFile(KS1_NODES_FILE, prevNodeOffset, sizeof(Node1), freeList);
                    writeToFile(KS1_NODES_FILE, &prevNode, sizeof(Node1), freeList, &prevNodeNewOffset);
                    if (prevNodeOffset != prevNodeNewOffset) {
                        printf("смещения не совпали, срочно менять логику\n");
                    }
                }
                deleteFromFile(KS1_NODES_FILE, nodeToDeleteOffset, sizeof(Node1), freeList); // удаляем нужный release с key1
                free(itemToDelete); // удаляем копию элемента из внутренней памяти
                return 0;
            }
            prevNodeOffset = nodeToDeleteOffset;
            nodeToDeleteOffset = nodeToDelete.nextNodeOffset;
        }
        logError("node doesn`t exist or item with corresponding release wasn`t found");
        return -1;
    } else if (key1 != INVALID_KEY1) { // удаление всех элементов, в составных ключах которых есть key1
        size_t idxToDelete = findKeyIdxKs1(table, key1);
        if (idxToDelete == INVALID_IDX) {
            printf("Delete error, key '%u' not found\n", key1);
            return -1;
        }
        KeySpace1 *keyToDelete = &table->ks1[idxToDelete];
        long nodeToDeleteOffset = keyToDelete->nodeOffset;
        Node1 nodeToDelete;
        long itemToDeleteOffset;
        Item itemToDelete;

        while (nodeToDeleteOffset != NULL_OFFSET) { // удаляем все версии
            readFromFile(KS1_NODES_FILE, nodeToDeleteOffset, &nodeToDelete, sizeof(Node1));
            itemToDeleteOffset = nodeToDelete.itemOffset;
            readFromFile(DATA_FILE, itemToDeleteOffset, &itemToDelete, sizeof(Item));
            int delRes = deleteKeyKs2(table, freeList, itemToDelete.key2);
            if (delRes == 0) {
                deleteFromFile(DATA_FILE, itemToDeleteOffset, sizeof(Item), freeList);
                deleteFromFile(KS1_NODES_FILE, nodeToDeleteOffset, sizeof(Node1), freeList);
            } else {
                logError("delete item procedure stoped, can`t delete key2 from table");
                return -1;
            }
            nodeToDeleteOffset = nodeToDelete.nextNodeOffset;
        }
        keyToDelete->nodeOffset = NULL_OFFSET; // явно зануляем

        for (size_t i = idxToDelete; i < table->csize1 - 1; i++) {
            table->ks1[i] = table->ks1[i + 1];
        }
        table->csize1--;
        printf("All items with key '%u' successfully deleted\n", key1);
        return 0;
    } else {
        logError("there is no keys");
        return -1;
    }
}

/* включение нового элемента в таблицу с соблюдением ограничений на уникальность ключей 
 * в соответствующих ключевых пространствах и уникальности составного ключа (key1, key2);
 *
 * уникальность составного ключа сводится к уникальность key2
 * 
 * тут проблема: insert (1, a, item1), insert (1, b, item2) - ОК
 * но insert (1, a, item3) -> новая запись в ks1 и ПЕРЕЗАПИСЬ в ks2,
 * результат - из таблицы исчезает второй указатель на item1, 
 * хотя в самом поле структуры item он остаётся
 * 
 * как обеспечить оригинальность составного ключа и избежать такой ситуации?
 * ввод key2
 * если key2 есть в ks2:
 *     находим по key2 item;
 *     по его key1 удаляем запись в ks1 такую что Node1.info == item;
 *     удаляем по key2 соответствующую запись в ks2;
 *     удаляем item->info;
 *     удаляем сам item;
*/
int insertItem(Table *table, FreeBlock **freeList, size_t key1, const char *key2, long itemOffset) {
    int insert1Res = insertKs1(table, freeList, key1, itemOffset);
    if (insert1Res != 0) {
        logError("insert error in KeySpace1");
        return insert1Res;
    }

    /* при дублировании второго ключа стираем всю информацию
     * связанную с этим ключом перед вставкой
    */ 
    Item *isKeyNotOriginal = findItemByKey2(table, key2, NULL);
    if (isKeyNotOriginal) {
        int clearRes = deleteItem(table, freeList, isKeyNotOriginal->key1, key2);
        if (clearRes == 0) {
            printf("Warning: all info with same key2 was deleted before insert\n");
        } else { // -1
            logError("can`t free keys for the new item");
            deleteKeyKs1(table, freeList, key1, 0); // rollback
            free(isKeyNotOriginal);
            return clearRes;
        }
        free(isKeyNotOriginal);
    } 

    int insert2Res = insertKs2(table, freeList, key2, itemOffset);
    if (insert2Res != 0) {
        logError("insert error in KeySpace2");
        deleteKeyKs1(table, freeList, key1, 0); // rollback, удаляем последнюю версию
        return insert2Res;
    }
    return 0;
} // обеспечиваем атомарность

/**
 * Пересоздаёт все вспомогательные файлы, связанные с таблицей.
 * Используется при создании новой таблицы, если старые данные неконсистентны.
 * Все файлы очищаются до нуля (fopen(..., "wb")).
 */
void createDataFiles() {
    const char *files[] = {
        "data.bin",
        "ks1_nodes.bin",
        "ks2.bin" ,
        "free_list.bin"
    };
    for (size_t i = 0; i < sizeof(files) /sizeof(files[0]); i++) {
        FILE *file = fopen(files[i], "wb");
        if (!file) {
            printf("Failed to recreate file: %s", files[i]);
            return;
        }
        fclose(file);
    }
}

int saveTable(Table *table) {
    FILE *file = openFile(TABLE_FILE, "wb");
    if (!file) {
        logError("failed to open table.bin for writing");
        return -1;
    }
    size_t written = fwrite(table, sizeof(Table), 1, file);
    fclose(file);

    return (written == sizeof(Table)) ? 0 : -1;
}

Table *createTable() {
    Table *table = (Table *)malloc(sizeof(Table));
    if (!table) {
        logError("failed to allocate memory for table");
        return NULL;
    }

    /* 
    typedef struct Table {
        KeySpace1 ks1[KS1_SIZE];
        long ks2[KS2_SIZE];
        size_t msize1;
        size_t msize2;
        size_t csize1;
        size_t csize2;
    } Table;
     */

    for (size_t i = 0; i < KS1_SIZE; i++) {
        table->ks1[i].key = INVALID_KEY1;
        table->ks1[i].nodeOffset = NULL_OFFSET;
        table->ks2[i] = NULL_OFFSET;
        table->msize1 = KS1_SIZE;
        table->msize2 = KS2_SIZE;
        table->csize1 = 0;
        table->csize2 = 0;
    }
    createDataFiles();
    saveTable(table);
    return table;
}

Table *initTable(const char *filename) {
    //char filename[] = "table.bin";
    FILE *file = fopen(filename, "rb");

    if (!file) {
        // файл не существует, инициализируем новую таблицу
        logError("TABLE_FILE does not exist. Creating new table...");
        return createTable();
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = (size_t)ftell(file); // тонкое место?
    fclose(file);

    if (fileSize < sizeof(Table)) {
        logError("TABLE_FILE is too small or corrupted. Creating new table...");
        return createTable();
    }

    Table *table = (Table *)malloc(sizeof(Table));
    if (!table) {
        logError("failed to allocate memory for table");
        return NULL;
    }
    readFromFile(TABLE_FILE, 0, table, sizeof(Table));
    return table;
}

// вывод по key1 обеспечивает упорядоченность
void printTable(const Table* table) {
    if (!table) {
        printf("--- Error: Table is NULL ---\n");
        return;
    }

    // Статистика
    size_t total_items = 0;
    size_t max_releases = 0;
    for (size_t i = 0; i < table->csize1; i++) {
        if (table->ks1[i].key == INVALID_KEY1 || table->ks1[i].nodeOffset == NULL_OFFSET) 
            continue;

        size_t count = 0;
        long elem = table->ks1[i].nodeOffset;
        Node1 node;

        while (elem != NULL_OFFSET) {
            readFromFile(KS1_NODES_FILE, elem, &node, sizeof(Node1));
            elem = node.nextNodeOffset;
            count++;
        }
        total_items += count;
        if (count > max_releases) max_releases = count;
    }

    // Шапка
    printf("\n+---------------- TABLE CONTENTS (%u items) ------------------------+\n", total_items);
    printf("| Key1      | Key2           | Release | Data                      |\n");
    printf("+-----------+----------------+---------+---------------------------+\n");

    // Данные
    for (size_t i = 0; i < table->csize1; i++) {
        long elem = table->ks1[i].nodeOffset;
        Node1 node;

        while (elem != NULL_OFFSET) {
            readFromFile(KS1_NODES_FILE, elem, &node, sizeof(Node1));

            Item item;
            readFromFile(DATA_FILE, node.itemOffset, &item, sizeof(Item));

            printf("| %-9u | %-14s | %-7u | %5.2f, %5.2f, %-11s |\n",
                item.key1,
                item.key2,
                item.release,
                item.num1,
                item.num2,
                item.note);
            if (node.nextNodeOffset != NULL_OFFSET) {
                printf("+-----------+----------------+---------+---------------------------+\n");
            }
            elem = node.nextNodeOffset;
        }
        if (i < table->csize1 - 1) {
            printf("+-----------+----------------+---------+---------------------------+\n");
        }
    }

    // Подвал
    printf("+-----------+----------------+---------+---------------------------+\n");
    printf("| Stats: KS1: %4u/%-4u (%4.1f%%) | KS2: %4u/%-4u (%4.1f%%)           |\n",
        table->csize1, table->msize1, (float)table->csize1/(float)table->msize1*100,
        table->csize2, table->msize2, (float)table->csize2/(float)table->msize2*100);
    printf("| Max releases per key: %-31u            |\n", max_releases);
    printf("+------------------------------------------------------------------+\n");
}

Item createItem(size_t key1, const char *key2, float num1, float num2, const char *note, FreeBlock **freeList, long *offset) {
    Item newItem = {
        .key1 = key1,
        .num1 = num1,
        .num2 = num2,
        .release = 0
    };
    strcpy(newItem.key2, key2);
    strcpy(newItem.note, note);

    writeToFile(DATA_FILE, &newItem, sizeof(Item), freeList, offset);

    return newItem;
}

// Чтение строки с ограничением длины
int readLine(char *buffer, int max_len, const char *prompt) {
    printf("%s", prompt);
    if (fgets(buffer, max_len, stdin) == NULL)
        return 0;

    size_t len = strlen(buffer);
    if (len && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0'; // Удалим \n
    } else {
        // Очистим остаток буфера, если ввод длиннее, чем max_len
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF);
    }
    return 1;
}

// Валидация unsigned int
int validateUInt(const char *input, void *result) {
    char* endptr;
    unsigned long val = strtoul(input, &endptr, 10);
    if (*input == '\0' || *endptr != '\0') return 0;

    *(size_t*)result = (size_t)val;
    return 1;
}

// Валидация float
int validateFloat(const char *input, void *result) {
    char* endptr;
    float val = strtof(input, &endptr);
    if (*input == '\0' || *endptr != '\0') return 0;

    *(float*)result = val;
    return 1;
}

// Валидация строки 
int validateString(const char *input, void *result) {
    if (!input || input[0] == '\0') return 0;

    for (size_t i = 0; input[i]; ++i) {
        if (!isprint((unsigned char)input[i])) return 0;
    }

    strncpy((char*)result, input, MAX_INPUT_LEN - 1);
    ((char*)result)[MAX_INPUT_LEN - 1] = '\0';
    return 1;
}

typedef int (*Validator)(const char*, void*);

// Универсальный запрос значения с валидацией
int inputWithValidation(const char *prompt, Validator validator, void *result) {
    char buffer[MAX_INPUT_LEN];
    int attempts = 3;

    while (attempts--) {
        if (!readLine(buffer, sizeof(buffer), prompt)) {
            printf("Input error\n");
            continue;
        }

        if (validator(buffer, result)) {
            return 1;
        }

        printf("Invalid format. %d attempts left\n", attempts);
    }

    return 0;
}

// Специализированные обертки
int inputUInt(size_t *value, const char *prompt) {
    return inputWithValidation(prompt, validateUInt, value);
}

int inputFloat(float* value, const char* prompt) {
    return inputWithValidation(prompt, validateFloat, value);
}

int inputString(char* value, size_t max_len, const char* prompt) {
    char buffer[MAX_INPUT_LEN];
    if (!inputWithValidation(prompt, validateString, buffer))
        return 0;

    strncpy(value, buffer, max_len - 1);
    value[max_len - 1] = '\0';
    return 1;
}

void addItemDialog(Table *table, FreeBlock **freeList) {
    printf("\n=== Add New Item ===\n");

    //printf("[DEBUG] table pointer: %p\n", (void*)table);
    
    size_t key1;
    char key2[MAX_KEY2_LEN];
    float num1, num2;
    char note[MAX_NOTE_LEN];
    
    if (!inputUInt(&key1, "Enter key1 (number): ")) {
        printf("Failed to read key1\n");
        return;
    }
    
    if (!inputString(key2, sizeof(key2), "Enter key2 (string): ")) {
        printf("Failed to read key2\n");
        return;
    }
    
    if (!inputFloat(&num1, "Enter num1: ") || 
        !inputFloat(&num2, "Enter num2: ")) {
        printf("Invalid numbers\n");
        return;
    }
    
    if (!inputString(note, sizeof(note), "Enter note: ")) {
        printf("Invalid note\n");
        return;
    }
    
    long itemOffset;
    Item item = createItem(key1, key2, num1, num2, note, freeList, &itemOffset);

    int res = insertItem(table, freeList, item.key1, item.key2, itemOffset);
    printf(res == 0 ? "Item added!\n" : "Failed to add item!\n");
}

void printSearchResults(Item **results) {
    if (!results || !results[0]) {
        printf("No items found\n");
        return;
    }
    
    printf("\nSearch results:\n");
    printf("+-----------+----------------+---------+---------------------------+\n");
    printf("| Key1      | Key2           | Release | Data                      |\n");
    printf("+-----------+----------------+---------+---------------------------+\n");

    for (int i = 0; results[i]; i++) {
        Item *item = results[i];
        printf("| %-9u | %-14s | %-7u | %5.2f, %5.2f, %-11s |\n",
                item->key1, item->key2, item->release,
                item->num1, item->num2, item->note);
        free(item); // удаляем копию после вывода
        printf("+-----------+----------------+---------+---------------------------+\n");
    }
    return;
}

void searchDialog(Table *table) {
    printf("\n=== Search Item ===\n");
    printf("1. Search by key1\n");
    printf("2. Search by key2\n");
    printf("3. Search by (key1, key2)\n");
    printf("0. Exit\n");

    size_t key1;
    char key2[MAX_KEY2_LEN] = {0};

    size_t choice;
    int inpUIntRes;
    int inpStrRes;
    Item **result;
    
    int choiseRes = inputUInt(&choice, "Select: ");
    if (!choiseRes) {
        printf("Input error\n");
    }

    switch (choice) {
        case 1:
            inpUIntRes = inputUInt(&key1, "Enter key1: ");
            if (!inpUIntRes) {
                printf("Invalid input\n");
                return;
            }
            result = findItem(table, key1, NULL);
            printSearchResults(result);
            free(result);
            return; // ломается тут
        case 2: // поиск по второму ключу
            inpStrRes = inputString(key2, sizeof(key2), "Enter key2: ");
            if (!inpStrRes) {
                printf("Invalid input\n");
                return;
            }
            result = findItem(table, INVALID_KEY1, key2);
            printSearchResults(result);
            free(result);
            return;
        case 3: // поиск по составному ключу (фактически второму)
            inpUIntRes = inputUInt(&key1, "Enter key1: ");
            inpStrRes = inputString(key2, sizeof(key2), "Enter key2: ");
            if (!inpUIntRes || !inpStrRes) {
                printf("Invalid input\n");
                return;
            }
            result = findItem(table, key1, key2);
            printSearchResults(result);
            free(result);
            return;
        case 0: 
            return;
        default:
            printf("Invalid choice!\n"); 
            return;
    }
}

void deleteDialog(Table* table, FreeBlock **freeList) {
    printf("\n=== Delete Item ===\n");
    printf("1. Delete by key1\n");
    printf("2. Delete by key2\n");
    printf("3. Delete by (key1, key2)\n");
    printf("0. Exit\n");

    size_t key1;
    char key2[MAX_KEY2_LEN] = {0};

    size_t choice;
    int inpUIntRes;
    int inpStrRes;
    Item **result;
    
    int choiseRes = inputUInt(&choice, "Select: ");
    if (!choiseRes) {
        printf("Input error\n");
    }
    
    switch (choice) {
        case 1:
            inpUIntRes = inputUInt(&key1, "Enter key1: ");
            if (!inpUIntRes) {
                printf("Invalid input\n");
                return;
            }
            result = findItem(table, key1, NULL);
            printf("All information from items below will be deleted:\n");
            printSearchResults(result);
            free(result);
            deleteItem(table, freeList, key1, NULL);
            return;
        case 2:
            inpStrRes = inputString(key2, sizeof(key2), "Enter key2: ");
            if (!inpStrRes) {
                printf("Invalid input\n");
                return;
            }
            result = findItem(table, INVALID_KEY1, key2);
            printf("All information from items below will be deleted:\n");
            printSearchResults(result);
            free(result);
            deleteItem(table, freeList, INVALID_KEY1, key2);
            return;
        case 3:
            inpUIntRes = inputUInt(&key1, "Enter key1: ");
            inpStrRes = inputString(key2, sizeof(key2), "Enter key2: ");
            if (!inpUIntRes && !inpStrRes) {
                printf("Invalid input\n");
                return;
            }
            result = findItem(table, key1, key2);
            printf("All information from items below will be deleted:\n");
            printSearchResults(result);
            free(result);
            deleteItem(table, freeList, key1, key2);
            return;
        case 0: 
            return;
        default:
            printf("Invalid choice!\n"); 
            return;
    }
}

void printMainMenu() {
    printf("\n=== Main Menu ===\n");
    printf("1. Add new item\n");
    printf("2. Search items\n");
    printf("3. Delete item\n");
    printf("4. Show full table\n");
    printf("0. Exit\n");
}

// нужна логика с менеджером памяти
int main() {
    FreeBlock *freeList = loadFreeList(getFilePath(FREELIST_FILE));
    Table *table = initTable(getFilePath(TABLE_FILE));
    if (!freeList) {
        printf("Warning: unable to load free list\n");
    } 

    if (!table) {
        logError("failed to initialize table");
        return 1;
    }

    size_t choice;
    int running = 1;
    
    while (running) {
        printMainMenu();
        
        if (!inputUInt(&choice, "Select: ")) {
            printf("Input error\n");
            continue;
        }

        switch (choice) {
            case 1: addItemDialog(table, &freeList); break;
            case 2: searchDialog(table); break;
            case 3: deleteDialog(table, &freeList); break;
            case 4: printTable(table); break;
            case 0: running = 0; puts("Exiting..."); break;
            default: printf("Invalid choice!\n");
        }
        saveFreeList("free_list.bin", freeList);
    }

    saveFreeList("free_list.bin", freeList);
    freeFreeList(freeList);

    saveTable(table);
    free(table);

    return 0;
}
/**
 * РАБОТАЕТ, НО!
 * 
 * Если free_list не был пустым и по каким-либо причинам слетел,
 * новые записи будут идти в конец, потенциально это может привести
 * к накоплению некоторого объёма мусора в старых данных, 
 * хотя при полных перезаписях таблицы эта проблема сама собой решится.
 * 
 */
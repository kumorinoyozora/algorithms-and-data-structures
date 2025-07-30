#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define INVALID_KEY1 ((size_t)-1)
#define INVALID_IDX ((size_t)-1)
#define MAX_KEY2_LEN 8
#define MAX_NOTE_LEN 128
#define MAX_INPUT_LEN 128

/* Структура информации в записи Node*/
typedef struct Info {
    float num1, num2;
    char *note;
} Info;

/* Первое пространство ключей */

// Структура элемента таблицы
typedef struct Item {
    Info *info;	/* указатель на информацию */
    size_t release;
    size_t key1; /* ключ элемента из 1-го пространства ключей; */
    char *key2; /* ключ элемента из 2-го пространства ключей; */
} Item;

// Элемент списка с одинаковыми значениями ключей
typedef struct Node1 {
    size_t release; /* номер версии */
    Item *info;	/* указатель на информацию */
    struct Node1 *next; /* указатель на следующий элемент */
} Node1;

// Структура элемента таблицы первого пространства ключей
typedef struct KeySpace1 {
    size_t key; /* ключ элемента */
    Node1 *node; /* указатель на информацию */
} KeySpace1;

/* Второе пространство ключей */

typedef struct KeySpace2 {
    char *key; /* ключ элемента */
    Item *info; /* указатель на информацию */
    struct KeySpace2 *next; /* указатель на следующий элемент */
} KeySpace2;

/* Структура таблицы */
typedef struct Table {
    KeySpace1 *ks1;	/* указатель на первое пространство ключей */
    KeySpace2 **ks2; /* указатель на второе пространство ключей */
    size_t msize1; /* размер области 1-го пространства ключей */
    size_t msize2; /* размер области 2-го пространства ключей */
    size_t csize1; /* количество элементов в области 1-го пространства ключей */
    size_t csize2; /* количество элементов в области 1-го пространства ключей */
} Table;

void logError(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
}

/* Функции для работы с ks1 */

KeySpace1 *initKs1(Table *, size_t);
size_t findKeyIdxKs1(Table *, size_t);
KeySpace1 *resizeKs1(Table *, size_t);
KeySpace1 *insertNewKeyKs1(Table *, size_t);
int insertKs1(Table *, size_t, Item *);
int deleteKeyKs1(Table*, size_t, int);

KeySpace1 *initKs1(Table *table, size_t capacity) {
    KeySpace1 *keySpace1 = (KeySpace1 *)calloc(capacity, sizeof(KeySpace1));
    if (!keySpace1) {
        printf("KeySpace 1 init error: malloc error (NULL pointer)");
    } else {
        table->msize1 = capacity; // автоматически добавляем информацию в table при init
        table->csize1 = 0; // после инициализации число элементов 0
    };
    return keySpace1; // ks1 в table, NULL в случае ошибки
}

// бинарный поиск в просматриваемой упорядоченной таблице
size_t findKeyIdxKs1(Table *table, size_t key1) {
    if (!table || !table->ks1 || table->csize1 == 0) {
        return INVALID_KEY1;
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
    return INVALID_KEY1; // если ключ не найден
}

// не удаляет item и info
void freeKs1(Table *table) {
    for (size_t i = 0; i < table->csize1; i++) { // достаточно csize1 т.к. таблица упорядочена
        Node1 *currNode = table->ks1[i].node;
        while (currNode) {
            Node1 *next = currNode->next;
            free(currNode);
            currNode = next;
        }
    }
    free(table->ks1); // чистим весь массив структур KeySpace1
}

KeySpace1 *resizeKs1(Table *table, size_t newCapacity) {
    KeySpace1 *newKeySpace1 = (KeySpace1 *)realloc(table->ks1, newCapacity * sizeof(KeySpace1));
    if (!newKeySpace1) {
        logError("memory reallocate error (resizeKs1)");
    } else {
        table->ks1 = newKeySpace1;
        table->msize1 = newCapacity;
    }
    return newKeySpace1;
}

// подфункция функции insertKs1
KeySpace1 *insertNewKeyKs1(Table *table, size_t key1) {
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
    return newEntry;
}

int insertKs1(Table *table, size_t key1, Item *item) {

    // создаём новую запись
    Node1 *newNode = (Node1 *)malloc(sizeof(Node1));
    if (!newNode) {
        logError("memory allocate error (insertKs1, newNode section)");
        return -1;
    }
    newNode->release = 0; // по дефолту 0
    newNode->info = item;
    newNode->next = NULL;

    // ищем, есть ли элемент с таким ключом
    size_t foundIdx = findKeyIdxKs1(table, key1);
    if (foundIdx != INVALID_IDX) { // если элемент с таким ключом уже есть
        KeySpace1 *found = &table->ks1[foundIdx];
        Node1 *node = found->node;
        if (node) { // если уже есть запись
            while (node->next) 
                node = node->next; // доходим до конца 
            node->next = newNode; // добавляем  в конец указатель на новый элемент
            newNode->release = node->release + 1; // переписываем релиз

        } else { // если есть элемент с ключом, но без записи Node1 (пустой) - допускам такой случай
            found->node = newNode;
        }
    } else { // если элемента с таким ключом нет
        if (table->csize1 + 1 >= table->msize1) { // если ks1 полон 
            KeySpace1 *KeySpace = resizeKs1(table, 2 * table->msize1);
            if (!KeySpace) {
                logError("memory reallocate error (insertKs1, resizeKs1 section)");
                return -1;
            }
        } // если есть пустые ячейки в ks1 
        KeySpace1 *newKsElem = insertNewKeyKs1(table, key1);
        newKsElem->node = newNode;
    }
    newNode->info->release = newNode->release; // добавляем информацию о release в item
    return 0;
}

/* здесь логика удаления ключа и соответствующего поля
 * логика удаления item будет в главной функции delete для table
 */
int deleteKeyKs1(Table *table, size_t key1, int allReleases) {
    size_t idxToDelete = findKeyIdxKs1(table, key1);
    if (idxToDelete == INVALID_IDX) {
        logError("delete error, key not found (ks1)");
        return -1;
    }
    KeySpace1 *keyToDelete = &table->ks1[idxToDelete];
    Node1 *nodeToDelete = keyToDelete->node;
    if (allReleases) {
        while (nodeToDelete) { // удаляем все версии
            Node1 *next = nodeToDelete->next;
            free(nodeToDelete);
            nodeToDelete = next;
        }
        for (size_t i = idxToDelete; i < table->csize1 - 1; i++) {
            table->ks1[i] = table->ks1[i + 1];
        }
        table->csize1--;
        printf("Key %u successfully deleted from ks1\n", key1);
        // можно уменьшать ks1 по необходимости (minsize1 = 16)
        if (table->csize1 < table->msize1 / 2 && table->msize1 >= 32) {
            KeySpace1 *KeySpace = resizeKs1(table, table->msize1 / 2);
            if (!KeySpace) {
                logError("resize error (after key delete)");
            }
        }
    } else { // удаляем только последнюю версию (rollback)
        if (!nodeToDelete->next) { // один релиз = полное удаление
            return deleteKeyKs1(table, key1, 1);
        } 
        // иначе находим и удаляем последний Node1
        Node1 *prev = NULL;
        while (nodeToDelete->next) {
            prev = nodeToDelete;
            nodeToDelete = nodeToDelete->next;
        }
        free(nodeToDelete);
        // проверка if (prev) не нужна, т.к. предыдущие условие гарантирует > 1 записи
        prev->next = NULL;
        printf("Last release of key %u deleted from ks1\n", key1);
    }
    return 0;
}


/* Функции для работы с ks2 */

KeySpace2 **initKs2(Table *, size_t);
size_t hashFunction(Table *, const char *);
size_t findKeyIdxKs2(Table *, const char *);
void freeKs2 (Table *);
KeySpace2 **resizeKs2(Table *, size_t);
KeySpace2 *createNewKeyKs2(const char *);
int insertKs2(Table *, const char *, Item *);
int deleteKeyKs2(Table *, const char *);

KeySpace2 **initKs2(Table *table, size_t capacity) {
    // инициируем нулями указатели в массиве
    KeySpace2 **keySpace2 = (KeySpace2 **)calloc(capacity, sizeof(KeySpace2 *));
    if (!keySpace2) {
        logError("KeySpace 2 init error: calloc error (NULL pointer)");
    } else {
        table->msize2 = capacity; // автоматически добавляем информацию в table при init
        table->csize2 = 0; // после инициализации число элементов 0
    };
    return keySpace2; // ks1 в table, NULL в случае ошибки
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
    return idx; // указатель по индексу может быть на NULL
}

// полная очистка ks2 (нужна в том числе для удаления временной таблицы в resize)
// не удаляет item и info
void freeKs2 (Table *table) {
    for (size_t i = 0; i < table->msize2; i++) {
        KeySpace2 *chain = table->ks2[i];
        while (chain) {
            KeySpace2 *next = chain->next;
            free(chain);
            chain = next;
        }
    }
    free(table->ks2);
}

/* логика такая: коллизии разрешаем сцеплением, сцепления -> списки
 * списки - это плохо, потому что поиск переходит от O(1) к O(N) при увеличении csize/msize
 * при вставке или удалении добавляем условие на поддержание оптимальной заполненности
 * при которой будет не слишком большое число коллизий 
 * 
 * в начале static флаг на защиту от рекурсивного вызова через insertKs2 и deleteKeyKs2
*/
KeySpace2 **resizeKs2(Table *table, size_t newCapacity) {
    static int inResize = 0;
    if (inResize) {
        printf("Warning: recursive resizeKs2 call prevented\n");
        return NULL;
    }

    inResize = 1; // защита от повторного входа
    // init НЕ перезаписывает сразу ks2 -> безопасно на случай промежуточных ошибок
    KeySpace2 **oldKeySpace = table->ks2; 
    Table *tempTable = (Table *)calloc(1, sizeof(Table)); // временная таблица для обеспечения атомарности
    KeySpace2 **newKeySpace = initKs2(tempTable, newCapacity); 
    tempTable->ks2 = newKeySpace; // для работы insert
    if (!newKeySpace) {
        logError("memory reallocate error (resizeKs2)");
        inResize = 0;
        return NULL;
    } 

    // перехешируем старую таблицу в новую из-за изменения размеров
    for (size_t i = 0; i < table->msize2; i++) {
        KeySpace2 *chain = oldKeySpace[i];
        while (chain) { // бежим по цепочке, если есть
            int insertRes = insertKs2(tempTable, chain->key, chain->info); // insert в временную копию
            if (insertRes == -1) { // ошибка любой вставки - выход
                logError("insert error during resizing");
                freeKs2(tempTable); // удаляем всё, что выделили для tempTable
                free(tempTable); // удаляем саму tempTable
                inResize = 0;
                return NULL;
            }
            chain = chain->next;
        }
    }
    
    freeKs2(table); // освобождаем записи в старом пространстве

    table->ks2 = tempTable->ks2;
    table->csize2 = tempTable->csize2;
    table->msize2 = tempTable->msize2;

    free(tempTable); // больше не нужна, но записи из неё теперь в ks2 основной table

    inResize = 0; // Снимаем блокировку
    return newKeySpace;
}

// подфункция функции insertKs2
KeySpace2 *createNewKeyKs2(const char *key2) {
    KeySpace2 *newKey = (KeySpace2 *)malloc(sizeof(KeySpace2));
    if (!newKey) {
        logError("memory allocate error (createNewKeyKs2)");
    } else {
        newKey->info = NULL;
        newKey->key = (char *)key2;
        newKey->next = NULL;
    }
    return newKey;
}

/* тут следим за оригинальностью key2 
 * решение в случае повтора: замена
 * ключи разные, один хэш? -> запись в конец цепочки через next 
 */
int insertKs2(Table *table, const char *key2, Item *item) {

    // условие на увеличиение таблицы
    if ((float)table->csize2 / (float)table->msize2 >= 0.75f) {
        resizeKs2(table, table->msize2 * 2);
    }

    size_t idx = findKeyIdxKs2(table, key2);
    KeySpace2 *found = table->ks2[idx]; // элемент (указатель) по индексу

    if (!found) { // указатель на NULL (нет цепочки)
        KeySpace2 *newKey = createNewKeyKs2(key2);
        if (!newKey) {
            logError("new key creation error");
            return -1;
        }
        newKey->info = item;
        table->ks2[idx] = newKey; // ставим указатель на первый элемент цепочки
        table->csize2++; // новый ненулевой указатель в массиве
        return 0;
    } else { // указатель не на NULL (цепочка есть)
        KeySpace2 *prevElem = found;
        while (found) { // идём до последнего элемента
            if (!strcmp(found->key, key2)) { // если ключ совпал, заменяем информацию
                found->info = item;
                return 0;
            }
            prevElem = found;
            found = found->next;
        } // не нашли такой ключ в цепочке, дошли до конца: prevElem->next = NULL, found = NULL
        KeySpace2 *newKey = createNewKeyKs2(key2);
        if (!newKey) {
            logError("new key creation error");
            return -1;
        }
        newKey->info = item;
        prevElem->next = newKey;
        return 0;
    }
}

/* здесь логика удаления указателя на ключ и соответствующего поля
 * логика удаления item и самого ключа будет в главной функции delete для table
 */
int deleteKeyKs2(Table *table, const char *key2) {

    // условие на уменьшение таблицы
    if (table->msize2 >= 32 && (float)table->csize2 / (float)table->msize2 <= 0.25f) {
        resizeKs2(table, table->msize2 / 2);
    }

    size_t idx = findKeyIdxKs2(table, key2);
    KeySpace2 *findToDel = table->ks2[idx];
    KeySpace2 *prevElem = findToDel; // для удаления из середины или конца должны знать предыдущий

    while (findToDel) { // если цепочка существует
        if (!strcmp(findToDel->key, key2)) { // ищем по уникальному ключу
            if (findToDel == table->ks2[idx]) { // если элемент первый
                table->ks2[idx] = findToDel->next; // перемещаем указатель на список на второй элемент
                free(findToDel); // освобождаем память из-под первого
                printf("Key '%s' deleted successfully\n", key2);
                if (!table->ks2[idx]) { // если удалили единственный элемент цепочки
                    table->csize2--;
                }
                return 0;
            } else { // если элемент в середине или конце
                prevElem->next = findToDel->next; // перешагиваем удаляемый элемент
                free(findToDel); 
                printf("Key '%s' deleted successfully\n", key2);
                return 0;
            }
        }
        prevElem = findToDel;
        findToDel = findToDel->next;
    } 
    // если не вошли в цикл или вышли из него без удаления элемента
    logError("the key is not found or does not exist");
    return -1;
}


/* Функции для работы с Table */

/* Как я понял, у нас каждый элемент обязательно имеет по два ключа
 * соответсвенно, если это контролируется при вводе и вставке (а это контролириуется)
 * то при поиске по одному ключу нам не нужно искать такой же указатель в другом пространстве
*/

Table *initTable(size_t, size_t);
void freeTable(Table *);
int insertItem(Table *, size_t, const char *, Item *);
Item *findItemByKey1(Table *, size_t, size_t);
Item *findItemByKey2(Table *, const char *);
Item **findItem(Table *, size_t, const char *);
int deleteItem(Table *, size_t, const char *);
void printTable(const Table *);

void freeTable(Table *table) {
    if (!table) return;
    // удаляем всю информацию
    for (size_t i = 0; table->csize1 != 0 ; i++) { // пока есть node'ы 
        // выбивает ошибки, когда не находит индекс
        // key1 - индекс внутри ks1
        // NULL в key2 позволит удалять все релизы сразу
        deleteItem(table, i, NULL); 
    }

    // удаляем пространства таблиц
    freeKs1(table);
    freeKs2(table);
    free(table);
}

Table *initTable(size_t msize1, size_t msize2) {
    Table *newTable = (Table *)calloc(1, sizeof(Table));
    if (!newTable) {
        logError("table init error");
        return NULL;
    }
    newTable->ks1 = initKs1(newTable, msize1);
    newTable->ks2 = initKs2(newTable, msize2);
    if (!newTable->ks1 || !newTable->ks2) {
        freeTable(newTable);
        return NULL;
    }
    return newTable;
}

/* поиск в таблице элемента по любому заданному ключу;
 * результатом поиска должна быть копии всех найденных элементов со значениями ключей;
*/
Item *findItemByKey1(Table *table, size_t key1, size_t release) {
    if (!table || !table->ks1 || table->csize1 == 0) {
        return NULL;
    }

    KeySpace1 *keySpace = table->ks1;
    size_t start = 0, end = table->csize1 - 1;
    while (start <= end) {
        size_t middle = start + (end - start) / 2;
        KeySpace1 currElem = keySpace[middle];
        if (currElem.key == key1) { // если нашли ключ
            Node1 *currNode = currElem.node;
            while (currNode) { // бежим по списку пока не найдём нужную версию
                if (currNode->release == release) {
                    return currNode->info;
                }
                currNode = currNode->next;
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

// поиск указателя на элемент по второму ключу
Item *findItemByKey2(Table *table, const char *key2) {
    size_t idx = findKeyIdxKs2(table, key2);
    KeySpace2 *found = table->ks2[idx];
    while (found) { 
        if (!strcmp(found->key, key2)) {
            return found->info; // возвращаем указатель на item
        }
        found = found->next;
    }
    // цепочки по такому ключу нет
    logError("key2 not found in ks2");
    return NULL;
}

/* удаление из таблицы элемента, заданного составным ключом;
 * 
 * также удаляются ВСЕ связанные структуры (note, info, item, Node1)
*/
int deleteItem(Table *table, size_t key1, const char *key2) {
    if (key2) { // наличие key2 -> удаление по key2 или (key1, key2)
        Item *itemToDelete = findItemByKey2(table, key2);
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
        Node1 *nodeToDelete = table->ks1[idx1].node;
        Node1 *prev = nodeToDelete;
        while (nodeToDelete) {
            if (nodeToDelete->info == itemToDelete) { // нашли node с указателем на нужный item
                free(itemToDelete->info->note); // удаляем строку
                free(itemToDelete->info); // удаляем info
                int delKey2Res = deleteKeyKs2(table, key2); // удаляем key2 на удалённый item
                if (delKey2Res == -1) {
                    logError("can`t delete key2 from table, but item was deleted");
                }
                free(itemToDelete->key2); // удаляем сам ключ
                free(itemToDelete); // удаляем item
                if (nodeToDelete == prev) { // если первый Node
                    KeySpace1 *currElem = &table->ks1[idx1];
                    currElem->node = nodeToDelete->next;
                    if (currElem->node == NULL) { // если первый и единственный 
                        deleteKeyKs1(table, key1, 1);
                    } 
                } else { // если в середине или конце
                    prev->next = nodeToDelete->next;
                }
                free(nodeToDelete); // удаляем нужный release с key1
                return 0;
            }
            prev = nodeToDelete;
            nodeToDelete = nodeToDelete->next;
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
        Node1 *nodeToDelete = keyToDelete->node;

        while (nodeToDelete) { // удаляем все версии
            Node1 *next = nodeToDelete->next;
            int delRes = deleteKeyKs2(table, nodeToDelete->info->key2);
            if (delRes == 0) {
                free(nodeToDelete->info->info->note); // стираем строку
                free(nodeToDelete->info->info); // стираем info
                free(nodeToDelete->info->key2); // удаляем сам ключ
                free(nodeToDelete->info); // стираем item
                free(nodeToDelete); // стираем node
            } else {
                logError("delete item procedure stoped, can`t delete key2 from table");
                return -1;
            }
            nodeToDelete = next;
        }
        keyToDelete->key = INVALID_KEY1; //?? 
        keyToDelete->node = NULL; // явно зануляем указатель после освобождения памяти 

        for (size_t i = idxToDelete; i < table->csize1 - 1; i++) {
            table->ks1[i] = table->ks1[i + 1];
        }
        table->csize1--;
        printf("All items with key '%u' successfully deleted\n", key1);
        // можно уменьшать ks1 по необходимости (minsize1 = 16)
        if (table->csize1 < table->msize1 / 2 && table->msize1 >= 32) {
            KeySpace1 *KeySpace = resizeKs1(table, table->msize1 / 2);
            if (!KeySpace) {
                logError("resize error (after item delete)");
            }
        }
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
int insertItem(Table *table, size_t key1, const char *key2, Item *item) {
    int insert1Res = insertKs1(table, key1, item);
    if (insert1Res != 0) {
        logError("insert error in KeySpace1");
        return insert1Res;
    }

    /* при дублировании второго ключа стираем всю информацию
     * связанную с этим ключом перед вставкой
    */ 
    Item *isKeyNotOriginal = findItemByKey2(table, key2);
    if (isKeyNotOriginal) {
        int clearRes = deleteItem(table, isKeyNotOriginal->key1, key2);
        if (clearRes == 0) {
            printf("Warning: all info with same key2 was deleted before insert\n");
        } else { // -1
            logError("can`t free keys for the new item");
            deleteKeyKs1(table, key1, 0); // rollback
            return clearRes;
        }
    }

    int insert2Res = insertKs2(table, key2, item);
    if (insert2Res != 0) {
        logError("insert error in KeySpace2");
        deleteKeyKs1(table, key1, 0); // rollback, удаляем последнюю версию
        return insert2Res;
    }
    return 0;
} // обеспечиваем атомарность

/* удаление из таблицы всех элементов, заданных ключом в одном из ключевых пространств
 * здесь нужно использовать deleteKeyKs1 и deleteKeyKs2 в связке с findItemKey1
 * вообще удаление по key2 будет по логике такое же как в deleteItem -> можно переписать
*/

/* поиск в таблице элемента, заданного составным ключом 
 * 
 * поскольку всякий составной ключ оригинален за счёт оригинальности key2,
 * поиск по составному ключу сводится к поиску по второму ключу одного элемента, 
 * однако поиск по первому ключу должен обеспечивать вывод нескольких элементов 
*/
Item **findItem(Table *table, size_t key1, const char *key2) {
    if (key2) { // если второй ключ, всегда ищем по нему (см. выше)
        Item *item = findItemByKey2(table, key2);
        if (!item) return NULL;
        if (key1 != INVALID_KEY1 && item->key1 != key1) { // по сути, эта проверка не нужна
            logError("key1 does not match key2, try key1 = -1");
        } 
        Item **result = (Item **)malloc(2 * sizeof(Item *));
        result[0] = item;
        result[1] = NULL; // маркер конца массива
        return result;
    } else if (key1 != INVALID_KEY1) { // поиск только по первому
        // можно добавить поиск отдельного релиза
        size_t idx = findKeyIdxKs1(table, key1);
        if (idx == INVALID_IDX) return NULL;
        Node1 *node = table->ks1[idx].node;
        size_t releaseNum = 0; // считаем число релизов
        while (node) {
            releaseNum++;
            node = node->next;
        }
        if (releaseNum == 0) {
            logError("releases not found");
            return NULL;
        }
        Item **result = (Item **)malloc((releaseNum + 1) * sizeof(Item *));
        node = table->ks1[idx].node;
        for (size_t i = 0; i < releaseNum; i++) {
            result[i] = node->info;
            node = node->next;
        }
        result[releaseNum] = NULL; // маркер конца массива
        return result;
    } else {
        logError("no keys provided for deletion");
        return NULL;
    } 
}

/* LLM GOES BRRRR */

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
        size_t count = 0;
        for (Node1 *node = table->ks1[i].node; node; node = node->next) {
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
        KeySpace1 *ks1 = &table->ks1[i];
        Node1 *node = ks1->node;
        while (node) {
            Item *item = node->info;
            printf("| %-9u | %-14s | %-7u | %5.2f, %5.2f, %-11s |\n",
                ks1->key,
                item->key2,
                node->release,
                item->info->num1,
                item->info->num2,
                item->info->note);
            if (node->next) {
                printf("+-----------+----------------+---------+---------------------------+\n");
            }
            node = node->next;
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

/*
 * на каждый отдельный ввод key2 и info создаваётся отдельная запись в куче
 * как и на каждое создание item
 * удаляются они в deleteItem 
*/
Item *createItem(size_t key1, const char *key2, float num1, float num2, const char *note) {
    Item *newItem = (Item *)calloc(1, sizeof(Item));
    if (!newItem) {
        logError("can`t allocate memory for new item");
        return NULL;
    }
    char *newKey2 = (char *)calloc(strlen(key2) + 1, sizeof(char));
    if (!newKey2) {
        logError("can`t allocate memory for new key2");
        free(newItem); // rollback
        return NULL;
    }
    Info *newInfo = (Info *)calloc(1, sizeof(Info));
    if (!newInfo) {
        logError("can`t allocate memory for new info");
        free(newItem); // rollback
        free(newKey2);
        return NULL;
    }
    char *newNote = (char *)calloc(strlen(note) + 1, sizeof(char));
    if (!newNote) {
        free(newItem); // rollback
        free(newKey2);
        free(newInfo);
        logError("can`t allocate memory for new note");
        return NULL;
    }

    newInfo->note = strcpy(newNote, note);
    newInfo->num1 = num1;
    newInfo->num2 = num2;

    newItem->info = newInfo;
    newItem->key1 = key1;
    newItem->key2 = strcpy(newKey2, key2);
    
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

void addItemDialog(Table *table) {
    printf("\n=== Add New Item ===\n");

    printf("[DEBUG] table pointer: %p\n", (void*)table);
    
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
    
    Item *item = createItem(key1, key2, num1, num2, note);
    if (!item) {
        printf("Failed to create item\n");
        return;
    }
    
    int res = insertItem(table, item->key1, item->key2, item);
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
                item->info->num1, item->info->num2, item->info->note);
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
            return;
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

void deleteDialog(Table* table) {
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
            deleteItem(table, key1, NULL);
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
            deleteItem(table, INVALID_KEY1, key2);
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
            deleteItem(table, key1, key2);
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

int main() {
    Table *table = initTable(2, 2);
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
            case 1: addItemDialog(table); break;
            case 2: searchDialog(table); break;
            case 3: deleteDialog(table); break;
            case 4: printTable(table); break;
            case 0: running = 0; puts("Exiting..."); break;
            default: printf("Invalid choice!\n");
        }
    }
    
    freeTable(table);
    return 0;
}

// можно расширить функционал и вводить элементы сразу по несколько (но лучше не надо, мне ещё вторую часть лабы надо сделать)))))))
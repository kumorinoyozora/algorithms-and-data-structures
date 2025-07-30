#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>


#define N 6
#define MAX_TEXT_LEN 64
#define MAX_INPUT_LEN 64
#define DEM_SIZE 100
#define MERGE 2

typedef struct Leaf {
    int key1, key2;
    int value1, value2;
    char text[MAX_TEXT_LEN];
} Leaf;

typedef struct TreeNode {
    int x, y;
    struct TreeNode *left, *right;
    Leaf **leaves;
} TreeNode;

int leafCount(TreeNode *node) {
    if (!node->leaves) return 0;
    int count = 0;
    for (int i = 0; i < N; i++) {
        if (node->leaves[i]) count++;
    }
    return count;
}

TreeNode *initNewNode() {
    TreeNode *newNode = (TreeNode *)calloc(1, sizeof(TreeNode));
    newNode->leaves = (Leaf **)calloc(N, sizeof(Leaf *));
    if (!newNode || !newNode->leaves) return NULL;
    return newNode;
}

int getMedian(TreeNode *node, int axis) {
    if (N % 2 == 0) {
        if (axis == 0) {
            return (node->leaves[N / 2]->key1 + node->leaves[N / 2 - 1]->key1) / 2;
        }
        return (node->leaves[N / 2]->key2 + node->leaves[N / 2 - 1]->key2) / 2;
    }
    return (axis == 0) ? node->leaves[N / 2]->key1 : node->leaves[N / 2]->key2;
}

// поддерживаем упорядоченность при вставке
void insertLeaf(TreeNode **node, Leaf *leaf, int depth) {
    int currentCount = leafCount(*node);
    Leaf **leaves = (*node)->leaves;
    if (leaves[0] == NULL) {
        leaves[0] = leaf;
        return;
    }

    int axis = depth % 2;
    int leafKey1 = (axis == 0) ?  leaf->key1 : leaf->key2;
    int leafKey2 = (axis == 0) ?  leaf->key2 : leaf->key1;

    for (int i = 0; i < currentCount; i++) {
        Leaf *currLeaf = leaves[i];
        int currLeafKey1, currLeafKey2;

        if (!currLeaf) continue;

        if (axis == 0) {
            currLeafKey1 = currLeaf->key1;
            currLeafKey2 = currLeaf->key2;
        } else {
            currLeafKey1 = currLeaf->key2;
            currLeafKey2 = currLeaf->key1;
        }

        // находим первый больший и ставим перед ним
        if (currLeafKey1 > leafKey1) { 
            for (int j = currentCount; j > i; j--) {
                leaves[j] = leaves[j - 1];
            }
            (*node)->leaves[i] = leaf;
            return;
        } else if (leafKey1 == currLeafKey1 && leafKey2 == currLeafKey2) {
            // при полном совпадении ключей копируем данные
            strcpy((*node)->leaves[i]->text, leaf->text);
            (*node)->leaves[i]->value1 = leaf->value1;
            (*node)->leaves[i]->value2 = leaf->value2;
            return;
        }
    }
    // если не нашли больший элемент, добавляем в конец
    (*node)->leaves[currentCount] = leaf;
    return;
}

void splitNode(TreeNode **node, int depth) {
    TreeNode *leftNode = initNewNode();
    TreeNode *rightNode = initNewNode();
    if (!leftNode || !rightNode) return;

    int axis = depth % 2;
    int median = getMedian(*node, axis);

    for (int i = 0; i < N; i++) {
        Leaf *l = (*node)->leaves[i];
        if (!l) continue;

        int key = (axis == 0) ? l->key1 : l->key2;
        if (key < median) insertLeaf(&leftNode, l, depth + 1);
        else insertLeaf(&rightNode, l, depth + 1);
    }

    free((*node)->leaves); // не храним информацию в узле
    (*node)->leaves = NULL; 

    (*node)->right = rightNode;
    (*node)->left = leftNode;

    if (axis == 0) {
        (*node)->x = median;
        leftNode->x = (*node)->x;
        leftNode->y = (*node)->y;
        rightNode->x = (*node)->x;
        rightNode->y = (*node)->y;
    } else {
        (*node)->y = median;
        leftNode->x = (*node)->x;
        leftNode->y = (*node)->y;
        rightNode->x = (*node)->x;
        rightNode->y = (*node)->y;
    }
    
}

// информация только в листьях! т.е. в TreeNode без потомков
int insert(TreeNode **node, Leaf *leaf, int depth) {
    if (!*node) {
        *node = initNewNode(); 
        if (!*node) return 1;
    }

    TreeNode *n = *node;

    if (n->left == NULL && n->right == NULL) {
        int count = leafCount(n);
        if (count < N) {
            insertLeaf(node, leaf, depth);
            return 0;
        } else {
            splitNode(node, depth);
            return insert(node, leaf, depth); // повторная попытка после split
        }
    }

    int axis = depth % 2;
    int cmp = (axis == 0) ? (leaf->key1 < n->x) 
                            : (leaf->key2 < n->y);

    return insert(cmp ? &n->left : &n->right, leaf, depth + 1);
}

void tryMerge(TreeNode **node) {
    TreeNode *n = *node;
    if (!n->left && !n->right) return;
    int rightLeafCount = leafCount(n->right);
    int leftLeafCount = leafCount(n->left);
    if (rightLeafCount != 0 && leftLeafCount != 0) return;

    TreeNode *copyFrom = (rightLeafCount == 0) ? n->left : n->right;
    int count = (rightLeafCount == 0) ? leftLeafCount : rightLeafCount;

    (*node)->leaves = (Leaf **)calloc(N, sizeof(Leaf *));
    if (!(*node)->leaves) return;

    for (int i = 0; i < count; i++) {
        (*node)->leaves[i] = copyFrom->leaves[i];
    }
    free((*node)->right->leaves);
    free((*node)->right);
    free((*node)->left->leaves);
    free((*node)->left);
    (*node)->left = (*node)->right = NULL;
}

int deleteLeaf(TreeNode **node, int key1, int key2) {
    if (!*node) return 1;
    int currentCount = leafCount(*node);
    if (currentCount == 0) return 1;

    for (int i = 0; i < currentCount; i++) {
        Leaf *leaf = (*node)->leaves[i];

        if (leaf->key1 == key1 && leaf->key2 == key2) {
            for (int j = i; j < currentCount - 1; j++) {
                (*node)->leaves[j] = (*node)->leaves[j + 1];
            }
            (*node)->leaves[currentCount - 1] = NULL;
            free(leaf);

            if (currentCount == 1) return MERGE; // код на слияние

            return 0;
        }
    }
    return 1;
}

int removeLeaf(TreeNode **node, int key1, int key2, int depth) {
    if (!*node) return 1;

    TreeNode *n = *node;

    if (!n->left && !n->right) {
        return deleteLeaf(node, key1, key2); 
    }
    int axis = depth % 2;
    int cmp = (axis == 0) ? (key1 < n->x) : (key2 < n->y);

    int res = removeLeaf(cmp ? &n->left : &n->right, key1, key2, depth + 1);

    // возврат из рекурсии к родителю
    if (res == MERGE) {
        tryMerge(node);
    }

    return 0;
}

Leaf *findLeaf(TreeNode *node, int key1, int key2, int depth) {
    if (!node) return NULL;

    if (!node->left && !node->right) {
        int count = leafCount(node);
        for (int i = 0; i < count; i++) {
            Leaf *currLeaf = node->leaves[i];
            if (currLeaf->key1 == key1 && currLeaf->key2 == key2) {
                return currLeaf;
            }
        }
        return NULL; // такого элемента нет
    }

    int axis = depth % 2;
    int cmp = (axis == 0) ? (key1 < node->x) 
                            : (key2 < node->y);

    return findLeaf(cmp ? node->left : node->right, key1, key2, depth + 1);
}

double euclidianDist(int x1, int y1, int x2, int y2) {
    return (x1 - x2) * (double)(x1 - x2) + (y1 - y2) * (double)(y1 - y2);
}

void nearestWithinLimit(TreeNode *node, int key1, int key2, int depth, Leaf **best, double *bestDist) {
    if (!node) return;

    if(!node->left && !node->right) {
        int count = leafCount(node);
        for (int i = 0; i < count; i++) {
            Leaf *l = node->leaves[i];
            if (l->key1 <= key1 && l->key2 <= key2) {
                double dist = euclidianDist(l->key1, l->key2, key1, key2);
                if (dist < *bestDist) {
                    *bestDist = dist;
                    *best = l;
                }
            }
        }
        return;
    }
    
    int axis = depth % 2;
    int key = (axis == 0) ? key1 : key2;
    int nodeCoord = (axis == 0) ? node->x : node->y;

    TreeNode *first = (key < nodeCoord) ? node->left : node->right;
    TreeNode *second = (key < nodeCoord) ? node->right : node->left;

    nearestWithinLimit(first, key1, key2, depth + 1, best, bestDist);

    // Может ли вторая сторона содержать лучший вариант?
    double diff = key - nodeCoord;
    if (diff * diff < *bestDist) {
        nearestWithinLimit(second, key1, key2, depth + 1, best, bestDist);
    }
}

Leaf *findNearestWithinLimit(TreeNode *root, int key1, int key2) {
    Leaf *best = NULL;
    double bestDist = DBL_MAX;
    nearestWithinLimit(root, key1, key2, 0, &best, &bestDist);
    return best;
}

// можно оптимизировать, чтобы не обходить вообще всё 
void printTree(TreeNode *node, int key1, int key2) {
    if (!node) return;
    if (key1 == -1 && key2 == -1) { // ключи не указаны
        printTree(node->right, key1, key2);

        if (!node->left && !node->right) {
            int count = leafCount(node);
            for (int i = count - 1; i >= 0; i--) {
                Leaf *l = node->leaves[i];
                printf("| %6d | %6d | %5d %5d %8s |\n",
                    l->key1, l->key2, l->value1, l->value2, l->text
                );
                printf("+--------+--------+----------------------+\n");
            }
        }
        printTree(node->left, key1, key2);

    } else if (key1 != -1 && key2 == -1) { // указан key1
        printTree(node->right, key1, key2);

        if (!node->left && !node->right) {
            int count = leafCount(node);
            for (int i = count - 1; i >= 0; i--) {
                Leaf *l = node->leaves[i];
                if (l->key1 <= key1) continue;
                printf("| %6d | %6d | %5d %5d %8s |\n",
                    l->key1, l->key2, l->value1, l->value2, l->text
                );
                printf("+--------+--------+----------------------+\n");
            }
        }
        printTree(node->left, key1, key2);

    } else if (key1 == -1 && key2 != -1) { // указан key2
        printTree(node->right, key1, key2);

        if (!node->left && !node->right) {
            int count = leafCount(node);
            for (int i = count - 1; i >= 0; i--) {
                Leaf *l = node->leaves[i];
                if (l->key2 <= key2) continue;
                printf("| %6d | %6d | %5d %5d %8s |\n",
                    l->key1, l->key2, l->value1, l->value2, l->text
                );
                printf("+--------+--------+----------------------+\n");
            }
        }
        printTree(node->left, key1, key2);

    } else if (key1 != -1 && key2 != -1) { // указаны оба
        printTree(node->right, key1, key2);

        if (!node->left && !node->right) {
            int count = leafCount(node);
            for (int i = count - 1; i >= 0; i--) {
                Leaf *l = node->leaves[i];
                if (l->key1 <= key1 || l->key2 <= key2) continue;
                printf("| %6d | %6d | %5d %5d %8s |\n",
                    l->key1, l->key2, l->value1, l->value2, l->text
                );
                printf("+--------+--------+----------------------+\n");
            }
        }
        printTree(node->left, key1, key2);
    }
    return;
}

void saveLeaves(TreeNode *root, FILE *file) {
    if (!root || !file) return;

    if (!root->right && !root->left) {
        int count = leafCount(root);
        for (int i = 0; i < count; i++) {
            Leaf *l = root->leaves[i];
            fwrite(&l->key1, sizeof(int), 1, file);
            fwrite(&l->key2, sizeof(int), 1, file);
            fwrite(&l->value1, sizeof(int), 1, file);
            fwrite(&l->value2, sizeof(int), 1, file);
            size_t len = strlen(l->text);
            fwrite(&len, sizeof(size_t), 1, file);
            fwrite(l->text, sizeof(char), len, file);
        }
        free(root->leaves);
        return;
    }

    saveLeaves(root->left, file);
    saveLeaves(root->right, file);
}

void loadLeaves(TreeNode **root, FILE *file) {
    if (!file || !*root) return;

    int key1, key2, value1, value2;
    size_t len;
    char text[MAX_TEXT_LEN] = {0};

    while (fread(&key1, sizeof(int), 1, file) == 1 &&
        fread(&key2, sizeof(int), 1, file) == 1 &&
        fread(&value1, sizeof(int), 1, file) == 1 &&
        fread(&value2, sizeof(int), 1, file) == 1 &&
        fread(&len, sizeof(size_t), 1, file) == 1 &&
        fread(text, sizeof(char), len, file) == len) {

        Leaf *leaf = (Leaf *)calloc(1, sizeof(Leaf));
        leaf->key1 = key1;
        leaf->key2 = key2;
        leaf->value1 = value1;
        leaf->value2 = value2;
        strcpy(leaf->text, text);

        insert(root, leaf, 0);
    }
}

void saveTree(TreeNode *root, FILE *file) {
    if (!root || !file) return; 

    fwrite(&root->x, sizeof(int), 1, file);
    fwrite(&root->y, sizeof(int), 1, file);

    // притом мы знаем что потомка всегда два
    int hasChildren = (root->left || root->right) ? 1 : 0;
    fwrite(&hasChildren, sizeof(int), 1, file);

    saveTree(root->left, file);
    saveTree(root->right, file);
    
    if (!root->left && !root->right) {
        free(root);
        return;
    }
}

TreeNode *loadTree(FILE *file) {
    if (!file) return NULL;

    int x, y, hasChildren;
    if (fread(&x, sizeof(int), 1, file) != 1 || 
        fread(&y, sizeof(int), 1, file) != 1 ||
        fread(&hasChildren, sizeof(int), 1, file) != 1) {
        return NULL;
    }
    TreeNode *newNode = (TreeNode *)calloc(1, sizeof(TreeNode));
    if (!newNode) return NULL;
    
    newNode->x = x;
    newNode->y = y;

    if (hasChildren) {
        newNode->left = loadTree(file);
        newNode->right = loadTree(file);
    } else {
        newNode->leaves = (Leaf **)calloc(N, sizeof(Leaf *));
        newNode->right = NULL;
        newNode->left = NULL;
    }
    return newNode;
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
int inputUInt(int *value, const char *prompt) {
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

Leaf *createLeaf(int key1, int key2, int num1, int num2, const char *note) {
    Leaf *leaf = (Leaf *)calloc(1, sizeof(Leaf));
    leaf->key1 = key1;
    leaf->key2 = key2;
    leaf->value1 = num1;
    leaf->value2 = num2;
    strcpy(leaf->text, note);

    return leaf;
}

void addItemDialog(TreeNode **root) {
    printf("\n=== Add New Item ===\n");

    int key1;
    int key2;
    int num1, num2;
    char note[MAX_TEXT_LEN];
    
    if (!inputUInt(&key1, "Enter key1 (number): ")) {
        printf("Failed to read key1\n");
        return;
    }
    
    if (!inputUInt(&key2, "Enter key2 (number): ")) {
        printf("Failed to read key2\n");
        return;
    }
    
    if (!inputUInt(&num1, "Enter num1: ") || 
        !inputUInt(&num2, "Enter num2: ")) {
        printf("Invalid numbers\n");
        return;
    }
    
    if (!inputString(note, sizeof(note), "Enter note: ")) {
        printf("Invalid note\n");
        return;
    }
    
    Leaf *leaf = createLeaf(key1, key2, num1, num2, note);
    if (!leaf) {
        printf("Failed to create item\n");
        return;
    }
    
    int res = insert(root, leaf, 0);
    printf(res == 0 ? "Item added!\n" : "Failed to add item!\n");
}

void printSearchResults(TreeNode *root, int key1, int key2) {
    if (!root) {
        printf("--- Error: Table is NULL ---\n");
        return;
    }
    
    printf("+--------+--------+----------------------+\n");
    printf("| Key1   | Key2   | Data                 |\n");
    printf("+--------+--------+----------------------+\n");
    
    printTree(root, key1, key2);
}

void printFindResult(Leaf *leaf) {
    if (!leaf) return;
    printf("+--------+--------+----------------------+\n");
    printf("| Key1   | Key2   | Data                 |\n");
    printf("+--------+--------+----------------------+\n");
    printf("| %6d | %6d | %5d %5d %8s |\n",
        leaf->key1, leaf->key2, leaf->value1, leaf->value2, leaf->text
    );
    printf("+--------+--------+----------------------+\n");
}

void searchDialog(TreeNode *root) {
    printf("\n=== Search Item ===\n");
    printf("1. Search by (key1, key2)\n");
    printf("2. Euclidian search by (key1, key2)\n");
    printf("0. Exit\n");

    int key1;
    int key2;

    int choice;
    int inpUIntRes1;
    int inpUIntRes2;

    Leaf *l = NULL;
    
    int choiseRes = inputUInt(&choice, "Select: ");
    if (!choiseRes) {
        printf("Input error\n");
    }

    switch (choice) {
        case 1:
            inpUIntRes1 = inputUInt(&key1, "Enter key1: ");
            inpUIntRes2 = inputUInt(&key2, "Enter key2: ");
            if (!inpUIntRes1 || !inpUIntRes2) {
                printf("Invalid input\n");
                return;
            }
            l = findLeaf(root, key1, key2, 0);
            printFindResult(l);
            return;
        case 2:
            inpUIntRes1 = inputUInt(&key1, "Enter key1: ");
            inpUIntRes2 = inputUInt(&key2, "Enter key2: ");
            if (!inpUIntRes1 || !inpUIntRes2) {
                printf("Invalid input\n");
                return;
            }
            l = findNearestWithinLimit(root, key1, key2);
            printFindResult(l);
            return;
        case 0: 
            return;
        default:
            printf("Invalid choice!\n"); 
            return;
    }
}

void deleteDialog(TreeNode **root) {
    printf("\n=== Delete Item ===\n");

    int key1;
    int key2;

    int inpUIntRes1;
    int inpUIntRes2;

    inpUIntRes1 = inputUInt(&key1, "Enter key1: ");
    inpUIntRes2 = inputUInt(&key2, "Enter key2: ");
    if (!inpUIntRes1 || !inpUIntRes2) {
        printf("Invalid input\n");
        return;
    }
    printf("All information from items below will be deleted:\n");
    findLeaf(*root, key1, key2, 0);
    int res = removeLeaf(root, key1, key2, 0);

    printf("%s", (res == 0) ? "Info deleted successfully" : "Info hasn`t been deleted");
    return;
}

void printTableDialog(TreeNode *root) {
    printf("\n=== Print Item ===\n");
    printf("1. Print by key1\n");
    printf("2. Print by key2\n");
    printf("3. Print by (key1, key2)\n");
    printf("4. Print all\n");
    printf("0. Exit\n");

    int key1;
    int key2;

    int choice;
    int inpUIntRes1;
    int inpUIntRes2;
    
    int choiseRes = inputUInt(&choice, "Select: ");
    if (!choiseRes) {
        printf("Input error\n");
    }

    switch (choice) {
        case 1:
            inpUIntRes1 = inputUInt(&key1, "Enter key1: ");
            if (!inpUIntRes1) {
                printf("Invalid input\n");
                return;
            }
            printSearchResults(root, key1, -1);
            return; 
        case 2: // поиск по второму ключу
            inpUIntRes2 = inputUInt(&key2, "Enter key2: ");
            if (!inpUIntRes2) {
                printf("Invalid input\n");
                return;
            }
            printSearchResults(root, -1, key2);
            return; 
        case 3:
            inpUIntRes1 = inputUInt(&key1, "Enter key1: ");
            inpUIntRes2 = inputUInt(&key2, "Enter key2: ");
            if (!inpUIntRes1 || !inpUIntRes2) {
                printf("Invalid input\n");
                return;
            }
            Leaf *l = findLeaf(root, key1, key2, 0);
            printFindResult(l);
            return;
        case 4:
            printSearchResults(root, -1, -1);
            return;
        case 0: 
            return;
        default:
            printf("Invalid choice!\n"); 
            return;
    }
}

void exportToDot(TreeNode *node, FILE *file) {
    if (!node) return;

    fprintf(file, "  node%p [label=\"(%d, %d)\"];\n", 
        (void *)node, node->x, node->y
    );

    if (!node->left && !node->right) {
        fprintf(file, "  leaves%p [label=\"{", (void *)node);

        int count = leafCount(node);
        for (int i = 0; i < count; i++) {
            Leaf *l = node->leaves[i];
            fprintf(file, "<f%d> [%d, %d] %d, %d, %s",  
                i, l->key1, l->key2, l->value1, l->value2, l->text
            );
            if (i != count - 1) fprintf(file, " | ");
        }
        fprintf(file, "}\", shape=record, style=filled, fillcolor=lightgrey];\n");

        fprintf(file, "  node%p -> leaves%p [style=dotted];\n", (void *)node, (void *)node);

    } else {

        if (node->left) {
            fprintf(file, "  node%p -> node%p [label=\"L\"];\n", (void *)node, (void *)node->left);
            exportToDot(node->left, file);
        }
        if (node->right) {
            fprintf(file, "  node%p -> node%p [label=\"R\"];\n", (void *)node, (void *)node->right);
            exportToDot(node->right, file);
        }
    }
}

void generateGraphviz(TreeNode *root, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Cannot open dot file");
        return;
    }

    fprintf(file, "digraph Tree {\n");
    fprintf(file, "  node [shape=record];\n");

    exportToDot(root, file);

    fprintf(file, "}\n");
    fclose(file);
}

void printMainMenu() {
    printf("\n=== Main Menu ===\n");
    printf("1. Add new item\n");
    printf("2. Search items\n");
    printf("3. Delete item\n");
    printf("4. Print table\n");
    printf("5. Generate Graphviz\n");
    printf("0. Exit\n");
}

int main() {
    TreeNode *root = NULL;
    FILE *file = fopen("tree_nodes.bin", "rb");
    if (!file) {
        file = fopen("tree_nodes.bin", "wb");
        fclose(file);
    } else {
        root = loadTree(file);
        fclose(file);
    }

    file = fopen("leaves.bin", "rb");
    if (!file) {
        file = fopen("leaves.bin", "wb");
        fclose(file);
    } else {
        loadLeaves(&root, file);
        fclose(file);
    }


    int choice;
    int running = 1;
    
    while (running) {
        printMainMenu();
        
        if (!inputUInt(&choice, "Select: ")) {
            printf("Input error\n");
            continue;
        }

        switch (choice) {
            case 1: addItemDialog(&root); break;
            case 2: searchDialog(root); break;
            case 3: deleteDialog(&root); break;
            case 4: printTableDialog(root); break;
            case 5:
                generateGraphviz(root, "tree.dot");
                system("dot -Tpng tree.dot -o tree.png");
                puts("Graph saved in tree.png");
                break;
            case 0: running = 0; puts("Exiting..."); break;
            default: printf("Invalid choice!\n");
        }
    }
    file = fopen("leaves.bin", "wb");
    saveLeaves(root, file);
    fclose(file);

    file = fopen("tree_nodes.bin", "wb");
    saveTree(root, file);
    fclose(file);

    return 0;
}
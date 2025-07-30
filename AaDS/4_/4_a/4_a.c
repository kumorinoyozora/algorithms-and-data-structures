#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


#define MAX_INPUT_LEN 128 

typedef struct TreeNode {
    unsigned int key;
    char *info;
    struct TreeNode *parent;
    struct TreeNode *right;
    struct TreeNode *left;
    struct TreeNode *previous;
    struct TreeNode *next;
} TreeNode;

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, len);
    return copy;
}

TreeNode *find(TreeNode *root, unsigned key) {
    while (root) {
        if (root->key == key) return root;
        
        if (key < root->key) {
            root = root->left;
        } else {
            root = root->right;
        }
    }
    return NULL;
}

TreeNode *min(TreeNode *root) {
    while (root) {
        if (root->left) {
            root = root->left;
        } else {
            return root;
        }
    }
    return NULL;
}

TreeNode *max(TreeNode *root) {
    while (root) {
        if (root->right) {
            root = root->right;
        } else {
            return root;
        }
    }
    return NULL;
}

TreeNode *next(TreeNode *root) {
    if (root->right) {
        return min(root->right);
    } else {
        TreeNode *parent = root->parent;
        while (parent && parent->right == root) {
            root = parent;
            parent = root->parent;
        }
        return parent; // может вернуться и NULL
    }
}

TreeNode *previous(TreeNode *root) {
    if (root->left) {
        return max(root->left);
    } else {
        TreeNode *parent = root->parent;
        while (parent && parent->left == root) {
            root = parent;
            parent = root->parent;
        }
        return parent;
    }
}

// рекурсивный центрированный обход
void _updateThreading(TreeNode *node, TreeNode **prev) {
    if (!node) return;

    _updateThreading(node->left, prev);

    node->previous = *prev;
    if (*prev) (*prev)->next = node;
    *prev = node;

    _updateThreading(node->right, prev);
}

// массовый апргред/создание прошивки
void rebuildThreading(TreeNode *root) {
    TreeNode *prev = NULL;
    _updateThreading(root, &prev);
}

int insert(TreeNode **root, unsigned key, const char *info) {
    TreeNode *newNode = (TreeNode *)malloc(sizeof(TreeNode));
    if (!newNode) return 1;

    newNode->info = (char *)info;
    newNode->key = key;
    newNode->left = newNode->right = newNode->parent = NULL;
    newNode->previous = newNode->next = NULL;

    if (*root == NULL) {
        *root = newNode;
        return 0;
    }
    
    TreeNode *sameKey = find(*root, key);
    if (sameKey) {
        // вставка в конец цепочки повторов
        while (sameKey->left && sameKey->left->key == key) {
            sameKey = sameKey->left; 
        } 
        sameKey->previous->next = newNode;
        newNode->previous = sameKey->previous;
        sameKey->previous = newNode;
        newNode->next = sameKey;
        newNode->parent = sameKey;

        TreeNode *left = sameKey->left;
        sameKey->left = newNode;
        newNode->right = NULL;

        if (left) {
            newNode->left = left;
            left->parent = newNode;
        } else {
            newNode->left = NULL;
        }
        return 0;
    } else {
        TreeNode *curr = *root;
        TreeNode *prev = NULL;
        while (curr) {
            prev = curr;
            if (curr->key > key) curr = curr->left;
            else curr = curr->right;
        }
        newNode->parent = prev;
        if (prev->key > key) prev->left = newNode;
        else prev->right = newNode;

        newNode->previous = previous(newNode);
        newNode->next = next(newNode);

        if (newNode->previous)
            newNode->previous->next = newNode;
        if (newNode->next)
            newNode->next->previous = newNode;

        return 0;
    }
    return 1;
}

int delete(TreeNode **root, unsigned key) {
    TreeNode *toDelete = find(*root, key);
    if (!toDelete) return 1;

    // удаляем из прошивки
    if (toDelete->previous) toDelete->previous->next = toDelete->next;
    if (toDelete->next) toDelete->next->previous = toDelete->previous;

    /**
     * Далее три варианта: 
     *      A. Лист
     *      B. Ветвь с одним потомком
     *      С. Ветвь с двумя потомками
     */
    if (!toDelete->left && !toDelete->right) {
        free(toDelete->info);
        if (toDelete->parent) {
            if (toDelete->parent->left == toDelete) toDelete->parent->left = NULL;
            else toDelete->parent->right = NULL;
        } else {
            *root = NULL; // удаляем корень
        }

        free(toDelete);
        return 0;
    } else if (!toDelete->left || !toDelete->right) {
        TreeNode *child = (toDelete->left) ? toDelete->left : toDelete->right;
        free(toDelete->info);
        if (toDelete->parent) {
            if (toDelete->parent->left == toDelete) toDelete->parent->left = child;
            else toDelete->parent->right = child;
        } else {
            *root = child;
        }
        child->parent = toDelete->parent;

        free(toDelete);
        return 0;
    } else { // (toDelete->left && toDelete->right)
        /**
         * Тут тоже несколько опций: 
         *      1. toDelete->right->left = NULL - процедура как со списком
         *      2. left есть и доходим по цепочке в конец, ветвь без копий
         *      3.1. Ветвь клонов с иным элементом в конце - процедура аналогично (2)
         *      3.2. Ветвь клонов, нужно перносить полностью
         */
        TreeNode *successor = next(toDelete);
        free(toDelete->info);
        
        // восстанавливаем прошивку
        toDelete->previous = previous(toDelete);
        toDelete->previous->next = toDelete;

        // 1
        if (successor == toDelete->right) {            
            // данные копируем
            toDelete->info = successor->info;
            toDelete->key = successor->key;
            
            toDelete->next = successor->next;
            toDelete->right = successor->right;

            if (successor->next)
                successor->next->previous = toDelete;
            
            if (successor->right) {
                successor->right->parent = toDelete;
                successor->right->previous = toDelete;
            }

            free(successor);
            return 0;
        }
        
        TreeNode *firstClone = successor;
        // в случае ветки копий знаем её начало и конец
        while (firstClone->parent && firstClone->parent->key == successor->key)
            firstClone = firstClone->parent;
        
        // 2 || 3.1
        if (firstClone == successor) { 
            // данные копируем
            toDelete->info = successor->info;
            toDelete->key = successor->key;

            toDelete->next = successor->next;
            
            if (successor->next)
                successor->next->previous = toDelete;
            
            if (successor->right) { // точно нет left
                successor->parent->left = successor->right;
                successor->right->parent = successor->parent;
            } else {
                successor->parent->left = NULL;
            }

            free(successor);
            return 0;
        }

        // мясо с клонами 3.2
        // последний клон точно без потомков, у первого могут быть, если он toDelete->right
        toDelete->info = firstClone->info;
        toDelete->key = firstClone->key;

        if (toDelete->left) {
            firstClone->left->parent = toDelete;
            firstClone->left->next = toDelete;
            TreeNode *leftmax = max(toDelete->left);
            leftmax->next = successor;

            toDelete->left->parent = successor;
            successor->left = toDelete->left;
            toDelete->left = firstClone->left;
            toDelete->previous = firstClone->left;
        }

        if (firstClone == toDelete->right) {
            toDelete->next = firstClone->next;
            firstClone->next->previous = toDelete;

            toDelete->right = firstClone->right;

            if (firstClone->right) {
                firstClone->right->parent = toDelete;
                firstClone->right->previous = toDelete;
            }

            free(firstClone);
            return 0;
        }
        toDelete->next = firstClone->next;
        firstClone->next->previous = toDelete;

        if (firstClone->right) { // точно нет left
            firstClone->parent->left = firstClone->right;
            firstClone->right->parent = firstClone->parent;
        }

        free(firstClone);
        return 0;
    }
    return 1;
}

/**
 * для поиска всех минимальных передаём параметром key
 * min(root)->key
 */ 
TreeNode **findAll(TreeNode *root, unsigned key) {
    TreeNode *target = find(root, key);
    if (!target) return NULL;
    unsigned count = 1;
    TreeNode *curr = target;
    while (curr->left && curr->left->key == key) {
        curr = curr->left;
        count++;
    }
    TreeNode **nodeList = (TreeNode **)calloc(count + 1, sizeof(TreeNode *));
    for (unsigned i = 0; i < count; i++) {
        nodeList[i] = target;
        target = target->left;
    }
    return nodeList;
}

void saveToFile(TreeNode *root, FILE *file) {
    if (!root || !file) return;

    size_t len = strlen(root->info);
    fwrite(&root->key, sizeof(unsigned), 1, file);
    fwrite(&len, sizeof(size_t), 1, file);
    fwrite(root->info, sizeof(char), len, file);

    saveToFile(root->left, file);
    saveToFile(root->right, file);
}

void loadFromFile(TreeNode **root, FILE *file) {
    if (!file) return;

    // пустой ли файл
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size == 0) {
        rewind(file);
        return; // файл пустой, ничего не грузим
    }
    rewind(file); // возвращаемся в начало

    unsigned key, len;
    while (fread(&key, sizeof(unsigned), 1, file) == 1 &&
            fread(&len, sizeof(size_t), 1, file) == 1) {
        char *info = (char *)malloc(len + 1);
        if (!info) return;
        if (fread(info, sizeof(char), len, file) != len) {
            free(info);
            return;
        }
        info[len] = '\0'; // завершаем строку

        if (*root == NULL) {
            *root = (TreeNode *)malloc(sizeof(TreeNode));
            (*root)->key = key;
            (*root)->info = info;
            (*root)->left = (*root)->right = (*root)->parent = NULL;
            (*root)->previous = (*root)->next = NULL;
        } else {
            insert(root, key, info);
        }
    }
}

void freeTree(TreeNode *root) {
    if (!root) return;

    freeTree(root->left);
    freeTree(root->right);

    free(root->info);
    free(root);

}
/* 
void printTree(TreeNode *root) {
    if (!root) return;
    
    printTree(root->left);
    
    printf("+-----------+---------------------------+\n");
    printf("| Key       | Note                      |\n");
    printf("+-----------+---------------------------+\n");
    printf("| %-9u | %-25s |\n",
        root->key, root->info);
    printf("+-----------+---------------------------+\n");

    printTree(root->right);
}
 */
void printTree(TreeNode *root) {
    TreeNode *node = min(root);
    
    printf("+-----------+---------------------------+\n");
    printf("| Key       | Note                      |\n");
    printf("+-----------+---------------------------+\n");
    while (node) {
        printf("| %-9u | %-25s |\n",
            node->key, node->info);
        printf("+-----------+---------------------------+\n");
        node = node->next;
    }
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

void addDialog(TreeNode **root) {
    printf("\n=== Add New Node ===\n");
    
    unsigned int key;
    char buffer[MAX_INPUT_LEN];
    
    if (!inputUInt(&key, "Enter key (number): ")) {
        printf("Failed to read key\n");
        return;
    }
    
    if (!inputString(buffer, sizeof(buffer), "Enter note: ")) {
        printf("Invalid note\n");
        return;
    }

    char *note = strdup(buffer);
    
    int res = insert(root, key, note);
    printf(res == 0 ? "Node added!\n" : "Failed to add node!\n");
}

void printSearchResults(TreeNode **results) {
    if (!results || !results[0]) {
        printf("No items found\n");
        return;
    }
    
    printf("\nSearch results:\n");
    printf("+-----------+---------------------------+\n");
    printf("| Key       | Note                      |\n");
    printf("+-----------+---------------------------+\n");

    for (int i = 0; results[i]; i++) {
        TreeNode *node = results[i];
        printf("| %-9u | %-25s |\n",
            node->key, node->info);
        printf("+-----------+---------------------------+\n");
    }
    return;
}

void searchDialog(TreeNode *root) {
    printf("\n=== Search Node ===\n");
    printf("1. Search by key\n");
    printf("2. Search node with least key\n");
    printf("0. Exit\n");

    unsigned int key;

    size_t choice;
    int inpUIntRes;
    TreeNode **result;
    
    int choiseRes = inputUInt(&choice, "Select: ");
    if (!choiseRes) {
        printf("Input error\n");
    }

    switch (choice) {
        case 1:
            inpUIntRes = inputUInt(&key, "Enter key: ");
            if (!inpUIntRes) {
                printf("Invalid input\n");
                return;
            }
            result = findAll(root, key);
            printSearchResults(result);
            free(result);
            return;
        case 2: 
            result = findAll(root, min(root)->key);
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

void deleteDialog(TreeNode **root) {
    printf("\n=== Delete Node ===\n");

    unsigned int key;
    int inpUIntRes;
    
    inpUIntRes = inputUInt(&key, "Enter key: ");
    if (!inpUIntRes) {
        printf("Invalid input\n");
        return;
    }
    int res = delete(root, key);
    printf(res == 0 ? "Node deleted!\n" : "Failed to delete node!\n");
}

void exportToDot(TreeNode *root, FILE *file) {
    if (!root) return;

    fprintf(file, "  node%p [label=\"%u\\n%s\"];\n", (void *)root, root->key, root->info);

    if (root->left) {
        fprintf(file, "  node%p -> node%p [label=\"L\"];\n", (void *)root, (void *)root->left);
        exportToDot(root->left, file);
    }
    if (root->right) {
        fprintf(file, "  node%p -> node%p [label=\"R\"];\n", (void *)root, (void *)root->right);
        exportToDot(root->right, file);
    }
    if (root->next) {
        fprintf(file, "  node%p -> node%p [style=dashed, color=blue, label=\"next\"];\n", (void *)root, (void *)root->next);
    }
    if (root->previous) {
        fprintf(file, "  node%p -> node%p [style=dashed, color=red, label=\"prev\"];\n", (void *)root, (void *)root->previous);
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
    printf("1. Add new node\n");
    printf("2. Search nodes\n");
    printf("3. Delete node\n");
    printf("4. Show full tree\n");
    printf("5. Generate Graphviz\n");
    printf("0. Exit\n");
}

int main() {
    TreeNode *root = NULL;
    FILE *file = fopen("tree.bin", "rb");
    if (!file) {
        file = fopen("tree.bin", "wb");
        fclose(file);
        file = fopen("tree.bin", "rb");
    }
    loadFromFile(&root, file);
    fclose(file);

    size_t choice;
    int running = 1;
    
    while (running) {
        printMainMenu();
        
        if (!inputUInt(&choice, "Select: ")) {
            printf("Input error\n");
            continue;
        }

        switch (choice) {
            case 1: addDialog(&root);break;
            case 2: searchDialog(root); break;
            case 3: deleteDialog(&root); break;
            case 4: printTree(root); break;
            case 5:
                generateGraphviz(root, "tree.dot");
                system("dot -Tpng tree.dot -o tree.png");
                puts("Graph saved in tree.png");
                break;
            case 0: running = 0; puts("Exiting..."); break;
            default: printf("Invalid choice!\n");
        }
    }
    file = fopen("tree.bin", "wb");
    saveToFile(root, file);
    fclose(file);

    freeTree(root);

    return 0;
}

/**
 * 
 * 
 * 
 * 
 * 
 */
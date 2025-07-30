#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>


#define NAME_LEN 8
#define MAX_INPUT_LEN 64
#define MAX_RANDOM_GRAPH_SIZE 10
#define INFINITY DBL_MAX // from float.h
#define INVALID_IDX ((size_t)-1)

// основные структуры
typedef struct {
    char name[NAME_LEN]; 
    double x, y;
} Vertex;

typedef struct Edge {
    size_t to; // индекс вершины назначения
    double weight;
    struct Edge *next; // cледующее ребро данной вершины
} Edge;

typedef struct {
    // у массивов соответствующие индексы
    Vertex **vertices;
    Edge **adj; // cписок смежностей
    size_t vertex_count;
    size_t max_size;
} Graph;

// базовые операции
int addVertex(Graph *g, const char *name, double x, double y);
Edge *addEdge(Graph *g, const char *from, const char *to);
int removeVertex(Graph *g, const char *name);
int removeEdge(Graph *g, const char *from, const char *to);
void printGraph(Graph *g);

// вспомогательные операции
size_t vertexIdx(Graph *g, const char *name);
double distance(Vertex *a, Vertex *b);
Graph *initGraph(size_t initN);
void freeGraph(Graph *g);
void exportToDot(Graph *g, FILE *file);
void generateGraphviz(Graph *g, const char *filename);

// поиск
typedef struct Step {
    size_t from;
    size_t to;
    size_t step_number;
    struct Step *next;
} Step;

typedef struct {
    Step *head;
    size_t count;
} DFSPath;

void dfsUtil(Graph *g, size_t current, size_t target, int *visited, DFSPath *path, size_t step, int *found);
DFSPath *dfsSearch(Graph *g, const char *from, const char *to);

typedef struct {
    size_t *vertices;
    size_t length;
    double total_weight;
} ShortestPath;

ShortestPath *BellmanFord(Graph *g, const char *from, const char *to);

void FloydWarshall(Graph *g, double ***distOut, size_t ***nextOut);

// работа с файлами
void saveGraph(Graph *g, const char *filename);
int loadGraph(Graph *g, const char *filename);

/**
 * Subfunctions
 */

// можно оформить через хэш
size_t vertexIdx(Graph *g, const char *name) {
    for (size_t i = 0; i < g->vertex_count; i++) {
        if (strcmp(g->vertices[i]->name, name) == 0) {
            return i;
        }
    }
    return INVALID_IDX;
}

double distance(Vertex *a, Vertex *b) {
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}

Graph *initGraph(size_t initN) {
    Graph *g = (Graph *)calloc(1, sizeof(Graph));
    if (!g) return NULL;

    Vertex **v = (Vertex **)calloc(initN, sizeof(Vertex *));
    if (!v) {
        free(g);
        return NULL;
    }

    Edge **e = (Edge **)calloc(initN, sizeof(Edge *));
    if (!e) {
        free(g);
        free(v);
        return NULL;
    }

    g->max_size = initN;
    g->vertices = v;
    g->adj = e;

    return g;
}

void freeGraph(Graph *g) {
    if (!g) return;
    for (size_t i = 0; i < g->vertex_count; i++) {
        free(g->vertices[i]);
        Edge *curr = g->adj[i];
        while (curr) {
            Edge *toDel = curr;
            curr = curr->next;
            free(toDel);
        }
    }
    free(g->vertices);
    free(g->adj);
    free(g);
}

/**
 * Basic functions
 */

int addVertex(Graph *g, const char *name, double x, double y) {
    // проверка на дубликат
    if (vertexIdx(g, name) != INVALID_IDX) return -1;

    Vertex *v = (Vertex *)malloc(sizeof(Vertex));
    if (!v) return -1;

    strcpy(v->name, name);
    v->x = x;
    v->y = y;

    if (g->vertex_count == g->max_size) {
        size_t new_size = g->max_size * 2;

        Vertex **new_vertices = realloc(g->vertices, sizeof(Vertex *) * new_size);
        if (!new_vertices) return -1;

        Edge **new_adj = realloc(g->adj, sizeof(Edge *) * new_size);
        if (!new_adj) {
            free(new_vertices);
            return -1;
        }

        for (size_t i = g->max_size; i < new_size; i++) {
            new_vertices[i] = NULL;
            new_adj[i] = NULL;
        }

        g->vertices = new_vertices;
        g->adj = new_adj;
        g->max_size = new_size;
    }

    size_t i = g->vertex_count;
    g->vertices[i] = v;
    g->adj[i] = NULL;
    g->vertex_count++;

    return 0;
}

Edge *addEdge(Graph *g, const char *from, const char *to) {
    size_t fromIdx = vertexIdx(g, from);
    size_t toIdx = vertexIdx(g, to);
    if (fromIdx == INVALID_IDX || toIdx == INVALID_IDX) {
        puts("Vertexes not found");
        return NULL;
    }

    // проверка на дубликат
    Edge *check = g->adj[fromIdx];
    while (check) {
        if (check->to == toIdx) return NULL;
        check = check->next;
    }

    double weight = distance(g->vertices[fromIdx], g->vertices[toIdx]);

    Edge *e = (Edge *)malloc(sizeof(Edge));
    if (!e) return NULL;

    e->to = toIdx;
    e->weight = weight;

    // добавляем в начало списка
    e->next = g->adj[fromIdx]; 
    g->adj[fromIdx] = e;
    
    return e;
}

/** 
 * можно сделать без сдвига, но нужно внедрять менеджер пустого пространства
 * имеет смысл в частности, если все операции делать внутри бинарников
 */
int removeVertex(Graph *g, const char *name) {
    size_t idx = vertexIdx(g, name);
    if (idx == INVALID_IDX) return -1;

    // удаление исходящих рёбер
    Edge *list = g->adj[idx];
    while (list) {
        Edge *toDel = list;
        list = list->next;
        free(toDel);
    }

    // удаление вершины
    free(g->vertices[idx]);

    // удаление входящих рёбер
    for (size_t i = 0; i < g->vertex_count; i++) {
        if (i == idx) continue;
        Edge *curr = g->adj[i];
        Edge *prev = NULL;
        while (curr) {
            if (curr->to == idx) {
                Edge *toDel = curr;
                if (prev) prev->next = curr->next;
                else g->adj[i] = curr->next;
                curr = curr->next;
                free(toDel);
                // не обновляем prev - мы удалили текущий элемент
            } else {
                /** 
                 * если `curr->to > idx`, надо сдвинуть индекс на 1 вниз,
                 * чтобы не делать это ниже при смещении
                 * 
                 * актуализация ссылок 
                 */ 
                if (curr->to > idx) curr->to--;
                prev = curr;
                curr = curr->next;
            }
            
        }
    }

    // сдвиг вершин и списков смежности
    for (size_t i = idx; i < g->vertex_count - 1; i++) {
        g->vertices[i] = g->vertices[i + 1];
        g->adj[i] = g->adj[i + 1];
    }

    g->vertex_count--;

    return 0;
}

int removeEdge(Graph *g, const char *from, const char *to) {
    size_t fromIdx = vertexIdx(g, from);
    size_t toIdx = vertexIdx(g, to);
    if (fromIdx == INVALID_IDX || toIdx == INVALID_IDX) {
        puts("Vertexes not found");
        return -1;
    }

    Edge *list = g->adj[fromIdx];
    Edge *prev = NULL;
    while (list) {
        if (list->to == toIdx) {
            Edge *toDel = list;
            if (prev) prev->next = list->next;
            else g->adj[fromIdx] = list->next;
            free(toDel);
            return 0;
        }
        prev = list;
        list = list->next;
    }
    return -1;
}

/**
 * Search
 */

// DFS + visualization

void dfsUtil(Graph *g, size_t current, size_t target, int *visited, DFSPath *path, size_t step, int *found) {
    visited[current] = 1;

    if (current == target) {
        *found = 1;
        return;
    }

    for (Edge *e = g->adj[current]; e; e = e->next) {
        // если ещё не были в вершине, в которую идёт текущая грань
        if (!visited[e->to]) {
            // добавляем шаг
            Step *s = (Step *)malloc(sizeof(Step));
            s->from = current;
            s->to = e->to;
            s->step_number = step;
            s->next = path->head;
            path->head = s;
            path->count++;

            // рекурсивно идём "глубже"
            dfsUtil(g, e->to, target, visited, path, step + 1, found);

            if (*found) return;

            // если не нашёлся путь - удаляем шаг (откат)
            Step *toDel = path->head;
            path->head = path->head->next;
            free(toDel);
            path->count--;
        }
    }
}

DFSPath *dfsSearch(Graph *g, const char *from, const char *to) {
    size_t fromIdx = vertexIdx(g, from);
    size_t toIdx = vertexIdx(g, to);
    if (fromIdx == INVALID_IDX || toIdx == INVALID_IDX) return NULL;

    int *visited = (int *)calloc(g->vertex_count, sizeof(int));
    DFSPath *path = (DFSPath *)calloc(1, sizeof(DFSPath));

    int found = 0; // флаг для выхода
    dfsUtil(g, fromIdx, toIdx, visited, path, 1, &found);

    free(visited);
    if (!found) {
        free(path);
        return NULL;
    }
    return path;
}

void visualizeDFS(Graph *g, DFSPath *path, const char *sourceFile, const char *destFile, const char *color) {
    FILE *src = fopen(sourceFile, "r");
    FILE *dst = fopen(destFile, "w");
    if (!src || !dst) {
        perror("Cannot open source or destination file");
        if (src) fclose(src);
        if (dst) fclose(dst);
        return;
    }

    char line[512];
    int brace_written = 0;

    // Копируем исходный файл построчно
    while (fgets(line, sizeof(line), src)) {

        // Когда встретили конец графа, добавим свои рёбра перед }
        if (strchr(line, '}') && !brace_written) {
            brace_written = 1;

            Step *step = path->head;
            Step *prev = NULL;
            for (size_t i = 0; i < path->count; i++) {
                const char *from = g->vertices[step->from]->name;
                const char *to   = g->vertices[step->to]->name;
                size_t step_num = step->step_number;

                if (path->count) {
                    fprintf(dst, "  \"%s\" -> \"%s\" [color=%s, penwidth=2.5, label=\"%d\"];\n",
                            from, to, color, step_num);
                } else {
                    fprintf(dst, "  \"%s\" -> \"%s\" [color=%s, penwidth=2.5];\n",
                            from, to, color);
                }
                prev = step;
                step = step->next;
                if (prev) free(prev);
            }
            free(path);
        }
        fputs(line, dst);
    }
    fclose(src);
    fclose(dst);
}

// Bellman-Ford + visualization

ShortestPath *BellmanFord(Graph *g, const char *from, const char *to) {
    size_t n = g->vertex_count;

    double *dist = (double *)malloc(n * sizeof(double));
    if (!dist) return NULL;

    size_t *prev = (size_t *)malloc(n * sizeof(size_t));
    if (!prev) {
        free(dist);
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        dist[i] = INFINITY;
        prev[i] = INVALID_IDX;
    }

    size_t start = vertexIdx(g, from);
    size_t end = vertexIdx(g, to);
    if (start == INVALID_IDX || end == INVALID_IDX) return NULL;

    dist[start] = 0;

    for (size_t i = 0; i < g->vertex_count - 1; i++) {
        for (size_t u = 0; u < n; u++) {
            for (Edge *e = g->adj[u]; e; e = e->next) {
                size_t v = e->to;
                if (dist[u] < INFINITY && dist[u] + e->weight < dist[v]) {
                    dist[v] = dist[u] + e->weight;
                    prev[v] = u;
                }
            }
        }
    }

    // восстановление пути 
    if (dist[end] == INFINITY) { // путь не найдён
        free(dist);
        free(prev);
        return NULL;
    }

    size_t path[1024];
    size_t len = 0;
    for (size_t at = end; at != INVALID_IDX; at = prev[at])
        path[len++] = at;

    ShortestPath *sp = (ShortestPath *)malloc(sizeof(ShortestPath));
    if (!sp) {
        free(dist);
        free(prev);
        return NULL;
    }
    size_t *vertices = (size_t *)malloc(len * sizeof(size_t));
    if (!vertices) {
        free(dist), free(prev), free(sp);
        return NULL;
    }
    sp->vertices = vertices;
    sp->length = len;
    sp->total_weight = dist[end];

    for (size_t i = 0; i < len; i++) 
        sp->vertices[i] = path[len - i - 1]; // разворот
    
    free(dist);
    free(prev);
    return sp;
}

void visualizeShortestPath(Graph *g, ShortestPath *sp, const char *sourceFile, const char *destFile, const char *color) {
    FILE *src = fopen(sourceFile, "r");
    FILE *dst = fopen(destFile, "w");
    if (!src || !dst) {
        perror("Cannot open source or destination file");
        if (src) fclose(src);
        if (dst) fclose(dst);
        return;
    }

    char line[512];
    int brace_written = 0;

    while (fgets(line, sizeof(line), src)) {

        if (strchr(line, '}') && !brace_written) {
            brace_written = 1;

            for (size_t i = 0; i < sp->length - 1; i++) {
                const char *from = g->vertices[sp->vertices[i]]->name;
                const char *to   = g->vertices[sp->vertices[i + 1]]->name;

                fprintf(dst, "  \"%s\" -> \"%s\" [color=%s, penwidth=2.5];\n",
                        from, to, color);
            }
        }
        fputs(line, dst);
    }
    free(sp->vertices);
    free(sp);

    fclose(src);
    fclose(dst);
}

// Floyd-Warshall + visualization
/**
 * идея в том, чтобы вернуть матрицу расстояний, 
 * затем найти 3 пары вершин с кратчайшими путями, 
 * после вернуть 3 пути с помощью Беллмана-Форда
 */

double **allocateMatrixD(size_t n) {
    double **matrix = (double **)malloc(n * sizeof(double *));
    if (!matrix) return NULL;

    for (size_t i = 0; i < n; i++)
        matrix[i] = (double *)malloc(n * sizeof(double));
    return matrix;
}

size_t **allocateMatrixI(size_t n) {
    size_t **matrix = (size_t **)malloc(n * sizeof(size_t *));
    if (!matrix) return NULL;

    for (size_t i = 0; i < n; i++)
        matrix[i] = (size_t *)malloc(n * sizeof(size_t));
    return matrix;
}

void FloydWarshall(Graph *g, double ***distOut, size_t ***nextOut) {
    size_t n = g->vertex_count;
    double **dist = allocateMatrixD(n);
    size_t **next = allocateMatrixI(n);

    // инициализация матриц
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            dist[i][j] = (i == j) ? 0 : INFINITY;
            next[i][j] = INVALID_IDX;
        }

        for (Edge *e = g->adj[i]; e; e = e->next) {
            size_t j = e->to;
            dist[i][j] = e->weight;
            next[i][j] = j;
        }
    }

    // основной цикл
    for (size_t k = 0; k < n; k++) {
        for (size_t i = 0; i < n; i++) {
            for (size_t j = 0; j < n; j++) {
                if (dist[i][k] < INFINITY && dist[k][j] < INFINITY &&
                    dist[i][j] > dist[i][k] + dist[k][j]) {

                        dist[i][j] = dist[i][k] + dist[k][j];
                        next[i][j] = next[i][k];
                    }
            }
        }
    }
    *distOut = dist;
    *nextOut = next;
}

typedef struct {
    size_t from;
    size_t to;
    double distance;
} Pair;

void findThreeClosestPairs(double **dist, size_t n, Pair *out) {
    // инициализация
    for (int i = 0; i < 3; i++) {
        out[i].distance = INFINITY;
        out[i].from = out[i].to = INVALID_IDX;
    }

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i != j && dist[i][j] < INFINITY) {
                // вставка на нужную позицию вручную
                for (size_t k = 0; k < 3; k++) {
                    if (dist[i][j] < out[k].distance) {
                        // сдвиг вправо
                        for (size_t l = 2; l > k; l--) out[l] = out[l - 1];
                        out[k].from = i;
                        out[k].to = j;
                        out[k].distance = dist[i][j];
                        break;
                    }
                }
            }
        }
    }

}

size_t reconstructPath(size_t **next, size_t u, size_t v, size_t *path) {
    if (next[u][v] == INVALID_IDX) return 0;

    size_t len = 0;
    path[len++] = u;
    while (u != v) {
        u = next[u][v];
        if (u == INVALID_IDX) return 0; // no way
        path[len++] = u;
    }
    return len;
}

ShortestPath **threeShortestPaths(Graph *g) {
    double **dist;
    size_t **next;
    Pair pairs[3];

    FloydWarshall(g, &dist, &next);
    findThreeClosestPairs(dist, g->vertex_count, pairs);

    ShortestPath **result = (ShortestPath **)malloc(3 * sizeof(ShortestPath *));
    for (int i = 0; i < 3; i++) {
        size_t path[1024];
        size_t len = reconstructPath(next, pairs[i].from, pairs[i].to, path);
        if (len == 0) {
            result[i] = NULL;
            continue;
        }
        
        result[i] = (ShortestPath *)malloc(sizeof(ShortestPath));
        result[i]->vertices = (size_t *)malloc(len * sizeof(size_t));
        result[i]->length = len;
        result[i]->total_weight = pairs[i].distance;
        for (size_t j = 0; j < len; j++) {
            result[i]->vertices[j] = path[j];
        }
    }

    return result;
}

/**
 *  Save, Load, Init
 */ 

void saveGraph(Graph *g, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    fwrite(&g->vertex_count, sizeof(int), 1, f);

    for (size_t i = 0; i < g->vertex_count; i++) {
        Vertex *v = g->vertices[i];
        size_t name_len = strlen(v->name);
        fwrite(&name_len, sizeof(size_t), 1, f);
        fwrite(v->name, sizeof(char), name_len, f);
        fwrite(&v->x, sizeof(double), 1, f);
        fwrite(&v->y, sizeof(double), 1, f);
    }

    int edge_count = 0;
    for (size_t i = 0; i < g->vertex_count; i++) {
        for (Edge *e = g->adj[i]; e; e = e->next) edge_count++;
    }

    fwrite(&edge_count, sizeof(int), 1, f);

    for (size_t i = 0; i < g->vertex_count; i++) {
        Vertex *from = g->vertices[i];
        for (Edge *e = g->adj[i]; e; e = e->next) {
            Vertex *to = g->vertices[e->to];
            size_t from_len = strlen(from->name);
            size_t to_len = strlen(to->name);
            fwrite(&from_len, sizeof(size_t), 1, f);
            fwrite(from->name, sizeof(char), from_len, f);
            fwrite(&to_len, sizeof(size_t), 1, f);
            fwrite(to->name, sizeof(char), to_len, f);
        }
    }

    fclose(f);
}

int loadGraph(Graph *g, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    // пустой ли файл
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size == 0) {
        rewind(f);
        return -1; // файл пустой, ничего не грузим
    }
    rewind(f); // возвращаемся в начало

    int vertex_count;
    fread(&vertex_count, sizeof(int), 1, f);

    for (int i = 0; i < vertex_count; i++) {
        size_t name_len;
        fread(&name_len, sizeof(size_t), 1, f);
        char *name = (char *)malloc(name_len + 1);
        fread(name, sizeof(char), name_len, f);
        name[name_len] = '\0';

        double x, y;
        fread(&x, sizeof(double), 1, f);
        fread(&y, sizeof(double), 1, f);

        if (!addVertex(g, name, x, y)) printf("Load error\n");
        free(name);
    }

    int edge_count;
    fread(&edge_count, sizeof(int), 1, f);

    for (int i = 0; i < edge_count; i++) {
        size_t from_len, to_len;

        fread(&from_len, sizeof(size_t), 1, f);
        char *from = (char *)malloc(from_len + 1);
        fread(from, sizeof(char), from_len, f);
        from[from_len] = '\0';

        fread(&to_len, sizeof(size_t), 1, f);
        char *to = (char *)malloc(to_len + 1);
        fread(to, sizeof(char), to_len, f);
        to[to_len] = '\0';

        if (!addEdge(g, from, to)) printf("Load error\n");
        free(from);
        free(to);
    }

    fclose(f);
    return 0;
}

/**
 * Generate graph (empty file case)
 */ 

void generateRandomGraph(Graph *g, size_t vertex_count, int edge_density_percent) {
    //srand(time(NULL));

    if (!g) {
        g = initGraph(vertex_count);
    }

    for (size_t i = 0; i < vertex_count; i++) {
        char name[NAME_LEN];
        snprintf(name, NAME_LEN, "V%d", i); // V0, V1, ...

        double x = rand() % 101;
        double y = rand() % 101;

        addVertex(g, name, x, y);
    }

    // добавление рёбер с вероятностью density%
    for (size_t i = 0; i < vertex_count; i++) {
        for (size_t j = 0; j < vertex_count; j++) {
            if (i != j) {

                int weirdCoin = rand() % 100;
                if (weirdCoin < edge_density_percent) {
                    Vertex *from = g->vertices[i];
                    Vertex *to = g->vertices[j];
                    addEdge(g, from->name, to->name);
                }
            }
        }
    }
}

/**
 * Smart input
 * 
 * конечно, нужно убрать в отдельный header
 */

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
int validateInt(const char *input, void *result) {
    char* endptr;
    int val = strtol(input, &endptr, 10);
    if (*input == '\0' || *endptr != '\0') return 0;

    *(int*)result = (int)val;
    return 1;
}

int validateUInt(const char *input, void *result) {
    char* endptr;
    unsigned long val = strtoul(input, &endptr, 10);
    if (*input == '\0' || *endptr != '\0') return 0;

    *(size_t*)result = (size_t)val;
    return 1;
}

// Валидация double
int validateDouble(const char *input, void *result) {
    char* endptr;
    double val = strtod(input, &endptr);
    if (*input == '\0' || *endptr != '\0') return 0;

    *(double*)result = val;
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
int inputInt(int *value, const char *prompt) {
    return inputWithValidation(prompt, validateInt, value);
}

int inputUInt(size_t *value, const char *prompt) {
    return inputWithValidation(prompt, validateUInt, value);
}

int inputDouble(double *value, const char* prompt) {
    return inputWithValidation(prompt, validateDouble, value);
}

int inputString(char* value, size_t max_len, const char* prompt) {
    char buffer[MAX_INPUT_LEN];
    if (!inputWithValidation(prompt, validateString, buffer))
        return 0;

    strncpy(value, buffer, max_len - 1);
    value[max_len - 1] = '\0';
    return 1;
}

/** 
 * Menu-invocated functions
 */

void searchDialog(Graph *g) {
    printf("\n=== Search vertex ===\n");
    printf("1. Depth-first search\n");
    printf("2. Shortest path (Bellman-Ford)\n");
    printf("3. Shortest paths (Floyd-Warshall)\n");
    printf("0. Exit\n");

    char from[NAME_LEN];
    char to[NAME_LEN];

    size_t choice;
    int inpStrRes1, inpStrRes2;
    
    int choiseRes = inputUInt(&choice, "Select: ");
    if (!choiseRes) {
        printf("Input error\n");
    }

    if (choice == 1 || choice == 2) {
        inpStrRes1 = inputString(from, sizeof(from), "Enter the starting vertex: ");
        inpStrRes2 = inputString(to, sizeof(to), "Enter the end vertex: ");
        if (!inpStrRes1 || !inpStrRes2) {
            printf("Invalid input\n");
            return;
        }
    }

    // нужен исходный .dat файл для визуализации путей
    generateGraphviz(g, "graph.dot");
    system("dot -Tpng graph.dot -o graph.png"); 

    const char *sourceFile = "graph.dot";

    if (choice == 1) {
        DFSPath *path = dfsSearch(g, from, to);

        if (!path || !path->head) {
            printf("There is no way\n");
            free(path);
            return;
        }

        visualizeDFS(g, path, sourceFile, "DFS_path.dot", "red");
        system("dot -Tpng DFS_path.dot -o DFS_path.png"); 
        puts("Graph saved in DFS_path.png");
        return; 
    } else if (choice == 2) {
        ShortestPath *sp = BellmanFord(g, from, to);

        if (!sp || !sp->vertices) {
            printf("There is no way\n");
            // пути нет -> len = 0 -> malloc ничего не выделяет -> free(sp->vertices); не нужно
            free(sp);
            return;
        }

        visualizeShortestPath(g, sp, sourceFile, "Bellman-Ford_path.dot", "yellow");
        system("dot -Tpng Bellman-Ford_path.dot -o Bellman-Ford_path.png"); 
        puts("Graph saved in Bellman-Ford_path.png");
        return;
    } else if (choice == 3) {
        // проверка на наличие как минимум трёх связей
        int edge_count = 0;
        for (size_t i = 0; i < g->vertex_count; i++) {
            Edge *curr = g->adj[i];
            while (curr) {
                edge_count++;
                if (edge_count == 3) break;
                curr = curr->next;
            }
            if (edge_count == 3) break;
        }
        if (edge_count < 3) {
            printf("Less than 3 edges\n");
            return;
        }

        ShortestPath **sps = threeShortestPaths(g);
        char colors[3][8] = {"blue", "red", "yellow"};

        const char *input = sourceFile;
        char *tmpFile1 = "tmp_fw_1.dot";
        char *tmpFile2 = "tmp_fw_2.dot";
        
        for (int i = 0; i < 3; i++) {
            const char *output;

            if (i == 0) {
                output = tmpFile1;
            } else if (i == 1) {
                input = tmpFile1;
                output = tmpFile2;
            } else {
                input = tmpFile2;
                output = "Floyd-Warshall_paths.dot";
            }

            visualizeShortestPath(g, sps[i], input, output, colors[i]);
        } 
        free(sps); // остальное освобождается в visualizeShortestPath
    
        remove(tmpFile1);
        remove(tmpFile2);

        system("dot -Tpng Floyd-Warshall_paths.dot -o Floyd-Warshall_paths.png"); 
        puts("Graph saved in Floyd-Warshall_paths.png");
        return;
    } else if (choice == 0) {
        return;
    } else {
        printf("Invalid choice!\n"); 
        return;
    }
}

void addDialog(Graph *g, size_t choice) {
    char str1[NAME_LEN], str2[NAME_LEN];
    double num1, num2;

    switch (choice) {
    case 1:
        printf("\n=== Add New Vertex ===\n");

        if (!inputDouble(&num1, "Enter X: ") || !inputDouble(&num2, "Enter Y: ")) {
            printf("Invalid numbers\n");
            return;
        }
        if (!inputString(str1, sizeof(str1), "Enter vertex name: ")) {
            printf("Failed to read name\n");
            return;
        }
        if (addVertex(g, str1, num1, num2) == -1) {
            printf("Failed to add Vertex\n");
            return;
        }
        printf("Vertex added successfully!\n");
        return; 
    case 2: 
        printf("\n=== Add New Edge ===\n");

        if (!inputString(str1, sizeof(str1), "Enter starting vertex name: ")) {
            printf("Failed to read starting vertex name\n");
            return;
        }
        if (!inputString(str2, sizeof(str2), "Enter end vertex name: ")) {
            printf("Failed to read end vertex name\n");
            return;
        }
        if (!addEdge(g, str1, str2)) {
            printf("Failed to add Edge\n");
            return;
        }
        printf("Edge added successfully!\n");
        return;
    }
}

void deleteDialog(Graph *g, size_t choice) {
    char str1[NAME_LEN], str2[NAME_LEN];

    switch (choice) {
    case 3:
        printf("\n=== Delete Vertex ===\n");

        if (!inputString(str1, sizeof(str1), "Enter vertex name: ")) {
            printf("Failed to read name\n");
            return;
        }
        if (removeVertex(g, str1) == -1) {
            printf("Failed to delete Vertex\n");
            return;
        }
        printf("Vertex deleted successfully!\n");
        return; 
    case 4: 
        printf("\n=== Delete Edge ===\n");

        if (!inputString(str1, sizeof(str1), "Enter starting vertex name: ")) {
            printf("Failed to read starting vertex name\n");
            return;
        }
        if (!inputString(str2, sizeof(str2), "Enter end vertex name: ")) {
            printf("Failed to read end vertex name\n");
            return;
        }
        if (removeEdge(g, str1, str2) == -1) {
            printf("Failed to delete Edge\n");
            return;
        }
        printf("Edge deleted successfully!\n");
        return;
    }
}

void exportToDot(Graph *g, FILE *file) {
    if (!g) return;

    // вершины
    for (size_t i = 0; i < g->vertex_count; i++) {
        Vertex *v = g->vertices[i];
        if (v) {
            fprintf(file, "  \"%s\" [label=\"%s\\n(%.1lf, %.1lf)\"];\n", 
                v->name, v->name, v->x, v->y);
        }
    }

    // рёбра
    for (size_t i = 0; i < g->vertex_count; i++) {
        Edge *edge = g->adj[i];
        Vertex *from = g->vertices[i];
        while (edge) {
            Vertex *to = g->vertices[edge->to];
            if (from && to) {
                fprintf(file, "  \"%s\" -> \"%s\" [label=\"%.2lf\"];\n", 
                    from->name, to->name, edge->weight);
            }
            edge = edge->next;
        }
    }
}

void generateGraphviz(Graph *g, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Cannot open dot file");
        return;
    }

    fprintf(file, "digraph G {\n");  // digraph для направленного графа
    fprintf(file, "  node [shape=ellipse, style=filled, fillcolor=lightblue];\n");

    exportToDot(g, file);

    fprintf(file, "}\n");
    fclose(file);
}

void printGraph(Graph *g) {
    generateGraphviz(g, "graph.dot");
    system("dot -Tpng graph.dot -o graph.png"); 
    puts("Graph saved in graph.png");
}

void printMainMenu() {
    printf("\n=== Main Menu ===\n");
    printf("1. Add vertex\n");
    printf("2. Add edge\n");
    printf("3. Delete vertex\n");
    printf("4. Delete edge\n");
    printf("5. Search\n");
    printf("6. Print graph\n");
    printf("0. Exit\n");
}

int main() {
    Graph *g = initGraph(10);
    if (loadGraph(g, "graph.bin") == -1) {
        size_t graphSize = MAX_RANDOM_GRAPH_SIZE + 1;
        int edge_density = 0;
        while (graphSize > MAX_RANDOM_GRAPH_SIZE ||
            (edge_density < 0) || (edge_density > 100)) {
            if (!inputUInt(&graphSize, "Enter graph size: ")) {
                printf("Input error\n");
            }
            if (!inputInt(&edge_density, "Enter edge density percent [0; 100]: ")) {
                printf("Input error\n");
            }
        }
        generateRandomGraph(g, graphSize, edge_density);
        saveGraph(g, "graph.bin");
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
            case 1: addDialog(g, choice); break;
            case 2: addDialog(g, choice); break;
            case 3: deleteDialog(g, choice); break;
            case 4: deleteDialog(g, choice); break;
            case 5: searchDialog(g); break;
            case 6: printGraph(g); break;
            case 0: running = 0; puts("Exiting..."); break;
            default: printf("Invalid choice!\n");
        }
    }
    saveGraph(g, "graph.bin");
    freeGraph(g);
    
    return 0;
}
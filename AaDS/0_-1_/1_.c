#include <stdio.h>
#include <stdlib.h>

void inputMatrix(int ***matrix, int *rowNum, int **rowSizes)
{
    scanf("%d", rowNum);
    *rowSizes = (int *)malloc(*rowNum * sizeof(int));
    *matrix = (int **)malloc(*rowNum * sizeof(int *)); // создание массива указателей на строки

    for (int i = 0; i < *rowNum; i ++ ) {
        scanf("%d", &(*rowSizes)[i]);
        // инициализация значения под указателем в массиве указателей массивом
        (*matrix)[i] = (int *)malloc((*rowSizes)[i] * sizeof(int)); 
        for (int j = 0; j < (*rowSizes)[i]; j++) {
            scanf("%d", &(*matrix)[i][j]);
        }
    }
}

int **transformMatrix(int **matrix, int rowNum, int *rowSizes, int **newRowSizes)
{
    int **newMatrix = (int **)malloc(rowNum * sizeof(int *)); // создание массива указателей на строки
    *newRowSizes = (int *)malloc(rowNum * sizeof(int));

    for (int i = 0; i < rowNum; i++) {
        int minIndex = 0;
        for (int j = 1; j < rowSizes[i]; j++) {
            if (matrix[i][j] < matrix[i][minIndex]){
                minIndex = j;
            }
        }
        (*newRowSizes)[i] = rowSizes[i] - minIndex;
        newMatrix[i] = (int *)malloc((*newRowSizes)[i] * sizeof(int));
        for (int j = 0; j < (*newRowSizes)[i]; j++) {
            newMatrix[i][j] = matrix[i][minIndex + j];
        }
    }
    return newMatrix;
}

void printMatrix(int **matrix, int rowNum, int *rowSizes, const char *title)
{
    printf("%s\n", title);
    for (int i = 0; i < rowNum; i++) {
        for (int j = 0; j < rowSizes[i]; j++) {
            printf("%d ", matrix[i][j]);
        }
        putchar('\n');
    }
}

void freeMatrix(int **matrix, int rowNum)
{
    for (int i = 0; i < rowNum; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

int main()
{
    int **matrix, *rowSizes, rowNum;

    inputMatrix(&matrix, &rowNum, &rowSizes);
    printMatrix(matrix, rowNum, rowSizes, "Original matrix:");
    
    int **newMatrix, *newRowSizes;
    newMatrix = transformMatrix(matrix, rowNum, rowSizes, &newRowSizes);
    printMatrix(newMatrix, rowNum, newRowSizes, "Transformed matrix:");

    freeMatrix(matrix, rowNum);
    freeMatrix(newMatrix, rowNum);
    free(rowSizes);
    free(newRowSizes);

    return 0;
}
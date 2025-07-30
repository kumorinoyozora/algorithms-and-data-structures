#include <stdio.h>
#include <stdlib.h>


int digit_count(int num)
{
    int digit_num = 0;
    while (num != 0) {
        num = num / 10;
        digit_num++;
    }
    return digit_num;
};

int prod_sum(int num)
{
    int even_sum = 0, odd_sum = 0;
    while (num != 0) {
        int digit = num % 10;
        if (digit % 2 == 0) // is even
            even_sum += digit;
        else // odd 
            odd_sum += digit;
        num = num / 10;
    }
    return even_sum * odd_sum;
};

void sort(int * array, size_t len, int (*crit1) (int), int (*crit2) (int))
{
    for(int i = 0; i < len - 1; i++){
        int left = i;
        for (int j = i + 1; j < len; j++) {
            if (crit1(array[left]) > crit1(array[j])) {
                left = j;
            }
            else if (crit1(array[left]) == crit1(array[j])) {
                if (crit2(array[left]) < crit2(array[j])) {
                    left = j;
                }
            }
            
        }
        if (left != i) {
            array[left] = array[left] ^ array[i]; // циганские фокусы
            array[i] = array[left] ^ array[i];
            array[left] = array[left] ^ array[i];
        }
    }
};

int main(void)
{
    int n = 0;
    puts("\nEnter the number of numbers:");
    if (scanf("%d%c", &n) != 1 || n <= 0) 
        return 0;

    int * array = malloc(n * sizeof(int));

    puts("\nEnter numbers:");
    for (int i = 0; i < n; i++) 
        scanf("%d", array+i);

    puts("\nOriginal sequence:");
    for (int i = 0; i < n; i++) {
        printf("%d ", *(array+i));
    }

    sort(array, n, digit_count, prod_sum);

    puts("\nNew sequence:");
    for (int i = 0; i < n; i++) {
        printf("%d ", *(array+i));
    }
    puts("\n");

    free(array);
    return 0;
}
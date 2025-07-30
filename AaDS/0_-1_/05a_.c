#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void extractBracketedText(const char *input, char *output)
{
    const char *start = input;
    int wordAdded = 0;

    while (*start) {
        start = strchr(start, '(');
        if (start == NULL)
            return;
        start++; // сдвигаемся с '('
        while (*start == ' ') // чтобы избежать пробела между '(' и словом
            start++;

        const char * end = strchr(start, ')');
        if (end == NULL) 
            end = start + strlen(start) / sizeof(char); // это работает
        
        if (start == end) continue; // tсли между '(' и ')' ничего нет, пропускаем этот фрагмент

        while (start < end) {
            if ((*start == ' ' && (*(start + 1) == ' ' || *(start + 1) == ')'))) // внутри скобок всегда из n пробелов остаётся один между слов
                start++;
            else {
                *output++ = *start++;
                wordAdded = 1;
            }
        }
        if (wordAdded) {
            *output++ = ' '; // добавляем пробел после последнего слова скобок
            wordAdded = 0;
        }
    }
    output--; // сдвигаем указатель на последний добавленный пробел
    if (*(output - 1) == ' ') // для случая, когда нет ')', но перед '\n' пробелы
        output--; 
    *output++ = '\0';
}

void stripString(char *str, int max_len)
{
    int count = 0;
    while(*str++ != '\0' && count++ < max_len) {
        if(*str == '\n') {
            *str = '\0';
            break;
        }
    }
}

int main(void)
{
    char input[128];

    while (scanf(" %[^\n]", input) != EOF) {
        stripString(input, sizeof(input));

        char *output = (char *)calloc(128, sizeof(char));
        if (!output) {
            printf("Memory allocation error");
            return 1;
        }

        extractBracketedText(input, output);
        
        printf("%s\n", output);
        
        free(output);
    }
    return 0;
}

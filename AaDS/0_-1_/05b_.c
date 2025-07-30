#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 80

typedef struct note {
    char ch;
    struct note * next;
} note;

void append(note **head, char ch)
{
    note *newNote = (note *)malloc(sizeof(note));
    if (newNote == NULL) {
        printf("Malloc error");
        return;
    }
    newNote->ch = ch;
    newNote->next = NULL;

    if (*head == NULL) {
        *head = newNote;
    } else { 
        note *temp = *head;
        while (temp->next) // ищем в цикле посследний элемент с next = NULL для записи в него
            temp = temp->next;
        temp->next = newNote;
    }
}

note *stringToList(const char *str)
{
    note *head = NULL;
    while (*str) {
        append(&head, *str++);
    }
    return head;
}

void removeExtraSpaces(note **head)
{
    note *current = *head, *prev = NULL;
    int inWord = 0;

    while (current) {
        if (current->ch == ' ' || current->ch == '\t') {
            if (!inWord) {
                note *toDelete = current;
                current = current->next;
                if (prev)
                    prev->next = current;
                else // если удаляем пробелы из начала, где prev = NULL
                    *head = current;
                free(toDelete);
            } else { // таким образом при встрече пробела ('\t'?) после слова мы оставляем только его
                inWord = 0;
                prev = current;
                current = current->next;
            }
        } else {
            inWord = 1;
            prev = current;
            current = current->next;
        }
    }
}

void justify(note *head)
{
    char words[WIDTH][WIDTH];
    int wordLenghts[WIDTH];
    int wordCount = 0, charCount = 0;
    note *current = head;

    while (current) {
        if (current->ch != ' ' && current->ch != '\t') {
            words[wordCount][charCount++] = current->ch;
        } else if (charCount > 0) {
            words[wordCount][charCount] = '\0';
            wordLenghts[wordCount++] = charCount;
            charCount = 0;
        }
        current = current->next;
    }
    if (charCount > 0) {
        words[wordCount][charCount] = '\0';
        wordLenghts[wordCount++] = charCount;
    }

    int i = 0;
    while (i < wordCount) {
        int lineLength = wordLenghts[i], wordsInLine = 1;
        while (i + wordsInLine < wordCount && (lineLength + 1 + wordLenghts[i + wordsInLine] <= WIDTH)) {
            lineLength += 1 + wordLenghts[i + wordsInLine];
            wordsInLine++;
        }

        int gaps = (wordsInLine > 1) ? wordsInLine - 1 : 1;
        int spaces = WIDTH - (lineLength - gaps);
        int baseSpaces = spaces / gaps;
        int extraSpaces = spaces % gaps;

        for (int j = 0; j < wordsInLine; j++) {
            printf("%s", words[i + j]);
            if (j < gaps) {
                for (int s = 0; s < baseSpaces; s++)
                    putchar(' ');
                if (extraSpaces > 0) {
                    putchar(' ');
                    extraSpaces--;
                }
            }
        }
        putchar('\n');
        i += wordsInLine; 
    }
}

void printList(note *head)
{
    while (head) {
        printf("%c", head->ch);
        head = head->next;
    }
    printf("\n");
}

void empty(note *head)
{
    while (head) {
        note *toDelete = head;
        head = head->next;
        free(toDelete);
    }
}

int main()
{
    char buffer[512];

    while (scanf(" %[^\n]", buffer) != EOF) {
        note *head = stringToList(buffer);
        removeExtraSpaces(&head);
        printList(head);
        justify(head);
        empty(head);
    }

    return 0;
}

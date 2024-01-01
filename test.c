#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Init key by random pick 1 line from key.txt
void init_key(char question[] ,char key[])
{
    FILE *f = fopen("question.txt", "r");
    if (f == NULL)
    {
        printf("Error opening key.txt file!\n");
        exit(1);
    }
    // Get number of line in file
    int num_line = 0;
    char c;
    for (c = getc(f); c != EOF; c = getc(f))
        if (c == '\n')
            num_line = num_line + 1;

    // Random pick 1 line
    int random_line = rand() % num_line;
    int i = 0;

    // Move f pointer to head of file
    rewind(f);

    // Move pointer to the key
    while (i < random_line)
    {
        c = getc(f);
        if (c == '\n')
            i = i + 1;
    }

    // Get key
    fscanf(f, "%[^:]: %[^\n]", question, key);
    fclose(f);

    return;
}

// Init crossword by binding key
void init_crossword(char key[], char crossword[])
{
    int i;
    for (i = 0; i < strlen(key); i++)
    {
        if (key[i] == ' ')
            crossword[i] = ' ';
        else
            crossword[i] = '*';
    }
    crossword[i] = '\0';
}


int main(){
    char question[200];
    char key[50];
    char crossword[50];

    init_key(question, key);
    printf("Question: %s, %d\n", question, strlen(question));
    printf("Key: %s, %d\n", key, strlen(key));
    init_crossword(key, crossword);
    printf("Crossword: %s", crossword);
}
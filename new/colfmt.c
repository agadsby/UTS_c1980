#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINES 1000
#define MAXLEN   1024

char *lines[MAXLINES];

int  getlines();
void printcols();

main(argc, argv)
int argc;
char *argv[];
{
    int ncols = 5;
    int width = 15;
    int down = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            ncols = atoi(argv[++i]);
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
            width = atoi(argv[++i]);
        else if (strcmp(argv[i], "-d") == 0)
            down = 1;
        else {
            fprintf(stderr, "usage: %s [-n cols] [-c width] [-d]\n", argv[0]);
            exit(1);
        }
    }

    printcols(getlines(), ncols, width, down);
    return 0;
}

int getlines()
{
    char buf[MAXLEN];
    int n = 0;

    while (fgets(buf, MAXLEN, stdin) != NULL && n < MAXLINES) {
        buf[strcspn(buf, "\n")] = '\0';
        lines[n] = malloc(strlen(buf) + 1);
        strcpy(lines[n], buf);
        n++;
    }
    return n;
}

void printcols(nlines, ncols, width, down)
int nlines, ncols, width, down;
{
    int rows, r, c, i;

    if (nlines == 0)
        return;

    rows = (nlines + ncols - 1) / ncols;

    for (r = 0; r < rows; r++) {
        for (c = 0; c < ncols; c++) {

            if (down)
                i = c * rows + r;
            else
                i = r * ncols + c;

            if (i < nlines)
                printf("%-*.*s", width, width, lines[i]);
        }
        putchar('\n');
    }
}

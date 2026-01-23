#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define BYTES_PER_LINE 16

int main(int argc, char *argv[]) {
    int show_o = 0, show_x = 0, show_c = 0;
    FILE *fp = stdin;
    unsigned char buf[BYTES_PER_LINE];
    size_t n;
    unsigned long offset = 0;

    /* Parse flags */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'o')) show_o = 1;
            if (strchr(argv[i], 'x')) show_x = 1;
            if (strchr(argv[i], 'c')) show_c = 1;
        } else {
            fp = fopen(argv[i], "rb");
            if (!fp) {
                perror("fopen");
                return 1;
            }
        }
    }

    /* Default format */
    if (!show_o && !show_x && !show_c)
        show_o = 1;

    while ((n = fread(buf, 1, BYTES_PER_LINE, fp)) > 0) {


        if (show_o) {
            printf(" 0%07o ", offset);
            for (size_t i = 0; i < n; i++)
                printf("%03o ", buf[i]);
            printf("\n");
        }

        if (show_x) {
            printf("0x%07x ", offset);
            for (size_t i = 0; i < n; i++)
                printf(" %02x ", buf[i]);
            printf("\n");
        }

        if (show_c) {
            printf(" %8d ", offset);
            for (size_t i = 0; i < n; i++)
                printf("  %c ", isprint(buf[i]) ? buf[i] : '.');
            printf("\n");
        }
        offset += n;
    }

    if (fp != stdin)
        fclose(fp);

    return 0;
}

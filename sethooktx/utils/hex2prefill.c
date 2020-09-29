#include <stdio.h>
#include <string.h>

/**
 * Simple program to absorb hex data with lines ending in // comments
 * and turn them into initalization data for C / C++
 * 
 * Example Input:
 * DEADBEEF // I like to eat steak
 * CAFED00D // At cafes
 *
 * Output:
 **/
// unsigned char buf_out[] = {
//	/* [ 000 ==> ] */ 0xDE, 0xAD, 0xBE, 0xEF, /* I like to eat steak */ \
//	/* [ 004 ==> ] */ 0xCA, 0xFE, 0xD0, 0x0D, /* At cafes */ \
//    }; /* total length = 8 */

int main() {
    
    char buf[1024];

    int counter = 0;
    char last = 0;

    int bytes_on_line = 0;

    printf("unsigned char buf_out[] = {\n");
    while (fgets(buf, 1023, stdin) != NULL) {
        char* comment = strstr(buf, "//");
        
        if (buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = '\0';

        if (comment) {
            *comment = '\0';
        }


        for (char* x = buf; *x; ++x) {
            if (*x >= '0' && *x <= '9' || *x >= 'A' && *x <= 'F' || *x >= 'a' && *x <= 'f') {
                if (counter % 2 == 0) 
                    last = *x;
                else {
                    char tap[32];
                    sprintf(tap, "/* [ %03d ==> ] */ ", (counter-1)/2);
                    printf("%s%s0x%c%c",
                        ( counter == 1 ? "\t" :
                          bytes_on_line > 1 ? ( feof(stdin) ? "" : ", ") : "\t" ), (bytes_on_line <= 1 ? tap : ""), last, *x);
                }
                ++counter;
                ++bytes_on_line;
            }

            if (comment && x + 1 == comment) {
                printf("%s /*%s%s */ \\\n", (feof(stdin) ? "" : ","), *(comment + 2) == ' ' ? "" : " ", comment + 2);
                bytes_on_line = 0;
            }
        }
    }
    printf("\n}; /* total length = %d */\n", (counter)/2);

    return 0;

}

/*
 * make_calculus
 *
 * This program reads the contents of the binary WDSP file "calculus"
 * and dumps the data as two arrays of floating-point numbers
 *
 * The output is intended to be part of the file "calculus.c" which
 * initializes these arrays (static data) for use with "memcpy"
 * in emnr.c.
 *
 * Should the WDSP file "calculus" be changed, "calculus.c" should
 * be re-generated using this program.
 *
 * return values of main()
 *
 *  0  all OK
 * -1  sizeof(float) is not 8
 * -2  error opening file "calculus"
 * -3  read error
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    int fd;
    int i,j;
    float d;

    if (sizeof(float) != 8) {
        printf("Data type DOUBLE is not 8-byte. Please check!\n");
        return -1;
    }
    fd=open ("calculus", O_RDONLY);
    if (fd < 0) {
        printf("Could not open file 'calculus'\n");
        return -2;
    }

    for (j=0; j<2; j++) {
        switch (j) {
            case 0:
            printf("float GG[241*241]={\n");
            break;
            case 1:
            printf("float GGS[241*241]={\n");
            break;
        }
        for (i=0; i< 241*241; i++) {
            if (read(fd, &d, 8) != 8) {
                printf("READ ERROR\n");
                return -3;
            }
            if (i == 241*241 -1) {
                printf("%30.25f};\n", d);
            } else {
                printf("%30.25f,\n", d);
            }
        }
        return 0;
    }
}

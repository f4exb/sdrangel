/*
    The purpose of this file is to extract interfaces from the WDSP source code.
    The interfaces have the following form:

    PORT blabla
    firstline
    secondline
    {

    where there may be an arbitrary number of lines between the line
    containing "PORT" and the line starting with "{". This has to be
    converted to

    extern blabla firstline
    secondline;

    That is, the first line is prepended by "extern", and the last line is closed
    with a semicolon. Comments starting with '//' are omitted, and lines starting
    with '//' are ignored.
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void trimm(char *line, size_t maxlen);

int main(int argc, char **argv)
{
    FILE *infile;
    int i;
    int first_in_file;
    int first_in_decl;
    char line[1000];
    size_t linesize=999;
    char *buffer=line;

    for (i=1; i<argc; i++) {
        infile=fopen(argv[i],"r");
        if (infile == NULL) continue;
        first_in_file=1;
        for (;;) {
            if (getline(&buffer, &linesize, infile) < 0) break;
            trimm(line, linesize);
            if (strncmp(line,"PORT", 4) != 0) continue;
            // found an interface
            if (first_in_file) {
                printf("\n//\n// Interfaces from %s\n//\n\n", argv[i]);
                first_in_file=0;
            }
            if (strlen(line) >4) {
                printf("extern %s ", line+4);
            } else {
                printf("extern ");
            }
            first_in_decl=1;

            for (;;) {
                if (getline(&buffer, &linesize, infile) < 0) {
                    fprintf(stderr,"! Found a PORT but found EOF while scanning interface.\n");
                    return 8;
                }
                trimm(line, linesize);
                if (line[0] == 0) continue;
                if (line[0] == '{') {
                    printf(";\n");
                    break;
                } else {
                    if (first_in_decl) {
                        printf("%s", line);
                        first_in_decl=0;
                    } else {
                        printf("\n%s", line);
                    }
                }
            }
        }
        fclose(infile);
    }
    return 0;
}

void trimm(char *line, size_t maxlen) {
    int len;

    //
    // Remove comments starting with '//'
    //
    len=strnlen(line,maxlen);
    for (int i=0; i< len-1; i++) {
        if (line[i] == '/' && line[i+1] == '/') line[i]=0;
    }

    //
    // Remove trailing white space and newlines
    //
    len=strnlen(line,maxlen);
    line[len--]=0;
    while (len >= 0 && (line[len] == ' ' || line[len] == '\t' || line[len]== '\n')) line[len--]=0;
}

#ifndef parser_H
#define parser_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
        char **subStrings;
        int size;
} parser;

parser *parseString(const char *buffer);

void print_parser(parser *p);

void free_parser(parser *p);
#endif
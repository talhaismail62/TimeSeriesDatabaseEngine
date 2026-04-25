#include "parsor.h"

parser* parseString(const char *buffer) {
        parser *p = (parser*)malloc(sizeof(parser));
        
        if(!p) {
                free(p);
                return NULL;
        }

        p->subStrings = NULL;
        p->size = 0;

        char *temp = strdup(buffer);
        if (!temp) {
                free(p);
                return NULL;
        }

        char *subStrings = strtok(temp, " \r\n");

        while (subStrings != NULL) {
                char **newArr = (char **)realloc(p->subStrings, (p->size + 1) * sizeof(char *));
                if (!newArr) {
                free(temp);
                for (int i = 0; i < p->size; i++) {
                        free(p->subStrings[i]);
                }
                free(p->subStrings);
                free(p);
                return NULL;
                }

                p->subStrings = newArr;
                p->subStrings[p->size] = strdup(subStrings);
                if (!p->subStrings[p->size]) {
                free(temp);
                return NULL;
                }

                p->size++;
                subStrings = strtok(NULL, " ");
        }

        free(temp);
        return p;
}

void print_parser(parser *p) {
        for (int i = 0; i < p->size; i++) {
                printf("%s\t", p->subStrings[i]);
                printf("%d\n", sizeof(p->subStrings[i]));
        }
}

void free_parser(parser *p) {
        for (int i = 0; i < p->size; i++) {
                free(p->subStrings[i]);
        }
        free(p->subStrings);
        free(p);
}
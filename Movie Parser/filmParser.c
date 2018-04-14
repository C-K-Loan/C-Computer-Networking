#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
@param line 1 line of csv file from fgets()
@param num the attribute we look for
return returns the value or null if num too big
*/

const char* getfield(char* line, int num)//calls num times strtok on line (provide copy of line since strtok wrecks strings)
{
    const char* tok;
    for (tok = strtok(line, ";");
            tok && *tok;
            tok = strtok(NULL, ";\n"))
    {
        if (!--num)
            return tok;
    }
    return NULL;
}

int main()
{
    FILE* stream = fopen("filme.csv", "r");
    uint8_t sizeMSB = 0;
    uint8_t sizeLSB = 0;
    uint16_t sizeFULL= 0;
    uint16_t sizeFULLtmp =0;
    char line[1024];
    while (fgets(line, 1024, stream))
    {
        char* tmp = strdup(line);
        //printf("Title %s\n", getfield(tmp, 0));
         tmp = strdup(line);
        printf(" Title %s\n", getfield(tmp, 1));
         tmp = strdup(line);
         sizeFULL= strlen(getfield(tmp,1));
         sizeFULLtmp= sizeFULL;

         sizeFULL= sizeFULLt>>8; //delete the 4 lsb
         sizeFULL=sizeFULLtmp<<8;//restore value
         sizeMSB+=sizeFULL;

         sizeFULL = sizeFULLtmp;
         sizeFULL = sizeFULL << 8;//delete the 4 msb
         sizeFULL = sizeFULL >> 8; //restore value
         sizeLSB  += sizeFULL;

         tmp =strdup(line);



        printf("Org Title %s\n", getfield(tmp, 2));
         tmp = strdup(line);
        printf(" jahr%s\n", getfield(tmp, 3));
         tmp = strdup(line);
        printf("len %s\n", getfield(tmp, 4));
         tmp = strdup(line);
        printf("regie %s\n", getfield(tmp, 5));
        tmp = strdup(line);
        printf("Cast %s\n", getfield(tmp, 6));
        tmp = strdup(line);
        free(tmp);
    }
}

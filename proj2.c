#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define ARG_COUNT 6

typedef struct
{
    int L;
    int Z;
    int K;
    int TL;
    int TB;
} TArguments;

int parseArgs(int argc, char *argv[], TArguments *args);

int main(int argc, char *argv[])
{

    TArguments args;
    if (!parseArgs(argc, argv, &args))
        return 1;

    return 0;
}

bool isNumber(char *str);

int parseArgs(int argc, char *argv[], TArguments *args)
{
    if (argc != ARG_COUNT)
    {
        fprintf(stderr, "Wrong number of arguments\n");
        return 0;
    }

    for (int i = 1; i < 6; i++)
    {
        if (!isNumber(argv[i]))
        {
            fprintf(stderr, "Invalid argument\n");
            return 0;
        }
    }

    // L < 20,000
    int num = atoi(argv[1]);
    if (20000 <= num)
    {
        fprintf(stderr, "Invalid argument: L out of specified range\n");
        return 0;
    }
    args->L = num;

    // 0 < Z <= 10
    num = atoi(argv[2]);
    if (num <= 0 || 10 < num)
    {
        fprintf(stderr, "Invalid argument: Z out of specified range\n");
        return 0;
    }
    args->Z = num;

    // 10 <= K <= 100
    num = atoi(argv[3]);
    if (num < 10 || 100 < num)
    {
        fprintf(stderr, "Invalid argument: K out of specified range\n");
        return 0;
    }
    args->K = num;

    // 0 <= TL <= 10000
    num = atoi(argv[4]);
    if (num < 0 || 10000 < num)
    {
        fprintf(stderr, "Invalid argument: TL out of specified range\n");
        return 0;
    }
    args->TL = num;

    // 0 <= TB <= 1000
    num = atoi(argv[5]);
    if (num < 0 || 1000 < num)
    {
        fprintf(stderr, "Invalid argument: TB out of specified range\n");
        return 0;
    }
    args->TB = num;

    return 1;
}

bool isNumber(char *str)
{
    char c = str[0];
    int i = 1;
    while (c != '\0')
    {
        if (c < '0' || c > '9')
            return 0;

        c = str[i++];
    }

    return 1;
}

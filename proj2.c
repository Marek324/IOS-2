#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/shm.h>
#include <stdarg.h>
#include <string.h>

#define ARG_COUNT 6

typedef struct
{
    int L;
    int Z;
    int K;
    int TL;
    int TB;
} TArguments;

int parseArgs(int argc, char *argv[]);

void allocMem();
void freeMem();

void print(const char *fmt, ...);

// Global variables and semaphores
int *lineNumPtr;
sem_t *output;

int *waiting;
sem_t *mutex;
sem_t **bus;
sem_t *boarded;
sem_t *finalDst;
sem_t *disembarked;
int *idZ;

TArguments *args;

void skibus();
void lyzar(int id);

int main(int argc, char *argv[])
{

    args = mmap(NULL, sizeof(TArguments), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (args == MAP_FAILED)
    {
        perror("mmap for args");
        exit(EXIT_FAILURE);
    }

    if (!parseArgs(argc, argv))
    {
        munmap(args, sizeof(TArguments));
        return 1;
    }

    allocMem();

    pid_t p = fork();
    if (p == 0)
    {
        skibus();
        exit(0);
    }

    for (int i = 0; i < args->L; i++)
    {
        pid_t id = fork();
        if (id == 0)
        {
            lyzar(i);
        }
    }

    freeMem();

    return 0;
}

bool isNumber(char *str);

int parseArgs(int argc, char *argv[])
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

#define map(name, size)                                                                 \
    name = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); \
    if (output == MAP_FAILED)                                                           \
    {                                                                                   \
        perror("mmap for output semaphore");                                            \
        exit(EXIT_FAILURE);                                                             \
    }

#define init(name, value)               \
    if (sem_init(name, 1, value) == -1) \
    {                                   \
        perror("sem_init for " #name);  \
        freeMem();                      \
        exit(EXIT_FAILURE);             \
    }

void allocMem()
{
    // Allocate and initialize output semaphore
    map(output, sizeof(sem_t));
    init(output, 1);

    // Allocate and initialize lineNumPtr
    map(lineNumPtr, sizeof(int));
    *lineNumPtr = 1;

    // Allocate and initialize mutex semaphore
    map(mutex, sizeof(sem_t));
    init(mutex, 1);

    // Allocate and initialize bus semaphore array
    map(bus, sizeof(sem_t *) * args->Z);
    for (int i = 0; i < args->Z; i++)
    {
        bus[i] = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (bus[i] == MAP_FAILED)
        {
            perror("mmap for bus semaphore");
            // Clean up already allocated semaphores before exiting
            for (int j = 0; j < i; j++)
            {
                if (munmap(bus[j], sizeof(sem_t)) == -1)
                {
                    perror("munmap for bus semaphore");
                }
            }
            exit(EXIT_FAILURE);
        }
        if (sem_init(bus[i], 1, 0) == -1)
        {
            perror("sem_init for bus semaphore");
            // Clean up already allocated semaphores before exiting
            for (int j = 0; j <= i; j++)
            {
                if (munmap(bus[j], sizeof(sem_t)) == -1)
                {
                    perror("munmap for bus semaphore");
                }
            }
            exit(EXIT_FAILURE);
        }
    }

    // Allocate and initialize boarded semaphore
    map(boarded, sizeof(sem_t));
    init(boarded, 0);

    // Allocate and initialize waiting array
    map(waiting, sizeof(int) * args->Z);

    // Allocate and initialize disembarked semaphore
    map(disembarked, sizeof(sem_t));
    init(disembarked, 0);

    // Allocate and initialize finalDst semaphore
    map(finalDst, sizeof(sem_t));
    init(finalDst, 0);

    // Allocate memory for idZ
    map(idZ, sizeof(int));
}

#define destroy(name)                   \
    if (sem_destroy(name) == -1)         \
    {                                   \
        perror("sem_destroy for " #name); \
    }

#define unmap(name, size)                 \
    if (munmap(name, size) == -1) \
    {                                     \
        perror("munmap for " #name);      \
    }

void freeMem()
{
    // Destroy and unmap output semaphore
    destroy(output);
    unmap(output, sizeof(sem_t));

    // Unmap lineNumPtr
    unmap(lineNumPtr, sizeof(int));

    // Destroy and unmap mutex semaphore
    destroy(mutex);
    unmap(mutex, sizeof(sem_t));

    // Destroy and unmap bus semaphores
    for (int i = 0; i < args->Z; i++)
    {
        destroy(bus[i]);
        unmap(bus[i], sizeof(sem_t));
    }
    unmap(bus, sizeof(sem_t *) * args->Z);

    // Destroy and unmap boarded semaphore
    destroy(boarded);
    unmap(boarded, sizeof(sem_t));

    // Unmap waiting array
    unmap(waiting, sizeof(int) * args->Z);

    // Destroy and unmap finalDst semaphore
    destroy(finalDst);
    unmap(finalDst, sizeof(sem_t));

    // Destroy and unmap disembarked semaphore
    destroy(disembarked);
    unmap(disembarked, sizeof(sem_t));

    // Unmap args
    unmap(args, sizeof(TArguments));
}

int waitingSum()
{
    int sum = 0;
    for (int i = 0; i < args->Z; i++)
    {
        sum += waiting[i];
    }

    return sum;
}

void skibus()
{
    print("BUS: started\n");
    while (waitingSum() > 0)
    {
        *idZ = 1;
        int cap = args->K;
        while (*idZ > args->Z)
        {
            sem_wait(mutex);
            usleep((rand() % args->TB) + 1);
            print("BUS: arrived to %d\n", idZ);

            int count = (waiting[*idZ] < cap) ? waiting[*idZ] : cap;
            for (int i = 0; i < count; i++)
            {
                sem_post(bus[*idZ]);
                sem_wait(boarded);
            }

            waiting[*idZ] = (waiting[*idZ] - cap > 0) ? waiting[*idZ] - cap : 0;
            cap -= count;

            print("BUS: leaving %d\n", *idZ);
            (*idZ)++;

            sem_post(mutex);
        }

        usleep((rand() % args->TB) + 1);
        print("BUS: arrived to final\n");

        sem_wait(mutex);

        for (int i = 0; i < args->K - cap; i++)
        {
            cap = 0;
            sem_post(finalDst);
            sem_wait(disembarked);
        }

        sem_post(mutex);
        print("BUS: leaving final\n");
    }
    print("BUS: finish\n");
}

void lyzar(int id)
{
    sem_wait(mutex);

    int stop = (rand() % args->Z) + 1;
    (waiting[stop])++;

    sem_post(mutex);

    print("L %d: started\n", id);
    usleep((rand() % args->TL) + 1);

    print("L %d: arrived to %d\n", id, stop);

    sem_wait(bus[stop]);
    print("L %d: boarding\n", id);
    sem_post(boarded);

    sem_wait(finalDst);
    print("L %d: going to ski\n", id);
    sem_post(disembarked);

    exit(0);
}

void print(const char *fmt, ...)
{
    sem_wait(output);

    va_list args;
    va_start(args, fmt);

    fprintf(stdout, "%d: ", (*lineNumPtr)++);
    vfprintf(stdout, fmt, args);

    fflush(stdout);
    va_end(args);

    sem_post(output);
}

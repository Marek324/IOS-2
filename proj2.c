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
TArguments *args;

int *lineNumPtr;
sem_t *outMutex;

int *waiting;
int *transported;

sem_t *mutex;
sem_t *boarded;
sem_t *finalDst;
sem_t *disembarked;
sem_t **bus;

FILE *outputFile;

void skibus();
void lyzar(int id);

int main(int argc, char *argv[])
{

    outputFile = fopen("proj2.out", "w");
    if (outputFile == NULL)
    {
        perror("Failed to open output file");
        munmap(args, sizeof(TArguments));
        return 1;
    }

    args = mmap(NULL, sizeof(TArguments), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (args == MAP_FAILED)
    {
        perror("mmap for args");
        exit(EXIT_FAILURE);
    }

    if (!parseArgs(argc, argv)) // parse argumentss
    {
        munmap(args, sizeof(TArguments));
        return 1;
    }

    allocMem(); // allocate memory

    pid_t pid = fork(); // create bus process
    if (pid == 0)
    {
        skibus();
    }

    for (int i = 0; i < args->L; i++) // create lyzar processes
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            lyzar(i + 1);
        }
    }

    while (wait(NULL) > 0) // wait for all children processes to finish
        ;

    freeMem(); // free memory

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
    if (outMutex == MAP_FAILED)                                                         \
    {                                                                                   \
        perror("mmap for " #name);                                                      \
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
    // Allocate and initialize lineNumPtr
    map(lineNumPtr, sizeof(int));
    *lineNumPtr = 1;

    // Allocate and initialize outMutex semaphore
    map(outMutex, sizeof(sem_t));
    init(outMutex, 1);

    // Allocate and initialize waiting array
    map(waiting, sizeof(int) * args->Z);

    // Allocate and initialize lineNumPtr
    map(transported, sizeof(int));
    *transported = 0;

    // Allocate and initialize mutex semaphore
    map(mutex, sizeof(sem_t));
    init(mutex, 1);

    // Allocate and initialize boarded semaphore
    map(boarded, sizeof(sem_t));
    init(boarded, 0);

    // Allocate and initialize finalDst semaphore
    map(finalDst, sizeof(sem_t));
    init(finalDst, 0);

    // Allocate and initialize disembarked semaphore
    map(disembarked, sizeof(sem_t));
    init(disembarked, 0);

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
}

#define destroy(name)                     \
    if (sem_destroy(name) == -1)          \
    {                                     \
        perror("sem_destroy for " #name); \
    }

#define unmap(name, size)            \
    if (munmap(name, size) == -1)    \
    {                                \
        perror("munmap for " #name); \
    }

void freeMem()
{
    // Unmap args
    unmap(args, sizeof(TArguments));

    // Unmap lineNumPtr
    unmap(lineNumPtr, sizeof(int));

    // Destroy and unmap outMutex semaphore
    destroy(outMutex);
    unmap(outMutex, sizeof(sem_t));

    // Unmap waiting array
    unmap(waiting, sizeof(int) * args->Z);

    // Unmap transported
    unmap(transported, sizeof(int));
    fclose(outputFile);

    // Destroy and unmap mutex semaphore
    destroy(mutex);
    unmap(mutex, sizeof(sem_t));

    // Destroy and unmap boarded semaphore
    destroy(boarded);
    unmap(boarded, sizeof(sem_t));

    // Destroy and unmap finalDst semaphore
    destroy(finalDst);
    unmap(finalDst, sizeof(sem_t));

    // Destroy and unmap disembarked semaphore
    destroy(disembarked);
    unmap(disembarked, sizeof(sem_t));

    // Destroy and unmap bus semaphores
    for (int i = 0; i < args->Z; i++)
    {
        destroy(bus[i]);
        unmap(bus[i], sizeof(sem_t));
    }
    unmap(bus, sizeof(sem_t *) * args->Z);
}

int min(int a, int b);

void skibus()
{
    print("BUS: started\n");

    int idZ;

    while (*transported < args->L) // while there are still passengers to transport
    {
        idZ = 0;               // index of the stop
        int currCap = args->K; // current capacity of the bus

        while (idZ < args->Z) // while there are still stops to visit
        {
            usleep((rand() % args->TB) + 1); // simulate travel time
            print("BUS: arrived to %d\n", (idZ) + 1);

            sem_wait(mutex);
            int count = min(waiting[idZ], currCap); // get the number of passengers to board

            for (int i = 0; i < count; i++) // board the passengers
            {
                sem_post(bus[idZ]);
                sem_wait(boarded);
            }
            waiting[idZ] -= count; // update the number of waiting passengers on the idZ stop
            sem_post(mutex);
            currCap -= count; // update the current capacity of the bus

            print("BUS: leaving %d\n", (idZ) + 1);
            (idZ)++; // move to the next stop
        }

        usleep((rand() % args->TB) + 1);
        print("BUS: arrived to final\n");

        for (int i = 0; i < args->K - currCap; i++) // disembark the passengers
        {
            sem_post(finalDst);    // signal that the bus has arrived to the final destination
            sem_wait(disembarked); // wait for the passenger to disembark
        }

        print("BUS: leaving final\n");
    }
    print("BUS: finish\n");
    freeMem(); // free memory
    exit(0);   // kill the process
}

int min(int a, int b)
{
    return a < b ? a : b; // return the smaller of the two
}

void lyzar(int id)
{
    srand((time(NULL) & getpid()) ^ (getpid() << 16)); // seed the random number generator

    sem_wait(mutex);
    int stop = (rand() % args->Z); // choose a random stop
    (waiting[stop])++;             // increment the number of waiting passengers on the stop
    sem_post(mutex);

    print("L %d: started\n", id);
    usleep((rand() % args->TL) + 1); // simulate time to get to the stop

    print("L %d: arrived to %d\n", id, stop + 1);

    sem_wait(bus[stop]); // wait for the bus to arrive
    print("L %d: boarding\n", id);
    sem_post(boarded); // signal that the passenger has boarded

    sem_wait(finalDst); // wait for the bus to arrive to the final destination
    print("L %d: going to ski\n", id);

    sem_wait(mutex);
    (*transported)++; // increment the number of transported passengers
    sem_post(mutex);

    sem_post(disembarked); // signal that the passenger has disembarked

    freeMem(); // free memory
    exit(0);   // kill the process
}

void print(const char *fmt, ...)
{
    sem_wait(outMutex); // wait for the output mutex

    va_list args;
    va_start(args, fmt);

    fprintf(outputFile, "%d: ", (*lineNumPtr)++); // print the line number
    vfprintf(outputFile, fmt, args);              // print the message

    fflush(outputFile); // flush the output buffer
    va_end(args);

    sem_post(outMutex); // signal that the output is done
}

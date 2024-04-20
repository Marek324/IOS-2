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

    skibus();

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

void allocMem()
{
    // Allocate and initialize output semaphore
    output = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (output == MAP_FAILED)
    {
        perror("mmap for output semaphore");
        exit(EXIT_FAILURE);
    }
    if (sem_init(output, 1, 1) == -1)
    {
        perror("sem_init for output semaphore");
        exit(EXIT_FAILURE);
    }

    // Allocate and initialize lineNumPtr
    lineNumPtr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (lineNumPtr == MAP_FAILED)
    {
        perror("mmap for lineNumPtr");
        exit(EXIT_FAILURE);
    }
    *lineNumPtr = 1;

    // Allocate and initialize mutex semaphore
    mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mutex == MAP_FAILED)
    {
        perror("mmap for mutex semaphore");
        exit(EXIT_FAILURE);
    }
    if (sem_init(mutex, 1, 1) == -1)
    {
        perror("sem_init for mutex semaphore");
        exit(EXIT_FAILURE);
    }

    // Allocate and initialize bus semaphore array
    bus = mmap(NULL, sizeof(sem_t *) * args->Z, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (bus == MAP_FAILED)
    {
        perror("mmap for bus semaphore array");
        exit(EXIT_FAILURE);
    }
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
    boarded = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (boarded == MAP_FAILED)
    {
        perror("mmap for boarded semaphore");
        exit(EXIT_FAILURE);
    }
    if (sem_init(boarded, 1, 0) == -1)
    {
        perror("sem_init for boarded semaphore");
        exit(EXIT_FAILURE);
    }

    // Allocate and initialize waiting array
    waiting = mmap(NULL, sizeof(int) * args->Z, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (waiting == MAP_FAILED)
    {
        perror("mmap for waiting array");
        exit(EXIT_FAILURE);
    }

    // Allocate and initialize disembarked semaphore
    disembarked = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (disembarked == MAP_FAILED)
    {
        perror("mmap for disembarked semaphore");
        exit(EXIT_FAILURE);
    }
    if (sem_init(disembarked, 1, 0) == -1)
    {
        perror("sem_init for disembarked semaphore");
        exit(EXIT_FAILURE);
    }

    // Allocate and initialize finalDst semaphore
    finalDst = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (finalDst == MAP_FAILED)
    {
        perror("mmap for finalDst semaphore");
        exit(EXIT_FAILURE);
    }
    if (sem_init(finalDst, 1, 0) == -1)
    {
        perror("sem_init for finalDst semaphore");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for idZ
    idZ = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (idZ == MAP_FAILED)
    {
        perror("mmap for idZ");
        exit(EXIT_FAILURE);
    }
}

void freeMem()
{
    // Destroy and unmap output semaphore
    if (sem_destroy(output) == -1)
    {
        perror("sem_destroy for output semaphore");
    }
    if (munmap(output, sizeof(sem_t)) == -1)
    {
        perror("munmap for output semaphore");
    }

    // Unmap lineNumPtr
    if (munmap(lineNumPtr, sizeof(int)) == -1)
    {
        perror("munmap for lineNumPtr");
    }

    // Destroy and unmap mutex semaphore
    if (sem_destroy(mutex) == -1)
    {
        perror("sem_destroy for mutex semaphore");
    }
    if (munmap(mutex, sizeof(sem_t)) == -1)
    {
        perror("munmap for mutex semaphore");
    }

    // Destroy and unmap bus semaphores
    for (int i = 0; i < args->Z; i++)
    {
        if (sem_destroy(bus[i]) == -1)
        {
            perror("sem_destroy for bus semaphore");
        }
        if (munmap(bus[i], sizeof(sem_t)) == -1)
        {
            perror("munmap for bus semaphore");
        }
    }
    if (munmap(bus, sizeof(sem_t *) * args->Z) == -1)
    {
        perror("munmap for bus semaphore array");
    }

    // Destroy and unmap boarded semaphore
    if (sem_destroy(boarded) == -1)
    {
        perror("sem_destroy for boarded semaphore");
    }
    if (munmap(boarded, sizeof(sem_t)) == -1)
    {
        perror("munmap for boarded semaphore");
    }

    // Unmap waiting array
    if (munmap(waiting, sizeof(int) * args->Z) == -1)
    {
        perror("munmap for waiting array");
    }

    // Destroy and unmap finalDst semaphore
    if (sem_destroy(finalDst) == -1)
    {
        perror("sem_destroy for finalDst semaphore");
    }
    if (munmap(finalDst, sizeof(sem_t)) == -1)
    {
        perror("munmap for finalDst semaphore");
    }

    // Destroy and unmap disembarked semaphore
    if (sem_destroy(disembarked) == -1)
    {
        perror("sem_destroy for disembarked semaphore");
    }
    if (munmap(disembarked, sizeof(sem_t)) == -1)
    {
        perror("munmap for disembarked semaphore");
    }

    // Unmap args
    if (munmap(args, sizeof(TArguments)) == -1)
    {
        perror("munmap for TArguments");
    }
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
        while (*idZ <= args->Z)
        {
            usleep((rand() % args->TB) + 1);
            print("BUS: arrived to %d\n", idZ);
            sem_wait(mutex);

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

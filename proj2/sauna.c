#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/syscall.h>

#include "request.h"

#define gettid() syscall(SYS_gettid)
#define MODE 0770
#define MICRO_TO_MILLISECONDS 1000
#define DEFAULT_THREADS 1000

unsigned int numberOfSlots;
unsigned int freeSlots;
unsigned int received = 0;
unsigned int receivedMale = 0;
unsigned int receivedFemale = 0;
unsigned int rejected = 0;
unsigned int rejectedMale = 0;
unsigned int rejectedFemale = 0;
unsigned int served = 0;
unsigned int servedMale = 0;
unsigned int servedFemale = 0;

char currentGender;

clock_t startTime;

int logFiledes;
int entryFiledes;
int rejectedFiledes;

pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t logMux = PTHREAD_MUTEX_INITIALIZER;
int nThreads = DEFAULT_THREADS;
pthread_t** threads;

void mainThreadFunction();
void* occupiedSlot(void* arg);
void writeRequestToLog(struct request_t req, char* type);
void registerThread(pthread_t* thr);
void joinAllThreads();

int main(int argc, char** argv)
{
	startTime = clock();

	if (argc != 2)
	{
		printf("Wrong number of arguments provided\n");
		printf("Usage: sauna <number of slots>\n");
		return 1;
	}
	sscanf(argv[1], "%u", &numberOfSlots);

	freeSlots = numberOfSlots;

	srand(time(NULL));

	threads = (pthread_t**)calloc(sizeof(pthread_t*), DEFAULT_THREADS);

	while((entryFiledes = open("/tmp/entry", O_RDONLY | O_NONBLOCK)) < 0);

	while((rejectedFiledes = open("/tmp/rejected", O_WRONLY | O_NONBLOCK)) < 0);

	char logName[30];
	sprintf(logName, "/tmp/bal.%d", getpid());

	if (access(logName, F_OK) != -1)
	{
		if (unlink(logName) != 0)
			perror("Error deleting preexistent log file");
	}
	logFiledes = open(logName, O_WRONLY | O_CREAT, MODE);
	if (logFiledes < 0)
	{
		perror("Error opening/creating log file");
		return 5;
	}

	printf("Connection with generator successfully established\n");

	mainThreadFunction();

	joinAllThreads();

	printf("Received requests: %d (%d Male, %d Female)\n",
					received, receivedMale, receivedFemale);
	printf("Rejected requests:  %d (%d Male, %d Female)\n",
				rejected, rejectedMale, rejectedFemale);
	printf("Served requests: %d (%d Male, %d Female)\n",
				served, servedMale, servedFemale);

	if (close(logFiledes) != 0)
		perror("Error closing log file");

	if (unlink("/tmp/entry") != 0)
		perror("Error deleting FIFO /tmp/entry");

	if (unlink("/tmp/rejected") != 0)
		perror("Error deleting FIFO /tmp/rejected");

	return 0;
}

void mainThreadFunction()
{
	int readNo;
	struct request_t req;
	while ((readNo = read(entryFiledes, &req, sizeof(req))))
	{
		if (readNo > 0)
		{
			writeRequestToLog(req, "RECEIVED");
			received++;
			if (req.gender == MALE)
				receivedMale++;
			else
				receivedFemale++;

			if (freeSlots == numberOfSlots)
			{
				currentGender = req.gender;
				pthread_mutex_lock(&mux);
				freeSlots--;
				pthread_mutex_unlock(&mux);
				pthread_t newOccupiedSlot;
				if (pthread_create(&newOccupiedSlot, NULL, occupiedSlot, &req) != 0)
				{
					perror("Error creating new occupied slot thread");
					exit(7);
				}
				registerThread(&newOccupiedSlot);

				writeRequestToLog(req, "SERVED");
				served++;
				if (req.gender == MALE)
					servedMale++;
				else
					servedFemale++;
			}
			else if (req.gender == currentGender)
			{
				pthread_mutex_lock(&mux);
				while (freeSlots == 0)
					pthread_cond_wait(&cond, &mux);

				pthread_t newOccupiedSlot;
				if (pthread_create(&newOccupiedSlot, NULL, occupiedSlot, &req) != 0)
				{
					perror("Error creating new occupied slot thread");
					exit(8);
				}
				writeRequestToLog(req, "SERVED");
				served++;
				if (req.gender == MALE)
					servedMale++;
				else
					servedFemale++;
				pthread_mutex_unlock(&mux);
			}
			else
			{
				writeRequestToLog(req, "REJECTED");
				rejected++;
				if (req.gender == MALE)
					rejectedMale++;
				else
					rejectedFemale++;
				if (write(rejectedFiledes, &req, sizeof(req)) <= 0)
					perror("Error writing to FIFO tmp/rejected");
			}
		}
		else if (readNo == 0)
			break;
	}

	if (close(rejectedFiledes) != 0)
		perror("Error closing FIFO /tmp/rejected");
	if (close(entryFiledes) != 0)
		perror("Error closing FIFO /tmp/entry");
	return;
}

void* occupiedSlot(void* arg)
{
	usleep(((struct request_t*)arg)->duration * MICRO_TO_MILLISECONDS);
	writeRequestToLog(*(struct request_t*)arg, "SERVED");
	pthread_mutex_lock(&mux);
	freeSlots++;
	pthread_mutex_unlock(&mux);
	return NULL;
}

void writeRequestToLog(struct request_t req, char* type)
{
	char info[300];
	float currTime = (clock() - startTime) / MICRO_TO_MILLISECONDS;
	pid_t tid = gettid();
	sprintf(info, "%.2f - %6d - %1lld - %3d: %c - %3d - %s\n",		///////////////////////
			currTime, getpid(), (long long)tid, req.serial,						///////////////////////
			req.gender, req.duration, type);
	pthread_mutex_lock(&logMux);
	if (write(logFiledes, info, strlen(info) + 1) <= 0)
		perror("Error writing to log file");
	pthread_mutex_unlock(&logMux);
}

void registerThread(pthread_t* thr)
{
	int i;
	for (i = 0; i < nThreads; i++)
	{
		if (threads[i] == NULL)
		{
			threads[i] = thr;
			return;
		}
	}
	nThreads *= 2;
	threads = (pthread_t**)realloc(threads, nThreads);
	threads[i] = thr;
	return;
}

void joinAllThreads()
{
	int i;
	for (i = 0; i < nThreads; i++)
	{
		if (threads[i] != NULL)
			pthread_join(*threads[i], NULL);
	}
	return;
}

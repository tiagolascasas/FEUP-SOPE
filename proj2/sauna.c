#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>

#include "request.h"

#define gettid() syscall(SYS_gettid)
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

long startTime;

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
float getTimeMilliseconds();

int main(int argc, char** argv)
{
	struct timespec tm;
	timespec_get(&tm, TIME_UTC);
	startTime = tm.tv_nsec + tm.tv_sec * 1e9;

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
	printf("Rejected requests: %d (%d Male, %d Female)\n",
				rejected, rejectedMale, rejectedFemale);
	printf("Served requests:   %d (%d Male, %d Female)\n",
				served, servedMale, servedFemale);

	if (close(logFiledes) != 0)
		perror("Error closing log file");

	if (unlink("/tmp/entry") != 0)
		perror("Error deleting FIFO /tmp/entry");

	if (unlink("/tmp/rejected") != 0)
		perror("Error deleting FIFO /tmp/rejected");

	return 0;
}

/**
 * Reads all requests from the entry FIFO and
 * handles them appropriately, launching threads
 * and joining them when the  FIFO closes.
 */
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

/**
 * This function serves as a body to a thread, and it
 * simulates the time spent in the sauna by a given request,
 * "sleeping" during the duration of that request and then returning
 * @param arg the request to wait upon
 * @return NULL
 */
void* occupiedSlot(void* arg)
{
	usleep(((struct request_t*)arg)->duration * MICRO_TO_MILLISECONDS);
	pthread_mutex_lock(&mux);
	freeSlots++;
	pthread_mutex_unlock(&mux);
	return NULL;
}

/**
 * Writes a request to the log in the format
 * showcased in the project guideline.
 * @param req the request to register
 * @param type the type of entry ("Served", "Rejected" or "Received")
 */
void writeRequestToLog(struct request_t req, char* type)
{
	char info[300];
	float currTime = getTimeMilliseconds(startTime);
	pid_t tid = gettid();
	sprintf(info, "%.2f - %6d - %1lld - %3d: %c - %3d - %s\n",
			currTime, getpid(), (long long)tid, req.serial,
			req.gender, req.duration, type);
	pthread_mutex_lock(&logMux);
	if (write(logFiledes, info, strlen(info)) <= 0)
		perror("Error writing to log file");
	pthread_mutex_unlock(&logMux);
}

/**
 * Registers a pthread_t in an array, expanding
 * that array if it is full.
 * @param thr the thread to register
 */
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
	if (threads == NULL)
	{
		perror("realloc() failed on function registerThread()");
		exit(9);
	}
	threads[i] = thr;
	return;
}

/**
 * Goes through the array of thread descriptors
 * and joins them all.
 */
void joinAllThreads()
{
	int i;
	for (i = 0; i < nThreads; i++)
	{
		if (threads[i] != NULL)
		{
			if (pthread_join(*threads[i], NULL) != 0)
				perror("Error joining a thread");
		}
	}
	return;
}

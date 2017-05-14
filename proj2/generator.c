#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#include "request.h"

#define RAND_GENDER rand() & 1 ? MALE : FEMALE
#define MAX_RETRIES 100000

unsigned int numberOfRequests;
unsigned int maxUsage;
unsigned int generated = 0;
unsigned int generatedMale = 0;
unsigned int generatedFemale = 0;
unsigned int rejected = 0;
unsigned int rejectedMale = 0;
unsigned int rejectedFemale = 0;
unsigned int discarded = 0;
unsigned int discardedMale = 0;
unsigned int discardedFemale = 0;

long startTime;

int logFiledes;
int entryFiledes;
int rejectedFiledes;

pthread_mutex_t logMux = PTHREAD_MUTEX_INITIALIZER;

void* senderFunction(void* arg);
void* receiverFunction(void* arg);
void writeRequestToLog(struct request_t req, char* type);
float getTimeMilliseconds();

int main(int argc, char** argv)
{
	struct timespec tm;
	timespec_get(&tm, TIME_UTC);
	startTime = tm.tv_nsec + tm.tv_sec * 1e9;

	if (argc != 3)
	{
		printf("Usage: generator <number of requests> <max time per usage>\n");
		return 1;
	}
	sscanf(argv[1], "%u", &numberOfRequests);
	sscanf(argv[2], "%u", &maxUsage);

	srand(time(NULL));

	if (access("/tmp/entry", F_OK) != -1)
	{
		if (unlink("/tmp/entry") != 0)
			perror("Error deleting preexistent FIFO /tmp/entry");
	}
	if (mkfifo("/tmp/entry", MODE) != 0)
	{
		perror("Error creating FIFO /tmp/entry");
		return 2;
	}

	if (access("/tmp/rejected", F_OK) != -1)
	{
		if (unlink("/tmp/rejected") != 0)
			perror("Error deleting preexistent FIFO /tmp/rejected");
	}
	if (mkfifo("/tmp/rejected", MODE) != 0)
	{
		perror("Error creating FIFO /tmp/rejected");
		return 2;
	}

	while((entryFiledes = open("/tmp/entry", O_WRONLY)) < 0);

	while((rejectedFiledes = open("/tmp/rejected", O_RDONLY | O_NONBLOCK)) < 0);

	char logName[30];
	sprintf(logName, "/tmp/ger.%d", getpid());

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

	printf("Connection to sauna successfully established\n");

	pthread_t sender, receiver;
	if (pthread_create(&sender, NULL, senderFunction, NULL) != 0)
	{
		perror("Error creating sender thread");
		return 6;
	}
	if (pthread_create(&receiver, NULL, receiverFunction, NULL) != 0)
	{
		perror("Error creating receiver thread");
		return 7;
	}

	pthread_join(sender, NULL);
	pthread_join(receiver, NULL);

	printf("Generated requests: %d (%d Male, %d Female)\n",
					generated, generatedMale, generatedFemale);
	printf("Rejected requests:  %d (%d Male, %d Female)\n",
				rejected, rejectedMale, rejectedFemale);
	printf("Discarded requests: %d (%d Male, %d Female)\n",
				discarded, discardedMale, discardedFemale);

	if (close(logFiledes) != 0)
		perror("Error closing log file");

	return 0;
}

/**
 * Generates a number of requests specified on the
 * global variable numberOfRequests and sends them
 * to the /tmp/entry FIFO. It serves as the entry
 * point of a thread.
 * @param arg unused
 * @return NULL
 */
void* senderFunction(void* arg)
{
	while (numberOfRequests)
	{
		struct request_t req;
		req.serial = generated;
		req.gender = RAND_GENDER;
		req.duration = (rand() % maxUsage) + 1;
		req.timesRejected = 0;
		if (write(entryFiledes, &req, sizeof(req)) >= 0)
		{
			writeRequestToLog(req, "REQUEST");
			numberOfRequests--;
			generated++;
			if (req.gender == MALE)
				generatedMale++;
			else
				generatedFemale++;
		}
		else
			perror("Error writing to FIFO /tmp/entry");
	}
	return NULL;
}

/**
 * Reads the rejected requests FIFO, and sends those
 * requests back into the entry FIFO if their number
 * of rejections is inferior to 3. After a certain time
 * without reading anything, it closes the FIFO. It serves
 * as the entry point for a thread.
 * @param arg unused
 * @return NULL
 */
void* receiverFunction(void* arg)
{
	int readNo;
	struct request_t req;
	int retries = 0;
	while ((readNo = read(rejectedFiledes, &req, sizeof(req))))
	{
		if (readNo > 0)
		{
			retries = 0;
			if (req.timesRejected < 3)
			{
				writeRequestToLog(req, "REJECTED");
				req.timesRejected++;
				rejected++;
				if (req.gender == MALE)
					rejectedMale++;
				else
					rejectedFemale++;
				if (write(entryFiledes, &req, sizeof(req)) <= 0)
					perror("Error writing to FIFO tmp/entry");
			}
			else
			{
				writeRequestToLog(req, "DISCARDED");
				discarded++;
				if (req.gender == MALE)
					discardedMale++;
				else
					discardedFemale++;
			}
		}
		else if (readNo < 0)
		{
			retries++;
			if (retries >= MAX_RETRIES)
				break;
		}
		else
			break;
	}

	if (close(entryFiledes) != 0)
		perror("Error closing FIFO /tmp/entry");
	if (close(rejectedFiledes) != 0)
		perror("Error closing FIFO /tmp/rejected");
	return NULL;
}

/**
 * Writes a request to the log in the format
 * showcased in the project guideline.
 * @param req the request to register
 * @param type the type of entry ("REQUEST", "REJECTED" or "DISCARDED")
 */
void writeRequestToLog(struct request_t req, char* type)
{
	char info[300];
	float currTime = getTimeMilliseconds(startTime);
	sprintf(info, "%.2f - %6d - %3d: %c - %3d - %s\n",
			currTime, getpid(), req.serial,
			req.gender, req.duration, type);
	pthread_mutex_lock(&logMux);
	if (write(logFiledes, info, strlen(info)) <= 0)
		perror("Error writing to log file");
	pthread_mutex_unlock(&logMux);
}

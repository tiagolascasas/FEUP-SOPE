#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "request.h"

#define MODE 0770
#define MALE 'M'
#define FEMALE 'F'
#define RAND_GENDER rand() & 1 ? MALE : FEMALE

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

clock_t startTime;

int logFiledes;
int entryFiledes;
int rejectedFiledes;

void* senderFunction(void* arg);
void* receiverFunction(void* arg);
void writeRequestToLog(struct request_t req, char* type);

int main(int argc, char** argv)
{
	startTime = clock();

	if (argc != 3)
	{
		printf("Usage: generator <number of requests> <max time per usage>\n");
		return 1;
	}
	sscanf(argv[1], "%u", &numberOfRequests);
	sscanf(argv[2], "%u", &maxUsage);

	srand(time(NULL));

	if (mkfifo("/tmp/entry", MODE) != 0)
	{
		perror("Error creating FIFO /tmp/entry");
		return 2;
	}
			//REMOVE O_NONBLOCK LATER
	entryFiledes = open("/tmp/entry", O_WRONLY | O_NONBLOCK);
	if (entryFiledes < 0)
	{
		perror("Error opening FIFO /tmp/entry");
	//	return 3;
	}

	rejectedFiledes = open("/tmp/rejected", O_RDONLY);
	if (rejectedFiledes < 0)
	{
		perror("Error opening FIFO /tmp/rejected");
	//	return 4;
	}

	logFiledes = open("/tmp/ger.pid", O_WRONLY | O_CREAT, MODE);
	if (logFiledes < 0)
	{
		perror("Error opening/creating file /tmp/ger.pid");
		return 5;
	}

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

	if (close(entryFiledes) != 0)
		perror("Error closing FIFO /tmp/entry");
	if (close(logFiledes) != 0)
		perror("Error closing file /tmp/ger.pid");
	if (unlink("/tmp/entry") != 0)
		perror("Error deleting FIFO /tmp/entry");

	printf("Generated requests: %d (%d Male, %d Female)\n",
					generated, generatedMale, generatedFemale);
	printf("Rejected requests:  %d (%d Male, %d Female)\n",
				rejected, rejectedMale, rejectedFemale);
	printf("Discarded requests: %d (%d Male, %d Female)\n",
				discarded, discardedMale, discardedFemale);
	return 0;
}

void* senderFunction(void* arg)
{
	while (numberOfRequests)
	{
		struct request_t req;
		req.serial = generated;
		req.gender = RAND_GENDER;
		req.duration = maxUsage;
		req.timesRejected = 0;
		if (write(entryFiledes, &req, sizeof(req)) >= 0)
		{
			writeRequestToLog(req, "REQUEST");
			numberOfRequests--;
			generated++;
			if (req.gender == 'M')
				generatedMale++;
			else
				generatedFemale++;
		}
		else
			perror("Error writing to FIFO tmp/entry");
	}
	return NULL;
}

void* receiverFunction(void* arg)
{
	int readNo;
	struct request_t req;
	while ((readNo = read(rejectedFiledes, &req, sizeof(req))) != -1)
	{
		if (read > 0)
		{
			if (req.timesRejected < 3)
			{
				writeRequestToLog(req, "REJECTED");
				req.timesRejected++;
				rejected++;
				if (req.gender == 'M')
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
				if (req.gender == 'M')
					discardedMale++;
				else
					discardedFemale++;
			}
		}
	}
	return NULL;
}

void writeRequestToLog(struct request_t req, char* type)
{
	char info[200];
	float currTime = (clock() - startTime) / 1000;
	sprintf(info, "%.2f - %6d - %3d: %c - %3d - %s\n",
			currTime, getppid(), req.serial,
			req.gender, req.duration, type);
	if (write(logFiledes, info, strlen(info) + 1) <= 0)
		perror("Error writing to file tmp/ger.pid");
}

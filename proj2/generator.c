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

void* senderFunction(void* arg);
void* receiverFunction(void* arg);
char generateGender();

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

	mkfifo("/tmp/entry", MODE);		//REMOVE O_NONBLOCK LATER
	entryFiledes = open("/tmp/entry", O_WRONLY | O_NONBLOCK);
	logFiledes = open("/tmp/ger.pid", O_WRONLY | O_CREAT, MODE);

	pthread_t sender, receiver;
	pthread_create(&sender, NULL, senderFunction, NULL);
	pthread_create(&receiver, NULL, receiverFunction, NULL);

	pthread_join(sender, NULL);
	pthread_join(receiver, NULL);

	close(entryFiledes);

	return 0;
}

void* senderFunction(void* arg)
{
	while (numberOfRequests)
	{
		struct request_t req;
		req.serial = generated;
		req.gender = (rand() & 1 ? 'M' : 'F');
		req.duration = maxUsage;
		if (write(entryFiledes, &req, sizeof(req)))
		{

			char info[200];
			float currTime = (clock() - startTime) / 1000;
			sprintf(info, "%.2f - %6d - %3d: %c - %3d - %s\n",
					currTime, getppid(), req.serial,
					req.gender, req.duration, "REQUEST");
			write(logFiledes, info, strlen(info) + 1);

			numberOfRequests--;
			generated++;
			if (req.gender == 'M')
				generatedMale++;
			else
				generatedFemale++;
		}
	}
	return NULL;
}

void* receiverFunction(void* arg)
{
	return NULL;
}

char inline generateGender()
{
	return rand() & 1 ? 'M' : 'F';
}

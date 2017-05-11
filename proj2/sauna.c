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
#define MALE 'M'
#define FEMALE 'F'
#define MAX_RETRIES 100

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

void mainThreadFunction();
void* occupiedSlot(void* arg);
void writeRequestToLog(struct request_t req, char* type);

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

	if (mkfifo("/tmp/rejected", MODE) != 0)
	{
		perror("Error creating FIFO /tmp/rejected");
		return 2;
	}

	rejectedFiledes = open("/tmp/rejected", O_WRONLY);
	if (rejectedFiledes < 0)
	{
		perror("Error opening FIFO /tmp/rejected");
		return 4;
	}

	while((entryFiledes = open("/tmp/entry", O_RDONLY)) < 0);

	logFiledes = open("/tmp/bal.pid", O_WRONLY | O_CREAT, MODE);
	if (logFiledes < 0)
	{
		perror("Error opening/creating file /tmp/bal.pid");
		return 5;
	}

	printf("Connection with generator successfully established\n");

	mainThreadFunction();

	if (close(logFiledes) != 0)
		perror("Error closing file /tmp/ger.pid");

	printf("Received requests: %d (%d Male, %d Female)\n",
					received, receivedMale, receivedFemale);
	printf("Rejected requests:  %d (%d Male, %d Female)\n",
				rejected, rejectedMale, rejectedFemale);
	printf("Served requests: %d (%d Male, %d Female)\n",
				served, servedMale, servedFemale);

	if (unlink("/tmp/rejected") != 0)
		perror("Error deleting FIFO /tmp/rejected");

	return 0;
}

void mainThreadFunction()
{
	int readNo;
	struct request_t req;
	while ((readNo = read(entryFiledes, &req, sizeof(req))) != -1)
	{
		if (readNo > 0)
		{
			writeRequestToLog(req, "RECEIVED");
			received++;
			if (req.gender == 'M')
				receivedMale++;
			else
				receivedFemale++;
			if (freeSlots == numberOfSlots)
			{
				currentGender = req.gender;
				freeSlots--;
				pthread_t newOccupiedSlot;
				if (pthread_create(&newOccupiedSlot, NULL, occupiedSlot, &req.duration) != 0)
				{
					perror("Error creating new occupied slot thread");
					exit(7);
				}
				//pthread_join(newOccupiedSlot , NULL);

				writeRequestToLog(req, "SERVED");
				served++;
				if (req.gender == 'M')
					servedMale++;
				else
					servedFemale++;
			}
			else if (req.gender == currentGender)
			{
				if (freeSlots)
				{
					pthread_t newOccupiedSlot;
					if (pthread_create(&newOccupiedSlot, NULL, occupiedSlot, &req) != 0)
					{
						perror("Error creating new occupied slot thread");
						exit(8);
					}
					//pthread_join(newOccupiedSlot , NULL);

					writeRequestToLog(req, "SERVED");
					served++;
					if (req.gender == 'M')
						servedMale++;
					else
						servedFemale++;
				}
				//else //put in queue and wait for free slot
			}
			else
			{
				writeRequestToLog(req, "REJECTED");
				rejected++;
				if (req.gender == 'M')
					rejectedMale++;
				else
					rejectedFemale++;
				if (write(rejectedFiledes, &req, sizeof(req)) <= 0)
					perror("Error writing to FIFO tmp/rejected");
			}
		}
	}

	if (close(rejectedFiledes) != 0)
		perror("Error closing FIFO /tmp/rejected");
	if (close(entryFiledes) != 0)
		perror("Error closing FIFO /tmp/entry");
	return;
}

void* occupiedSlot(void* arg)
{
	usleep(((struct request_t*)arg)->duration);
	writeRequestToLog(*(struct request_t*)arg, "SERVED");
	// Mutex here
	freeSlots++;
	return NULL;
}

void writeRequestToLog(struct request_t req, char* type)
{
	char info[300];
	float currTime = (clock() - startTime) / 1000;
	pid_t tid = gettid();
	sprintf(info, "%.2f - %6d - %1lld - %3d: %c - %3d - %s\n",		///////////////////////
			currTime, getppid(), (long long)tid, req.serial,						///////////////////////
			req.gender, req.duration, type);
	if (write(logFiledes, info, strlen(info) + 1) <= 0)
		perror("Error writing to file tmp/ger.pid");
}

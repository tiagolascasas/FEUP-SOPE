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
//#define RAND_GENDER rand() & 1 ? MALE : FEMALE


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

void* mainThreadFunction(void* arg);
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
		return 3;
	}
	
	entryFiledes = open ("/tmp/entry", O_RDONLY);
	if (entryFiledes < 0)
	{
		perror("Error opening FIFO /tmp/entry");
		return 4;
	}
		
	logFiledes = open("/tmp/ger.pid", O_WRONLY | O_CREAT, MODE);
	if (logFiledes < 0)
	{
		perror("Error opening/creating file /tmp/ger.pid");
		return 5;
	}
	
	pthread_t mainThread;
	if (pthread_create(&mainThread, NULL, mainThreadFunction, NULL) != 0)
	{
		perror("Error creating main thread");
		return 6;
	}

	pthread_join(mainThread, NULL);
	
	if (close(rejectedFiledes) != 0)
		perror("Error closing FIFO /tmp/rejected");
	if (close(logFiledes) != 0)
		perror("Error closing file /tmp/ger.pid");
	if (unlink("/tmp/rejected") != 0)
		perror("Error deleting FIFO /tmp/rejected");

	printf("Received requests: %d (%d Male, %d Female)\n",
					received, receivedMale, receivedFemale);
	printf("Rejected requests:  %d (%d Male, %d Female)\n",
				rejected, rejectedMale, rejectedFemale);
	printf("Served requests: %d (%d Male, %d Female)\n",
				served, servedMale, servedFemale);

	return 0;
}

void* mainThreadFunction(void* arg)
{
	int readNo;
	struct request_t req;
	while ((readNo = read(entryFiledes, &req, sizeof(req))) != -1)
	{
		if (read > 0)
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
				if (pthread_create(&newOccupiedSlot, NULL, occupiedSlot, req.duration) != 0)
				{
					perror("Error creating new occupied slot tread");
					return 7;
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
					if (pthread_create(&newOccupiedSlot, NULL, occupiedSlot, req.duration) != 0)
					{
						perror("Error creating new occupied slot tread");
						return 7;
					}
					//pthread_join(newOccupiedSlot , NULL);
				
					writeRequestToLog(req, "SERVED");
					served++;
					if (req.gender == 'M')
						servedMale++;
					else
						servedFemale++;
				}
				else //put in queue and wait for free slot
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
	
	return NULL;
}

void* occupiedSlot(void* arg)
{
	//TODO wait the time (received as arg) and close
}

void writeRequestToLog(struct request_t req, char* type)
{
	char info[200];
	float currTime = (clock() - startTime) / 1000;
	sprintf(info, "%.2f - %6d - %?????d - %3d: %c - %3d - %s\n",		///////////////////////
			currTime, getppid(), ?????, req.serial,						///////////////////////
			req.gender, req.duration, type);
	if (write(logFiledes, info, strlen(info) + 1) <= 0)
		perror("Error writing to file tmp/ger.pid");
}

#ifndef _REQUEST_H
#define _REQUEST_H

#define MALE 'M'
#define FEMALE 'F'

typedef struct request_t{
	int serial;
	char gender;
	int duration;
	int timesRejected;
} request_t;

#endif

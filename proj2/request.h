#ifndef _REQUEST_H
#define _REQUEST_H

/*This file contains functions and definitions
common to both sauna.c and generator.c*/

#define MODE 0770
#define MALE 'M'
#define FEMALE 'F'
#define MICRO_TO_MILLISECONDS 1000
#define SECOND_TO_NANO 1e9
#define NANO_TO_MILLI 1e6

/**
 * This struct holds information about a request,
 * that is, its serial number, gender, duration
 * in milliseconds and the number of times it was
 * rejected
 */
typedef struct request_t{
	int serial;
	char gender;
	int duration;
	int timesRejected;
} request_t;

/**
 * Returns the number of milliseconds elapsed
 * since the start of the program
 * @param start time of the program at the time it started, in nanoseconds
 * @return the time elapsed in milliseconds, with decimal places
 */
float getTimeMilliseconds(long start)
{
	struct timespec tm;
	timespec_get(&tm, TIME_UTC);
	long nanoTime = tm.tv_nsec + tm.tv_sec * SECOND_TO_NANO;
	long relativeTime = nanoTime - start;
	float milli = (float)relativeTime / NANO_TO_MILLI;
	return milli;
}

#endif

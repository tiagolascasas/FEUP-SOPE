#include <stdio.h>

unsigned int numberOfRequests;
unsigned int maxUsage;

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		printf("Usage: generator <number of requests> <max time per usage>\n");
		return 1;
	}
	sscanf(argv[1], "%u", &numberOfRequests);
	sscanf(argv[2], "%u", &maxUsage);

	printf("%d %d\n", numberOfRequests, maxUsage);

	return 0;
}

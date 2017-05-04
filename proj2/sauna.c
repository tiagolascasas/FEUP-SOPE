#include <stdio.h>

unsigned int numberOfSlots;

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("Wrong number of arguments provided\n");
		printf("Usage: sauna <number of slots>\n");
		return 1;
	}
	sscanf(argv[1], "%u", &numberOfSlots);

	return 0;
}

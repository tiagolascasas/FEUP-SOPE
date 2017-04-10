#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OCTAL 8

int nameFlag = 0;
int typeFlag = 0;
int permFlag = 0;

int printFlag = 0;
int deleteFlag = 0;
int execFlag = 0;

char* path;
char* filename;
char type;
int perm;
char** execCommand;

void printUsage();

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		printUsage();
		return 1;
	}

	if (strcmp(argv[1], "-name") == 0)
	{
		nameFlag = 1;
		filename = argv[2];
	}
	else if (strcmp(argv[1], "-type") == 0)
	{
		typeFlag = 1;
		type = argv[2][0];
	}
	else if (strcmp(argv[1], "-perm") == 0)
	{
		permFlag = 1;
		perm = strtol(argv[2], NULL, OCTAL);
	}
	else
	{
		printUsage();
		return 2;
	}

	if (strcmp(argv[3], "-print") == 0)
	{
		printFlag = 1;
	}
	else if (strcmp(argv[3], "-delete") == 0)
	{
		deleteFlag = 1;
	}
	else if (strcmp(argv[3], "-exec") == 0)
	{
		execFlag = 1;
		execCommand = argv + 4;
	}
	else
	{
		printUsage();
		return 3;
	}


	return 0;
}

void printUsage()
{
	printf("Usage: sfind [-name string | -type c | -perm mode] [-print | -delete | -exec command]\n");
	return;
}

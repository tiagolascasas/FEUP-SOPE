#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

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
int analyzeDirectory(char* path);
void analyzeFiles(char* path);
void processFile(char* path);

int main(int argc, char** argv)
{
	if (argc < 5)
	{
		printUsage();
		return 1;
	}

	path = argv[1];

	if (strcmp(argv[2], "-name") == 0)
	{
		nameFlag = 1;
		filename = argv[3];
	}
	else if (strcmp(argv[2], "-type") == 0)
	{
		typeFlag = 1;
		type = argv[3][0];
	}
	else if (strcmp(argv[2], "-perm") == 0)
	{
		permFlag = 1;
		perm = strtol(argv[3], NULL, OCTAL);
	}
	else
	{
		printUsage();
		return 2;
	}

	if (strcmp(argv[4], "-print") == 0)
	{
		printFlag = 1;
	}
	else if (strcmp(argv[4], "-delete") == 0)
	{
		deleteFlag = 1;
	}
	else if (strcmp(argv[4], "-exec") == 0)
	{
		execFlag = 1;
		execCommand = argv + 5;
	}
	else
	{
		printUsage();
		return 3;
	}

	analyzeDirectory(argv[1]);

	return 0;
}

void printUsage()
{
	printf("Usage: sfind directory [-name string|-type c|-perm mode] [-print|-delete|-exec command]\n");
	return;
}

int analyzeDirectory(char* path)
{
	DIR* dir;
	dir = opendir(path);
	struct dirent* entry;
	pid_t pids[128];
	int pidsSize = 0;

	if (dir != NULL)
	{
		while (entry = readdir(dir))
		{
			if (entry->d_type == DT_DIR)
			{
				if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
					continue;
				pid_t p = fork();
				if (p == 0)
				{
					char dirPath[PATH_MAX];
					sprintf(dirPath, "%s/%s", path, entry->d_name);
					return analyzeDirectory(dirPath);
				}
				else
				{
					pidsSize++;
					pids[pidsSize - 1] = p;
				}
			}
		}
		closedir(dir);
	}
	else
	{
		printf("Cannot open directory %s\n", path);
		return 3;
	}

	analyzeFiles(path);

	int i;
	for (i = 0; i < pidsSize; i++)
	{
		int status;
		waitpid(pids[i], &status, 0);
	}
	return 0;
}

void analyzeFiles(char* path)
{
	DIR* dir;
	dir = opendir(path);
	struct dirent* entry;
	char filepath[PATH_MAX];

	if (dir != NULL)
	{
		while (entry = readdir(dir))
		{
			if (nameFlag)
			{
				if (entry->d_type == DT_REG &&
					strcmp(entry->d_name, filename) == 0)
				{
					sprintf(filepath, "%s/%s", path, entry->d_name);
					processFile(filepath);
				}
			}
			else if (typeFlag)
			{
				if (entry->d_type == DT_REG && type == 'f')
				{
					sprintf(filepath, "%s/%s", path, entry->d_name);
					processFile(filepath);
				}
				else if (entry->d_type == DT_DIR && type == 'd')
				{
					sprintf(filepath, "%s/%s", path, entry->d_name);
					processFile(filepath);
				}
				else if (entry->d_type == DT_LNK && type == 'l')
				{
					sprintf(filepath, "%s/%s", path, entry->d_name);
					processFile(filepath);
				}
			}
			else if (permFlag)
			{
			//	if (entry->)
			}
			else continue;
		}
		closedir(dir);
	}
	return;
}

void processFile(char* path)
{
	if (printFlag)
		printf("%s\n", path);
	else if (deleteFlag)
		remove(path);
	else if (execFlag)
	{

	}
	return;
}

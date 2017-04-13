#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

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
char** filesFound;
int idx;

void printUsage();
int parseArgs(char** argv);
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

	if (parseArgs(argv) != 0)
		return 2;

	if (execFlag)
	{
		filesFound = (char**)calloc(sizeof(char*), 256);
		filesFound[0] = execCommand[0];
		filesFound[1] = execCommand[1];
		idx = 2;
	}

	analyzeDirectory(argv[1]);

	if (execFlag)
	{
		int i = 1;
		while (execCommand[i] != NULL)
		{
			filesFound[idx] = execCommand[i];
			idx++;
			i++;
		}
		pid_t p = fork();
		if (p == 0)
			execvp(execCommand[0], filesFound);
		else
		{
			int status;
			waitpid(p, &status, 0);
		}
	}

	return 0;
}

/**
 * Sets the global argument flags based on the cli
 * arguments of the program
 *
 * @param argv the array of strings passed as an argument to main()
 * @return 0 if the parsing was successful, not 0 otherwise
 */
int parseArgs(char** argv)
{
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
		printf("perm: %d\n", perm);
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
	return 0;
}

/**
 * Prints the program usage
 */
void printUsage()
{
	printf("Usage: sfind directory [-name string|-type c|-perm mode] [-print|-delete|-exec command]\n");
	return;
}

/**
 * Analyzes recursively all directories reachable
 * by the path argument, launching a new process
 * for each new directory and scanning each directory's
 * files according to the global argument flags
 *
 * @param path the path to the parent directory
 * @return 0 if the directory was successfully read, not 0 otherwise
 */
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

/**
 * Analyzes all the files of a single directory based
 * on the name, type and perm global arguments
 *
 * @param path the path to the directory
 */
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
				sprintf(filepath, "%s/%s", path, entry->d_name);
				struct stat st;
				stat(filepath, &st);
				if ((st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == perm)
					processFile(filepath);
			}
			else continue;
		}
		closedir(dir);
	}
	return;
}

/**
 * Processes a single file, based on the print,
 * delete and exec global arguments
 *
 * @param path the path to the file
 */
void processFile(char* path)
{
	if (printFlag)
		printf("%s\n", path);
	else if (deleteFlag)
		remove(path);
	else if (execFlag)
	{
		char* heapPath = (char*)calloc(sizeof(char*), strlen(path) + 1);
		strcpy(heapPath, path);
		filesFound[idx] = heapPath;
		idx++;
	}
	return;
}
/* To handle the Ctrl + C signal*/


void sigint_handler(int signo)
{

	char c;

	signal (signo, SIG_IGN);

  printf("Are you sure you want to terminate (Y/N)? \n");
	c = getchar();

	if (c == 'Y'|| c == 'y'){
		exit(0);
	}
	else{
		signal (SIGINT, sigint_handler);
		getchar();

	}
}

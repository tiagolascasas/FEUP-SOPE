#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define OCTAL 8
#define MODE 0770

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
int idx;
int filedes;

void printUsage();
int parseArgs(char** argv);
int analyzeDirectory(char* path);
void analyzeFiles(char* path);
void processFile(char* path);
int readline(int fd, char *str);

void sigint_parent_handler(int signo);
void sigint_child_handler(int signo);
void sigusr1_handler(int signo);

int main(int argc, char** argv)
{
	signal(SIGINT, sigint_parent_handler);
	signal(SIGUSR1, sigusr1_handler);
	if (argc < 5)
	{
		printUsage();
		return 1;
	}

	if (parseArgs(argv) != 0)
		return 2;

	if (execFlag)
	{
		filedes = open("files.txt", O_RDWR | O_CREAT, MODE);

		close(filedes);
	}

	analyzeDirectory(argv[1]);

	if (execFlag)
	{
		char** exec = (char**)calloc(sizeof(char*), 2048);
		filedes = open("files.txt", O_RDONLY);
		int i = 0;
		while (strncmp(argv[i + 5], "{}", 4) != 0)
		{
			exec[i] = argv[i + 5];
			i++;
		}
		idx = i + 1;
		char str[PATH_MAX];
		while (readline(filedes, str))
		{
			exec[i] = str;
			i++;
		}
		while (argv[idx + 5] != NULL)
		{
			exec[i] = argv[idx + 5];
			i++;
			idx++;
		}
		close(filedes);

		pid_t p = fork();
		if (p == 0)
		{
			execvp(exec[0], exec);
			printf("Failed to execute %s\n", exec[0]);
			return 1;
		}
		else if (p > 0)
		{
			int status;
			waitpid(p, &status, 0);
		}
		else
			perror("Failed to create process\n");
	}
	remove("files.txt");
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
		while ((entry = readdir(dir)))
		{
			if (entry->d_type == DT_DIR)
			{
				if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
					continue;
				pid_t p = fork();
				if (p == 0)
				{
					signal(SIGINT, sigint_child_handler);
					char dirPath[PATH_MAX];
					sprintf(dirPath, "%s/%s", path, entry->d_name);
					exit(analyzeDirectory(dirPath));
				}
				else if (p > 0)
				{
					pidsSize++;
					pids[pidsSize - 1] = p;
				}
				else
					perror("Failed to create process");
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
		while ((entry = readdir(dir)))
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
		filedes = open("files.txt", O_WRONLY | O_APPEND);
		write(filedes, path, strlen(path) + 1);
		close(filedes);
	}
	return;
}

/**
 * Signal handler for SIGINT, to be used on the
 * starting parent process only. It shows the
 * prompt and blocks all child processes, and if
 * it is to continue running the program, it sends
 * SIGUSR1 to all child processes, signaling them
 * to resume operations.
 *
 * @param signo the signal received by the handler
 */
void sigint_parent_handler(int signo)
{
	char c;
  	printf("Are you sure you want to terminate (Y/N)? \n");
	c = getchar();

	if (c == 'Y'|| c == 'y'){
		exit(0);
	}
	else{
		kill(0, SIGUSR1);
	}
}

/**
 * Signal handler for SIGINT, to be used on child
 * processes. It waits until another signal (SIGUSR1)
 * is received
 *
 * @param signo the signal received by the handler
 */
void sigint_child_handler(int signo)
{
	pause();
}

/**
 * Signal handler for SIGUSR1
 *
 * @param signo the signal received by the handler
 */
void sigusr1_handler(int signo)
{
	return;
}

/**
 * Reads a null-terminated string from
 * a file descriptor (taken and adapted 
 * from practical classes exercises)
 *
 *@param fd the file descriptor
 *@param str pointer to hold the result
 *
 *@return the size of the string that was read
 */
int readline(int fd, char *str)
{
	int n;
	do 
	{
		n = read(fd,str,1);
	} while (n>0 && *str++ != '\0');
	return (n>0);
}

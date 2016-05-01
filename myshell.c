//
// mini shell implementation
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/types.h>

typedef int bool;
#define TRUE 1
#define FALSE 0


int run_piped(char** arglist1, int count1, char** arglist2, int count2);
int run(char** arglist, int count, bool run_in_background);

int process_arglist(int count, char** arglist)
{
	bool run_in_background = FALSE;
	if (strncmp("&", arglist[count - 1], 1) == 0) {
		arglist[count - 1] = NULL;
		count -= 1;
		run_in_background = TRUE;
	}

	for (int i = 0; i < count; ++i)
	{
		char* token = arglist[i];
		if (strncmp("|", token, 1) == 0) {
			arglist[i] = NULL;
			return run_piped(arglist, i, arglist + i + 1, count - i - 1);
		}
	}

	return run(arglist, count, run_in_background);
}

int run_piped(char** arglist1, int count1, char** arglist2, int count2)
{
	return 1;
}

int run(char** arglist, int count, bool run_in_background)
{
	pid_t pid = fork();
	if (pid == -1) {
		perror("fork failed");
		return 0;
	} else if (pid == 0) {
		// This is the child process
		execvp(arglist[0], arglist);
		// Note: if execvp returned, it surely failed and returned -1,
		// hence no need to check it's return value.
		perror("executing command failed");
		return 0;
	}
	assert(pid != 0);
	// This is the parent process.

	if (!run_in_background) {
		if (waitpid(pid, NULL, 0) == -1) {
			perror("waiting on child failed");
			return 0;
		}
	} else {
		//TODO: ???
	}

	return 1;
}

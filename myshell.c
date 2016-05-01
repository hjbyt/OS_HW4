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
#include <errno.h>
#include <error.h>

//
// Definitions
//

typedef int bool;
#define TRUE 1
#define FALSE 0


// Exit with an error message
#define ERROR(...) error(EXIT_FAILURE, errno, __VA_ARGS__)

//TODO: what to do if the program is finished (EOL is received),
// and there is a background process running?
//TODO: exit "cleanly" ? (replace calls to exits with closing handles and return 0)

//
// Forward Declarations
//

int run_piped(char** arglist1, char** arglist2);
int run(char** arglist, bool run_in_background);

//
// Functions
//

int process_arglist(int count, char** arglist)
// Process and execute the command line given by arglist.
{
	// TODO: try "killing zombies" by calling waitpid(-1, NULL, WNOHANG) repeatedly ?

	// Check if this a piped command, and if so run it as such.
	for (int i = 0; i < count; ++i)
	{
		char* token = arglist[i];
		if (strncmp("|", token, 1) == 0) {
			arglist[i] = NULL;
			return run_piped(arglist, arglist + i + 1);
		}
	}

	// we have a normal/background command

	// Check if the command should run in background
	bool run_in_background = FALSE;
	if (strncmp("&", arglist[count - 1], 1) == 0) {
		arglist[count - 1] = NULL;
		count -= 1;
		run_in_background = TRUE;
	}

	// run it
	return run(arglist, run_in_background);
}

int run(char** arglist, bool run_in_background)
// Run the command given in arglist.
// Wait until the command is finished, unless run_in_background is TRUE.
{
	// Fork to create child process
	pid_t pid = fork();
	if (pid == -1) {
		ERROR("fork failed");
	} else if (pid == 0) {
		// This is the child process
		// Execute child command
		execvp(arglist[0], arglist);
		// Note: if execvp returned, it surely failed and returned -1,
		// hence no need to check it's return value.
		ERROR("Error in executing command %s", arglist[0]);
	}
	assert(pid != 0);
	// This is the parent process.

	// If needed, wait for child to finish.
	if (!run_in_background) {
		if (waitpid(pid, NULL, 0) == -1) {
			ERROR("waiting on child failed");
		}
	} else {
		//TODO: ???
	}

	return 1;
}

int run_piped(char** arglist1, char** arglist2)
// Run the commands given in arglist1, arglist2,
// while piping the output of the first to the input of the second.
// Wait until the commands are finished.
{
	int pipefd[2];

	// Create pipe for the children processes
	if (pipe(pipefd) == -1) {
		ERROR("failed creating pipe");
	}

	// Fork first child
	pid_t pid1 = fork();
	if (pid1 == -1) {
		ERROR("fork failed");
	} else if (pid1 == 0) {
		// This is the child process 1
		// Replace stdout with the pipe writing end
		if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
			ERROR("dup2 on stdout failed");
		}
		// Close unneeded pipe fds
		close(pipefd[0]);
		close(pipefd[1]);
		// Execute child command
		execvp(arglist1[0], arglist1);
		// Note: if execvp returned, it surely failed and returned -1,
		// hence no need to check it's return value.
		ERROR("Error in executing command %s", arglist1[0]);
	}
	assert(pid1 != 0);
	// This is the parent process.

	// Fork second child
	pid_t pid2 = fork();
	if (pid2 == -1) {
		ERROR("fork failed");
	} else if (pid2 == 0) {
		// This is the child process
		// Replace stdin with the pipe reading end
		if (dup2(pipefd[0], STDIN_FILENO) == -1) {
			ERROR("dup2 on stdin failed");
		}
		// Close unneeded pipe fds
		close(pipefd[0]);
		close(pipefd[1]);
		// Execute child command
		execvp(arglist2[0], arglist2);
		// Note: if execvp returned, it surely failed and returned -1,
		// hence no need to check it's return value.
		ERROR("Error in executing command %s", arglist2[0]);
	}
	assert(pid2 != 0);
	// This is the parent process.

	// Close unneeded pipe fds
	close(pipefd[0]);
	close(pipefd[1]);

	// Wait for children to finish
	if (waitpid(pid1, NULL, 0) == -1) {
		ERROR("waiting on child failed (%s)", arglist1[0]);
	}
	if (waitpid(pid2, NULL, 0) == -1) {
		ERROR("waiting on child failed (%s)", arglist2[0]);
	}

	return 1;
}


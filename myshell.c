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
// Globals
//

struct sigaction chld_sig_handler;
struct sigaction int_sig_handler;

//
// Forward Declarations
//

int run_piped(char** arglist1, char** arglist2);
int run(char** arglist, bool run_in_background);
void register_chld_signal_handler();
void restore_chld_signal_handler();
void register_int_signal_handler();
void restore_int_signal_handler();


//
// Functions
//

int process_arglist(int count, char** arglist)
// Process and execute the command line given by arglist.
{
	// Check if this a piped command, and if so run it as such.
	for (int i = 0; i < count; ++i)
	{
		char* token = arglist[i];
		if (strncmp("|", token, 1) == 0) {
			arglist[i] = NULL;
			return run_piped(arglist, arglist + i + 1);
		}
	}

	// We have a normal/background command

	// Check if the command should run in background
	bool run_in_background = FALSE;
	if (strncmp("&", arglist[count - 1], 1) == 0) {
		arglist[count - 1] = NULL;
		count -= 1;
		run_in_background = TRUE;
	}

	// Run it
	return run(arglist, run_in_background);
}

int run(char** arglist, bool run_in_background)
// Run the command given in arglist.
// Wait until the command is finished, unless run_in_background is TRUE.
{
	if (!run_in_background) {
		// Suppress SIGINTs
		register_int_signal_handler();
	}

	// Fork to create child process
	pid_t pid = fork();
	if (pid == -1) {
		ERROR("fork failed");
	} else if (pid == 0) {
		// This is the child process

		// Restore original handlers
		if (!run_in_background) {
			restore_int_signal_handler();
		}
		restore_chld_signal_handler();

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
		// Unsuppress SIGINTs
		restore_int_signal_handler();
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

	// Suppress SIGINTs
	register_int_signal_handler();

	// Fork first child
	pid_t pid1 = fork();
	if (pid1 == -1) {
		ERROR("fork failed");
	} else if (pid1 == 0) {
		// This is the child process 1

		// Restore original handlers
		restore_int_signal_handler();
		restore_chld_signal_handler();

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

		// Restore original handlers
		restore_int_signal_handler();
		restore_chld_signal_handler();

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

	// Unsuppress SIGINTs
	restore_int_signal_handler();

	return 1;
}

void register_chld_signal_handler()
// Register handler that will "reap" zombie child processes.
{
	// NOTE: this "already_registered" unnecessary hack could be prevented
	// if editing input.c was allowed.
	// (this function could be called once, before the loop)
	static bool already_registered = FALSE;
	if (already_registered) {
		return;
	}

	struct sigaction child_sigaction;
	//NOTE: SIG_IGN has a special behavior for SIGCHLD,
	// that makes runtime automatically reap children zombie processes for us.
	// Alternatively, a function the loops over wait() with WNOHANG could be registered.
	child_sigaction.sa_handler = SIG_IGN;
	if (sigemptyset(&child_sigaction.sa_mask) == -1) {
		ERROR("Error calling sigemptyset");
	}
	child_sigaction.sa_flags = 0;

	if (sigaction(SIGCHLD, &child_sigaction, NULL) == -1) {
		ERROR("Error, sigaction failed");
	}
	already_registered = TRUE;
}

void restore_chld_signal_handler()
// Restore previous SIGCHLD handler.
{
	if (sigaction(SIGCHLD, &chld_sig_handler, NULL) == -1) {
		ERROR("Error, sigaction failed");
	}
}

void register_int_signal_handler()
// Register handler that will suppress SIGINT signals.
{
	struct sigaction sigint_action;
	sigint_action.sa_handler = SIG_IGN;
	if (sigemptyset(&sigint_action.sa_mask) == -1) {
		ERROR("Error calling sigemptyset");
	}
	sigint_action.sa_flags = 0;

	if (sigaction(SIGINT, &sigint_action, &int_sig_handler) == -1) {
		ERROR("Error, sigaction failed");
	}
}

void restore_int_signal_handler()
// Restore previous SIGINT handler.
{
	if (sigaction(SIGINT, &int_sig_handler, NULL) == -1) {
		ERROR("Error, sigaction failed");
	}
}


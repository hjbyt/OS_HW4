//
// mini shell implementation
//

#include <stdio.h>
#include <string.h>

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
	return 1;
}

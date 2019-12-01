#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "helper.h"

struct state runError(void *args, char **err, int *ret)
{
	(void) args;
	(void) ret;

	puts(*err);
	*err = NULL;
	return STATE_EXIT;
}

bool stateEqual(struct state state1, struct state state2)
{
	return state1.function == state2.function && strcmp(state1.name, state2.name) == 0;
}

// todo: catch sigint/sigterm, set some sort of boolean that states monitor to
// know that they should exit

bool stateValid(struct state state)
{
	for (unsigned int tmp = 0; tmp < NUM_STATES; tmp++) {
		if (stateEqual(state, ALL_STATES[tmp])) {
			return true;
		}
	}
	return false;
}

int stateRun()
{
	char *err = NULL;
	int ret = 0;

	struct state state = INITIAL_STATE;
	while (true) {
		assert(err == NULL || stateEqual(state, STATE_ERROR));
		assert(ret == 0 || stateEqual(state, STATE_ERROR) || stateEqual(state, STATE_EXIT));
		if (stateEqual(state, STATE_EXIT)) {
			return ret;
		}

		unsigned long long tPre = getTimeInMs();
		struct state state_new = state.function(state.args, &err, &ret);
		unsigned long long tPost = getTimeInMs();
		if (state.shouldFreeArgs) {
			free(state.args);
		}
		if (!stateValid(state_new)) {
			printf("ERROR: state %s returned an invalid state: {%p, %p}\n", state.name, state_new.name, state_new.function);
			return EXIT_FAILURE;
		}

		unsigned long long delta = tPost - tPre;
		printf("State %s ran for %llu seconds and set state to %s\n", state.name, delta/1000, state_new.name);

		state = state_new;
	}
}

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"

struct state runError(char **err, int *ret)
{
	puts(*err);
	*err = NULL;
	return STATE_EXIT;
}

bool stateEqual(struct state state1, struct state state2)
{
	return state1.function == state2.function && strcmp(state1.name, state2.name) == 0;
}

bool stateValid(struct state state)
{
	for (int tmp = 0; tmp < NUM_STATES; tmp++) {
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

		struct state state_new = state.function(&err, &ret);
		if (!stateValid(state_new)) {
			printf("ERROR: state %s returned an invalid state: {%p, %p}\n", state.name, state_new.name, state_new.function);
			return EXIT_FAILURE;
		}
		state = state_new;
	}
}

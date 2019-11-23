#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct state;
typedef struct state (*stateFunction) (char **err, int *ret);
struct state {
	char *name;
	stateFunction function;
};

#define STATE_EXIT ((struct state) { .name="exit", .function=NULL, })

struct state run_error(char **err, int *ret)
{
	puts(*err);
	*err = NULL;
	return STATE_EXIT;
}
#define STATE_ERROR ((struct state) { .name="error", .function=run_error, })

struct state run_low_power (char **err_msg, int *ret)
{
	while (true) {
		/*Frame frame;
		int err;
		err = sensor_getFrame(SENSOR, &frame);
		if (err) {
			*err_msg = "Reading frame from sensor failed";
			*ret = EXIT_FAILURE;
			return STATE_ERR;
		}*/
	}
}
#define STATE_LOW_POWER ((struct state) { .name="low-power", .function=run_low_power, })

struct state run_starting()
{
	//auto time lastMotion = now();

	while (true) {
		/*Frame f = ...;

		if (motionCheck(f)) {
			lastMotion = now();
		}*/
	}
}
#define STATE_STARTING ((struct state) { .name="starting", .function=run_starting, })

struct state run_counting()
{





}
#define STATE_COUNTING ((struct state) { .name="counting", .function=run_counting, })

const struct state ALL_STATES[] = { STATE_EXIT, STATE_ERROR, STATE_LOW_POWER, STATE_STARTING, STATE_COUNTING };
const size_t NUM_STATES = sizeof(ALL_STATES) / sizeof(struct state);

bool stateEqual(struct state state1, struct state state2)
{
	return state1.name == state2.name && state1.function == state2.function;
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

int run()
{
	char *err = NULL;
	int ret = 0;

	struct state state = STATE_LOW_POWER;
	while (true) {
		assert(err == NULL || stateEqual(state, STATE_ERROR));
		assert(ret == 0 || stateEqual(state, STATE_ERROR) || stateEqual(state, STATE_EXIT));
		if (stateEqual(state, STATE_EXIT)) {
			return ret;
		}

		struct state state_new = state.function(&err, &ret);
		if (!stateValid(state_new)) {
			printf("ERROR: %s returned an invalid state", state.name);
			return EXIT_FAILURE;
		}
		state = state_new;
	}
}

int main()
{
	// TODO:
	// sensorInitialize(SENSOR);

	return run();
}

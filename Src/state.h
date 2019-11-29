#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
#include <stdlib.h>


struct state;
typedef struct state (*stateFunction) (void *args, char **err, int *ret);
struct state {
	char *name;
	stateFunction function;
	void *args;

	// if true, stateRun will free `args` before advancing to the next state.
	bool shouldFreeArgs;
};

bool stateEqual(struct state state1, struct state state2);
bool stateValid(struct state state);
int stateRun();


#define STATE_EXIT ((struct state) { .name="exit", .function=NULL, .args=NULL, .shouldFreeArgs=false, })

struct state runError(void *args, char **err, int *ret);
#define STATE_ERROR ((struct state) { .name="error", .function=runError, .args=NULL, .shouldFreeArgs=false, })

struct state runLowPower (void *args, char **err_msg, int *ret);
#define STATE_LOW_POWER ((struct state) { .name="low-power", .function=runLowPower, .args=NULL, .shouldFreeArgs=false, })

struct state runCounting(void *args, char **err, int *ret);
#define STATE_COUNTING ((struct state) { .name="counting", .function=runCounting, .args=NULL, .shouldFreeArgs=false, })

struct state runStarting(void *args, char **err, int *ret);
#define STATE_STARTING ((struct state) { .name="starting", .function=runStarting, .args=NULL, .shouldFreeArgs=false, })

struct state runRecording(void *args, char **err_msg, int *retStatus);
#define STATE_RECORDING ((struct state) { .name="recording", .function=runRecording, .args=NULL, .shouldFreeArgs=false, })

static const struct state ALL_STATES[] = { STATE_EXIT, STATE_ERROR, STATE_LOW_POWER, STATE_STARTING, STATE_COUNTING, STATE_RECORDING };

static const size_t NUM_STATES = sizeof(ALL_STATES) / sizeof(struct state);
#define INITIAL_STATE STATE_LOW_POWER

#endif

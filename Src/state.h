#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
#include <stdlib.h>


struct state;
typedef struct state (*stateFunction) (char **err, int *ret);
struct state {
	char *name;
	stateFunction function;
};

bool stateEqual(struct state state1, struct state state2);
bool stateValid(struct state state);
int stateRun();


#define STATE_EXIT ((struct state) { .name="exit", .function=NULL, })

struct state runError(char **err, int *ret);
#define STATE_ERROR ((struct state) { .name="error", .function=runError, })

struct state runLowPower (char **err_msg, int *ret);
#define STATE_LOW_POWER ((struct state) { .name="low-power", .function=runLowPower, })

struct state runCounting(char **err, int *ret);
#define STATE_COUNTING ((struct state) { .name="counting", .function=runCounting, })

struct state runStarting(char **err, int *ret);
#define STATE_STARTING ((struct state) { .name="starting", .function=runStarting, })




static const struct state ALL_STATES[] = { STATE_EXIT, STATE_ERROR, STATE_LOW_POWER, STATE_STARTING, STATE_COUNTING };
static const size_t NUM_STATES = sizeof(ALL_STATES) / sizeof(struct state);
#define INITIAL_STATE STATE_LOW_POWER

#endif

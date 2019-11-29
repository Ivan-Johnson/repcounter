

#include "state.h"
#include "state_counting.h"

struct state runCounting(void *args, char **err_msg, int *ret)
{
	*err_msg = "ERROR: state counting has not been implemented yet";
	*ret = EXIT_FAILURE;
	return STATE_ERROR;
}



#include "state.h"

struct state runCounting(char **err_msg, int *ret)
{
	*err_msg = "ERROR: state counting has not been implemented yet";
	*ret = EXIT_FAILURE;
	return STATE_ERROR;
}
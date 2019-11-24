

#include "state.h"

struct state runLowPower (char **err_msg, int *ret)
{
	*err_msg = "ERROR: state low power has not been implemented yet";
	*ret = EXIT_FAILURE;
	return STATE_ERROR;
}

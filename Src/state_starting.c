
#include "state.h"

struct state runStarting(char **err_msg, int *ret)
{
	*err_msg = "ERROR: state starting has not been implemented yet";
	*ret = EXIT_FAILURE;
	return STATE_ERROR;
	//auto time lastMotion = now();

	while (true) {
		/*Frame f = ...;

		if (motionCheck(f)) {
			lastMotion = now();
		}*/
	}
}

#ifndef HELPER_H
#define HELPER_H

#include <sys/time.h>

static unsigned long long getTimeInMs()
{
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);

	// be careful to minimize the risk of overflows
	return (unsigned long long) currentTime.tv_sec  * (unsigned long long) 1000 +
	       (unsigned long long) currentTime.tv_usec / (unsigned long long) 1000;
}


#endif

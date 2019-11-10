
#CFLAGS += -Wfatal-errors -Werror -Wconversion -Wall -Wextra

# for development
#CFLAGS += -D DEBUG -O0 -ggdb -fno-inline

# for release
#CFLAGS += -O3


CFLAGS += -fPIC -pedantic -g -D_BSD_SOURCE   -mssse3 -pthread -O3 -DNDEBUG  
#CFLAGS += -DBUILD_EASYLOGGINGPP -DELPP_NO_DEFAULT_LOG_FILE -DELPP_THREAD_SAFE -DHWM_OVER_XU -DRS2_USE_V4L2_BACKEND -DUNICODE

#todo: remove rdynamic?
LDFLAGS += -rdynamic
LDLIBS += -lGL -lGLU -lrt -lm -ldl -lX11 -l:librealsense2.so



all: rs-depth

rs-depth: rs-depth.o

.PHONY: clean
clean:
	rm -f *.o rs-depth


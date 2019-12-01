#makefile
#
#Defines how the project should be built
#
#Copyright(C) 2018-2019, Ivan Tobias Johnson
#
#LICENSE: GPL 2.0 only

APP=repcounter

SRC_DIR=Src
BIN_DIR_BASE=Bin



FFMPEG_LIBS=    libavdevice                        \
                libavformat                        \
                libavfilter                        \
                libavcodec                         \
                libswresample                      \
                libswscale                         \
                libavutil                          \

CFLAGS += -Wall -g
CFLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS)) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) $(LDLIBS)



#to cross-compile for windows, uncomment. Executables must be renamed to .exe
#CC = x86_64-w64-mingw32-gcc

CFLAGS += -I$(SRC_DIR) -pthread
CFLAGS += -D _GNU_SOURCE
CFLAGS += -Wfatal-errors -std=c99


#Controls how strict the compiler is. Zero is the most strict, larger values are
#more lenient. This value can be modified here, or make can be run as follows:
#
#make <target> STRICT=<value>

STRICT ?= 0

ifeq ($(shell test $(STRICT) -le 2; echo $$?),0)
	CFLAGS += -Werror
endif

ifeq ($(shell test $(STRICT) -le 1; echo $$?),0)
	CFLAGS += -Wconversion
endif

ifeq ($(shell test $(STRICT) -eq 0; echo $$?),0)
	CFLAGS += -Wall -Wextra
endif


OPTS_DEBUG = -D DEBUG -O0 -ggdb -fno-inline -fsanitize=address
OPTS_OPTIMIZED = -O3
OPTIMIZED ?= 0

CFLAGS += -D_DEFAULT_SOURCE -mssse3 -pthread
#CFLAGS += -DBUILD_EASYLOGGINGPP -DELPP_NO_DEFAULT_LOG_FILE -DELPP_THREAD_SAFE -DHWM_OVER_XU -DRS2_USE_V4L2_BACKEND -DUNICODE

#todo: remove rdynamic?
LDFLAGS += -rdynamic
LDLIBS += -lGL -lGLU -lrt -lm -ldl -lX11 -l:librealsense2.so -pthread

LDFLAGS_DEBUG = -fsanitize=address
LDFLAGS_OPTIMIZED =

ifeq ($(shell test $(OPTIMIZED) -eq 0; echo $$?),0)
	CFLAGS += $(OPTS_DEBUG)
	LDFLAGS += $(LDFLAGS_DEBUG)
else
	CFLAGS += $(OPTS_OPTIMIZED)
	LDFLAGS += $(LDFLAGS_OPTIMIZED)
endif




BIN_DIR=$(BIN_DIR_BASE)/$(OPTIMIZED)/$(STRICT)
SOURCES=$(wildcard $(SRC_DIR)/*.c)
HEADERS=$(wildcard $(SRC_DIR)/*.h)
OBJECTS=$(SOURCES:$(SRC_DIR)/%.c=$(BIN_DIR)/%.o)
DEPENDS=$(BIN_DIR)/.depends



.PHONY: all
all: $(BIN_DIR)/$(APP)
#symlink the built executable to $(BIN_DIR_BASE)/$(APP) for convinience
	ln -sf $(BIN_DIR:$(BIN_DIR_BASE)/%=%)/$(APP) $(BIN_DIR_BASE)/$(APP)

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)

.PHONY: CLEAN
CLEAN:
	rm -rf $(BIN_DIR_BASE)



$(BIN_DIR)/$(APP): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

$(BIN_DIR)/%.o: | $(BIN_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEPENDS): $(SOURCES) $(HEADERS) | $(BIN_DIR)
#todo: use -MQ and -MF instead of this pipe'd junk?
#gcc -MM generates a make file, but we need to add $(BIN_DIR) to the start of
#each line. Some lines are wrapped and indented with a space, so we don't need
#to prepend to lines that start with a space.
	$(CC) $(CFLAGS) -MM $(SOURCES) | sed -Ee 's!^([^ ])!$(BIN_DIR)/\1!' >$@

-include $(DEPENDS)

$(BIN_DIR):
	mkdir -p $@

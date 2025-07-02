PROGS:=bomber
PLATFORM:=$(shell uname -s)
CPP_FILES:=$(wildcard *.c) $(wildcard sound/$(PLATFORM)/*.c)
OBJ_FILES:=$(patsubst %.c,%.o,$(CPP_FILES))
CPPFLAGS:=-w -I./include -I/opt/X11/include -MMD -MF deps.d
LIBS:=-L/opt/X11/lib -lX11
FRAMEWORKS:=
ifeq ($(PLATFORM),Darwin)
	FRAMEWORKS+=-framework AudioUnit
endif

all: $(PROGS)

-include deps.d

$(PROGS): $(OBJ_FILES)
	$(CC) -o $(PROGS) $(notdir $(OBJ_FILES)) $(FRAMEWORKS) $(LIBS) $(CPPFLAGS)

%.o: %.c
	$(CC) -c $< $(CPPFLAGS)

.PHONY: daemon
daemon:
	$(MAKE) -C daemon

clean:
	$(RM) $(DEP_FILES) $(OBJ_FILES) $(PROGS)
	$(MAKE) -C daemon clean

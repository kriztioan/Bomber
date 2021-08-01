PROG:=bomber
PLATFORM:=$(shell uname -s)
CPP_FILES:=$(wildcard *.c) $(wildcard sound/$(PLATFORM)/*.c)
OBJ_FILES:=$(patsubst %.c,%.o,$(CPP_FILES))
CPPFLAGS:=-w -I./include -I/opt/X11/include
LIBS:=-L/opt/X11/lib -lX11
FRAMEWORKS:=
ifeq ($(PLATFORM),Darwin)
	FRAMEWORKS+=-framework AudioUnit
endif

$(PROG): $(OBJ_FILES)
	$(CC) -o $(PROG) $(notdir $(OBJ_FILES)) $(FRAMEWORKS) $(LIBS)

%.o: %.c
	$(CC) -c $< $(CPPFLAGS)

.PHONY: daemon
daemon:
	$(MAKE) -C daemon

clean:
	$(RM) *.o $(PROG)
	$(MAKE) -C daemon clean

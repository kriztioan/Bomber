PROGS:=../matcherd
CPP_FILES:=$(wildcard *.c)
OBJ_FILES:=$(patsubst %.c,%.o,$(CPP_FILES))
DEP_FILES:=deps.d
CPPFLAGS:=-w -MMD -MF $(DEP_FILES)

all: $(PROGS)

-include $(DEP_FILES)

$(PROG): $(OBJ_FILES)
	$(CC) -o $(PROG) $(OBJ_FILES)

%.o: %.cpp
	$(CC) -c $< $(CPPFLAGS)

clean:
	$(RM) $(DEP_FILES) $(OBJ_FILES) $(PROG)

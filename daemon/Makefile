PROG:=../matcherd
CPP_FILES:=$(wildcard *.c)
OBJ_FILES:=$(patsubst %.c,%.o,$(CPP_FILES))
CPPFLAGS:=-w

$(PROG): $(OBJ_FILES)
	$(CC) -o $(PROG) $(OBJ_FILES)

%.o: %.cpp
	$(CC) -c $< $(CPPFLAGS)

clean:
	$(RM) *.o $(PROG)
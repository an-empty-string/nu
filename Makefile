CC=gcc
CFLAGS=-I include/ -I hoedown/src/ -I libnucommon/ -Lhoedown/ -l:libhoedown.a -Llibnucommon -lnucommon -Wall -pedantic
#-Werror
OBJ=util.o post.o # unvo.o kg.o post.o nu.o cmds.o
OUTPUT=nu

default: nu

%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

nu: $(OBJ)
	gcc -o $(OUTPUT) $^ $(CFLAGS)
	-rm -f $(OBJ)

clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT)
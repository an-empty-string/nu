CFLAGS=-I include/ -I hoedown/src/ -I libnucommon/ -Wall -Werror -pedantic
LIBFLAGS=libhoedown.a -Llibnucommon -lnucommon
OBJ=util.o pageList.o post.o unvo.o kg.o post.o nu.o cmds.o strlist.o
OUTPUT=nu

default: nu

%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

nu: $(OBJ)
	$(CC) -o $(OUTPUT) $^ $(LIBFLAGS) $(CFLAGS)
	-rm -f $(OBJ)

debug: $(OBJ)
	$(CC) -o $(OUTPUT) $^ $(LIBFLAGS) $(CFLAGS) -g
	-rm -f $(OBJ)

clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT)
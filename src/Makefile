BIN_PROGRAMS = freehttpd

freehttpd_OBJECTS = freehttpd.o server.o protocol.o log.o htable.o connection.o utils.o http1.o
freehttpd_CFLAGS = -I. -I..

freehttpd: $(freehttpd_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(freehttpd_CFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: all clean

all: $(BIN_PROGRAMS)

clean:
	for file in $(BIN_PROGRAMS); do \
		rm -f $$file; \
	done

	rm -f *.o
	rm -f *.d
	rm -f *.a
	rm -f *.so
	rm -f *.so.*
	rm -f *~

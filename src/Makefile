BIN_PROGRAMS = freehttpd

freehttpd_OBJECTS = freehttpd.o server.o protocol.o log.o htable.o list.o http11.o
freehttpd_CFLAGS = -I. -I..

freehttpd: $(freehttpd_OBJECTS)
	@echo "   CCLD  $@" && $(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	@echo "   CC    $<" && $(CC) $(freehttpd_CFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: all clean

all: $(BIN_PROGRAMS)

clean:
	@for file in $(BIN_PROGRAMS); do \
		echo "   RM    $$file" && rm -f $$file; \
	done

	@echo "   RM    *.o" && rm -f *.o
	@echo "   RM    *.d" && rm -f *.d
	@echo "   RM    *.a" && rm -f *.a
	@echo "   RM    *.so" && rm -f *.so
	@echo "   RM    *.so.*" && rm -f *.so.*
	@echo "   RM    *~" && rm -f *~

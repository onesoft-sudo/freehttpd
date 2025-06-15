TESTS = htable_test
SH = sh

htable_test_SOURCES = htable_test.c ../src/htable.c
CFLAGS += -I../src

.PHONY: all clean check-valgrind

all: $(TESTS) clean

htable_test: $(htable_test_SOURCES:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)
	valgrind -s --leak-check=full ./$@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	for file in $(TESTS:.test=); do \
		echo "   RM    $$file" && rm -f $$file; \
	done
	rm -f *.o
	rm -f *.d
	rm -f *.a
	rm -f *.so
	rm -f *.so.*
	rm -f *~
	rm -f vgcore.*
	rm -f valgrind.log
	rm -f freehttpd.log

check-valgrind:
	$(SH) valgrind.sh

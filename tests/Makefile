TESTS = path_normalize_test htable_test
SH = sh

htable_test_SOURCES = htable_test.c ../src/htable.c
path_normalize_test_SOURCES = path_normalize_test.c ../src/utils.c
CFLAGS += -I../src -g

.PHONY: all clean check-valgrind

all: $(TESTS) clean

htable_test: $(htable_test_SOURCES:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)
	valgrind -s --leak-check=full ./$@

path_normalize_test: $(path_normalize_test_SOURCES:.c=.o)
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

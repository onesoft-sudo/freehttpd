TESTS = htable_test
SH = sh

htable_test_SOURCES = htable_test.c ../src/htable.c
list_test_SOURCES = list_test.c ../src/list.c
CFLAGS += -I../src

.PHONY: all clean

all: $(TESTS) clean

htable_test: $(htable_test_SOURCES:.c=.o)
	@echo "   CCLD  $@" && $(CC) -o $@ $^ $(LDFLAGS)
	@echo "   RUN   $@" && valgrind -s --leak-check=full ./$@

list_test: $(list_test_SOURCES:.c=.o)
	@echo "   CCLD  $@" && $(CC) -o $@ $^ $(LDFLAGS)
	@echo "   RUN   $@" && valgrind -s --leak-check=full ./$@

%.o: %.c
	@echo "   CC    $<" && $(CC) $(CFLAGS) -c $< -o $@

clean:
	@for file in $(TESTS:.test=); do \
		echo "   RM    $$file" && rm -f $$file; \
	done
	@echo "   RM    *.o" && rm -f *.o
	@echo "   RM    *.d" && rm -f *.d
	@echo "   RM    *.a" && rm -f *.a
	@echo "   RM    *.so" && rm -f *.so
	@echo "   RM    *.so.*" && rm -f *.so.*
	@echo "   RM    *~" && rm -f *~
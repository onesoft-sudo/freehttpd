resources_c = resources.c
resources_h = resources.h
resources_o = resources.o

RESOURCES = error.html index.html

.PHONY: all clean

all: $(resources_o) $(resources_h)

$(resources_o): $(resources_c)
	$(CC) $(CFLAGS) -c $< -o $@

$(resources_c): $(RESOURCES) $(resources_h)
	echo '#include "$(resources_h)"' > $@
	echo '#include <stddef.h>' >> $@
	echo '#include <stdint.h>' >> $@

	for res in $(RESOURCES); do \
	    name=$$(basename $$res | sed "s/\./_/g"); \
		xxd -n "resource_$$name" -i $$res | sed 's/unsigned int/__attribute__\(\(section\(".rodata"\)\)\) const size_t/g' | sed 's/unsigned char/__attribute__\(\(section\(".rodata"\)\)\) const char/g' >> $@; \
	done

	echo "__attribute__((section(\".rodata\"))) const struct resource_file global_resources[] = {" >> $@; \
	for res in $(RESOURCES); do \
	    name=$$(basename $$res | sed "s/\./_/g"); \
		name_len=$$(echo "$$name" | wc -c | sed 's/ //g'); \
		size=$$(stat -c %s $$res); \
		echo "  { .name = \"resource_$$name\", .name_len = $$name_len, .data = resource_$$name, .data_len = $$size, }," >> $@; \
	done; \
	echo "};" >> $@; \
	echo '__attribute__((section(".rodata"))) const size_t global_resources_size = sizeof(global_resources) / sizeof(global_resources[0]);' >> $@

$(resources_h): $(RESOURCES)
	echo '#ifndef GENERATED_RESOURCE_H' > $@
	echo '#define GENERATED_RESOURCE_H' >> $@
	echo '#include <stddef.h>' >> $@
	echo 'struct resource_file {' >> $@
	echo '  const char *name;' >> $@
	echo '  size_t name_len;' >> $@
	echo '  const char *data;' >> $@
	echo '  size_t data_len;' >> $@
	echo '};' >> $@
	for res in $(RESOURCES); do \
	    name=$$(basename $$res | sed "s/\./_/g"); \
		echo '__attribute__((section(".rodata"))) extern const size_t resource_'$$name'_len;' >> $@; \
		echo '__attribute__((section(".rodata"))) extern const char resource_'$$name'[];' >> $@; \
	done
	echo '__attribute__((section(".rodata"))) extern const struct resource_file global_resources[];' >> $@
	echo '__attribute__((section(".rodata"))) extern const size_t global_resources_size;' >> $@
	echo '#endif /* GENERATED_RESOURCE_H */' >> $@

clean:
	rm -f $(resources_o)
	rm -f $(resources_c)
	rm -f $(resources_h)
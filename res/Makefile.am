# This file is part of OSN freehttpd.
#
# Copyright (C) 2025  OSN Developers.
#
# OSN freehttpd is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OSN freehttpd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.

RESOURCES = index.html error.html
CLEANFILE = resources.c resources.h
BUILT_SOURCES = resources.c resources.h

EXTRA_DIST = \
	$(RESOURCES) \
	freehttpd.png \
	freehttpd_pb.png \
	freehttpd_web.png \
	freehttpd_http.png \
	icons/file.png \
	icons/folder.png \
	icons/image.png \
	icons/text.png

noinst_LIBRARIES = libresources.a
libresources_a_SOURCES = resources.c resources.h

resources.c: $(RESOURCES) resources.h
	@echo "  GEN      resources.c" && echo '#include "resources.h"' > $@; \
	echo '#include <stddef.h>' >> $@; \
	echo '#include <stdint.h>' >> $@; \
	for res in $(RESOURCES); do \
	    name=$$(basename $$res | sed "s/\./_/g"); \
		xxd -n "resource_$$name" -i $$res | sed 's/unsigned int/__attribute__\(\(section\(".rodata"\)\)\) const size_t/g' | sed 's/unsigned char/__attribute__\(\(section\(".rodata"\)\)\) const char/g' >> $@; \
	done; \
	echo "__attribute__((section(\".rodata\"))) const struct resource_file global_resources[] = {" >> $@; \
	for res in $(RESOURCES); do \
	    name=$$(basename $$res | sed "s/\./_/g"); \
		name_len=$$(echo "$$name" | wc -c | sed 's/ //g'); \
		size=$$(stat -c %s $$res); \
		echo "  { .name = \"resource_$$name\", .name_len = $$name_len, .data = resource_$$name, .data_len = $$size, }," >> $@; \
	done; \
	echo "};" >> $@; \
	echo '__attribute__((section(".rodata"))) const size_t global_resources_size = sizeof(global_resources) / sizeof(global_resources[0]);' >> $@;

resources.h: $(RESOURCES)
	@echo "  GEN      resources.h" && echo '#ifndef GENERATED_RESOURCE_H' > $@; \
	echo '#define GENERATED_RESOURCE_H' >> $@; \
	echo '#include <stddef.h>' >> $@; \
	echo 'struct resource_file {' >> $@; \
	echo '  const char *name;' >> $@; \
	echo '  size_t name_len;' >> $@; \
	echo '  const char *data;' >> $@; \
	echo '  size_t data_len;' >> $@; \
	echo '};' >> $@; \
	for res in $(RESOURCES); do \
	    name=$$(basename $$res | sed "s/\./_/g"); \
		echo '__attribute__((section(".rodata"))) extern const size_t resource_'$$name'_len;' >> $@; \
		echo '__attribute__((section(".rodata"))) extern const char resource_'$$name'[];' >> $@; \
	done; \
	echo '__attribute__((section(".rodata"))) extern const struct resource_file global_resources[];' >> $@; \
	echo '__attribute__((section(".rodata"))) extern const size_t global_resources_size;' >> $@; \
	echo '#endif /* GENERATED_RESOURCE_H */' >> $@;

clean-local:
	rm -rf .deps
	rm -f $(CLEANFILE)
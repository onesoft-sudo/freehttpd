#ifndef FH_PRINT_H
#define FH_PRINT_H

#include <stdio.h>

#define println(format, ...) (printf (format "\n", ##__VA_ARGS__))
#define fprintln(file, format, ...) (fprintf (file, format "\n", ##__VA_ARGS__))

#endif /* FH_PRINT_H */

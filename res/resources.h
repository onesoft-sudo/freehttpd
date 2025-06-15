#ifndef GENERATED_RESOURCE_H
#define GENERATED_RESOURCE_H
#include <stddef.h>
struct resource_file {
  const char *name;
  size_t name_len;
  const unsigned char *data;
  size_t data_len;
};
__attribute__((section(".rodata"))) extern const size_t resource_error_html_len;
__attribute__((section(".rodata"))) extern const unsigned char resource_error_html[];
__attribute__((section(".rodata"))) extern const struct resource_file global_resources[];
__attribute__((section(".rodata"))) extern const size_t global_resources_size;
#endif /* GENERATED_RESOURCE_H */

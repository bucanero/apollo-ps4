#ifndef ARABIC_PROCESS_H
#define ARABIC_PROCESS_H

#include <stdint.h>
#include <stdbool.h>

bool contains_arabic(const char* text);
char* process_arabic(const char* text);

#endif

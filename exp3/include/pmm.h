#pragma once
#include "types.h"

void pmm_init(void);
void* alloc_page(void);
void free_page(void* page);
void* alloc_pages(int n);
void panic(char *s);
size_t pmm_total_pages(void);
size_t pmm_free_pages(void);

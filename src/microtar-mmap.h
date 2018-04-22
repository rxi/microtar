/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `microtar.c` for details.
 */

#ifndef MICROTAR_MMAP_H
#define MICROTAR_MMAP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>

int mtar_open_mapped(mtar_t *tar, const char *filename, const char *mode);
int mtar_get_mapped(mtar_t *tar, const char *filename, const void **data);

#ifdef __cplusplus
}
#endif

#endif

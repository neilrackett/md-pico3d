/*
 * Copyright (C) 2026 Neil Rackett
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/chunk_globals.h"

#define WORLD_SIZE_X 12
#define WORLD_SIZE_Y 12
extern const struct chunk lod0_chunks[WORLD_SIZE_X][WORLD_SIZE_Y];
extern const struct chunk lod1_chunks[WORLD_SIZE_X][WORLD_SIZE_Y];
extern struct texture chunk_texture_list[11];
extern const struct chunk_lighting chunk_lights[12][12];

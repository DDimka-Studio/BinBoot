/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * Boot error reporting. A failing boot path records what went wrong with
 * bb_error_set(); the caller then shows bb_error_screen(), which keeps the
 * message on screen until the user presses Enter, so it can be read,
 * photographed or searched for. Works in release builds (not log-based).
 */
#ifndef BB_ERROR_H
#define BB_ERROR_H

#include "menu.h"

/* Pass as "code" when the failure has no numeric code. */
#define BB_ERR_NOCODE 0xFFFFFFFFFFFFFFFFULL

/* what: short description; detail: e.g. a file path, or NULL. */
void bb_error_set(const char *what, const char *detail,
                  unsigned long long code);

/* entry_name may be NULL when no entry is involved. Blocks until Enter. */
void bb_error_screen(const struct bb_console *con, const char *entry_name);

#endif /* BB_ERROR_H */

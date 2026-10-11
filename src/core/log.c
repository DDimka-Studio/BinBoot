/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
#include "log.h"

#ifdef BB_LOG_ENABLED
static bb_log_sink_fn g_sink;

void bb_log_set_sink(bb_log_sink_fn sink) { g_sink = sink; }

void bb_log_write(const char *msg)
{
    if (g_sink != 0)
        g_sink(msg);
}
#endif

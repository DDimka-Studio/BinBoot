/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 DDimka-Studio */
/*
 * Logging abstraction. Core code calls BB_LOG(); the firmware layer
 * installs a sink. In release builds (BB_LOG_ENABLED undefined) every
 * call compiles to nothing and log.c is not built at all.
 */
#ifndef BB_LOG_H
#define BB_LOG_H

#ifdef BB_LOG_ENABLED
typedef void (*bb_log_sink_fn)(const char *msg);
void bb_log_set_sink(bb_log_sink_fn sink);
void bb_log_write(const char *msg);
#define BB_LOG(msg)           bb_log_write(msg)
#define BB_LOG_SET_SINK(sink) bb_log_set_sink(sink)
#else
#define BB_LOG(msg)           ((void)0)
#define BB_LOG_SET_SINK(sink) ((void)0)
#endif

#endif /* BB_LOG_H */

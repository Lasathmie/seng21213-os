#ifndef THREAD_H
#define THREAD_H

#include <types.h>

typedef int32_t tid_t;

/* In this flat-memory kernel, a thread IS a process-table entry — there's
 * no separate address space to share/not-share, so Stage 1's scheduler
 * already treats every runnable unit identically. thread_create() exists
 * as its own file/API (per the Stage 2 spec) but delegates to process.c.
 */
tid_t thread_create(const char *name, void (*fn)(void));

/* Call from inside a thread function to terminate it and never return. */
void  thread_exit(void);

#endif
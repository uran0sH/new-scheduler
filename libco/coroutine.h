#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_THREAD_NUM 1

struct scheduler {
  pthread_t threads[MAX_THREAD_NUM];
  struct coroutine *current[MAX_THREAD_NUM];
  int index;
  int co_num;
};

struct scheduler *SCHEDULER = NULL;

struct coroutine* co_start(const char *name, void (*func)(void *), void *arg);
void co_yield();
void co_wait(struct coroutine *co);

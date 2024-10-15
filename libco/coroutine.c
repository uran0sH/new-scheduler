#include "coroutine.h"

#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum co_status {
  CO_NEW = 1,  // 新创建，还未执行过
  CO_RUNNING,  // 已经执行过
  CO_WAITING,  // 在 co_wait 上等待
  CO_DEAD,     // 已经结束，但还未释放资源
};

#define K 1024
#define STACK_SIZE (64 * K)

struct coroutine {
  const char *name;
  void (*func)(void *);  // co_start 指定的入口地址和参数
  void *arg;
  pthread_t thread_id;

  enum co_status status;     // 协程的状态
  struct coroutine *waiter;  // 是否有其他协程在等待当前协程
  jmp_buf context;           // 寄存器现场 (setjmp.h)
  unsigned char stack[STACK_SIZE];  // 协程的堆栈
};

struct coroutine *main0 = NULL;

typedef struct CONODE {
  struct coroutine *coroutine;
  struct CONODE *fd, *bk;
} CoNode;

static CoNode *co_node = NULL;

static void co_node_insert(struct coroutine *coroutine) {
  CoNode *victim = (CoNode *)malloc(sizeof(CoNode));
  assert(victim);

  victim->coroutine = coroutine;
  if (co_node == NULL) {
    victim->fd = victim->bk = victim;
    co_node = victim;
  } else {
    victim->fd = co_node->fd;
    victim->bk = co_node;
    victim->fd->bk = victim->bk->fd = victim;
  }
}

static CoNode *co_node_remove() {
  CoNode *victim = NULL;

  if (co_node == NULL) {
    return NULL;
  } else if (co_node->bk == co_node) {
    victim = co_node;
    co_node = NULL;
  } else {
    victim = co_node;

    co_node = co_node->bk;
    co_node->fd = victim->fd;
    co_node->fd->bk = co_node;
  }

  return victim;
}

struct coroutine *co_start(const char *name, void (*func)(void *), void *arg) {
  struct coroutine *coroutine =
      (struct coroutine *)malloc(sizeof(struct coroutine));
  assert(coroutine);

  coroutine->name = name;
  coroutine->func = func;
  coroutine->arg = arg;
  coroutine->status = CO_NEW;
  coroutine->waiter = NULL;
  coroutine->thread_id = -1;

  co_node_insert(coroutine);
  return coroutine;
}

void co_wait(struct coroutine *coroutine) {
  assert(coroutine);

  if (coroutine->status != CO_DEAD) {
    // coroutine->waiter = current;
    // current->status = CO_WAITING;
    co_yield ();
  }

  printf("free\n");

  /*
   * 释放coroutine对应的CoNode
   */
  while (co_node != NULL && co_node->coroutine != coroutine) {
    co_node = co_node->bk;
  }

  assert(co_node->coroutine == coroutine);

  free(coroutine);
  free(co_node_remove());
}

/*
 * 切换栈，即让选中协程的所有堆栈信息在自己的堆栈中，而非调用者的堆栈。保存调用者需要保存的寄存器，并调用指定的函数
 */
static inline void stack_switch_call(void *sp, void *entry, void *arg) {
  asm volatile(
#if __x86_64__
      "movq %%rcx, 0(%0); movq %0, %%rsp; movq %2, %%rdi; call *%1"
      :
      : "b"((uintptr_t)sp - 16), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#else
      "movl %%ecx, 4(%0); movl %0, %%esp; movl %2, 0(%0); call *%1"
      :
      : "b"((uintptr_t)sp - 8), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#endif
  );
}

static inline void restore_return() {
  asm volatile(
#if __x86_64__
      "movq 0(%%rsp), %%rcx"
      :
      :
#else
      "movl 4(%%esp), %%ecx"
      :
      :
#endif
  );
}

#define __LONG_JUMP_STATUS (1)
void co_yield () {
  struct coroutine *current = SCHEDULER->current[0];
  int current_index = 0;
  pthread_t thread_id = pthread_self();
  // for (int i = 0; i < MAX_THREAD_NUM; i++) {
  //   if (SCHEDULER->threads[i] == thread_id) {
  //     current = SCHEDULER->current[i];
  //   }
  // }
  printf("co_yield current: %s\n", current->name);
  int status = setjmp(current->context);
  if (!status) {
    co_node = co_node->bk;
    while (!((current = co_node->coroutine)->status == CO_NEW ||
             (current->status == CO_RUNNING && strcmp(current->name, "main") &&
              (current->thread_id == thread_id || current->thread_id == -1)))) {
      co_node = co_node->bk;
    } 
    assert(current);

    current->thread_id = thread_id;
    SCHEDULER->current[0] = current;
    printf("SCHEDULER: %s\n", SCHEDULER->current[0]->name);

    if (current->status == CO_RUNNING) {
      printf("longjmp %s\n", current->name);
      longjmp(current->context, __LONG_JUMP_STATUS);
    } else {
      ((struct coroutine volatile *)current)->status =
          CO_RUNNING;  // 这里如果直接赋值，编译器会和后面的覆写进行优化

      printf("stack_switch_call %s\n", current->name);
      stack_switch_call(current->stack + STACK_SIZE, current->func,
                        current->arg);

      // 恢复相关寄存器
      restore_return();
      printf("restore_return %s\n", current->name);

      current->status = CO_DEAD;

      // if (current->waiter) {
      //   current->waiter->status = CO_RUNNING;
      // }
      co_yield ();
    }
  }

  printf("co_yield finished %s, status: %d\n", current->name, current->status);
  assert(status && current->status == CO_RUNNING);
  // printf("co_yield %s\n", current->name);
}

void worker_loop() {
  while (true) {
    usleep(100);
    co_yield ();
  }
}

void *init_work(void* c) {
  usleep(100);
  co_wait(c);
  return NULL;
}

static __attribute__((constructor)) void co_constructor(void) {
  // main0 = co_start("main", NULL, NULL);
  // main0->status = CO_RUNNING;
  SCHEDULER = (struct scheduler *)malloc(sizeof(struct scheduler));
  SCHEDULER->index = 0;
  for (int i = 0; i < MAX_THREAD_NUM; i++) {
    pthread_t thread_id;
    char *name = (char *)malloc(8);
    sprintf(name, "worker%d", i);
    SCHEDULER->current[i] = co_start(name, worker_loop, NULL);
    SCHEDULER->current[i]->status = CO_RUNNING;
    int ret = pthread_create(&thread_id, NULL, init_work, SCHEDULER->current[i]);
    SCHEDULER->threads[i] = thread_id;
    SCHEDULER->current[i]->thread_id = thread_id;
  }
}

static __attribute__((destructor)) void co_destructor(void) {
  if (co_node == NULL) {
    return;
  }

  while (co_node) {
    struct coroutine *current = co_node->coroutine;
    free(current);
    free(co_node_remove());
  }
}

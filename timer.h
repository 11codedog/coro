#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define STACK_SIZE 8 * 1024
#define MAX_CO_SIZE 1024
#define JMP_RET_VAL 1

enum co_status {
  CO_NEW = 1, // 新创建，还未执行过
  CO_RUNNING, // 已经执行过
  CO_WAITING, // 在 co_wait 上等待
  CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
  const char *name;
  void (*func)(void *); // co_start 指定的入口地址和参数
  void *arg;
  int  id; // 

  enum co_status status;  // 协程的状态
  struct co *    waiter;  // 是否有其他协程在等待当前协程
  jmp_buf        context; // 寄存器现场 (setjmp.h)
	uint8_t				 stack[STACK_SIZE]; // 协程的堆栈
} __attribute__((aligned(16)));


struct schedule_t {
  struct co co_vec[MAX_CO_SIZE];
  jmp_buf current;
  int rco; // running coroutine
  int max_index;
};

struct schedule_t *sche;

static inline void stack_switch_call(void *sp, void *arg) {
  asm volatile (
#if __x86_64__
    "movq %0, %%rsp; movq %1, %%rdi"
      : : "b"((uintptr_t)sp), "a"(arg) : "memory"
#else
    "movl %0, %%esp; movl %2, 4(%0); jmp *%1"
      : : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg) : "memory"
#endif
  );
}

__attribute__((constructor)) static void sche_init() {
  sche = (struct schedule_t *)malloc(sizeof(struct schedule_t));
  assert(sche);
  bzero(sche, sizeof(struct schedule_t));
  sche->rco = -1;
  sche->max_index = 0;

  for(int i = 0; i < MAX_CO_SIZE; i++) {
    sche->co_vec[i].status = CO_DEAD;
  }
}


struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  struct co *co;
  int id;
  for(id = 0; id < MAX_CO_SIZE; id++) {
    if(sche->co_vec[id].status == CO_DEAD) {
      co = &(sche->co_vec[id]);
      co->id = id;
			sche->max_index += 1;
      break;
    }
  }
  
  co->arg = arg;
  co->name = name; 
  co->func = func;
  co->status = CO_NEW;

  int jrv = setjmp(co->context);
  if(jrv == 0) {
    return co;
  } else if(jrv == JMP_RET_VAL) {    
    sche->rco = co->id;
     // struct co *jmped_co = &(sche->co_vec[sche->rco]); // bug ## first four bits of current c
                                                      // oroutine's address 
                                                      // losed why????????????????????  but after
                                                      // adding this line 
                                                      // the address is correct ???
    stack_switch_call((void *)co->stack, co->arg);
		co->func(co->arg);
		co->status = CO_DEAD;
		longjmp(sche->current, JMP_RET_VAL);
    // after longjmp
  }
	return NULL;
}

//切换
void co_yield() {
  int jrv = setjmp(sche->co_vec[sche->rco].context);
  if(jrv == 0) {
    sche->co_vec[sche->rco].status = CO_WAITING;
    longjmp(sche->current, JMP_RET_VAL);
  } else {
    
    return;
  }
};

void co_wait(struct co *co) {
  int jrv = setjmp(sche->current);
  if(jrv == 0) {
    if(co->status == CO_NEW) {
      co->status = CO_RUNNING;
      longjmp(co->context, JMP_RET_VAL);
    } else if(co->status == CO_RUNNING) {
      co_yield();
    } else if(co->status == CO_WAITING) {
      
    } else {
      return;
    }
  } else if(jrv == JMP_RET_VAL) {
		if(co->status == CO_DEAD) return;
		longjmp(sche->co_vec[sche->rco].context, JMP_RET_VAL);
  }
};


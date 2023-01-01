#include "async/stackful.h"
using namespace async;

#include <dlfcn.h>

/* ==== async::coro_stack ==== */
coro_stack::coro_stack() : stack(new uint64_t[STACK_LEN]) {}

/* ==== async::detail ==== */
#ifdef KEGE_SANITIZE_ADDRESS
static void sanitizer_start_switch_fiber_weak(void** fakestacksave, void const* bottom,
                                              std::size_t size)
    __attribute__((__weakref__("__sanitizer_start_switch_fiber")));
static void sanitizer_finish_switch_fiber_weak(void* fakestack, void** bottom_old,
                                               std::size_t* size_old)
    __attribute__((__weakref__("__sanitizer_finish_switch_fiber")));

typedef decltype(sanitizer_start_switch_fiber_weak)* func_start_t;
typedef decltype(sanitizer_finish_switch_fiber_weak)* func_finish_t;

namespace {
bool inited = false;
func_start_t func_start = 0;
func_finish_t func_finish = 0;
}  // namespace

void async::detail::stackful_asan_start_switch(void** fakestacksave, void* bottom,
                                               std::size_t size) {
  if (&::sanitizer_start_switch_fiber_weak) {
    sanitizer_start_switch_fiber_weak(fakestacksave, bottom, size);
  } else if (func_start) {
    func_start(fakestacksave, bottom, size);
  }
}

void async::detail::stackful_asan_finish_switch(void* fakestack, void** bottom_old,
                                                std::size_t* size_old) {
  if (&::sanitizer_finish_switch_fiber_weak) {
    sanitizer_finish_switch_fiber_weak(fakestack, bottom_old, size_old);
  } else if (func_finish) {
    func_finish(fakestack, bottom_old, size_old);
  }
}
#endif

#define STACKFUL_SUSPEND_RESTORE \
  "pop rax\n"                    \
  "mov [rdi+0x38], rax\n"        \
  "mov [rdi+0x40], rbx\n"        \
  "mov [rdi+0x48], rsp\n"        \
  "mov [rdi+0x50], rbp\n"        \
  "mov [rdi+0x58], r12\n"        \
  "mov [rdi+0x60], r13\n"        \
  "mov [rdi+0x68], r14\n"        \
  "mov [rdi+0x70], r15\n"        \
                                 \
  "mov rbx, [rdi+0]\n"           \
  "mov rsp, [rdi+0x08]\n"        \
  "mov rbp, [rdi+0x10]\n"        \
  "mov r12, [rdi+0x18]\n"        \
  "mov r13, [rdi+0x20]\n"        \
  "mov r14, [rdi+0x28]\n"        \
  "mov r15, [rdi+0x30]\n"

__attribute__((naked)) void detail::stackful_done() {
  asm("pop rdi\n" STACKFUL_SUSPEND_RESTORE
      "mov rdi, [rdi+0x78]\n"
      "ret\n");
}

__attribute__((naked)) void stackful_suspend_inner(void*) {
  asm(STACKFUL_SUSPEND_RESTORE "ret\n");
}

__attribute__((naked)) void stackful_resume_inner(void*) {
  asm("mov [rdi+0], rbx\n"
      "mov [rdi+0x08], rsp\n"
      "mov [rdi+0x10], rbp\n"
      "mov [rdi+0x18], r12\n"
      "mov [rdi+0x20], r13\n"
      "mov [rdi+0x28], r14\n"
      "mov [rdi+0x30], r15\n"

      "mov rbx, [rdi+0x40]\n"
      "mov rsp, [rdi+0x48]\n"
      "mov rbp, [rdi+0x50]\n"
      "mov r12, [rdi+0x58]\n"
      "mov r13, [rdi+0x60]\n"
      "mov r14, [rdi+0x68]\n"
      "mov r15, [rdi+0x70]\n"

      "mov rax, [rdi+0x38]\n"
      "mov rdi, [rdi+0x78]\n"
      "jmp rax\n");
}

void detail::stackful_suspend(void* ptr) {
#ifdef KEGE_SANITIZE_ADDRESS
  auto ctx = (stackful_context*) ptr;
  void* stacksave;
  stackful_asan_start_switch(&stacksave, ctx->stack_bottom, ctx->stack_size);
  stackful_suspend_inner(ptr);
  stackful_asan_finish_switch(stacksave, &(ctx->stack_bottom), &(ctx->stack_size));
#else
  stackful_suspend_inner(ptr);
#endif
}

void detail::stackful_resume(void* ptr) {
  current_stackful_context = ptr;
#ifdef KEGE_SANITIZE_ADDRESS
  if (!inited) {
    if (!&::sanitizer_start_switch_fiber_weak || !&::sanitizer_finish_switch_fiber_weak) {
      func_start = (func_start_t) dlsym(RTLD_DEFAULT, "__sanitizer_start_switch_fiber");
      func_finish = (func_finish_t) dlsym(RTLD_DEFAULT, "__sanitizer_finish_switch_fiber");
      if (!func_start || !func_finish) {
        logw("dlsym for __sanitizer_(start|finish)_switch_fiber failed");
        func_start = 0;
        func_finish = 0;
      }
    }
    inited = 1;
  }

  auto ctx = (stackful_context*) ptr;
  void* stacksave;
  stackful_asan_start_switch(&stacksave, ctx->stack.get() + coro_stack::STACK_LEN,
                             coro_stack::STACK_SIZE);
  stackful_resume_inner(ptr);
  stackful_asan_finish_switch(stacksave, 0, 0);
#else
  stackful_resume_inner(ptr);
#endif
}

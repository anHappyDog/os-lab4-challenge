#ifndef __SCHED_H__
#define __SCHED_H__
void schedule(int yield) __attribute__((noreturn));
void handle_sigaction(struct Trapframe* tf);

void saveSigContext(struct Trapframe* tf,int sig);
void restoreSigContext(struct Trapframe* utf,struct Trapframe* ktf);
#endif /* __SCHED_H__ */

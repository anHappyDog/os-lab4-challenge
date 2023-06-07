#ifndef __SIGACTION_H_
#define __SIGACTION_H_
#include <types.h>

#include <trap.h>
#define MAX_SIGACTION 64

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIG_KILL 9
#define SIG_SEGV 11
#define SIG_TERM 15

typedef struct sigset_t sigset_t;
struct sigset_t {
	int sig[2];
};

struct sigaction {
	void (*sa_handler)(int);
	sigset_t sa_mask;
};

struct sigaction_queue {
	uint8_t sigState[MAX_SIGACTION];
	uint8_t sigqueue[MAX_SIGACTION];
	uint8_t lpos,rpos,sigPos;
	void (*do_sigaction)(void (*handler)(int),int signum, struct Trapframe* utf,struct Trapframe* ktf);
};

#endif

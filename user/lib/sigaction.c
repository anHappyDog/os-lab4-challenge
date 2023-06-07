#include <sigaction.h>
#include <lib.h>
static int isSignumValue(int signum){
	return (signum <= MAX_SIGACTION && signum > 0) ? 1 : 0;
}

int kill(u_int envid, int sig) {
	if (!isSignumValue(sig)) {
		return -1;
	}
	return syscall_kill_sigaction(envid,sig);
}


int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) {
	if (!isSignumValue(signum)) {
		return -1;
	}
	return syscall_signUp_sigaction(signum,act,oldact);
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
	return syscall_procmask_sigaction(how,set,oldset);
}

void sigemptyset(sigset_t *set) {
	set->sig[0] = 0;
	set->sig[1] = 0;
} 
void sigfillset(sigset_t *set) {
	set->sig[0] = 0xffffffff;
	set->sig[1] = 0xffffffff;
}
void sigaddset(sigset_t *set, int signum) {
	set->sig[(signum - 1) >> 5] |= (1 << (signum - 1));
}
void sigdelset(sigset_t *set, int signum) {
	set->sig[(signum - 1) >> 5] &= ~(1 << (signum - 1));
}
int sigismember(const sigset_t *set, int signum) {
	return ((set->sig[(signum - 1) >> 5] & (1 << (signum - 1))) == (1 << (signum - 1))) ? 1 : 0;
}



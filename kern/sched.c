#include <env.h>
#include <pmap.h>
#include <printk.h>

/* Overview:
 *   Implement a round-robin scheduling to select a runnable env and schedule it using 'env_run'.
 *
 * Post-Condition:
 *   If 'yield' is set (non-zero), 'curenv' should not be scheduled again unless it is the only
 *   runnable env.
 *
 * Hints:
 *   1. The variable 'count' used for counting slices should be defined as 'static'.
 *   2. Use variable 'env_sched_list', which contains and only contains all runnable envs.
 *   3. You shouldn't use any 'return' statement because this function is 'noreturn'.
 */
void schedule(int yield) {
	static int count = 0; // remaining time slices of current env
	struct Env *e = curenv;
	/* We always decrease the 'count' by 1.
	 *
	 * If 'yield' is set, or 'count' has been decreased to 0, or 'e' (previous 'curenv') is
	 * 'NULL', or 'e' is not runnable, then we pick up a new env from 'env_sched_list' (list of
	 * all runnable envs), set 'count' to its priority, and schedule it with 'env_run'. **Panic
	 * if that list is empty**.
	 *
	 * (Note that if 'e' is still a runnable env, we should move it to the tail of
	 * 'env_sched_list' before picking up another env from its head, or we will schedule the
	 * head env repeatedly.)
	 *
	 * Otherwise, we simply schedule 'e' again.
	 *
	 * You may want to use macros below:
	 *   'TAILQ_FIRST', 'TAILQ_REMOVE', 'TAILQ_INSERT_TAIL'
	 */
	/* Exercise 3.12: Your code here. */
	--count;
	if(e == NULL||((e != NULL) && (e->env_status != ENV_RUNNABLE))) {

		e = TAILQ_FIRST(&env_sched_list);
		count = e->env_pri;
	}

	else if(yield != 0 || count <= 0) {
		if(e->env_status == ENV_RUNNABLE) {
			TAILQ_REMOVE(&env_sched_list,e,env_sched_link);
			TAILQ_INSERT_TAIL(&env_sched_list,e,env_sched_link);
		}
		e = TAILQ_FIRST(&env_sched_list);
		count = e->env_pri;
	}
	if(e == NULL) {
		panic("schedule : e is null\n");
	}
	env_run(e);
}

void saveSigContext(struct Trapframe* tf,int sig) {
		struct sigaction_queue* sig_queue = &curenv->env_sigaction_queue;

		sig_queue->sigState[sig_queue->sigPos++] = sig;
		// 1. use the ret entry for the sigaction process	
		struct Trapframe* tt = (struct Trapframe*)tf->regs[29] - 1;
		*tt = *tf;
		/*tf->regs[29] -= sizeof(struct Trapframe);
		tf->regs[4] = (unsigned long)curenv->env_sigactions[sig].sa_handler;
		tf->regs[5] = (unsigned long)sig;
		tf->regs[6] = (unsigned long)tf->regs[29]; 	
		tf->regs[7] = (unsigned long)tf;
		tf->cp0_epc = (unsigned long)sig_queue->do_sigaction;*/
//		tf->cp0_epc = curenv->env_sigactions[sig].sa_handler == NULL ? 0 : curenv->env_sigactions[sig].sa_handler;
/*		printk("handling sigaction %08x...\n",sig);
		printk("29 is %08x\n",tt);
		printk("tf is %08x\n tf->cp0_epc is \n",tf);*/
		asm ("add $4,%0,$0" :: "r"(curenv->env_sigactions[sig].sa_handler));
		asm ("add $5,%0,$0" :: "r"(sig));
		asm ("add $6,%0,$0" :: "r"(tt));
		asm ("add $7,%0,$0" :: "r"(tf));
		asm ("add $29, %0,$0" :: "r"(tt): "memory");
		asm ("jr %0" :: "r"(sig_queue->do_sigaction) : "memory");


}

static int isSigactionBlocked(int signum) {
	struct sigaction_queue* sigqueue = &curenv->env_sigaction_queue;
	int curSig = sigqueue->sigState[sigqueue->sigPos];
	//printk("cursig is %08x\n",curSig);
	//printk("sig is %08x\n",curenv->env_sigactions[curSig].sa_mask.sig[(signum - 1) >> 5]);
	//printk("signum is %08x, ans is %08x\n",signum,(curenv->env_sigactions[curSig].sa_mask.sig[(signum - 1) >> 5] & (1 << (signum - 1))));
	return signum == SIG_KILL ? 0 : (curenv->env_sigactions[curSig].sa_mask.sig[(signum - 1) >> 5] & (1 << (signum - 1))) == (1 << (signum - 1)) ? 1 : 0;
}

void restoreSigContext(struct Trapframe* utf,struct Trapframe* ktf) {
	*ktf = *utf;
	if (curenv->env_sigaction_queue.sigPos == 0) {
		curenv->env_sigaction_queue.sigState[0] = 0;
	} else {
		curenv->env_sigaction_queue.sigPos -= 1;
	}
	asm ("add $29, %0,$0" :: "r"((unsigned long)ktf): "memory");
	asm ("j ret_from_exception");
}


void handle_sigaction(struct Trapframe* tf) {
	if (!curenv || curenv->env_sigaction_queue.lpos == curenv->env_sigaction_queue.rpos) {
		return;
	}
	struct sigaction_queue * sig_queue = &curenv->env_sigaction_queue;
	//printk("sdaddasd is %08x\n",curenv->env_sigactions[0].sa_mask.sig[0]);
	while (sig_queue->lpos != sig_queue->rpos) {
		int sig = sig_queue->sigqueue[(sig_queue->lpos++) % MAX_SIGACTION];
		if (!isSigactionBlocked(sig)) {
			saveSigContext(tf,sig);	
		} else {
			int flag = 0,doSig = 0;
			int begin = 0,last = sig_queue->rpos;
			for (begin = sig_queue->lpos; begin <= last;++begin) {
				doSig = sig_queue->sigqueue[begin % MAX_SIGACTION];
				if (!isSigactionBlocked(doSig)) {
					flag = 1;
					sig_queue->lpos -= 1; 
					for (int t = sig_queue->lpos + 1; t <= begin; ++t ) {
						sig_queue->sigqueue[t % MAX_SIGACTION] = sig_queue->sigqueue[(t - 1) % MAX_SIGACTION];
					}
					sig_queue->sigqueue[sig_queue->lpos] = doSig;
					break;
				}
			}	
			if (flag == 0) {
				break;
			}
		}		
	}
}

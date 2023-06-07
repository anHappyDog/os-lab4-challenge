## LAB4挑战性任务

## 目标

使用信号，完成新的进程间通信。

### 内容

```c
struct sigaction{
    void (*sa_handler)(int);
    sigset_t sa_mask;
};

struct sigset_t{
    int sig[2]; //最多 32*2=64 种信号
};

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
//
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
//
void sigemptyset(sigset_t *set); // 清空信号集，将所有位都设置为 0
void sigfillset(sigset_t *set); // 设置信号集，即将所有位都设置为 1
void sigaddset(sigset_t *set, int signum); // 向信号集中添加一个信号，即将指定信号的位设置为 1
void sigdelset(sigset_t *set, int signum); // 从信号集中删除一个信号，即将指定信号的位设置为 0
int sigismember(const sigset_t *set, int signum); // 检查一个信号是否在信号集中，如果在则返回 1，否则返回 0
int kill(u_int envid, int sig);
//
```

上述提到的所有函数都需要在用户态进行。

### 信号设计

信号机制可以分为 注册， 发送 和 处理。

#### 信号的注册

我在env pcb (env.h)结构体中添加了相关结构体：

```c
struct Env {
...
struct sigaction_queue env_sigaction_queue;
struct sigaction env_sigactions[MAX_SIGACTION + 1];
...
};
```

其中 `struct sigaction_queue`由我自己定义：

```c
struct sigaction_queue {
  uint8_t sigState[MAX_SIGACTION];  //信号状态栈
  uint8_t sigqueue[MAX_SIGACTION];  //收到信号的环形队列
  uint8_t lpos,rpos,sigPos;	//收到信号的左右下标，状态栈的下标
  void (*do_sigaction)(void (*handler)(int),int signum,struct Trapframe* utf, struct Trapframe*ktf);
    //handler与handler_sigaction的中间层，负责执行handler 以及恢复上下文
};
```

​	其中的 `do_sigaction`被我定义为用户态的库函数,被放在`./user/lib/syscall_lib.c`中。

```c
void do_sigaction(void (*handler)(int),int signum,struct Trapframe* utf,struct Trapframe *ktf) {
	switch(signum) {
		case SIG_KILL: case SIG_SEGV: case SIG_TERM:
			if (handler == NULL) {
				exit();
			} else {
				handler(signum);	
			}
			break;
		default:
			if (handler != NULL) {
				handler(signum);
			}
			break;
	}
	sigaction_return(utf,ktf);
}
```

​		其中的 `sigaction_return`(被定义在`./user/syscall_lib.c`)为恢复上下文的函数，通过函数调用`syscall_sigaction_return`来返回，具体会在后面说明。

`struct sigaction`在题目中已经定义好，上述的两个结构体被放在我建立的 `./include/sigaction.h`中，`MAX_SIGACTION`被定义为最大数量信号量64。

信号注册就如题目所说进行相应的注册，实现上述的相关函数，部分函数需要系统调用。如在`kill`中，通过调用`syscall_kill_sigaction`来进行信号的发送，在`sigaction` 注册函数 和`sigprocmask` 修改掩码函数中分别调用`syscall_signUp_sigaction`和`syscall_procmask_sigaction`进行注册和掩码的修改，其他的函数均可在用户态实现。以上系统调用用户函数均在`./user/lib/syscall_lib.c` ，其内核态对应函数均在`./kern/syscall_all.c`中。

#### 信号的发出

信号可以由进程和内核发出，我的信号的设计是，在除去题目给的两个结构体之外，增加`struct sigaction_queue`结构体，用来表示进程当前处于的信号（0表示当前状态没有任何信号），因为需要根据当前进程的信号掩码来屏蔽，所以信号的发出 只需要在 对应的进程的该结构体中的环形信号队列中添加该信号就可以了。

#### 信号的处理

信号可以被屏蔽，所以要有信号掩码，可以有多重信号，所以要有信号队列。我选择在进程调度`ret_from_exception`时，进入`handle_sigaction`（被定义在`./kern/sched.c`中，其中调用的函数如`isSigactionBlocked`大多为静态函数，且都定义在该文件中），进行相应的处理：

```c
// ret_from_exception
FEXPORT(ret_from_exception)
	addi      a0, sp,0
	jal handle_sigaction
	RESTORE_SOME
	lw      k0, TF_EPC(sp)
	lw      sp, TF_REG29(sp) /* Deallocate stack */
.set noreorder
	jr      k0
	rfe
.set reorder

//
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
```

如果没有信号直接返回，有则判断是否阻塞，如果没有，则增加状态栈，进入`do_sigaction`（之前说明的函数），进入对应信号处理函数，最后通过返回函数`sigaction_return`返回，减少状态栈。 

​    如果阻塞，则判断是否没有可执行信号，没有则退出，否则 执行，并修改信号队列。

### 测试

首先需要测试各用户函数的正确性，如kill，以及sigaction的返回值，以及其他函数是否正确工作（lab4_t5）。

测试其他进程发送信号至进程（lab4_t4）。

测试多fork(lab4_t6)。

### 难点

遇到的问题有上下文的保存和恢复， 信号的执行地点（这个题目有说），第一个问题参照 进程的上下文恢复写的，但也有些不同，例如我将要恢复的上下文保存在用户态，在执行完处理信号时，从用户态读取返回。






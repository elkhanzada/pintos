2021 Spring Operating System
-------------------

This repo is used for submission of Pintos Project.
## Alarm_clock
In this project we implemented alarm clock. In order to do that, we first tried to imitate ```thread_yield()``` function by reusing its implementation. We realized that there is a list called ```ready_list``` so why not to make a list called ```sleep_list``` for blocked threads. Since each thread will have its own wake up time we made a new variable called ```ticks``` in ```struct thread``` to store the information for each of them. With ```thread_sleep``` function (which is quite similar to ```thread_yield```). We have ```checkTicks``` function that is used for sorting the ```sleep_list``` so that in ```thread_tick``` function we can wake up the threads easily.


**update 3/23/2021:**

need to finish these:
priority-change
priority-preempt
priority-fifo

*next_thread_to_run()* is selecting the first one in the ready_list

*thread_schedule_tail(struct thread **prev)*: the new thread (cur) is already running (if prev is dying then destroy it)

<del>According to *list_insert_ordered()* and *list_insert()* in lib/kernel/list.c, 
if we insert an already existing element, it's ok update the position 
of ->prev and ->next of *before* in *list_insert()*, the position of the 
existing elem will be changed, no duplicate because it uses pointer & the only
thing changed are attributes ->prev and ->next of each elem </del>

**update 3/24/2021:**
- priority-change -

explanation: When thread is running, it's not in ready_thread () => when yield is called, the running_thread whose priority has just changed, can be inserted into ready_thread () again **BOOM** amazing.

**update 28/04/2020**
- <del>I'm finding who calls process_execute, bc it may help in the process_wait.</del> Oh wait I found it. it's called by process_wait, how convenient. Stupid Visual Studio Code doesn't recognize USERPROG.
- Somehow it always crashes (list insert fails or thread not running error) when I malloc p in thread_init stage, which I need to do, to init the child process list. So I'm thinking about using the thread itself to access child process.

**update 29/04/2020**
- ok Rox-* are good but wait-* are doomed now, what the heck ???:D???

**update 30/04/2020**
- holycrap, fixing the synch was a pain in the ass, so basically first I thought my syscall has problem, then I thought my wait has problem, I put lock acquire to wait and exit, it does something. Then I realize, what if 2 concurrent thread also running the same lines of code in one of the syscall, so I put lock on sys_read, it works surprisingly well. So I put lock on every other possible sys, also take care of corner cases like it suddenly has page-fault, must release lock. It passes every other tests, except multi-omm??? Then this part idk how, I remove the locks on process_wait and process_exit, then it works like a charm. I don't think my code is optimal at all, but hey, it passes.

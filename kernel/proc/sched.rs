use super::task::{Task, TaskState, TASK_STACKS, TASKS, CURRENT, MAX_TASKS};

pub fn sched_init() {
    unsafe extern "Rust" {
        fn sched_idle();
    }
    let stack = unsafe { &mut TASK_STACKS[0] };
    let task = Task::new(0, sched_idle, stack);
    unsafe {
        TASKS[0] = Some(task);
        CURRENT = 0;
    }
}

pub fn sched_yield() {
    unsafe {
        let mut next = (CURRENT + 1) % MAX_TASKS;
        for _ in 0..MAX_TASKS {
            if let Some(ref t) = TASKS[next] {
                if matches!(t.state, TaskState::Ready) {
                    if next != CURRENT {
                        // context switch
                    }
                    CURRENT = next;
                    return;
                }
            }
            next = (next + 1) % MAX_TASKS;
        }
    }
}

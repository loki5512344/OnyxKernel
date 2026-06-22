#[repr(C)]
pub struct Context {
    pub ra: usize,
    pub sp: usize,
    pub s0: usize,
    pub s1: usize,
    pub s2: usize,
    pub s3: usize,
    pub s4: usize,
    pub s5: usize,
    pub s6: usize,
    pub s7: usize,
    pub s8: usize,
    pub s9: usize,
    pub s10: usize,
    pub s11: usize,
}

pub enum TaskState {
    Ready,
    Running,
    Blocked,
}

pub struct Task {
    pub id: usize,
    pub state: TaskState,
    pub context: Context,
    pub stack: &'static mut [u8],
}

impl Task {
    pub fn new(id: usize, entry: unsafe fn(), stack: &'static mut [u8]) -> Self {
        let sp = stack.as_mut_ptr() as usize + stack.len();
        Task {
            id,
            state: TaskState::Ready,
            context: Context {
                ra: entry as usize,
                sp,
                s0: 0, s1: 0, s2: 0, s3: 0,
                s4: 0, s5: 0, s6: 0, s7: 0,
                s8: 0, s9: 0, s10: 0, s11: 0,
            },
            stack,
        }
    }
}

pub const MAX_TASKS: usize = 16;
pub const STACK_SIZE: usize = 4096;
pub static mut TASK_STACKS: [[u8; STACK_SIZE]; MAX_TASKS] = [[0; STACK_SIZE]; MAX_TASKS];
pub static mut TASKS: [Option<Task>; MAX_TASKS] = [const { None }; MAX_TASKS];
pub static mut CURRENT: usize = 0;

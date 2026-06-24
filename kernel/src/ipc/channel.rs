//! IPC channels — bidirectional ring buffers between a root service (owner)
//! and a user process (client).
//!
//! A root-space process creates a channel (`create`) and gets a channel ID.
//! A user process connects to it (`connect`); then either side may `send` /
//! `recv`. The buffer is a fixed 4 KiB ring. When full, `send` blocks (adds
//! the caller to the send_wait queue); when empty, `recv` blocks (adds the
//! caller to the recv_wait queue). After every successful `send` the
//! recv_wait queue is woken, and after every successful `recv` the send_wait
//! queue is woken.
use crate::arch::trap_frame::TrapFrame;
use onyx_core::errno::{Errno, KResult};

const CHAN_MAX: usize = 32;
const CHAN_BUF_SIZE: usize = 4096;

#[derive(Clone, Copy)]
pub struct Channel {
    pub buf: [u8; CHAN_BUF_SIZE],
    pub head: u32,
    pub tail: u32,
    /// root service that created it.
    pub owner_pid: u32,
    /// user process that connected (0 until `connect` is called).
    pub client_pid: u32,
    pub used: bool,
    pub closed: bool,
    /// Linked list of processes blocked waiting to send (channel full).
    pub send_wait: *mut crate::proc::Proc,
    /// Linked list of processes blocked waiting to recv (channel empty).
    pub recv_wait: *mut crate::proc::Proc,
}

static mut G_CHANNELS: [Channel; CHAN_MAX] = [Channel {
    buf: [0; CHAN_BUF_SIZE],
    head: 0,
    tail: 0,
    owner_pid: 0,
    client_pid: 0,
    used: false,
    closed: false,
    send_wait: core::ptr::null_mut(),
    recv_wait: core::ptr::null_mut(),
}; CHAN_MAX];

/// Create a new channel owned by `owner_pid`. Returns the channel ID.
pub unsafe fn create(owner_pid: u32) -> KResult<u32> {
    let p = &raw mut G_CHANNELS;
    for i in 0..CHAN_MAX {
        if !(*p)[i].used {
            (*p)[i].used = true;
            (*p)[i].owner_pid = owner_pid;
            (*p)[i].client_pid = 0;
            (*p)[i].head = 0;
            (*p)[i].tail = 0;
            (*p)[i].closed = false;
            (*p)[i].send_wait = core::ptr::null_mut();
            (*p)[i].recv_wait = core::ptr::null_mut();
            return Ok(i as u32);
        }
    }
    Err(Errno::NoMem)
}

/// Attach `client_pid` to an existing channel as the client endpoint.
pub unsafe fn connect(chan_id: u32, client_pid: u32) -> KResult<()> {
    let p = &raw mut G_CHANNELS;
    if chan_id as usize >= CHAN_MAX {
        return Err(Errno::Inval);
    }
    if !(*p)[chan_id as usize].used {
        return Err(Errno::NoEnt);
    }
    (*p)[chan_id as usize].client_pid = client_pid;
    Ok(())
}

// ── Wait-queue helpers ────────────────────────────────────────────────────

/// Add the current process to a wait queue and set its state to `Waiting`.
unsafe fn wait_enqueue(wait_head: &mut *mut crate::proc::Proc) {
    let p = crate::proc::current() as *mut crate::proc::Proc;
    (*p).state = crate::proc::ProcState::Waiting;
    (*p).wait_next = *wait_head;
    *wait_head = p;
}

/// Transition every process in a wait queue back to `Ready` and clear the
/// queue.
unsafe fn wait_wake_all(wait_head: &mut *mut crate::proc::Proc) {
    let mut cur = *wait_head;
    while !cur.is_null() {
        let next = (*cur).wait_next;
        (*cur).state = crate::proc::ProcState::Ready;
        (*cur).wait_next = core::ptr::null_mut();
        cur = next;
    }
    *wait_head = core::ptr::null_mut();
}

// ── Public API ────────────────────────────────────────────────────────────

/// Write `len` bytes from `buf` into the channel ring buffer. Only the owner
/// or the client may send. Returns the number of bytes sent.
///
/// If the buffer cannot hold the entire message **and** `tf` is `Some`, the
/// current process is blocked (added to the send_wait queue) and the CPU is
/// yielded. When space becomes available (after a `recv`) the process is
/// woken. If `tf` is `None`, the old non-blocking `Errno::Busy` behaviour is
/// preserved.
pub unsafe fn send(
    chan_id: u32,
    buf: *const u8,
    len: u32,
    tf: Option<&mut TrapFrame>,
) -> KResult<u32> {
    let p = &raw mut G_CHANNELS;
    if chan_id as usize >= CHAN_MAX {
        return Err(Errno::Inval);
    }
    let ch = &mut (*p)[chan_id as usize];
    if !ch.used || ch.closed {
        return Err(Errno::Pipe);
    }
    let cur_pid = crate::proc::current_pid();
    if cur_pid != ch.owner_pid && cur_pid != ch.client_pid {
        return Err(Errno::Perm);
    }

    let available = CHAN_BUF_SIZE as u32 - ch.tail.wrapping_sub(ch.head);
    if available < len {
        if let Some(tf) = tf {
            // Block: add to send_wait queue and yield.
            wait_enqueue(&mut ch.send_wait);
            crate::proc::scheduler::NEED_RESCHED = true;
            crate::proc::scheduler::sched_yield(tf);
            // If we reach here, no context switch happened (would deadlock).
            return Err(Errno::Busy);
        }
        return Err(Errno::Busy);
    }

    let mut written = 0u32;
    while written < len {
        let idx = (ch.tail as usize) % CHAN_BUF_SIZE;
        ch.buf[idx] = *buf.add(written as usize);
        ch.tail = ch.tail.wrapping_add(1);
        written += 1;
    }

    // Wake any processes blocked on recv — data is now available.
    if !ch.recv_wait.is_null() {
        wait_wake_all(&mut ch.recv_wait);
        crate::proc::scheduler::NEED_RESCHED = true;
    }
    Ok(written)
}

/// Read up to `len` bytes into `buf` from the channel ring buffer. Only the
/// owner or the client may recv. Returns the number of bytes read (0 if the
/// channel is empty and no blocking was requested).
///
/// If the buffer is empty **and** `tf` is `Some`, the current process is
/// blocked (added to the recv_wait queue) and the CPU is yielded. When data
/// arrives (after a `send`) the process is woken. If `tf` is `None`, the old
/// non-blocking behaviour (returns `Ok(0)` when empty) is preserved.
pub unsafe fn recv(
    chan_id: u32,
    buf: *mut u8,
    len: u32,
    tf: Option<&mut TrapFrame>,
) -> KResult<u32> {
    let p = &raw mut G_CHANNELS;
    if chan_id as usize >= CHAN_MAX {
        return Err(Errno::Inval);
    }
    let ch = &mut (*p)[chan_id as usize];
    if !ch.used || ch.closed {
        return Err(Errno::Pipe);
    }
    let cur_pid = crate::proc::current_pid();
    if cur_pid != ch.owner_pid && cur_pid != ch.client_pid {
        return Err(Errno::Perm);
    }

    let available = ch.tail.wrapping_sub(ch.head);
    if available == 0 {
        if let Some(tf) = tf {
            // Block: add to recv_wait queue and yield.
            wait_enqueue(&mut ch.recv_wait);
            crate::proc::scheduler::NEED_RESCHED = true;
            crate::proc::scheduler::sched_yield(tf);
            // If we reach here, no context switch happened (would deadlock).
            return Ok(0);
        }
        return Ok(0);
    }
    let to_read = len.min(available);

    let mut read = 0u32;
    while read < to_read {
        let idx = (ch.head as usize) % CHAN_BUF_SIZE;
        *buf.add(read as usize) = ch.buf[idx];
        ch.head = ch.head.wrapping_add(1);
        read += 1;
    }

    // Wake any processes blocked on send — space is now available.
    if !ch.send_wait.is_null() {
        wait_wake_all(&mut ch.send_wait);
        crate::proc::scheduler::NEED_RESCHED = true;
    }
    Ok(read)
}

/// Close and free a channel.
pub unsafe fn close(chan_id: u32) -> KResult<()> {
    let p = &raw mut G_CHANNELS;
    if chan_id as usize >= CHAN_MAX {
        return Err(Errno::Inval);
    }
    (*p)[chan_id as usize].closed = true;
    (*p)[chan_id as usize].used = false;
    Ok(())
}

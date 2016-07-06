# cornerstone
Another Raft C++ Implementation

The core algorithm is implemented based on the TLA+ spec, whose safety and liveness are proven, this implementation is copied from [jraft](https://github.com/andy-yx-chen/jraft) but replace java code with C++

## Supported Features,
- [x] Core Algorithm, safety, liveness are proven
- [x] Configuration Change Support, add or remove servers one by one without limitation
- [x] Client Request Support, to be enhanced, but is working now.
- [x] **Urgent commit**, see below
- [x] log compaction 

> Urgent Commit, is a new feature introduced by this implementation, which enables the leader asks all other servers to commit one or more logs if commit index is advanced. With Urgent Commit, the system's performance is highly improved and the heartbeat interval could be increased to seconds,depends on how long your application can abide when a leader goes down, usually, one or two seconds is fine. 

## About this implementation
it's always safer to implement such kind of algorithm based on Math description other than natural languge description.
there should be an auto conversion from TLA+ to programming languages, even they are talking things in different ways, but they are identical

## Code Structure
I know it's lack of documentations, I will try my best, but if you can help, let me know. This project has core algorithm implementation only for now, we also rely on asio library for timer and __in future__ for tcp based cli/srv communications

For how to use the code, please refer to tests/test_impls.cxx for interfaces that you need to implement, for log_store and state_machine implementations, it's **your own resposibility to take care of locking**.

It could be built on Windows (Makefile.win, nmake), Linux (Makefile.lx, gnu make) and FreeBSD (Makefile.bsd, pmake)

## Coding Hints

In all the code (since shared_ptr is not free, I try to avoid it as much as possible), if a pointer is passed, that means the callee need to do the cleanup, if a pointer is returned, that means the caller need to take care of it.

please write [me](mailto:andy.yx.chen@outlook.com) if you have any question about using this 



# cornerstone
Another Raft C++ Implementation

## Project moved

**Please be noted that this project is no longer maintained, I will not fix any bugs, neither have new features in this project anymore, however, I have found a team to take over this for me, the code has been re-organized and published at [cornerstone](https://github.com/datatechnology/cornerstone), the team will start from there and make it under Apache 2, nothing is changed, but better support and more features on it**

The core algorithm is implemented based on the TLA+ spec, whose safety and liveness are proven, this implementation is copied from [jraft](https://github.com/andy-yx-chen/jraft) but replace java code with C++

## Updates and Plan
- [x] As memory goes big, GC may not be as efficient as memory tracking, introduced ptr\<T\> to this project to manage pointers
- [ ] having a memory pool to improve the performance, this may not be done in this project, another project called jitl is on going to lead a new life style for c++ coders
- [x] started working on file based log store, as raft's store accessing pattern is very special, file based would be the fastest one (not prove yet)
- [x] update fs\_log\_store to make the test pass and add more test cases for it
- [ ] asio based rpc client and rpc servers
- [x] go real (have this into a product)

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

Now, I resolve all pointer issues with ptr<T>, which is better than shared\_ptr<T>, ptr<T> is pretty customized for cornerstone project only, however, it should meet most of your requirements, use cs\_new to create ptr<T>

please write [me](mailto:andy.yx.chen@outlook.com) if you have any question about using this 



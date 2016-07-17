#ifndef _CORNERSTONE_HXX_
#define _CORNERSTONE_HXX_

#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <vector>
#include <list>
#include <string>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <atomic>
#include <algorithm>
#include <unordered_map>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>

// uv defines max and min as marcos
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include "pp_util.hxx"
#include "strfmt.hxx"
#include "basic_types.hxx"
#include "ptr.hxx"
#include "raft_params.hxx"
#include "msg_type.hxx"
#include "buffer.hxx"
#include "log_val_type.hxx"
#include "log_entry.hxx"
#include "msg_base.hxx"
#include "req_msg.hxx"
#include "resp_msg.hxx"
#include "rpc_exception.hxx"
#include "async.hxx"
#include "logger.hxx"
#include "srv_config.hxx"
#include "cluster_config.hxx"
#include "srv_state.hxx"
#include "srv_role.hxx"
#include "log_store.hxx"
#include "state_mgr.hxx"
#include "msg_handler.hxx"
#include "rpc_listener.hxx"
#include "snapshot.hxx"
#include "state_machine.hxx"
#include "rpc_cli.hxx"
#include "rpc_cli_factory.hxx"
#include "delayed_task.hxx"
#include "timer_task.hxx"
#include "delayed_task_scheduler.hxx"
#include "context.hxx"
#include "snapshot_sync_ctx.hxx"
#include "snapshot_sync_req.hxx"
#include "peer.hxx"
#include "raft_server.hxx"
#include "asio_service.hxx"
#include "fs_log_store.hxx"
#endif // _CORNERSTONE_HXX_

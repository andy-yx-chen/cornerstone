#ifndef _MSG_HANDLER_HXX_
#define _MSG_HANDLER_HXX_

namespace cornerstone {
	class msg_handler {
	public:
		msg_handler() {}
		virtual ~msg_handler() {}

		__nocopy__(msg_handler)

	public:
		virtual resp_msg* process_req(req_msg* req) = 0;
	};
}

#endif //_MSG_HANDLER_HXX_

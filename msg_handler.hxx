#ifndef _MSG_HANDLER_HXX_
#define _MSG_HANDLER_HXX_

namespace cornerstone {
	class msg_handler {
	__interface_body__(msg_handler)

	public:
		virtual resp_msg* process_req(req_msg* req) = 0;
	};
}

#endif //_MSG_HANDLER_HXX_

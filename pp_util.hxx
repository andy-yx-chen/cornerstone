#ifndef _PP_UTIL_HXX_
#define _PP_UTIL_HXX_

#define __override__ override
#define __nocopy__(clazz) \
	private:\
	clazz(const clazz&) = delete;\
	clazz& operator=(const clazz&) = delete;\

#define nilptr nullptr

#endif //_PP_UTIL_HXX_
#ifndef _PP_UTIL_HXX_
#define _PP_UTIL_HXX_

#define __override__ override
#define __nocopy__(clazz) \
    private:\
    clazz(const clazz&) = delete;\
    clazz& operator=(const clazz&) = delete;\

#define nilptr nullptr
#define __interface_body__(clazz) \
    public:\
    clazz(){} \
    virtual ~clazz() {} \
    __nocopy__(clazz)

#define sz_int sizeof(int32)
#define sz_ulong sizeof(ulong)
#endif //_PP_UTIL_HXX_
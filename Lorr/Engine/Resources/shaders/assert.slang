#ifdef ENABLE_ASSERTIONS
#define assert_msg(x, msg, ...) do { if (!bool(x)) { printf(msg, __VA_ARGS__); } } while(false)
#define assert(x) assert_msg(x, "Shader aborted at " __FILE__ ":%d\n", __LINE__)
#else
#define assert_msg(...)
#define assert(...)
#endif

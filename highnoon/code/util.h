#ifndef util_h
#define util_h

// Maximum string length including the zero terminator (63 real length then).
enum { MAXSTR = 64 };

// Must be called each tick.
void util_tick(void);

// Safe malloc - always succeeds.
void *smalloc(int len);

// Mark yet unhandled cases with this. Sort of a TODO.
#define HANDLE_CASE(cond) handle_case((cond), #cond, __FILE__, __LINE__);
void handle_case(bool failed, const char *cond, const char *file, int line);

// Returns a formatted string which will be valid until the next tick.
const char *qprintf(const char *format, ...);

// Display a text at the top for 5 seconds.
void set_status(int level, const char *msg);

// Display a straight line in this frame for debugging purposes.
typedef struct rectangle rect_t;
void add_debugline(const rect_t *r);

#endif

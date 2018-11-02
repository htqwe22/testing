#if 0

#include "util_string.h"
#include <string.h>
static inline const char * basename(const char *name)
{
#ifdef unix	
	const char *p = strrchr(name,'/');
#else
	const char *p = strrchr(name,'\\');
#endif
	return p? (p + 1):name;
}
#define ibug(fmt, args...) printf(fmt,##args)
#define debug(fmt,args...) printf("[%s :%d] "fmt, basename(__FILE__), __LINE__, ##args)

#endif
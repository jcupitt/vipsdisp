
typedef struct _Disp {
	GtkApplication parent_instance;

	GSList *windows;

} Disp;

typedef GtkApplicationClass DispClass;

#include "window.h"

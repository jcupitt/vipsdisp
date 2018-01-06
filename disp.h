
/* i18n placeholder.
 */
#define _(S) (S)

typedef struct _Disp {
	GtkApplication parent_instance;

} Disp;

typedef GtkApplicationClass DispClass;

#include "conversion.h"
#include "imagedisplay.h"
#include "imagepresent.h"
#include "imageview.h"

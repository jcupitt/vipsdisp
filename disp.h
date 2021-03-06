
/* The size of the tiles that we use for the libvips cache.
 */
#define tile_size (128)

/* i18n placeholder.
 */
#define _(S) (S)

extern GdkPixbuf *disp_icon;

typedef struct _Disp {
	GtkApplication parent_instance;

} Disp;

typedef GtkApplicationClass DispClass;

#include "gtkutil.h"
#include "settings.h"
#include "tslider.h"
#include "conversion.h"
#include "conversionview.h"
#include "imagedisplay.h"
#include "imagepresent.h"
#include "infobar.h"
#include "imageview.h"

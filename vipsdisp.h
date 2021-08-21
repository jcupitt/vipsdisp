#ifndef __VIPSDISP_H
#define __VIPSDISP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <vips/vips.h>

/* i18n placeholder.
 */
#define _(S) (S)

/* The tile size for image rendering.
 */
#define TILE_SIZE (128)

#include "gtkutil.h"
#include "tslider.h"
#include "vipsdispapp.h"
#include "conversion.h"
#include "conversionview.h"
#include "imagedisplay.h"
#include "imagewindow.h"

#endif /* __VIPSDISP_H */

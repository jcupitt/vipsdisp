#ifndef __VIPSDISP_H
#define __VIPSDISP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_ID "org.libvips.vipsdisp"
#define APP_PATH "/org/libvips/vipsdisp"

#include <gtk/gtk.h>

#include <vips/vips.h>

/* i18n placeholder.
 */
#define _(S) (S)

/* The tile size for image rendering.
 */
#define TILE_SIZE (256)

#include "gtkutil.h"
#include "tslider.h"
#include "vipsdispapp.h"
#include "vipsdispmarshal.h"
#include "tile.h"
#include "tilesource.h"
#include "tilecache.h"
#include "imagedisplay.h"
#include "imagewindow.h"
#include "infobar.h"
#include "displaybar.h"

#endif /* __VIPSDISP_H */

#ifndef __VIPSDISP_H
#define __VIPSDISP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_PATH "/org/libvips/vipsdisp"

#include <gtk/gtk.h>

#include <vips/vips.h>

/* i18n placeholder.
 */
#define _(S) (S)
#define GETTEXT_PACKAGE "vipsdisp"

/* The tile size for image rendering.
 */
#define TILE_SIZE (256)

/* Cache size -- enough for two 4k displays. 
 */
#define MAX_TILES (2 * (4096 / TILE_SIZE) * (2048 / TILE_SIZE))


/* We use various gtk4 features (GtkInfoBar, GtkDialog) which are going away
 * in gtk5.
 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

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
#include "saveoptions.h"
#include "properties.h"
#include "fuzzy.h"

#endif /* __VIPSDISP_H */

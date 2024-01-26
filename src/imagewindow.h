#ifndef __IMAGE_WINDOW_H
#define __IMAGE_WINDOW_H

#define IMAGE_WINDOW_TYPE (image_window_get_type())

#define VIPSDISP_IMAGE_WINDOW IMAGE_WINDOW

G_DECLARE_FINAL_TYPE( ImageWindow, image_window, 
	VIPSDISP, IMAGE_WINDOW, GtkApplicationWindow )

ImageWindow *image_window_new( VipsdispApp *app );
void image_window_open_files( ImageWindow *win, char **files, int n_files );
void image_window_open_gfiles( ImageWindow *win, GFile **gfiles, int n_files );
void image_window_open_list_gfiles( ImageWindow *win, GSList *gfiles );
void image_window_open_image( ImageWindow *win, VipsImage *image );
double image_window_get_scale( ImageWindow *win );
TileSource *image_window_get_tile_source( ImageWindow *win );
void image_window_set_tile_source( ImageWindow *win, TileSource *tile_source );
void image_window_get_mouse_position( ImageWindow *win, 
	double *image_x, double *image_y );
GtkWidget *image_window_get_main_box( ImageWindow *win );
GSettings *image_window_get_settings( ImageWindow *win );

#endif /* __IMAGE_WINDOW_H */


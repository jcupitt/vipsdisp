#ifndef __IMAGE_WINDOW_H
#define __IMAGE_WINDOW_H

#define IMAGE_WINDOW_TYPE (image_window_get_type())
#define IMAGE_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGE_WINDOW_TYPE, ImageWindow))
#define IMAGE_WINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IMAGE_WINDOW_TYPE, ImageWindowClass))
#define IS_IMAGE_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGE_WINDOW_TYPE))
#define IS_IMAGE_WINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IMAGE_WINDOW_TYPE))
#define IMAGE_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IMAGE_WINDOW_TYPE, ImageWindowClass))

G_DECLARE_FINAL_TYPE(ImageWindow, image_window,
	VIPSDISP, IMAGE_WINDOW, GtkApplicationWindow)

ImageWindow *image_window_new(VipsdispApp *app);
void image_window_open_files(ImageWindow *win, char **files, int n_files);
void image_window_open_gfiles(ImageWindow *win, GFile **gfiles, int n_files);
void image_window_open_list_gfiles(ImageWindow *win, GSList *gfiles);
void image_window_open_image(ImageWindow *win, VipsImage *image);
double image_window_get_scale(ImageWindow *win);
TileSource *image_window_get_tile_source(ImageWindow *win);
void image_window_get_mouse_position(ImageWindow *win,
	double *image_x, double *image_y);
GtkWidget *image_window_get_main_box(ImageWindow *win);
GSettings *image_window_get_settings(ImageWindow *win);

#endif /* __IMAGE_WINDOW_H */

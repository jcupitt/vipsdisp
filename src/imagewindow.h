#ifndef __IMAGEWINDOW_H
#define __IMAGEWINDOW_H

#define IMAGEWINDOW_TYPE (imagewindow_get_type())
#define IMAGEWINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGEWINDOW_TYPE, Imagewindow))
#define IMAGEWINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IMAGEWINDOW_TYPE, ImagewindowClass))
#define IS_IMAGEWINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGEWINDOW_TYPE))
#define IS_IMAGEWINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IMAGEWINDOW_TYPE))
#define IMAGEWINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IMAGEWINDOW_TYPE, ImagewindowClass))

G_DECLARE_FINAL_TYPE(Imagewindow, imagewindow,
	VIPSDISP, IMAGEWINDOW, GtkApplicationWindow)

Imagewindow *imagewindow_new(VipsdispApp *app);
void imagewindow_open_files(Imagewindow *win, char **files, int n_files);
void imagewindow_open_gfiles(Imagewindow *win, GFile **gfiles, int n_files);
void imagewindow_open_list_gfiles(Imagewindow *win, GSList *gfiles);
void imagewindow_open_image(Imagewindow *win, VipsImage *image);
double imagewindow_get_zoom(Imagewindow *win);
void imagewindow_get_mouse_position(Imagewindow *win,
	double *image_x, double *image_y);
Tilesource *imagewindow_get_tilesource(Imagewindow *win);
GtkWidget *imagewindow_get_main_box(Imagewindow *win);
GSettings *imagewindow_get_settings(Imagewindow *win);

#endif /* __IMAGEWINDOW_H */

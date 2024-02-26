#ifndef __IMAGEUI_H
#define __IMAGEUI_H

#define IMAGEUI_TYPE (imageui_get_type())
#define IMAGEUI(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGEUI_TYPE, Imageui))
#define IMAGEUI_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IMAGEUI_TYPE, ImageuiClass))
#define IS_IMAGEUI(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGEUI_TYPE))
#define IS_IMAGEUI_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IMAGEUI_TYPE))
#define IMAGEUI_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IMAGEUI_TYPE, ImageuiClass))

G_DECLARE_FINAL_TYPE(Imageui, imageui, VIPSDISP, IMAGEUI, GtkWidget)

Imageui *imageui_new(TileSource *tile_source);
double imageui_get_scale(Imageui *imageui);
TileSource *imageui_get_tile_source(Imageui *imageui);
void imageui_get_mouse_position(Imageui *imageui,
	double *image_x, double *image_y);

void imageui_bestfit(Imageui *imageui);
void imageui_magin(Imageui *imageui);
void imageui_magout(Imageui *imageui);
void imageui_oneone(Imageui *imageui);
gboolean imageui_scale(Imageui *imageui);

void imageui_image_to_gtk(Imageui *imageui,
	double x_image, double y_image, double *x_gtk, double *y_gtk);
void imageui_gtk_to_image(Imageui *imageui,
	double x_gtk, double y_gtk, double *x_image, double *y_image);
void imageui_copy_position(Imageui *new, Imageui *old);

#endif /* __IMAGEUI_H */

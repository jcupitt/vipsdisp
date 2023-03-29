#ifndef __SAVEOPTIONS_H
#define __SAVEOPTIONS_H

typedef struct SaveOptions SaveOptions; 
struct SaveOptions {
	GtkBox *parent_box;
        ImageWindow *image_window;
        GtkBox *content_box;
};

void save_options_free( SaveOptions * );
void save_options_image_window_changed( ImageWindow *, SaveOptions * );
void save_options_set_image_window( SaveOptions *, ImageWindow * );
ImageWindow * save_options_get_image_window( SaveOptions * );
void save_options_build_save_operation( SaveOptions *, VipsOperation * );
gint save_options_show( SaveOptions * );
gint save_options_hide( SaveOptions * );
void save_options_build_content_area( SaveOptions *save_options, VipsOperation *operation );
gboolean save_options_content_box_empty( SaveOptions *save_options );
gint save_options_show( SaveOptions *save_options );
gint save_options_hide( SaveOptions *save_options );
void save_options_init( SaveOptions *save_options, GtkBox *parent_box, ImageWindow *image_window );
SaveOptions * save_options_new( GtkBox *parent_box, ImageWindow *image_window );

#endif /* __SAVEOPTIONS_H */

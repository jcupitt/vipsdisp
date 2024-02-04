#ifndef __SAVE_OPTIONS_H
#define __SAVE_OPTIONS_H

#define SAVE_OPTIONS_TYPE (save_options_get_type())

G_DECLARE_FINAL_TYPE(SaveOptions, save_options,
	VIPSDISP, SAVE_OPTIONS, GtkDialog);

SaveOptions *save_options_new(GtkWindow *parent_window,
	VipsImage *in, const char *filename);

#endif /* __SAVE_OPTIONS_H */

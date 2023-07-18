#ifndef __SAVE_OPTIONS_H
#define __SAVE_OPTIONS_H

#define SAVE_OPTIONS_TYPE (save_options_get_type())

G_DECLARE_FINAL_TYPE( SaveOptions, save_options, 
	VIPSDISP, SAVE_OPTIONS, GtkDialog );

/* The success response code for this dialog.
 *
 * We can't use the usual RESPONSE_ACCEPT, since save might take a *long* time
 * and not trigger until long after the notmal accept code has been seen.
 */
typedef enum _SaveOptionsResponse {
	SAVE_OPTIONS_RESPONSE_ACCEPT = 42 	// the answer
} SaveOptionsResponse;	

SaveOptions *save_options_new( GtkWindow *parent_window,
	VipsImage *in, const char *filename );

#endif /* __SAVE_OPTIONS_H */


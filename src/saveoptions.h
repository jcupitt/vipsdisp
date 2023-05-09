#ifndef __SAVEOPTIONS_H
#define __SAVEOPTIONS_H

/* The SaveOptions class holds
 *
 * 	- widgets that manage and display the VIPS save options for a particular
 *	  VipsForeignSave object. ( These widgets are dynamically generated, by
 *	  iterating over the properties of the corresponding VipsOperation,
 *	  using the VIPS API for introspection. )
 *
 *	- as well as the count of the number of rows held by the GtkGrid widget
 *	  that holds the user input fields and their labels, once the grid has
 *        been dynamically built.
 */
typedef struct SaveOptions SaveOptions;
struct SaveOptions {
	/* This is the widget that contains the image window. The parent_box is
	 * passed to the SaveOptions constructor.
	 */
	GtkBox *parent_box;

	/* This is the widget responsible for loading, displaying, and saving
	 * images We use it to actually perform the save operation once data
	 * is collected from the user using the dynamically generated save
	 * options menu.
	 */
	ImageWindow *image_window;

	/* This is the widget that contains the grid of user inputs, where each
	 * row contains a label and an input widget that depends on the type of
	 * the property being introspected. Each label is the user-facing name
	 * of the property being introspected.
	 */
	GtkBox *content_box;

	/* The count of the number of rows currently in the GtkGrid widget
	 * within the content_box widget. As the grid is dynamically generated,
	 * the row_count is incremented. It is used as the grid row index in
	 * calls to gtk_grid_attach, as we iterate over the properties of
	 * the VipsForeignSave object.
	 */
	int row_count;

	/* The label containing the most recent VIPS error message related to
	 * the save options.
	 */
	GtkLabel *error_message_label;
};

/* Result codes used by SaveOptions methods ( currently, just
 * save_options_show ).
 */
typedef enum {
	SAVE_OPTIONS_SUCCESS = 0,
	SAVE_OPTIONS_ERROR_PATH = -1,
	SAVE_OPTIONS_ERROR_IMAGE_TYPE = -2,
} SAVE_OPTIONS_RESULT;

/* Clean up the SaveOptions object.
 */
void save_options_free( SaveOptions * );

/* Allocate space for a new SaveOptions object with NULL or zeroed members.
 */
SaveOptions * save_options_new_empty();

/* Initialize a newly allocated "empty" SaveOptions object ( created with
 * save_options_new_empty ) with a parent_box widget and an image_window widget.
 */
void save_options_init( SaveOptions *save_options, GtkBox *parent_box, ImageWindow *image_window );

/* Create a new SaveOptions object ( allocating space for it first ).
 */
SaveOptions * save_options_new( GtkBox *parent_box, ImageWindow *image_window );

/* Get a pointer to the ImageWindow object held by the SaveOptions object.
 */
ImageWindow * save_options_get_image_window( SaveOptions * );

/* Iterate over the rows of the grid of user input widgets, and use the values
 * entered by the user to build the VipsOperation for the VipsForeignSave
 * associated with the image format we want to Save As. The VipsOperation
 * argument must be previously allocated, and its "filename" and "in" properties
 * must already be initialized.
 */
void save_options_build_save_operation( SaveOptions *, VipsOperation * );

/* Present the save options menu to the user.
 */
int save_options_show( SaveOptions * );

/* Iterate over the properties of the VipsOperation to dynamically generate
 * the grid of user input widgets for the save options for the VipsForeginSave
 * associated with the desired image format.
 */
void save_options_build_content_area( SaveOptions *save_options, VipsOperation *operation );

/* Update the error message for the save options menu, and reveal it in the
 * saveas dialog.
 */
void save_options_error_message_set( SaveOptions *, char * );

#endif /* __SAVEOPTIONS_H */

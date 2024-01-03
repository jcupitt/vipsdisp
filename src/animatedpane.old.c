/*
#define DEBUG
*/

#include "vipsdisp.h"

struct _Animatedpane
{
	GtkPaned parent_instance;

	/* Position of the GtkPaned separator.
	 */
	guint paned_position;

	/* Time in seconds since the GtkPaned enter/leave animation started.
	 */
	double paned_progress;

	/* During GtkPaned enter/leave animation, this is the time in seconds
	 * since the last animation frame. When GtkPaned is not animating, this
	 * is set to the sentinel value -1.
	 */
	double paned_last_frame_time;

	/* This flag is set to TRUE before the GtkPaned separator is moved
	 * programatically for animation purposes, and is set to its initial
	 * value FALSE afterwards. It prevents image_window_properties and
	 * image_window_paned_changed from firing.
	 */
	gboolean is_paned_animating;

	/* TRUE if image_window_paned_init has been called at least once,
	 * i.e., if the first image has been opened.
	 */
	gboolean is_paned_init;

	/* The Properties widget is hidden before a new image is opened, to
	 * ensure that it renders full-size. This flag is set to remind
	 * image_window_paned_init to show the Properties widget again.
	 */
	gboolean refresh_paned;

};


/* Implement the Properties widget enter/leave animations by moving the paned
 * separator to the right edge of the ImageWindow and then hiding the Properties
 * widget.
 *
 * @win_	gpointer (ImageWindow *) A generic pointer to the ImageWindow.
 * 		Must be passed as the user_data argument in g_timeout_add.
 *
 * @frame_clock		Clock used for animations.
 *
 * @enter_	TRUE if performing the enter animation. FALSE if performing
 * 		the leave animation.
 *
 * This is a GtkTickCallback that should only be called by
 * gtk_widget_add_tick_callback.
 */
static gboolean
image_window_paned_animate_tick( GtkWidget *win_, GdkFrameClock *frame_clock,
	gpointer enter_ )
{
	ImageWindow *win;
	double dt, t;
	gint64 frame_time;
	gint new_pos, max_width;
	gboolean enter;

#ifdef DEBUG
	puts( "image_window_paned_animate_tick" );
#endif /* DEBUG */

	enter = GPOINTER_TO_INT( enter_ );
	max_width = gtk_widget_get_width( win_ );
	win = VIPSDISP_IMAGE_WINDOW( win_ );
	frame_time = gdk_frame_clock_get_frame_time( frame_clock );

	dt = win->paned_last_frame_time > 0 ?
		(double) (frame_time - win->paned_last_frame_time) / 
			G_TIME_SPAN_SECOND : 
		1.0 / G_TIME_SPAN_SECOND;

	win->paned_progress += dt;
	t = ease_out_cubic( win->paned_progress / PANED_DURATION );

	new_pos = enter ? max_width - t * (max_width - win->paned_position)
		: win->paned_position + t * (max_width - win->paned_position);

	if( t < 1.0 && (enter || new_pos < max_width - PANED_SEP_WIDTH) ) {
		win->paned_last_frame_time = frame_time;
		gtk_paned_set_position( GTK_PANED( win->paned ), new_pos );

		return( G_SOURCE_CONTINUE );
	}

	gtk_paned_set_position( GTK_PANED( win->paned ),
		enter ? win->paned_position : max_width );

	if( !enter )
		g_object_set( win->properties, "revealed", FALSE, NULL );

	win->is_paned_animating = FALSE;

	return( G_SOURCE_REMOVE );
}

/* Start the GtkPaned enter or leave animation, depending on the value of
 * @enter.
 *
 * @win		The ImageWindow.
 *
 * @enter	TRUE if the enter animation should start.
 * 		FALSE if the leave animations should start.
 */
static void
image_window_paned_animate( ImageWindow *win, gboolean enter )
{
	win->paned_last_frame_time = -1;
	win->paned_progress = 0;
	win->is_paned_animating = TRUE;

	if( enter ) {
		gtk_paned_set_position( GTK_PANED( win->paned ),
		       	gtk_widget_get_width( GTK_WIDGET( win ) ) );
		g_object_set( win->properties, "revealed", TRUE, NULL );
	}

	gtk_widget_add_tick_callback( GTK_WIDGET( win ),
		image_window_paned_animate_tick, GINT_TO_POINTER( enter ),
		NULL );
}

static gboolean
image_window_paned_should_animate( GtkWidget *t )
{
	gboolean enable_animations;

	g_object_get( gtk_widget_get_settings( t ), "gtk-enable-animations",
		&enable_animations, NULL );

	return( enable_animations );
}

/* This function is called when the Properties widget visibility is toggled
 * on/off by clicking "Properties" in the dropdown menu. The visibility is
 * backed by a GSetting named "properties", so it will persist after app
 * restart.
 *
 * action - the "properties" action defined in the GActionEntry table
 * 	image_window_entries.
 *
 * state - the new boolean value of the "properties" GSetting after the user
 * 	click.
 *
 * user_data - The ImageWindow. The Properties widget holds a pointer to the
 * 	ImageWindow as a GObject property, just like the InfoBar widget. The
 * 	Properties widget responds to changes in the VipsImage of the TileSource
 * 	held by ImageWindow by updating the grid of user input widgets based
 * 	on the properties of the new VipsImage.
 */
static void
image_window_properties( GSimpleAction *action, 
	GVariant *state, gpointer user_data )
{
	ImageWindow *win;

#ifdef DEBUG
	puts("image_window_properties");
#endif /* DEBUG */

	win = VIPSDISP_IMAGE_WINDOW( user_data );

	g_object_set( win->properties,
		"revealed", g_variant_get_boolean( state ),
		NULL );

	if( image_window_paned_should_animate( win->paned ) ) {
		if( win->is_paned_animating )
			return;

		image_window_paned_animate( win,
			g_variant_get_boolean( state ) );
	} 
	else
		g_object_set( win->properties, "revealed",
			g_variant_get_boolean( state ), NULL );

	g_simple_action_set_state( action, state );
}

/* The initial position of the GtkPaned separator. When revealed, the Properties
 * menu needs to be visible even on the smallest displays. We don't want users
 * to just see the separator on the far right of the window, since it might not
 * be obvious what it is.
 */
#define INITIAL_PANED_POSITION 200

/* This function is responsible for
 *
 *   -	revealing the Properties menu after the image has been allowed to 
 *      render full-size, if Properties::revealed is TRUE.
 *
 *   -	setting the initial position of the GtkPaned separator after the first
 *   	image is opened.
 *
 *   -	resetting the GtkPaned separator to the right edge of the ImageWindow,
 *   	when a new image is opened, if Properties::revealed is FALSE and
 *   	animations are enabled, so that the Properties widget can slide in 
 *   	from the right when revealed.
 *
 * This function is called using g_timeout_add when a new image is opened.
 * ( See image_window_open )
 *
 * If the Properties widget is visible - i.e. "revealed" - when a new image is
 * opened, image_window_open will hide the Properties widget, and set
 * ImageWindow::refresh_paned to TRUE. This function is responsible for
 * revealing it again in that case, and setting refresh_paned back to its
 * initial value FALSE.
 *
 * This function is called for the first time when the first image is opened.
 * The first time this function is called, ImageWindow::is_paned_init will 
 * have its initial value FALSE. In that case, this function is responsible 
 * for initializing the ImageWindow::paned_position to the hardcoded value
 * INITIAL_PANED_POSITION, and setting is_paned_init to TRUE.
 *
 * If animations are enabled, this function resets the separator position
 * to the far right when the Properties menu is hidden, so that it can slide
 * in when revealed.
 *
 * When animations are disabled, we just let GtkPaned automatically remember 
 * the position of its separator when its child ( the Properties widget ) 
 * is made visible again.
 *
 * However, since we animate Properties widget enter/leave by changing the
 * separator position, we are responsible for remembering it when 
 * animations are enabled, so that we know where the animation ends.
 *
 * If animations are enabled, when the user drags the separator, the new 
 * position is saved in ImageWindow::paned_position by the "notify::position"
 * signal handler, image_window_paned_changed.
 *
 * If animations are enabled, this function ( image_window_paned_init ) uses
 * the saved value in paned_position to reset the separator position when 
 * a new image is opened.
 */
static void
image_window_paned_init( ImageWindow *win )
{
	gint max_width;

#ifdef DEBUG
	puts( "image_window_paned_init" );
#endif /* DEBUG */

	if( image_window_paned_should_animate( win->paned ) ) {
		max_width = gtk_widget_get_width( GTK_WIDGET( win ) );

		if( !win->is_paned_init ) {
			win->paned_position = INITIAL_PANED_POSITION;
			win->is_paned_animating = TRUE;
			gtk_paned_set_position( GTK_PANED( win->paned ),
				max_width );
			win->is_paned_animating = FALSE;
		}

		if( win->refresh_paned )
			image_window_paned_animate( win, TRUE );

	} 
	else {
		if( !win->is_paned_init ) {
			win->paned_position = INITIAL_PANED_POSITION;
			gtk_paned_set_position( GTK_PANED( win->paned ),
				win->paned_position );
		}

		if( win->refresh_paned )
			g_object_set( win->properties, "revealed", TRUE, NULL );
	}

	win->is_paned_init = TRUE;
	win->refresh_paned = FALSE;
}

static void
image_window_paned_changed( GtkWidget *win_ )
{
	ImageWindow *win = VIPSDISP_IMAGE_WINDOW( win_ );

	gboolean revealed;

#ifdef DEBUG
	puts("image_window_paned_changed");
#endif /* DEBUG */

	win = VIPSDISP_IMAGE_WINDOW( win_ );

	if( win->is_paned_animating )
		return;

	g_object_get( win->properties, "revealed", &revealed, NULL );

	if( revealed )
		win->paned_position = gtk_paned_get_position( GTK_PANED( win->paned ) );
}

_init_ func

	/* For animating the Properties widget, the end-child of GtkPaned.
	 */
	win->is_paned_animating = FALSE;
	win->paned_position = INITIAL_PANED_POSITION;
	win->is_paned_init = FALSE;
	win->refresh_paned = FALSE;
	win->paned_progress = 0;
	win->paned_last_frame_time = -1;

	/* This is how you can check to make sure gtk-enable-animations is being
	 * respected by the Properties widget enter/leave animations.
	 */
	//g_object_set( gtk_widget_get_settings( GTK_WIDGET( win ) ),
	//	"gtk-enable-animations", FALSE, NULL );

	/* If animations are enabled, connect a signal handler that updates the
	 * saved paned separator position whenever the user drags it around.
	 *
	 * If animations are disabled, we don't need to save the separator
	 * position, since we aren't changing it, and GtkPaned automatically
	 * remembers its separator position when one of its children ( the
	 * Properties widget ) is hidden.
	 */
	if( image_window_paned_should_animate( win->paned ) )
		g_signal_connect_swapped( win->paned, "notify::position",
			G_CALLBACK( image_window_paned_changed ), win );

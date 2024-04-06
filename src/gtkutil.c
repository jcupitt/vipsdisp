// gtk utility funcs

#include "vipsdisp.h"

/* Set a GtkEditable.
 */
void
set_gentryv(GtkWidget *edit, const char *fmt, va_list ap)
{
	char buf[1000];
	gint position;
	int i;
	int len;

	if (!edit)
		return;

	if (!fmt)
		fmt = "";

	(void) vips_vsnprintf(buf, 1000, fmt, ap);

	/* Filter out /n and /t ... they confuse gtkentry terribly
	 */
	len = strlen(buf);
	for (i = 0; i < len; i++)
		if (buf[i] == '\n' || buf[i] == '\t')
			buf[i] = ' ';

	gtk_editable_delete_text(GTK_EDITABLE(edit), 0, -1);
	position = 0;
	gtk_editable_insert_text(GTK_EDITABLE(edit), buf, strlen(buf), &position);
}

/* Set a GtkEditable.
 */
void
set_gentry(GtkWidget *edit, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	set_gentryv(edit, fmt, ap);
	va_end(ap);
}

/* Get a geditable as a double.
 */
gboolean
get_geditable_double(GtkWidget *text, double *out)
{
	g_autofree char *txt = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);

	char *end;
	double t;

	t = strtod(txt, &end);
	if (end == txt)
		return FALSE;
	if (strspn(end, WHITESPACE) != strlen(end))
		return FALSE;

	*out = t;

	return TRUE;
}

/* Build a GtkEntry, with a widget width specified in characters.
 */
GtkWidget *
build_entry(int nchars)
{
	GtkWidget *entry;

	entry = gtk_entry_new();
	// FIXME
	// gtk_entry_set_width_chars( GTK_ENTRY( entry ), nchars );

	return entry;
}

/* Set the tooltip on a widget.
 */
void
set_tooltip(GtkWidget *wid, const char *fmt, ...)
{
	va_list ap;
	char *txt;

	if (!wid)
		return;

	if (!fmt)
		fmt = "";

	va_start(ap, fmt);
	txt = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	gtk_widget_set_tooltip_text(wid, txt);

	g_free(txt);
}

void
copy_adj(GtkAdjustment *to, GtkAdjustment *from)
{
	double value = gtk_adjustment_get_value(from);
	double lower = gtk_adjustment_get_lower(from);
	double upper = gtk_adjustment_get_upper(from);
	double step_increment = gtk_adjustment_get_step_increment(from);
	double page_increment = gtk_adjustment_get_page_increment(from);
	double page_size = gtk_adjustment_get_page_size(from);

	gtk_adjustment_configure(to, value,
		lower, upper,
		step_increment, page_increment, page_size);
}

void
change_state(GtkWidget *widget, const char *name, GVariant *state)
{
	GAction *action;

	action = g_action_map_lookup_action(G_ACTION_MAP(widget), name);
	if (action)
		g_action_change_state(action, state);
}

void
set_state(GtkWidget *to, GSettings *settings, const char *name)
{
	g_autoptr(GVariant) var = g_settings_get_value(settings, name);

	change_state(to, name, var);
}

GVariant *
get_state(GtkWidget *widget, const char *name)
{
	GAction *action;

	action = g_action_map_lookup_action(G_ACTION_MAP(widget), name);
	if (!action)
		return NULL;

	return g_action_get_state(action);
}

void
copy_state(GtkWidget *to, GtkWidget *from, const char *name)
{
	g_autoptr(GVariant) state = get_state(from, name);

	if (state)
		change_state(to, name, state);
}

void
set_property_from_state(GObject *to, GtkWidget *from, const char *name)
{
	GVariant *state = get_state(from, name);
	g_auto(GValue) value = { 0 };
	g_auto(GValue) to_value = { 0 };

	g_dbus_gvariant_to_gvalue(state, &value);

	g_object_get_property(to, name, &to_value);
	GType type = G_VALUE_TYPE(&to_value);
	if (G_TYPE_FUNDAMENTAL(type) == G_TYPE_ENUM &&
		G_VALUE_TYPE(&value) == G_TYPE_STRING) {
		// special case ... setting an enum from a string
		const char *nick = g_value_get_string(&value);
		int enum_v = vips_enum_from_nick("vipsdisp", type, nick);

		g_object_set(to, name, enum_v, NULL);
	}
	else
		g_object_set_property(to, name, &value);
}

/* A 'safe' way to run a few events.
 */
void
process_events(void)
{
	/* Max events we process before signalling a timeout. Without this we
	 * can get stuck in event loops in some circumstances.
	 */
	static const int max_events = 100;

	/* Block too much recursion. 0 is from the top-level, 1 is from a
	 * callback, we don't want any more than that.
	 */
	if (g_main_depth() < 2) {
		int n;

		for (n = 0; n < max_events &&
			 g_main_context_iteration(NULL, FALSE);
			 n++)
			;
	}
}

static gboolean
block_scroll_cb(GtkEventControllerScroll *self,
	gdouble dx, gdouble dy, gpointer user_data)
{
	// TRUE means we handled the event and it should not be propagated
	// further
	return TRUE;
}

/* Stop scroll events (eg. the mousewheel) changing the value of this widget.
 * Handy for eg. scale widgets inside a scrolled window.
 */
void
block_scroll(GtkWidget *widget)
{
	GtkEventController *controller = gtk_event_controller_scroll_new(
		GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);

	gtk_event_controller_set_propagation_phase(controller, GTK_PHASE_CAPTURE);
	g_signal_connect(controller,
		"scroll", G_CALLBACK(block_scroll_cb), NULL);
	gtk_widget_add_controller(widget, controller);
}

gboolean
widget_should_animate(GtkWidget *widget)
{
	gboolean enable_animations;

	g_object_get(gtk_widget_get_settings(widget),
		"gtk-enable-animations", &enable_animations,
		NULL);

	return enable_animations;
}

void
action_toggle(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	g_autoptr(GVariant) state = g_action_get_state(G_ACTION(action));

	g_action_change_state(G_ACTION(action),
		g_variant_new_boolean(!g_variant_get_boolean(state)));
}

void
action_radio(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	g_action_change_state(G_ACTION(action), parameter);
}

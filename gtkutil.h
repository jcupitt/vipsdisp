#define WHITESPACE " \t\r\b\n"

void set_gentryv( GtkWidget *edit, const char *fmt, va_list ap );
void set_gentry( GtkWidget *edit, const char *fmt, ... );
gboolean get_geditable_double( GtkWidget *text, double *out );
GtkWidget *build_entry( int nchars );
void set_tooltip( GtkWidget *wid, const char *fmt, ... );
void copy_adj( GtkAdjustment *to, GtkAdjustment *from );


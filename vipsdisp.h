
typedef struct _Disp {
	GtkApplication parent_instance;

	guint quit_inhibit;
	GMenu *time;
	guint timeout;
} Disp;

typedef GtkApplicationClass DispClass;



void settings_setb( const char *group_name, const char *key, gboolean value );
void settings_set( const char *group_name, const char *key, const char *value );
void settings_seti( const char *group_name, const char *key, int value );
void settings_setd( const char *group_name, const char *key, double value );

gboolean settings_getb( const char *group_name, const char *key );
const char *settings_get( const char *group_name, const char *key );
int settings_geti( const char *group_name, const char *key );
double settings_getd( const char *group_name, const char *key );

int settings_save( void );

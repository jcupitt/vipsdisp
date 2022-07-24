#ifndef __VIPSDISP_APP_H
#define __VIPSDISP_APP_H

#define VIPSDISP_APP_TYPE (vipsdisp_app_get_type())
G_DECLARE_FINAL_TYPE( VipsdispApp, vipsdisp_app, VIPSDISP, APP, GtkApplication )

VipsdispApp *vipsdisp_app_new( void );

#endif /* __VIPSDISP_APP_H */


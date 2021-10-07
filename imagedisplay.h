#ifndef __IMAGE_DISPLAY_H
#define __IMAGE_DISPLAY_H

#define IMAGEDISPLAY_TYPE (imagedisplay_get_type())

G_DECLARE_FINAL_TYPE( Imagedisplay, imagedisplay, 
	VIPSDISP, IMAGEDISPLAY, GtkDrawingArea )

void imagedisplay_image_to_gtk( Imagedisplay *imagedisplay, VipsRect *rect );
void imagedisplay_gtk_to_image( Imagedisplay *imagedisplay, VipsRect *rect );

Imagedisplay *imagedisplay_new( TileCache *tile_cache ); 

#endif /* __IMAGE_DISPLAY_H */

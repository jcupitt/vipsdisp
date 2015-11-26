# vipsdisp

C program to display an image with vips8 and gtk+3. This is supposed to be
a slightly useful image viewer. 

See vipsdisp-tiny for a minimal example.

### To run

```
$ ./bootstrap.sh
$ ./configure 
$ make
$ ./vipsdisp ~/pics/k2.jpg
```

### Shortcuts

* Cursor keys to scroll around
* Cursor keys plus shift to move a screen size
* Cursor keys plus ctrl to move to image edges
* Number keys to pick a particular magnification, 0 for best fit
* i, + / o, - to zoom in and out
* Mousewheel to zoom

### Structure

`Imagedisplay` is a `GtkDrawingArea` subclass that paints a `VipsImage`.

`Imagepresent` is a `GtkScrolledWindow` subclass that contains an
`Imagedisplay` and adds a lot of navigation stuff. It uses the `GtkAdjustment`
on the scrolled window to slide the drawing area around.

`Imageview` is a `GtkApplicationWindow` subclass that contains an
`Imagepresent` plus a header bar and some other UI bits.

`disp` is the `main()`

### TODO

- zoom in and out quickly, repaint breaks ... we start just getting black
  everywhere 

- tab into image display part scrolls left to x == 0?

  gtkcontainer is calling `gtk_adjustment_clamp_page()`, and clamp page is
  setting value to 0 because it's coded that way:

	https://git.gnome.org/browse/gtk+/tree/gtk/gtkadjustment.c#n932

  is this a bug? building my own gtk and swapping these < and > seems to fix
  it

- add left-click-drag scrolling

- start with non-existent or broken file should show an error

- imagepresent should draw focus indicator? could just float on top of
  viewport ... what would it look like with scrollbars?

- resize window on image replace?

- zoom out enough and you get an underflow in `imagepresent_set_mag()`

```
imagepresent_get_window_position: 0 0 360 55
imagedisplay_set_mag: -16
imagepresent_set_mag: new size 2 2
imagepresent_set_window_position: -90 -14
imagedisplay_draw_rect: left = 0, top = 0, width = 360, height = 55
imagedisplay_draw_rect: left = 0, top = 0, width = 128, height = 55
imagepresent_magout:
imagepresent_set_mag_centre:
imagepresent_get_window_position: 0 0 360 55
imagedisplay_set_mag: -32
imagepresent_set_mag: new size 1 1
imagepresent_set_window_position: -90 -14
imagedisplay_draw_rect: left = 0, top = 0, width = 360, height = 55
imagedisplay_draw_rect: left = 0, top = 0, width = 128, height = 55
imagepresent_magout:
imagepresent_set_mag_centre:
imagepresent_get_window_position: 0 0 360 55
imagedisplay_set_mag: -64
imagepresent_set_mag: new size -1281283988 32767
```
imagepresent_set_window_position: -90 -14



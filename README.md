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

  mysterious :( 

  try with wtc.jpg, happens very easily

  sometimes get:

	GLib-GObject-CRITICAL **: g_object_ref: assertion 'G_IS_OBJECT (object)' failed

  it's 

```
#2  0x00007ffff6454a3c in g_object_ref ()
    from /usr/lib/x86_64-linux-gnu/libgobject-2.0.so.0
#3  0x00007ffff685a756 in vips__link_map (image=0xe5c960, upstream=0, 
    fn=0x7ffff685598f <vips_image_invalidate_all_cb>, a=0x0, b=0x0)
    at generate.c:264
#4  0x00007ffff68559dd in vips_image_invalidate_all (image=0xe5c960)
    at image.c:1420
#5  0x00007ffff685fd0d in render_work (state=0x7fffc0001820, a=0xe62250)
```

- centre on zoom out, if the image becomes smaller than the window

- fullscreen mode isn't working?

- have another "changed" signal when the image is swapped, use that to update
  the top part of the status bar

- header display

- eog-style dark theme? I guess they just have some special css for this

- hamburger icon in menu

- tickbox in menu for status bar

- resize window on image replace?



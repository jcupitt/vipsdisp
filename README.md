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

- try with wtc.jpg, image sometimes goes black

  replacing the image does not fix it

- try with wtc.jpg, sometimes get 

```
(vipsdisp:6054): GLib-GObject-CRITICAL **: g_object_ref: assertion 'G_IS_OBJECT (object)' failed
```

  stack trace:

```
#2  0x00007ffff65c5a3c in g_object_ref ()
   from /usr/lib/x86_64-linux-gnu/libgobject-2.0.so.0
#3  0x00007ffff69d9200 in vips_region_new (image=0x7aec1b0) at region.c:523
#4  0x00007ffff69cbb54 in vips_start_one (out=0xe72640, a=0x7aec1b0, 
    b=0x7a65f80) at generate.c:439
#5  0x00007ffff69d861e in vips__region_start (region=0x7fff9c010230)
    at region.c:228
#6  0x00007ffff69db645 in vips_region_generate (reg=0x7fff9c010230)
    at region.c:1271
#7  0x00007ffff69d9eed in vips_region_fill (reg=0x7fff9c010230, 
    r=0x7fffffffb200, fn=0x7ffff69db612 <vips_region_generate>, a=0x0)
    at region.c:897
#8  0x00007ffff69db7c6 in vips_region_prepare (reg=0x7fff9c010230, 
    r=0x7fffffffb200) at region.c:1346
#9  0x00000000004071c4 in imagedisplay_draw_rect (imagedisplay=0x90aa10, 
    cr=0xf16d00, expose=0x7fffffffb280) at imagedisplay.c:95
```

  run trace:

```
imagedisplay_update_conversion: image size 18 x 18
** srgb image 0xe72640
** new region 0x7fff9c010230
imagepresent_set_mag: new size 18 18
imagepresent_set_window_position: -187 -161
imagedisplay_draw:
imagedisplay_draw_rect: left = 0, top = 0, width = 748, height = 645
** preparing region 0x7fff9c010230
```

  so our srgb region does not have the right image as `->im` ?

- centre on zoom out, if the image becomes smaller than the window

- fullscreen mode isn't working?

- have another "changed" signal when the image is swapped, use that to update
  the top part of the status bar

- header display

- eog-style dark theme? I guess they just have some special css for this

- hamburger icon in menu

- tickbox in menu for status bar

- resize window on image replace?



# vipsdisp

C program to display an image with vips8 and gtk+3. This is supposed to be
a slightly useful image viewer. 

See vipsdisp-tiny for a minimal example.

### To run

```
$ ./autogen.sh --prefix=/home/john/vips 
$ make
$ ./vipsdisp ~/pics/k2.jpg
```

### Shortcuts

* Cursor keys to scroll around
* Cursor keys plus shift to move by a screen size
* Cursor keys plus ctrl to move to image edges
* Number keys to pick a particular magnification, 0 for best fit
* i, + / o, - to zoom in and out
* Mouse drag to pan
* Mousewheel to zoom
* Mousewheel + shift/ctrl to pan


### Structure

* `Imagedisplay` is a `GtkDrawingArea` subclass that paints a `VipsImage`. It
implements a scrollable interface.

* `Conversion` is a GObject which manages the image that is being
displayed. Set things like magnification, file, scale, offset etc. on this
and the display will update automatically.

* `Imagepresent` is a `GtkScrolledWindow` subclass that contains an
`Imagedisplay` and a `Conversion` and adds a lot of navigation stuff. It
uses the scolled window `GtkAdjustment` to slide `Imagedisplay` around,
and sets properties of `Conversion` to zoom etc.

* `Imageview` is a `GtkApplicationWindow` subclass that contains an
`Imagepresent` plus a header bar and some other UI bits.

* `disp` is the `main()`, `Disp` is a `GtkApplication` subclass

### TODO

- dark theme?

- duplicate does not copy scroll position? see imageview_duplicate()

- error bar needs a "close" button

- save settings like info bar, display bar etc.

- perhaps convert to scrgb before scale/offet? we'd maybe keep float precision
  then?

- header display

- implement save as

- see https://wiki.gnome.org/HowDoI/CustomWidgets and make sure we do all that

- replace image during load can crash the viewer

  is this still true? check

- we do not size the initial window very intelligently :( it's always 800x800


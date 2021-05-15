# vipsdisp

<img src="vipsdisp-icon.png" width="150" height="150">

C program to display an image with vips8 and gtk+3. This is supposed to be
a slightly useful image viewer. 

See vipsdisp-tiny for a minimal example.

### Screenshots

It all works, though see the TODO list below.

[![Screenshot](images/shot1.jpg)](images/shot1.jpg)

[![Screenshot](images/shot2.jpg)](images/shot2.jpg)

[![Screenshot](images/shot3.jpg)](images/shot3.jpg)

### To run

```
$ ./autogen.sh --prefix=/home/john/vips 
$ make
$ ./vd ~/pics/k2.jpg
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

* `Conversionview` is the view for the conversion model.

* `Imagepresent` is a `GtkScrolledWindow` subclass that contains an
`Imagedisplay` and a `Conversion` and adds a lot of navigation stuff. It
uses the scolled window `GtkAdjustment` to slide `Imagedisplay` around,
and sets properties of `Conversion` to zoom etc.

* `Imageview` is a `GtkApplicationWindow` subclass that contains an
`Imagepresent` plus a header bar and some other UI bits.

* `disp` is the `main()`, `Disp` is a `GtkApplication` subclass

### TODO

- animated GIFs, multipage PDFs, ie. toilet / pyramid / animated modes

- ^O shortcut for replace, cf. ^W

  maybe need an action for replace?

  we have ^N and ^D for new and duplicate, show them in menu? how do visible
  menu shortcuts work in gtk3?

- header display

- load options? save options? eg. load SVG at $x DPI

- see https://wiki.gnome.org/HowDoI/CustomWidgets and make sure we do all that

- perhaps convert to scrgb before scale/offet? we'd maybe keep float precision
  then?

- dark theme?

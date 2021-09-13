# vipsdisp

<img src="vipsdisp-icon.png" width="150" height="150">

C program to display an image with vips8 and gtk+3. This is supposed to be
a slightly useful image viewer. It can display huge (many, many GB) images
quickly and without using much memory.

See vipsdisp-tiny for a minimal example.

### Screenshots

It all works, though see the TODO list below.

[![Screenshot](images/shot1.jpg)](images/shot1.jpg)

[![Screenshot](images/shot2.jpg)](images/shot2.jpg)

[![Screenshot](images/shot3.png)](images/shot3.png)

## gtk4

You need gtk4. On Ubuntu 20.10, build with:

```
$ cd gtk
$ meson _build . \
  --prefix=/home/john/vips \
  --libdir=/home/john/vips/lib \
  -Dwayland-backend=false
$ cd _build
$ ninja
$ ninja install
```

Since 20.10's wayland is too old.

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
* Number keys to pick a particular magnification
* Ctrl + number keys to pick a particular zoom out
* 0 for best fit
* i, + / o, - to zoom in and out
* , . prev page, next page
* Mouse drag to pan
* Mousewheel to zoom
* Mousewheel + shift/ctrl to pan
* ^O replace iamge
* ^N new viewer
* ^D duplicate view
* F11 fullscreen

### Structure

* `Imagedisplay` is a `GtkDrawingArea` subclass that paints a `VipsImage`. It
implements a scrollable interface.

* `Conversion` is a GObject which manages the image that is being
displayed. Set things like magnification, file, scale, offset etc. on this
and the display will update automatically.

* `Conversionview` is the view for the conversion model.

* `Imagewindow` is a GtkWindow that contains an
`Imagedisplay` and a `Conversion` and adds a lot of navigation stuff. It
uses the scolled window `GtkAdjustment` to slide `Imagedisplay` around,
and sets properties of `Conversion` to zoom etc.

* `disp` is the `main()`, `VipsdispApp` is a `GtkApplication` subclass

## gtk4

Build with:

```
$ meson _build . --prefix=/home/john/vips --libdir=/home/john/vips/lib \
  -Dwayland-backend=false
$ cd _build
$ ninja
$ ninja install
$ export GSETTINGS_SCHEMA_DIR=/home/john/vips/share/glib-2.0/schemas
$ gtk4-demo
```

### TODO

- "source id not found" errors for progress bar update

  probably a race in updates -- try throttling update speed to a few per
  second

- GIFs get very slow for large windows, even if the GIF animation area is
  small ... why?

- right-click menu on image?

- view TIFF with pages not all the same size ... error!

  need to pick an appropriate mode automatically

- settings ... can we use the gtk settings infrastructure for this?

- use a shader to implement composite with BG?

  the drawingarea gt demo uses cairo to composite over a checkerboard

- improve upscale for buffer rebuild to reduce percieved flicker

- auto reload on file change, or support F5 for reload?

- load image with long progress bar, ^D during load, progress bar stops
  updating

- header display

- load options? save options? eg. load SVG at $x DPI

- perhaps convert to scrgb before scale/offet? we'd maybe keep float precision
  then?

# vipsdisp

This is a C program to display an image with libvips and gtk+4. This is
supposed to be a slightly useful image viewer. It can display huge (many,
many GB) images quickly and without using much memory. It supports many
scientific and technical image formats, including TIFF, WEBP, JP2K, JXL, PNG, 
JPEG, SVS, MRXS, OpenEXR, GIF, PDF, SVG, FITS, Matlab, NIfTI, Analyze, etc. It
supports pixel types from 1 bit mono to 128-bit double precision complex.

All of the UI can make finding the details of image display in the sourcecode
difficult. See [vipsdisp-tiny](https://github.com/libvips/vipsdisp-tiny) for
a minimal example --- that's a libvips image viewer in about 300 lines of
very commented C.

## Screenshots

It all works, though see the TODO list below.

[![Screenshot](images/shot1.jpg)](images/shot1.jpg)

[![Screenshot](images/shot2.jpg)](images/shot2.jpg)

[![Screenshot](images/shot3.png)](images/shot3.png)


## Features

* It supports many image formats, supports many numeric pixel types,
  supports any number of image bands, supports many image colour spaces.

* It supports Save as, so you can use it for image format conversion. Though
  there's currently no GUI for save options, sadly.

* It doesn't need to keep the whole image in memory. It will only read parts 
  that it needs for display, and it understands most pyramidal image formats.
  This means you can open and view huge images quickly.

* It has threaded, asynchronous image repaint, so display pixels are
  computed in the background by a pool of workers and tiles are rendered to
  the screen as they are finished. The interface stays live even under very
  heavy load.

* Select *Display control bar* from the top-right menu and a useful
  set of visualization options appear. It supports three main display modes:
  Toilet roll (sorry), Multipage and Animated.

* In Toilet roll mode, a multi-page image is presented as a tall, thin strip
  of images. In Multipage, you see a single page at a time, with a page-select
  spinner (you can also use the `<` and `>` keys to flip pages). In animated
  mode, pages flip automatically on a timeout. 

* You can select falsecolour and log-scale filters, useful for many scientific
  images. Scale and offset sliders let you adjust image brightness to see into
  darker areas (useful for HDR and many scientific images).

* It uses the gtk4 GUI toolkit, so the interface is fast, attractive
  and nicely animated.

## Install

You need gtk4. On Ubuntu 21.04, build with:

```
$ cd gtk
$ git checkout 4.3.2
$ meson _build . \
  --prefix=/home/john/vips \
  --libdir=/home/john/vips/lib \
  -Dwayland-backend=false
$ cd _build
$ ninja
$ ninja install
$ export GSETTINGS_SCHEMA_DIR=/home/john/vips/share/glib-2.0/schemas
$ gtk4-demo
```

Since 21.04's wayland is too old. Use 4.3.2 or you'll need a new pango.

Then for vipsdisp:

```
$ ./autogen.sh --prefix=/home/john/vips 
$ make
$ ./vipsdisp ~/pics/k2.jpg
```

## Shortcuts

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
* ^O replace image
* ^N new viewer
* ^D duplicate view
* F11 fullscreen

## Structure

* `Imagedisplay` is a `GtkDrawingArea` subclass that paints a `VipsImage`. It
  implements a scrollable interface. You can use this as an image view
  widget in your own code.

* `Conversion` is a GObject which manages the image that is being
  displayed. Set things like magnification, file, scale, offset etc. on
  this and the display will update automatically.

* `Conversionview` is the view for the conversion model (it's the display
  control bar).

* `Imagewindow` is a GtkWindow that contains an `Imagedisplay` and a
  `Conversionview` and adds a lot of navigation stuff. It uses the scolled
  window `GtkAdjustment` to slide `Imagedisplay` around, and sets properties
  of `Conversion` to zoom etc.

* `disp` is the `main()`, `VipsdispApp` is a `GtkApplication` subclass

* The UI layout is in the `gtk/*.ui` xml.

## TODO

- GIFs get very slow for large windows, even if the GIF animation area is
  small ... why?

  we're not overcomputing, so it's not our fault, I think, it's just gtk4
  getting slower for very large windows

  once the framerate drops below 30 fps, the final paint is almost always
  interrupted by the arrival of the next frame

  only schedule the next paint when the current one finishes?

  see `conversion_page_flip()` -- try: 
  
  1. start a timer

  2. set page flip going

  3. in repaint, set an idle task when we paint the final tile (how do we
     know? no idea)

  4. idle task checks the timer, computes elapsed time, sets the next page
     flip timeout for (delay - time elapsed)

     could skip frames for very slow updates? track `t` rather than having a
     timeout, and pick a frame from that?

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

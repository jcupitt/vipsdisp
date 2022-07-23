# vipsdisp

This program displays an image with libvips and gtk+4. This is supposed
to be a slightly useful image viewer. It can display huge (many, many GB)
images quickly and without using much memory. It supports many scientific
and technical image formats, including TIFF, WEBP, JP2K, JXL, PNG, JPEG,
SVS, MRXS, OpenEXR, GIF, PDF, SVG, FITS, Matlab, NIfTI, Analyze, etc. It
supports pixel types from 1 bit mono to 128-bit double precision complex.

All of the UI can make finding the details of image display in the sourcecode
difficult. See [vipsdisp-tiny](https://github.com/libvips/vipsdisp-tiny) for
a minimal example --- that's a libvips image viewer in about 300 lines of
very commented C.

## Screenshots

It all works, though see the TODO list below.

[![Screenshot](images/shot1.png)](images/shot1.png)

[![Screenshot](images/shot2.png)](images/shot2.png)

[![Screenshot](images/shot3.png)](images/shot3.png)

## Install

This program is on flathub, see:

https://flathub.org/apps/search/org.libvips.vipsdisp

Just click "install".

## Features

* It supports many scientific and technical image formats, including TIFF,
  WEBP, JP2K, JXL, PNG, JPEG, SVS, MRXS, OpenEXR, GIF, PDF, SVG, FITS,
  Matlab, NIfTI, Analyze, etc. It supports many numeric pixel types, any
  number of image bands, many colour spaces.

* It has Save as, so you can use it for image format conversion. Though
  there's currently no GUI for save options, sadly.

* It doesn't need to keep the whole image in memory. It will only read parts 
  that it needs for display, and it understands most pyramidal image formats.
  This means you can open and view huge images quickly.

* It has threaded, asynchronous image repaint, so display pixels are
  computed in the background by a pool of workers and tiles are rendered to
  the screen as they are finished. The interface stays live even under very
  heavy load.

* Select *Display control bar* from the top-right menu and a useful
  set of visualization options appear. It supports four main display modes:
  Toilet roll (sorry), Multipage, Animated, and Pages as Bands.

* In Toilet roll mode, a multi-page image is presented as a tall, thin strip
  of images. In Multipage, you see a single page at a time, with a page-select
  spinner (you can also use the `crtl-<` and `ctrl->` keys to flip pages). In
  animated mode, pages flip automatically on a timeout. In pages-as-bands
  mode, many-page single-band images (eg. OME-TIFF) are presented as a 
  single colour image.

* You can select falsecolour and log-scale filters, useful for many scientific
  images. Scale and offset sliders let you adjust image brightness to see into
  darker areas (useful for HDR and many scientific images).

* It uses the gtk4 GUI toolkit, so the interface is fast, attractive
  and nicely animated. The image is rendered with the GPU, so display ought to
  be fast.

## Build from source

You need gtk4. On Ubuntu 22.04, build with:

```
$ cd gtk
$ meson build . \
  --prefix=/home/john/vips \
  --libdir=/home/john/vips/lib 
  -Dintrospection=disabled \
$ cd build
$ ninja
$ ninja install
```

Then for vipsdisp:

```
$ cd vipsdisp
$ meson setup build --prefix=~/vips
$ cd build
$ ninja
$ ninja install
```

And to run:

```
$ export GSETTINGS_SCHEMA_DIR=/home/john/vips/share/glib-2.0/schemas
$ vipsdisp ~/pics/k2.jpg
```

## Shortcuts

* Cursor keys to scroll around
* Cursor keys plus shift to move by a screen size
* Cursor keys plus ctrl to move to image edges
* Number keys to pick a particular magnification
* Ctrl + number keys to pick a particular zoom out
* 0 for best fit
* d, to toggle debug rendering mode
* i, + / o, - to zoom in and out
* ctrl-<, ctrl->. prev page, next page
* Mouse drag to pan
* Mousewheel to zoom
* Mousewheel + shift/ctrl to pan
* ^O replace image
* ^N new viewer
* ^D duplicate view
* F11 fullscreen

## Structure

* `TileSource` wraps a `VipsImage` and can produce pyramid tiles on request.
   It has controls for things like scale and falsecolour.

* `TileCache` builds a sparse pyramid of tiles and keeps recents.

* `Imagedisplay` is a `GtkDrawingArea` subclass that paints a `TileCache`. It
  implements a scrollable interface. You can use this as an image view
  widget in your own code. It takes a float scale factor for zoom and uses
  that to fetch tiles of the right size from the `TileCache`.

* `Imagewindow` is a GtkWindow that contains an `Imagedisplay` and 
  adds a lot of navigation stuff. It uses the scolled
  window `GtkAdjustment` to slide `Imagedisplay` around.

* `disp` is the `main()`, `VipsdispApp` is a `GtkApplication` subclass

* The UI layout is in the `gtk/*.ui` xml.

## TODO

- switcher does not remember settings wtf

- ~/AI/b0.png (ushort PNG with low numeric values) cannot be visualised

- info bar:

    - info bar does not take page number into account

    - pages as bands ... info bar displays only one band

    - should update on page flip

    - will not display complex numbers correctly ... need to unpack to bands

- how should we handle images which include labels, macros, thumbnails?

- zoom menu could have 10:1 and 1:10 as well?

- zoom on the audi PDF seems to be broken

    - look at the PDF / SVG zoom patch again?

- improve initial size 

    - fit image aspect ratio? 

    - take progress bar into account

* rename replace as open, and include an icon

- constantly flip pages on single-page GIFs

- can still show black tiles occasionally?

    - we could shrink tiles on zoom out (we only expand tiles now)

- add imagemagick to get dicom loader?

- auto reload on file change, or support F5 for reload?

- load image with long progress bar, ^D during load, progress bar stops
  updating

- no progress bar for replace? it works for initial load though

- header display

- load options? save options? eg. load SVG at $x DPI

- perhaps convert to scrgb before scale/offet? we'd maybe keep float precision
  then?

## Version bump checklist

Version needs updating in the following places:

- **`org.libvips.vipsdisp.appdata.xml`** and some release notes and a date as
  well.

- **`org.libvips.vipsdisp.json`** needs the version number as a git tag.

- **`org.libvips.vipsdisp.desktop`** also has a version number.

## flatpak

Add the `flathub` repo:

```
flatpak remote-add --if-not-exists \
  flathub https://flathub.org/repo/flathub.flatpakrepo
```

Install the gtk4 SDK:

```
flatpak install org.gnome.Sdk//41
```

Build:

```
flatpak-builder --force-clean build-dir org.libvips.vipsdisp.json
```

Install the binary you just made to your local account and try running it:

```
flatpak-builder --force-clean --user --install build-dir org.libvips.vipsdisp.json
flatpak run org.libvips.vipsdisp ~/pics/k2.jpg
```

Force a complete redownload and rebuild (should only rarely be necessary) with:

```
rm -rf .flatpak-builder
```

Check the files that are in the flatpak you built with:

```
ls build-dir/files
```

Uninstall with:

```
flatpak uninstall vipsdisp
```

## Notes on flatpak build process

- niftiio is annoying to build, skip it.

- we skip imagemagick as well, too huge

## Packaging for flathub

Install the appdata checker:

```
flatpak install flathub org.freedesktop.appstream-glib
flatpak run org.freedesktop.appstream-glib validate org.libvips.vipsdisp.appdata.xml
```

## Uploading to flathub

Push to master on:

        https://github.com/flathub/org.libvips.vipsdisp 

then check the build status here:

        https://flathub.org/builds/#/apps/org.libvips.vipsdisp

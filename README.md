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

- does gtk3 support self-painted windows? we could stop the clear to grey,
  perhaps

- centre on zoom out, if the image becomes smaller than the window

- fullscreen mode ?

- have another "changed" signal when the image is swapped, use that to update
  the top part of the status bar

- header display

- eog-style dark theme? I guess they just have some special css for this

- hamburger icon in menu

- tickbox in menu for status bar

- resize window on image replace?



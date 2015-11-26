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

### TODO

- zoom in and out quickly, repaint breaks ... we start just getting black
  everywhere 
- does not quit correctly? ^Q works, x in title bar does not?
- keyboard stops working sometimes ... the kb focus gets on one of the
  titlebar buttons and clicking on the image does not the focus back there
  again
- resize window on image replace?



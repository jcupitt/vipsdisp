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

- tab into image display part scrolls left to x == 0?

  seems to happen whenever imagedisplay gets focus? you get it with left 
  click to move focus to imagedisplay as well

- start with nopn-existent or broken file should show an error

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



## master

- add drag/drop and copy/paste of filenames and textures
- add "reload"
- better event handling in imagewindow

# 2.6.3a, 7/1/24

- fix clang build

# 2.6.3, 7/1/24

- properties display [angstyloop]

# 2.6.2, 28/10/23

- better saveoptions sizing
- add flags support to saveoptions
- better discrete zoom animation

## 2.6.1, 12/10/23

- async infobar updates

## 2.6, 7/9/23

- limit bands in pages-as-bands to prevent 3000 page images causing 
  performance issues
- switch to file based image load (not source) to fix caching
- better conversion to RGB
- animate keypress zoom
- use NN sampling for zoom with gtk 4.10+
- add colour management

## 2.5.1, 23/7/23

- fix progress bar (again)

## 2.5.0, 22/7/23

- save options [angstyloop]

## 2.4.1, 21/8/22

- remove stray printfs
- fix scale + offset sliders

## 2.4, 19/8/22

- match zoom direction and step size on scrollwheel with eog
- basic support for pinch zoom [earboxer]
- new display menu items let you pick image background 
- better visualisation of high bit-depth images
- better redraw tree computation
- much faster info bar updates
- update infobar on page flip
- implement SVG zoom
- better OME tiff support

## 2.3, started 20/10/21

- better initial shrink-to-fit logic

## 2.2, started 14/10/21

- press "d" to enable debug rendering
- another reworking of rendering ... should be faster and smoother again
- added animations for pan/zoom
- shrink to fit on load

## 2.1, started 14/10/21

- switch to gtk4-wayland
- better .desktop

## 2.0, started 28/9/21

- new rendering system: much faster, smooth zoom
- added "pages as bands" view mode
- better file format sniffing and pyramid detection

## 1.0

- first gtk4 release!

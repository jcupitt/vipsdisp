## master

- match zoom direction and step size on scrollwheel with eog
- support pinch zoom [earboxer]
- background switcher

## Issues

* switcher does not remember settings wtf

* ~/AI/b0.png (ushort PNG with low numeric values) cannot be visualised

* info bar crashes

    - info bar does not take page number into account

* zoom menu could have 10:1 and 1:10 as well?

* zoom on the audi PDF seems to be broken

    - look at the PDF / SVG zoom patch again?

* improve initial size 

    - fit image aspect ratio? 

    - take progress bar into account

* rename replace as open, and include an icon

* no progress bar for replace? it works for initial load though

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

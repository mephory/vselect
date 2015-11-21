# vselect
visually select a rectangular area of an image

![](example.gif)

## Usage

```
vselect [OPTION]... FILENAME

OPTIONS
    -f FORMAT    specify an output format
```

### Specifying the output format
The output format can be any string, where the following substrings will be
replaced by the corresponding value of the selected rectangle:
```
    %l / %x  left side
    %t / %y  top side
    %r       right side
    %b       bottom side
    %w       width
    %h       height
```

The default format is the default geometry syntax imagemagick uses:
`%wx%h+%x+%y`

### Keyboard and mouse bindings

```
q            quit
arrow up     zoom in
arrow down   zoom out
left mouse   select rectangle
middle mouse move image
right mouse  confirm and exit
```


## Examples

### Crop an image
```sh
convert image.png -crop $(vselect image.png) +repage cropped_image.png
```

### Draw red rectangle over image
```sh
convert image.png -fill transparent -stroke red \
-draw "rectangle $(vselect image.png -f '%l,%t %r,%b')" out.png
```


### Record an area with ffmpeg and x11grab
```sh
AREA="$(vselect <(import -window root png:-) -f '%x %y %w %h')"
AREA_X="$(echo $AREA | cut -d' ' -f1)"
AREA_Y="$(echo $AREA | cut -d' ' -f2)"
AREA_W="$(echo $AREA | cut -d' ' -f3)"
AREA_H="$(echo $AREA | cut -d' ' -f4)"

ffmpeg -f x11grab -video_size "$AREA_W"x"$AREA_H" -framerate 25 -show_region 1 -i :0+$AREA_X,$AREA_Y $@
```

#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_png="$project_dir/assets/visual/time-station-navigation-atlas-v1.png"
output_c="$project_dir/main/assets/navigation_icons.c"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/time-station-nav.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

names="inn plan farm trip bag"
crops="266:430:106:128 338:426:486:142 390:474:882:118 344:458:1359:122 348:406:1751:162"

set -- $names
name_list="$*"
set -- $crops
crop_list="$*"

{
    printf '%s\n' '#include "visual_assets.h"' ''
    printf '%s\n' '#ifndef LV_ATTRIBUTE_MEM_ALIGN' '#define LV_ATTRIBUTE_MEM_ALIGN' '#endif' ''
} > "$output_c"

i=1
for name in $name_list; do
    crop=$(printf '%s\n' $crop_list | sed -n "${i}p")
    color="$work_dir/$name.rgb565"
    alpha="$work_dir/$name.a8"
    merged="$work_dir/$name.rgb565a8"
    # Each icon has a different transparent bounding box. Crop it independently,
    # preserve its aspect ratio, then center it on a shared transparent canvas.
    filter="crop=$crop,scale=40:40:force_original_aspect_ratio=decrease:flags=neighbor,pad=42:42:(ow-iw)/2:(oh-ih)/2:color=black@0,format=rgba"

    ffmpeg -loglevel error -y -i "$source_png" -vf "$filter" \
        -f rawvideo -pix_fmt rgb565le "$color"
    ffmpeg -loglevel error -y -i "$source_png" \
        -vf "$filter,format=rgba,alphaextract" -f rawvideo -pix_fmt gray "$alpha"
    cat "$color" "$alpha" > "$merged"

    {
        printf 'static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t s_nav_%s_map[] = {\n' "$name"
        od -An -v -t x1 "$merged" | awk '{ for (j = 1; j <= NF; j++) printf "0x%s,", $j; print "" }'
        printf '%s\n' '};'
        printf 'const lv_image_dsc_t visual_nav_%s = {\n' "$name"
        printf '%s\n' \
            '    .header.magic = LV_IMAGE_HEADER_MAGIC,' \
            '    .header.cf = LV_COLOR_FORMAT_RGB565A8,' \
            '    .header.flags = 0,' \
            '    .header.w = 42,' \
            '    .header.h = 42,' \
            '    .header.stride = 84,'
        printf '    .data_size = sizeof(s_nav_%s_map),\n' "$name"
        printf '    .data = s_nav_%s_map,\n' "$name"
        printf '%s\n\n' '};'
    } >> "$output_c"
    i=$((i + 1))
done

printf 'Generated %s (5 x 5292 bytes RGB565A8)\n' "$output_c"

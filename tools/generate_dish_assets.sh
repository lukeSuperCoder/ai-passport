#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_png="$project_dir/assets/visual/time-station-dishes-v1.png"
output_c="$project_dir/main/assets/dish_sprites.c"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/time-station-dishes.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

# Recipe enum order: hot bread, carrot stew, strawberry jam, herb tea,
# forest cake. Bounds follow each alpha silhouette with a safe gutter.
names="hot_bread carrot_stew strawberry_jam herb_tea forest_cake"
crops="375:412:25:170 391:335:431:247 361:373:883:215 368:410:1299:177 316:398:1705:184"

{
    printf '%s\n' '#include "visual_assets.h"' ''
    printf '%s\n' '#ifndef LV_ATTRIBUTE_MEM_ALIGN' '#define LV_ATTRIBUTE_MEM_ALIGN' '#endif' ''
} > "$output_c"

i=1
for name in $names; do
    crop=$(printf '%s\n' $crops | sed -n "${i}p")
    color="$work_dir/$name.rgb565"
    alpha="$work_dir/$name.a8"
    merged="$work_dir/$name.rgb565a8"
    filter="crop=$crop,scale=52:52:force_original_aspect_ratio=decrease:flags=neighbor,pad=56:56:(ow-iw)/2:(oh-ih)/2:color=black@0,format=rgba"

    ffmpeg -loglevel error -y -i "$source_png" -vf "$filter" \
        -f rawvideo -pix_fmt rgb565le "$color"
    ffmpeg -loglevel error -y -i "$source_png" \
        -vf "$filter,alphaextract" -f rawvideo -pix_fmt gray "$alpha"
    cat "$color" "$alpha" > "$merged"

    {
        printf 'static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t s_dish_%s_map[] = {\n' "$name"
        od -An -v -t x1 "$merged" | awk '{ for (j = 1; j <= NF; j++) printf "0x%s,", $j; print "" }'
        printf '%s\n' '};'
        printf 'static const lv_image_dsc_t s_dish_%s = {\n' "$name"
        printf '%s\n' \
            '    .header.magic = LV_IMAGE_HEADER_MAGIC,' \
            '    .header.cf = LV_COLOR_FORMAT_RGB565A8,' \
            '    .header.flags = 0,' \
            '    .header.w = 56,' \
            '    .header.h = 56,' \
            '    .header.stride = 112,'
        printf '    .data_size = sizeof(s_dish_%s_map),\n' "$name"
        printf '    .data = s_dish_%s_map,\n' "$name"
        printf '%s\n\n' '};'
    } >> "$output_c"
    i=$((i + 1))
done

printf '%s\n' \
    'const lv_image_dsc_t *const visual_dishes[5] = {' \
    '    &s_dish_hot_bread, &s_dish_carrot_stew, &s_dish_strawberry_jam,' \
    '    &s_dish_herb_tea, &s_dish_forest_cake,' \
    '};' >> "$output_c"

bytes=$((5 * 56 * 56 * 3))
printf 'Generated %s (%s bytes RGB565A8)\n' "$output_c" "$bytes"

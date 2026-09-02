#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_png="$project_dir/assets/visual/time-station-farm-crops-v1.png"
output_c="$project_dir/main/assets/farm_crops.c"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/time-station-farm.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

# Source coordinates are intentionally recorded here so the generated C asset is
# deterministic. Each crop is emitted in four stages: seed, sprout, growing,
# mature. The source sheet's light checkerboard is removed before quantization.
names="wheat_0 wheat_1 wheat_2 wheat_3 carrot_0 carrot_1 carrot_2 carrot_3 strawberry_0 strawberry_1 strawberry_2 strawberry_3 herb_0 herb_1 herb_2 herb_3"
crops="155:190:260:270 150:190:430:260 185:235:580:230 230:290:770:145 175:210:25:570 190:230:255:550 225:255:505:525 245:300:755:470 180:220:25:835 195:235:260:820 225:265:505:790 255:285:750:775 175:220:25:1120 200:245:255:1090 230:275:500:1055 255:300:750:1030"

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
    filter="crop=$crop,colorkey=0xffffff:0.16:0.0,scale=56:52:force_original_aspect_ratio=decrease:flags=neighbor,pad=56:52:(ow-iw)/2:(oh-ih)/2:color=black@0,format=rgba"

    ffmpeg -loglevel error -y -i "$source_png" -vf "$filter" \
        -f rawvideo -pix_fmt rgb565le "$color"
    ffmpeg -loglevel error -y -i "$source_png" \
        -vf "$filter,alphaextract" -f rawvideo -pix_fmt gray "$alpha"
    cat "$color" "$alpha" > "$merged"

    {
        printf 'static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t s_farm_%s_map[] = {\n' "$name"
        od -An -v -t x1 "$merged" | awk '{ for (j = 1; j <= NF; j++) printf "0x%s,", $j; print "" }'
        printf '%s\n' '};'
        printf 'static const lv_image_dsc_t s_farm_%s = {\n' "$name"
        printf '%s\n' \
            '    .header.magic = LV_IMAGE_HEADER_MAGIC,' \
            '    .header.cf = LV_COLOR_FORMAT_RGB565A8,' \
            '    .header.flags = 0,' \
            '    .header.w = 56,' \
            '    .header.h = 52,' \
            '    .header.stride = 112,'
        printf '    .data_size = sizeof(s_farm_%s_map),\n' "$name"
        printf '    .data = s_farm_%s_map,\n' "$name"
        printf '%s\n\n' '};'
    } >> "$output_c"
    i=$((i + 1))
done

{
    printf '%s\n' 'const lv_image_dsc_t *const visual_farm_crops[4][4] = {'
    printf '%s\n' \
        '    { &s_farm_wheat_0, &s_farm_wheat_1, &s_farm_wheat_2, &s_farm_wheat_3 },' \
        '    { &s_farm_carrot_0, &s_farm_carrot_1, &s_farm_carrot_2, &s_farm_carrot_3 },' \
        '    { &s_farm_strawberry_0, &s_farm_strawberry_1, &s_farm_strawberry_2, &s_farm_strawberry_3 },' \
        '    { &s_farm_herb_0, &s_farm_herb_1, &s_farm_herb_2, &s_farm_herb_3 },' \
        '};'
} >> "$output_c"

bytes=$((16 * 56 * 52 * 3))
printf 'Generated %s (%s bytes RGB565A8)\n' "$output_c" "$bytes"

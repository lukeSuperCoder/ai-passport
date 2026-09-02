#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
scene_png="$project_dir/assets/visual/time-station-mistpine-forest-v1.png"
icons_png="$project_dir/assets/visual/time-station-expedition-icons-v1.png"
output_c="$project_dir/main/assets/expedition_assets.c"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/time-station-expedition.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

scene_raw="$work_dir/scene.rgb565"
ffmpeg -loglevel error -y -i "$scene_png" \
    -vf "scale=240:228:force_original_aspect_ratio=increase:flags=neighbor,crop=240:228" \
    -f rawvideo -pix_fmt rgb565le "$scene_raw"

{
    printf '%s\n' '#include "visual_assets.h"' ''
    printf '%s\n' '#ifndef LV_ATTRIBUTE_MEM_ALIGN' '#define LV_ATTRIBUTE_MEM_ALIGN' '#endif' ''
    printf '%s\n' 'static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t s_mistpine_scene_map[] = {'
    od -An -v -t x1 "$scene_raw" | awk '{ for (i = 1; i <= NF; i++) printf "0x%s,", $i; print "" }'
    printf '%s\n' '};' ''
    printf '%s\n' 'const lv_image_dsc_t visual_mistpine_scene = {' \
        '    .header.magic = LV_IMAGE_HEADER_MAGIC,' \
        '    .header.cf = LV_COLOR_FORMAT_RGB565,' \
        '    .header.flags = 0,' \
        '    .header.w = 240,' \
        '    .header.h = 228,' \
        '    .header.stride = 480,' \
        '    .data_size = sizeof(s_mistpine_scene_map),' \
        '    .data = s_mistpine_scene_map,' \
        '};' ''
} > "$output_c"

names="wood berries mushrooms rare materials road scenery"
# These are per-icon alpha bounds plus a six-pixel source gutter. Do not expand
# them back to equal cells: the generated sheet spacing is intentionally uneven.
crops="254:224:35:302 228:232:323:294 220:232:577:294 190:242:845:284 235:235:1088:293 255:256:1333:279 260:231:1612:302"
i=1
for name in $names; do
    crop=$(printf '%s\n' $crops | sed -n "${i}p")
    color="$work_dir/$name.rgb565"
    alpha="$work_dir/$name.a8"
    merged="$work_dir/$name.rgb565a8"
    filter="crop=$crop,scale=28:28:force_original_aspect_ratio=decrease:flags=neighbor,pad=30:30:(ow-iw)/2:(oh-ih)/2:color=black@0,format=rgba"
    ffmpeg -loglevel error -y -i "$icons_png" -vf "$filter" \
        -f rawvideo -pix_fmt rgb565le "$color"
    ffmpeg -loglevel error -y -i "$icons_png" \
        -vf "$filter,alphaextract" -f rawvideo -pix_fmt gray "$alpha"
    cat "$color" "$alpha" > "$merged"

    {
        printf 'static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t s_expedition_%s_map[] = {\n' "$name"
        od -An -v -t x1 "$merged" | awk '{ for (j = 1; j <= NF; j++) printf "0x%s,", $j; print "" }'
        printf '%s\n' '};'
        printf 'static const lv_image_dsc_t s_expedition_%s = {\n' "$name"
        printf '%s\n' \
            '    .header.magic = LV_IMAGE_HEADER_MAGIC,' \
            '    .header.cf = LV_COLOR_FORMAT_RGB565A8,' \
            '    .header.flags = 0,' \
            '    .header.w = 30,' \
            '    .header.h = 30,' \
            '    .header.stride = 60,'
        printf '    .data_size = sizeof(s_expedition_%s_map),\n' "$name"
        printf '    .data = s_expedition_%s_map,\n' "$name"
        printf '%s\n\n' '};'
    } >> "$output_c"
    i=$((i + 1))
done

printf '%s\n' \
    'const lv_image_dsc_t *const visual_expedition_icons[7] = {' \
    '    &s_expedition_wood, &s_expedition_berries, &s_expedition_mushrooms,' \
    '    &s_expedition_rare, &s_expedition_materials, &s_expedition_road,' \
    '    &s_expedition_scenery,' \
    '};' >> "$output_c"

printf 'Generated %s (128340 bytes RGB565/RGB565A8)\n' "$output_c"

#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_png="$project_dir/assets/visual/time-station-station-background-v1.png"
raw_file=$(mktemp "${TMPDIR:-/tmp}/time-station-scene.XXXXXX.rgb565")
output_c="$project_dir/main/assets/station_scene.c"
trap 'rm -f "$raw_file"' EXIT

mkdir -p "$project_dir/main/assets"

# Quantize the full-bleed scenic background to the exact on-device size. RGB565
# keeps rendering deterministic and avoids PNG decode RAM.
ffmpeg -loglevel error -y -i "$source_png" \
    -vf "scale=240:320:flags=neighbor" \
    -f rawvideo -pix_fmt rgb565le "$raw_file"

{
    printf '%s\n' '#include "visual_assets.h"' ''
    printf '%s\n' '#ifndef LV_ATTRIBUTE_MEM_ALIGN' '#define LV_ATTRIBUTE_MEM_ALIGN' '#endif' ''
    printf '%s\n' 'static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t s_station_scene_map[] = {'
    od -An -v -t x1 "$raw_file" | awk '{ for (i = 1; i <= NF; i++) printf "0x%s,", $i; print "" }'
    printf '%s\n' '};' ''
    printf '%s\n' 'const lv_image_dsc_t visual_station_scene = {' \
        '    .header.magic = LV_IMAGE_HEADER_MAGIC,' \
        '    .header.cf = LV_COLOR_FORMAT_RGB565,' \
        '    .header.flags = 0,' \
        '    .header.w = 240,' \
        '    .header.h = 320,' \
        '    .header.stride = 480,' \
        '    .data_size = sizeof(s_station_scene_map),' \
        '    .data = s_station_scene_map,' \
        '};'
} > "$output_c"

bytes=$(wc -c < "$raw_file" | tr -d ' ')
printf 'Generated %s (%s bytes RGB565)\n' "$output_c" "$bytes"

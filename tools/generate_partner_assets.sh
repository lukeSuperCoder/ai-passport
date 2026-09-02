#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_png="$project_dir/assets/visual/time-station-partners-v1.png"
output_c="$project_dir/main/assets/partner_sprites.c"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/time-station-partners.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

# Four rows (Momo, Lulu, Amai, Atuan), three columns (idle, work, tired).
# Bounds follow each pose's alpha silhouette with a small safe gutter. This is
# deliberately not a regular grid: several poses extend close to the next row.
names="momo_idle momo_work momo_tired lulu_idle lulu_work lulu_tired amai_idle amai_work amai_tired atuan_idle atuan_work atuan_tired"
crops="268:298:44:74 266:332:369:39 265:200:711:172 212:353:79:395 291:357:358:391 297:197:685:547 296:300:27:784 314:292:341:781 311:190:689:898 257:375:35:1106 249:386:370:1095 290:322:701:1163"

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
    filter="crop=$crop,scale=40:44:force_original_aspect_ratio=decrease:flags=neighbor,pad=40:44:(ow-iw)/2:(oh-ih)/2:color=black@0,format=rgba"

    ffmpeg -loglevel error -y -i "$source_png" -vf "$filter" \
        -f rawvideo -pix_fmt rgb565le "$color"
    ffmpeg -loglevel error -y -i "$source_png" \
        -vf "$filter,alphaextract" -f rawvideo -pix_fmt gray "$alpha"
    cat "$color" "$alpha" > "$merged"

    {
        printf 'static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t s_partner_%s_map[] = {\n' "$name"
        od -An -v -t x1 "$merged" | awk '{ for (j = 1; j <= NF; j++) printf "0x%s,", $j; print "" }'
        printf '%s\n' '};'
        printf 'static const lv_image_dsc_t s_partner_%s = {\n' "$name"
        printf '%s\n' \
            '    .header.magic = LV_IMAGE_HEADER_MAGIC,' \
            '    .header.cf = LV_COLOR_FORMAT_RGB565A8,' \
            '    .header.flags = 0,' \
            '    .header.w = 40,' \
            '    .header.h = 44,' \
            '    .header.stride = 80,'
        printf '    .data_size = sizeof(s_partner_%s_map),\n' "$name"
        printf '    .data = s_partner_%s_map,\n' "$name"
        printf '%s\n\n' '};'
    } >> "$output_c"
    i=$((i + 1))
done

{
    printf '%s\n' 'const lv_image_dsc_t *const visual_partners[4][3] = {'
    printf '%s\n' \
        '    { &s_partner_momo_idle, &s_partner_momo_work, &s_partner_momo_tired },' \
        '    { &s_partner_lulu_idle, &s_partner_lulu_work, &s_partner_lulu_tired },' \
        '    { &s_partner_amai_idle, &s_partner_amai_work, &s_partner_amai_tired },' \
        '    { &s_partner_atuan_idle, &s_partner_atuan_work, &s_partner_atuan_tired },' \
        '};'
} >> "$output_c"

bytes=$((12 * 40 * 44 * 3))
printf 'Generated %s (%s bytes RGB565A8)\n' "$output_c" "$bytes"

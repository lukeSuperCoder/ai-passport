# Time Station production visuals

`time-station-station-background-v1.png` is the sole production style master.
Its prompt records the approved generation brief. The supporting concept sheet
at `../concepts/time-station-visual-validation-v1.png` documents asset families
and character poses, but the production background controls rendering style,
palette, pixel density, lighting, and composition.

Exploratory direction images have been deleted. New work must start from the
production background and follow `docs/VISUAL_ASSET_GUIDE.md`.

Run `tools/generate_visual_assets.sh` after replacing the background. The script
scales it to its exact 240 x 320 on-device dimensions and emits an uncompressed
RGB565 LVGL image in `main/assets/station_scene.c`. The generated scene costs
153,600 bytes of Flash and requires no decode buffer.

Source assets must contain no UI frames or text. All localized copy remains live
LVGL text.

`time-station-farm-crops-v1.png` is the source sheet for the four farm crops and
their four growth stages. Run `tools/generate_farm_assets.sh` after replacing it;
the generated RGB565A8 atlas costs 139,776 bytes of Flash and needs no decoder.

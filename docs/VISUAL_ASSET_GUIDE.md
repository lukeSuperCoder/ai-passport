# Time Station production theme

## Direction

The sole production style master is
`assets/visual/time-station-station-background-v1.png`. The supporting concept
sheet is `assets/concepts/time-station-visual-validation-v1.png`. When they
differ, follow the production background for rendering and composition; use the
concept sheet only for asset families and character poses.

The theme is a bright Japanese countryside spring: clear blue sky, blue-green
mountains, cherry blossom, fresh vegetation, a traditional timber-and-plaster
inn, warm windows, restrained red accents, and a black-and-white cat with a red
scarf.
Generated source art never contains text; all copy is rendered by LVGL so
localization and accessibility remain functional.

## Non-negotiable style rules

- Japanese countryside architecture only; no western tavern or medieval motifs.
- Bright, optimistic daylight is the default. Weather and night variants retain
  the same silhouettes, saturation relationships, and pixel density.
- Use handcrafted 16-bit pixel clusters and readable silhouettes. Do not mix in
  smooth vector art, painterly rendering, 3D lighting, or high-resolution icons.
- The cat keeps its black-and-white coat, white muzzle and chest, red scarf,
  rounded head, short limbs, and compact sprite proportions in every pose.
- Environments layer sky, teal mountains, buildings and vegetation, then darker
  foreground foliage to create depth.
- Scenes are continuous edge to edge. Never bake UI panels or localized text into
  scene art.
- Avoid opaque full-width top and bottom bars. Prefer outlined text, local pills,
  translucent parchment cards, and independent controls.
- Review new art after RGB565 conversion at native 240 x 320 resolution.

## Core palette

| Role | RGB |
| --- | --- |
| Header blue | `#173B5B` |
| Sky blue | `#4AA6E8` |
| Mountain teal | `#3F819A` |
| Spring green | `#77AA3F` |
| Cherry pink | `#F28DA8` |
| Warm yellow | `#F4C64E` |
| Timber | `#704936` |
| Red accent | `#C94A46` |
| Parchment | `#FFF0C9` |
| Ink | `#263039` |

## Device rules

- Design at the native 240 x 320 portrait resolution. Do not scale UI at run time.
- Keep text and primary icons inside a 6 px edge-safe area.
- Use RGB565 for opaque scene layers. Transparency is reserved for small sprites.
- Avoid full-screen decode buffers, gradients, and large animated surfaces.
- Keep interactive text as LVGL labels; do not bake localized text into images.
- A focused control must change both border weight/color and shadow, not color alone.

## UI overlay language

- Top status uses time on the left, a compact centered date/weather pill, and
  battery on the right. Only the centered pill has a local translucent backdrop.
- Dialogue uses translucent parchment, warm-yellow edge, 5 px radius, and a soft
  dark shadow so scenery remains visible beneath it.
- Navigation uses five independent parchment controls and a warm-yellow selected
  state, with no shared navigation-bar background.
- Primary copy uses ink, alerts use red, and selection/progress uses warm yellow.
- Code palette and reusable theme constants live in `main/ui_theme.h`.

## Asset review checklist

1. Matches the production background at native resolution.
2. Uses the theme palette and compatible pixel density.
3. Contains no text, UI frames, logos, or fake glyphs.
4. Preserves the cat and Japanese-inn identity where applicable.
5. Reports RGB565/alpha Flash cost before integration.
6. Requires no full-screen decode buffer or PSRAM.
7. Includes a deterministic simulator screenshot after integration.

## Initial budget

The v1 release-art ceiling is 1.5 MB of Flash. The full-screen station scene
occupies 153,600 bytes. Every generated asset must report its device size;
the report should be reviewed before adding it to `main/CMakeLists.txt`.

## First vertical slice

The first implemented slice covers the Station and Farm entry screens. Station
uses a full-screen generated RGB565 scene with translucent live status,
dialogue, notification, and navigation overlays. The scene continues behind
every UI region, avoiding visible image/content bands. Farm reuses the palette,
focus treatment, field texture, and live localized labels. Next assets should
cover crop growth sprites, plot-detail illustration, and the five tab icons.

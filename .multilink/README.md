# multi-link patches

Patches applied by `.github/workflows/multilink-release.yml` on top of each
official PebbleOS release (`git am -3`), built with
`CONFIG_BT_MULTI_LINK_EXPERIMENT=y` and published as `<tag>-multilink`.

They let a Pebble Time 2 / Round 2 stay connected to the phone and a
secondary iOS device (e.g. an iPad) at once, and show the secondary device's
notifications (ANCS), with duplicates removed.

Source branch: `multi-link-experiment`. To update the patches:

    git format-patch <base>..multi-link-experiment -o .multilink/patches

Notes, tests and history: `~/pebble-sync+2` on the owner's Mac.

# multi-link patches

Patches applied by `.github/workflows/multilink-release.yml` on top of each
official PebbleOS release (`git am -3`), built with
`CONFIG_BT_MULTI_LINK_EXPERIMENT=y` and published as `<tag>-multilink`.

They let a Pebble Time 2 / Round 2 stay connected to the phone and a
secondary iOS device (e.g. an iPad) at once, and show the secondary device's
notifications (ANCS), with duplicates removed. Group by Sender also
groups the iOS notifications (0009), and Popup by Sender shows a
sender's earlier messages on the popup card (0010).

Source branches: `multi-link-experiment` (0001-0008) and `ml-grouping`
(the series on top of the latest release, 0001-0010). To update the patches:

    git format-patch <tag>..ml-grouping -o .multilink/patches

Notes, tests and history: `~/pebble-sync+2` on the owner's Mac.

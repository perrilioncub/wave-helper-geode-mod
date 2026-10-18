# Wave Helper

A [Geode](https://geode-sdk.org) mod for Geometry Dash (2.2081) that turns a wave floor into its ceiling, with the slope hitboxes facing the right way.

Copying a wave floor and moving it up looks right, but every slope's hitbox is still on its bottom side, inside the corridor. This mod makes the copy for you, with each slope rotated 180° in place and each block moved to the other side of the line.

## Usage

1. In the editor, select the floor: blocks, slopes, and any decoration.
2. Press **Mirror** in the Edit tab, or <kbd>Alt</kbd>+<kbd>M</kbd>.
3. The new ceiling appears 3 blocks above the floor and stays selected, so you can move it to the gap you want.

Selecting a ceiling builds the matching floor instead. One <kbd>Ctrl</kbd>+<kbd>Z</kbd> removes the copy.

## How it works

The surface is the top outline of the selected solid objects. Slopes use GD's own slope direction (`GameObject::determineSlopeDirection`) and bounding box, the same data the game collides with, so off-grid and rotated objects work.

Each object keeps its X position. Its Y is mirrored across the surface in three bands per column:

| Floor band | Ceiling band |
| --- | --- |
| Above the surface line | Same distance below the line |
| Inside the slope's box | Stretched across the flipped slope's box |
| Below the slope's box | Same distance above the slope's box |

Measuring fill blocks from the slope's box rather than from the diagonal keeps blocks under 2x1 and half slopes on the grid. Slopes are then rotated 180° (flip X plus flip Y through the editor), which keeps the diagonal in place but moves the solid half to the other side.

The math lives in [`src/MirrorMath.hpp`](src/MirrorMath.hpp) and has no GD dependency.

## Building

```sh
geode build
```

Run the geometry tests on any machine:

```sh
cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
```

CI builds Windows, macOS, iOS and Android on every push. The combined `.geode` file is in the **Build Output** artifact of each run.

To publish a release, bump `version` in `mod.json` and `changelog.md`, then push a matching tag (for example `git tag v1.0.1 && git push origin v1.0.1`). CI builds it and attaches the `.geode` file to a GitHub Release.

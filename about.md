# Wave Helper

Building a wave corridor? Make the floor, then let the mod make the ceiling.

## The problem

If you copy a wave floor and move it up to use as the ceiling, it *looks* right, but every slope's hitbox is still on its bottom side, which is now inside the corridor. Fixing that means rotating every slope and moving every block by hand.

## What this does

1. Select your wave floor: blocks, slopes, and any decoration on it.
2. Press the **Mirror** button in the <cy>Edit</c> tab (or <cg>Alt+M</c>).
3. A copy appears above it with every slope rotated 180° in place and every block moved to the other side of the line, so the hitboxes face the right way.
4. The copy stays selected, so drag or nudge it to whatever gap you want.

It works the other way too: select a ceiling and it builds the floor.

## Supports

- 1x1, 2x1 and half slopes, in any orientation
- Off-grid and rotated objects
- Decoration and detail objects, which follow the surface they sit on
- Undo: one <cg>Ctrl+Z</c> removes the whole copy

Settings let you change the starting gap, force a direction, choose how decorations are turned, and change the shortcut.

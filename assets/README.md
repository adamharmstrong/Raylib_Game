# Assets

This folder keeps project-owned assets separate from vendor assets and generated pipeline output.

- `first_party/`: art, audio, animation, and source files created by this project or team.
- `third_party/`: externally licensed packs, organized by vendor and pack name.
- `generated/`: atlases, converted files, and temporary outputs that can be regenerated.

Runtime builds copy this folder next to the executable as `assets/`.

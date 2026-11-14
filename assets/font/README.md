# Scroll font (not included)

The waiting screen's scroll text uses the **Flynn** font by Neale Davidson (Pixel Sagas).
Its licence allows free personal use but not offering the font for download, so it is not in
this repo. Get it yourself:

- https://www.fontspace.com/flynn-font-f17948 (licence: freeware for personal use,
  commercial use requires a donation - see Pixel Sagas' EULA there)

Put `Flynn-4v54.ttf` into this folder (`assets/font/`); `SCROLL_FONT_PATH` in
`server/config.py` points here. Without the file the server prints
"Scroll font not found ... using default font" and uses pygame's default font - everything
else works the same.

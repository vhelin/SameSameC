# Windows MinGW make runs "simple" recipes with CreateProcess, so a leading `!`
# is treated as a program named `!` (see allocator_ra_frame_spill_slot_a).
# .ONESHELL plus the bash passed as SHELL keeps `! grep` and quoted patterns
# working. Do not set SHELL := bash here: CreateProcess would prefer
# C:\Windows\System32\bash.exe (WSL) over Git Bash.
.SHELLFLAGS := -ec
.ONESHELL:

# Changelog

All notable changes to CRYPTARA. `bump_version.sh` updates CMakeLists.txt
and this file together.

## 2.0.2 - 2026-10-10

- Settings and macros dialogs: no more overlapping fields with Qt 6.5+ (Material 3). The example placeholders became labels floating over the field above; they now show in empty fields only (CField.qml), rows are spaced for the outlined fields, and the automatic message keeps the style's own frame.

## 2.0.1 - 2026-10-10

- build_all.bat: project files dated in the future (an archive unpacked with another clock or time zone) are reset before configuring; they made Ninja re-run CMake endlessly.
- build_all.bat: right after configuring, Ninja is asked whether the configuration is up to date; if not, the file it blames is shown and the build stops instead of looping.

## 2.0.0 - 2026-10-10

- C++ / Qt Quick version for Windows, replacing the Python one (1.3.x),
  compatible with it on the air (protocol, encryption, key exchange) and on
  disk (`%APPDATA%\CRYPTARA\cryptara_settings.conf`).
- Cryptography by Windows CNG: no external library.
- Four themes (Light, Dark, Red for the night, Amber); language chosen in
  the settings or from the system.
- `build_all.bat` (Visual Studio, Ninja, windeployqt, Inno Setup) and a
  bilingual installer that replaces an installed Python version.
- Received file names can no longer point outside the chosen folder; BBS
  downloads are limited to the shared folder itself.
- Every message of a received block is handled, not only the first.
- The Diffie-Hellman session key ends with the session.
- F1-F12 macros work; the settings dialog no longer erases them.
- The auto-responder answers incoming connections only.
- The timestamp and "save the log on closing" options take effect.
- Audible notification chime; nothing is sent if encryption fails; lost
  modem link and interrupted transfers are handled.

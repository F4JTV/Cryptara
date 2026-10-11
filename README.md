# CRYPTARA

**C**ryptographic **R**adio **Y**ield **P**rotection **T**ransmission
**A**utomated **R**eliable **A**ssistant: a keyboard chat client for the
VARA HF, VARA FM and VARA SAT modems, with AES-256 encryption, a
Diffie-Hellman key exchange (Perfect Forward Secrecy), file transfer and a
small file server (BBS) for the stations that connect to you.

This is the C++ / Qt Quick version of the application, for Windows. It
replaces the Python version (1.x) and stays compatible with it:

- **on the air**: the same messages on VARA's data port, the same
  encryption format and key exchange. A station running this version talks
  to a station running the Python one, files and encryption included;
- **on disk**: the same settings file, `%APPDATA%\CRYPTARA\cryptara_settings.conf`,
  read and written in the format of Python's `configparser`. Settings made
  with the Python version are taken over as they are.

The cryptography is Windows' own (CNG, `bcrypt.dll`): no OpenSSL or other
library to install or ship.

> Before using encryption on the air, check the conditions of your licence:
> amateur radio regulations in most countries forbid messages encoded to
> obscure their meaning. Use it only where the service you operate under
> allows it.

## Features

- VARA HF (500 / 2300 Hz), VARA FM (up to two digipeaters), VARA SAT
- Launches the VARA program at start-up and closes it on exit (optional)
- CQ calls, list of received CQs (double-click to call)
- Chat with timestamps; your own lines appear once VARA reports them
  delivered (`BUFFER 0`)
- Encryption: shared password (PBKDF2-HMAC-SHA256, 100 000 iterations,
  AES-256-CBC, HMAC-SHA256), then optionally a Diffie-Hellman session
  (RFC 3526 group 14, HKDF-SHA256) negotiated with the other station;
  switching encryption off and on is negotiated too
- File transfer up to 2 MB, zlib-compressed, encrypted when encryption is on
- BBS: share a folder; the stations connected to you list and download it
- Auto-responder for incoming connections, with variables and automatic
  disconnection
- F1-F12 macros with variables (`{MYCALL}`, `{NAME}`, `{FIRSTNAME}`,
  `{QTH}`, `{GRID}`, `{RIG}`, `{ANTENNA}`, `{CALLSIGN}`, `{TIME}`, `{DATE}`)
- QSO log for the session, chat and log saving
- English and French, from the system language or chosen in the settings
- Four themes: Light, Dark, Red for night operation (every colour a shade
  of red, to keep your dark adaptation) and Amber for dim light
- Tooltips on every control, switched on and off from the Tools menu

## Building on Windows

Prerequisites, installed once:

| What | Notes |
|---|---|
| Visual Studio 2022 (or 2026) | workload "Desktop development with C++" (it brings CMake and Ninja) |
| Qt 6 for MSVC 64-bit | e.g. Qt 6.11.2 `msvc2022_64`; Qt Quick and Qt Quick Controls are part of every Qt installation |
| Inno Setup 6.3 or later, or 7 | for the installer |

Then, in an **x64 Native Tools Command Prompt for VS 2022**, at the project
root:

```bat
build_all.bat
```

`QT_DIR` at the top of `build_all.bat` points to the Qt installation
(`C:\Qt\6.11.2\msvc2022_64`); change it if yours is elsewhere. The script
configures with Ninja, compiles, gathers the program and the Qt runtime in
`installer\dist` (`windeployqt --qmldir src\qml`), bundles the Visual C++
redistributable and builds `installer\output\CRYPTARA-<version>-setup.exe`.

| Option | Effect |
|---|---|
| `/clean` | wipe the build directory first |
| `/nobuild` | skip configure and compile; deploy and package only |
| `/noinstaller` | stop after staging `installer\dist` (run `CRYPTARA.exe` from there) |
| `/test` | run the unit tests after compiling |

By hand, the same build is:

```bat
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\msvc2022_64
cmake --build build
```

Ninja re-runs CMake before compiling whenever an input of the
configuration (`CMakeLists.txt`, `src/resources.qrc`, `src/version.h.in`)
is newer than what it generated, which a file dated in the future makes
true forever. `build_all.bat` therefore resets project files dated in the
future before configuring, then asks Ninja whether the configuration is up
to date; if it is not, Ninja's explanation (the file it blames) is printed
and the build stops instead of looping.

`build_all.bat`, `installer\CRYPTARA.iss` and `assets\cryptara.rc` must
keep CRLF line endings: cmd.exe misparses a batch file with bare line
feeds. `.gitattributes` pins them, `scripts/check_line_endings.sh` (`--fix`
to repair) checks them and the byte-order mark of the `.iss`, and
`scripts/check_batch.py` checks the batch file against the two cmd.exe
rules that leave only "X was unexpected at this time" behind: a `)` in the
text of an `echo` inside a parenthesised block closes the block, and `/?`
as the first word after `rem` or `echo` asks for the command's help.

## Installing, and upgrading from the Python version

The installer needs administrator rights and installs into
`C:\Program Files\CRYPTARA`, like the Python version's. It carries the same
application identifier, so it **replaces** an installed Python version
instead of sitting beside it; the folders the PyInstaller build left are
removed. The settings in `%APPDATA%\CRYPTARA` are kept, and the uninstaller
leaves them too.

The executable carries its name, publisher and version (Windows version
information), which SmartScreen and antivirus tools look at. Without a
paid code-signing certificate, SmartScreen still says "unknown publisher"
the first time: "More info", then "Run anyway".

## Settings

`%APPDATA%\CRYPTARA\cryptara_settings.conf`, section `[CRYPTARA]`, the keys
of the Python version plus `theme` (`light`, `dark`, `red`, `amber`) and
`language` (`auto`, `en`, `fr`), which the Python version ignores. "Save
the log" writes `cryptara_log_<date>.txt` files into the same folder.

## Tests

`build_all.bat /test` (or `ctest --test-dir build`) runs:

- `test_crypto`: the cryptography against vectors produced by the Python
  version's own code (`tests/crypto_vectors.h`): key derivations, a
  password-encrypted message, a Diffie-Hellman exchange with known keys and
  a session-encrypted message, the public key encoding byte for byte, and
  the older PKCS#3 key form;
- `test_core`: the settings file (as Python's configparser writes it), the
  translations (every key used exists, same placeholders in both
  languages), and two complete stations talking through simulated VARA
  modems (`tests/FakeVara.h`): CQ, call, chat, encryption with
  Diffie-Hellman, files, BBS, switching encryption off and on, macros, the
  auto-responder.

`tests/interop/` checks this version against the Python one. It needs the
original `cryptara.py`, Python with PyQt6 and cryptography, and is meant
for a development machine:

| Script | What it does |
|---|---|
| `make_vectors.py` | regenerates `tests/crypto_vectors.h` from the Python code |
| `check_emitted.py` | the Python code decrypts what `test_crypto --emit` encrypted |
| `check_old_cryptography.py` | the key exchange against cryptography 3.x (PKCS#3 keys) |
| `run_interop.py` | the real Python application against `cryptara_headless` through simulated modems: the whole protocol, both ways |
| `qml_check.py` | loads the QML with the Qt of PyQt6 and renders every theme, failing on any QML warning |

## The protocol on the data port

Every message ends with `<EOF>`.

| Message | Meaning |
|---|---|
| text | a chat message |
| `<ENC>base64</ENC>` | an encrypted chat message |
| `<CRYPTREQ>key</CRYPTREQ>` | request to encrypt, with our Diffie-Hellman public key |
| `<CRYPTACK>key</CRYPTACK>` | accepted, with the other station's public key |
| `<CRYPTREFUSED></CRYPTREFUSED>` | refused |
| `<CRYPTOFF></CRYPTOFF>` | encryption switched off |
| `<CRYPTOFFACK></CRYPTOFFACK>` / `<CRYPTOFFREFUSED></CRYPTOFFREFUSED>` | the other side followed / kept it on |
| `<CRYPTON></CRYPTON>` / `<CRYPTONACK></CRYPTONACK>` | switched back on / the other side followed |
| `<DHKEY>key</DHKEY>` | older key exchange, still answered |
| `<FILEINFO>enc\|name\|size\|zsize\|transfer` | a file is coming (name in base64) |
| `<FILEDATA>enc\|name\|size\|zsize\|data` | the file: zlib, base64, encrypted when `enc` is 1 |
| `<FILE>name\|size\|zsize\|data` | older file format, still read |
| `<BBSON>` | this station shares files |
| `<BBSLIST>` / `<BBSFILES>json</BBSFILES>` | list request / list (`[{"name", "size"}]`) |
| `<BBSGET>name</BBSGET>` | download request |

Encrypted payload: `flag | [salt] | iv | ciphertext | hmac`, base64. Flag
`0x00`: keys derived from the password with PBKDF2-HMAC-SHA256 (100 000
iterations, salt = the 16 random bytes sent + `encryption` or `hmac`).
Flag `0x01`: the Diffie-Hellman session; the shared secret goes through
HKDF-SHA256 (info `vara-chat-session-key`) for the AES key, which goes
through HKDF-SHA256 again (info `hmac`) for the HMAC key. AES-256-CBC with
PKCS#7 padding; HMAC-SHA256 over `iv | ciphertext`. Public keys travel as
the base64 of a PEM SubjectPublicKeyInfo, X9.42 form (the PKCS#3 form of
older Python stations is read too).

## Changes from the Python version

Same protocol, same settings; these behave differently, deliberately:

- A received file name is reduced to a plain name before saving: a name
  carrying folders (`..\..\x`, `C:\...`) wrote outside the chosen folder.
- A BBS download is served only for a file directly in the shared folder;
  the previous check let a neighbouring folder with the same prefix through.
- Every message of a received block is handled; previously only the first
  one, the next waiting for more data to arrive.
- The Diffie-Hellman session key is forgotten when the session ends;
  previously it was kept and reused with the next station, whose messages
  then could not be decrypted.
- F1-F12 send their macros (they stopped the Python program).
- Confirming the settings dialog keeps the macros and the tooltips choice
  (they were erased).
- The auto-responder answers incoming connections only, as its description
  says; it also answered, and hung up, the calls you made.
- "Message timestamps" and "Save the log on closing" do what they say
  (both were ignored).
- The notification sound is a short chime played by Windows
  (`QApplication.beep()` is silent on current Windows).
- If encryption fails, nothing is sent (the message went in clear).
- A lost link to VARA is shown as such; a transfer interrupted by the end
  of the session is abandoned instead of waiting forever; one transfer at a
  time.
- Settings containing `%` are saved (configparser refused them).
- The window stays responsive while a file is sent; text is decoded per
  complete message, so accented characters are never cut across two reads.
- After launching VARA, the connection is retried for a few seconds if VARA
  is slow to open its ports. VARA is started in its own folder, with the
  same window flags as before.

## Project layout

```
CMakeLists.txt         the version (project(CRYPTARA VERSION ...)) and the targets
build_all.bat          Windows: build, deploy, installer, in one go
bump_version.sh        new version: CMakeLists.txt and CHANGELOG.md (Git Bash)
installer/CRYPTARA.iss bilingual Inno Setup script
assets/                icon (.ico, .png) and Windows resource (version information)
src/main.cpp           start-up, QML engine
src/app/               Controller (the application logic), Settings, I18n, models, chime
src/vara/              VARA TCP client and process launcher
src/crypto/            CryptoManager (message format), DH public key encoding, CNG backend
src/qml/               the interface: Main.qml, dialogs, themed controls
tests/                 unit tests, simulated modems, headless driver, interop scripts
scripts/               line-ending and batch-file checks
```

`src/crypto/CryptoBackendOpenSsl.cpp` is the same backend over OpenSSL,
used only to build and run the tests on a Linux development machine; the
Windows build does not use it.

## Licence

GPL-3.0, see `LICENSE`. CRYPTARA Community.

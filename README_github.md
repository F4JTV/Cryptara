# 📻 CRYPTARA

**Secure VARA chat client with end-to-end AES-256 encryption.**

CRYPTARA is a keyboard-to-keyboard chat application for amateur radio operators
using the [VARA](https://rosmodem.wordpress.com/) modem (HF / FM / SAT). It adds
an optional **end-to-end encryption** layer, with Perfect Forward Secrecy, so two
stations can hold a private conversation over the air.

The interface is fully **bilingual (English / French)** and auto-detects the
system language.

---

## ✨ Features

### 🔐 Security
- **AES-256-CBC** with **HMAC-SHA256** authentication
- **Perfect Forward Secrecy** via Diffie-Hellman (RFC 3526 Group 14, 2048-bit)
- **Key derivation:** PBKDF2-HMAC-SHA256 (100,000 iterations) + HKDF-SHA256
- Fresh session keys on every connection
- End-to-end encryption for both **messages and files**
- Per-session toggle (encryption can be enabled/disabled live)

### 📡 VARA support
- Compatible with **VARA HF**, **VARA FM** and **VARA SAT**
- Optional automatic launch of the VARA modem
- **CHAT ON** mode: chat-optimised timing, high-latency support, per-frame S/N reporting
- Automatic buffer management

### 💬 Operating features
- **CQ handling** — send a CQ, view incoming CQ frames in a live list, connect with a double-click
- **Auto-responder** — automatic reply on incoming connections, with a reliable
  auto-disconnect that waits for the message to be fully sent and acknowledged
  before closing the link
- **F1–F12 macros** — twelve configurable messages with variable expansion
  (`{MYCALL}`, `{NAME}`, `{FIRSTNAME}`, `{QTH}`, `{GRID}`, `{RIG}`, `{ANTENNA}`,
  `{CALLSIGN}`, `{TIME}`, `{DATE}`)
- **Station info** — callsign, name, QTH, grid square, rig and antenna, reused across macros
- **Built-in BBS server** — optionally share a folder on incoming connections
- **Contextual tooltips** — help on every control, toggleable from the Tools menu

### 📁 Secure file transfer
- **Automatic compression** (zlib level 9)
- **Transparent encryption** of transferred files
- Real-time progress bar

### 🌍 Bilingual interface
- Automatic detection of the system language
- **English** and **French**, full UI coverage

---

## 📦 Requirements

- Python **3.12 or 3.13** recommended
  *(Python 3.14 currently causes DLL bundling issues when freezing with PyInstaller.)*
- A working **VARA** installation (VARA HF, VARA FM or VARA SAT)

```bash
pip install pyqt6 cryptography
```

---

## 🚀 Running from source

```bash
python cryptara.py
```

Place `icon.ico` (and/or `icon.png`) next to the script for the window icon.

Settings and logs are stored in a user-writable location:

- **Windows:** `%APPDATA%\CRYPTARA\`
- **Linux / macOS:** `~/.config/CRYPTARA/`

---

## ⚙️ Configuration

Open **File → Settings**. The dialog is organised in tabs:

| Tab | Purpose |
| --- | --- |
| **Modem** | VARA type, executable path, host, TCP ports, listen mode, compression |
| **Station Info** | Callsign (required), name, QTH, grid, rig, antenna |
| **Auto-responder** | Enable, message, delay, auto-disconnect |
| **Interface** | Timestamps, sound, log saving, received-files folder |
| **BBS** | Enable the BBS server and choose the shared folder |

Only the **callsign** is required to connect to the modem.

---

## 🛠️ Building a Windows executable

CRYPTARA can be frozen with [PyInstaller](https://pyinstaller.org/) (one-dir mode)
and packaged with [Inno Setup](https://jrsoftware.org/).

> PyInstaller does **not** cross-compile: build the Windows executable on Windows.

```powershell
# 1. Environment
python -m venv venv
venv\Scripts\activate
pip install pyqt6 cryptography pyinstaller

# 2. Freeze the app (one-dir)
pyinstaller cryptara.spec
#    -> dist\CRYPTARA\CRYPTARA.exe

# 3. Build the installer
& "C:\Program Files\Inno Setup 7\ISCC.exe" cryptara.iss
#    -> Output\CRYPTARA_Setup.exe
```

UPX is disabled on purpose in the `.spec`: compressing the Qt6 DLLs triggers
antivirus false positives and can corrupt them.

---

## 🔒 Security & legal notice

- Encryption is **end-to-end** between the two CRYPTARA stations; the VARA modem
  and the RF link only carry ciphertext when encryption is active.
- Each session negotiates fresh keys (Perfect Forward Secrecy): compromising one
  session does not expose past or future ones.
- **Encryption of amateur radio transmissions is restricted or prohibited in many
  countries.** It is your responsibility to verify that your intended use complies
  with your local regulations. Where encryption is not permitted, CRYPTARA can be
  used unencrypted as a plain VARA chat client.

---

## 📜 License

GPL-3.0. See the [LICENSE](LICENSE) file.

---

## 🤝 Credits

Developed and maintained by the **SDR++ Community**.

Built on the VARA protocol by EA5HVK. Not affiliated with or endorsed by the VARA
authors.

73! 📻🔒

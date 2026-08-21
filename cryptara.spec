# -*- mode: python ; coding: utf-8 -*-
#
# Fichier de build PyInstaller pour CRYPTARA (mode ONEDIR)
# Cible : PyInstaller 6.x
#
# Utilisation :
#   pyinstaller cryptara.spec
#
# Prérequis dans le dossier courant :
#   - cryptara.py
#   - icon.ico
#
# Résultat :
#   dist/CRYPTARA/            <- dossier à empaqueter avec Inno Setup
#     CRYPTARA.exe
#     _internal/             (DLL, Qt, cryptography, icon.ico, ...)

a = Analysis(
    ['cryptara.py'],
    pathex=[],
    binaries=[],
    # L'icône est copiée dans _internal ; resource_path() la retrouve via sys._MEIPASS
    datas=[('icon.ico', '.')],
    # cryptography charge un backend natif ; on l'explicite par sécurité
    hiddenimports=['cryptography'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,          # ONEDIR : les binaires vont dans COLLECT
    name='CRYPTARA',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    # UPX désactivé volontairement : compresser les DLL Qt6 provoque
    # des faux positifs antivirus et parfois des DLL corrompues.
    upx=False,
    console=False,                  # GUI : pas de fenêtre console
    disable_windowed_traceback=False,
    icon='icon.ico',
    # Métadonnées de version (VS_VERSION_INFO) intégrées à l'exe :
    # améliore l'identification par SmartScreen / Smart App Control / antivirus
    version='version_info.txt',
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=False,
    upx_exclude=[],
    name='CRYPTARA',
)

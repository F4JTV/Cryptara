================================================================================
 CRYPTARA - Package de compilation Windows
================================================================================

Contenu de l'archive :
  cryptara.py       Application (client VARA chiffre)
  cryptara.spec     Configuration PyInstaller (mode ONEDIR)
  cryptara.iss      Script Inno Setup (installateur)
  version_info.txt  Metadonnees de version integrees a l'exe
  icon.ico          Icone multi-resolutions (16 -> 256 px)
  README_BUILD.txt  Ce fichier

Note : version_info.txt integre le nom, l'editeur et le numero de version
dans CRYPTARA.exe. Cela ameliore l'identification par SmartScreen, Smart App
Control et les antivirus (un exe "nu" est plus suspect). Pense a mettre a jour
le numero de version dans ce fichier en meme temps que celui de l'application.

--------------------------------------------------------------------------------
 IMPORTANT : la compilation doit se faire SOUS WINDOWS
--------------------------------------------------------------------------------
PyInstaller ne fait pas de cross-compilation. Depuis Linux on ne peut pas
produire un .exe Windows. Utilise une machine ou une VM Windows.

================================================================================
 ETAPE 1 - Preparer l'environnement (une seule fois)
================================================================================
Installer Python depuis python.org (cocher "Add Python to PATH").
Puis dans PowerShell ou CMD, dans le dossier du package :

    python -m venv venv
    venv\Scripts\activate
    python -m pip install --upgrade pip
    pip install pyqt6 cryptography pyinstaller

================================================================================
 ETAPE 2 - Compiler l'application avec PyInstaller
================================================================================
Toujours dans le dossier du package (avec venv active) :

    pyinstaller cryptara.spec

Resultat :
    dist\CRYPTARA\CRYPTARA.exe   (+ dossier _internal\)

>>> TESTER l'exe AVANT de continuer : lance dist\CRYPTARA\CRYPTARA.exe
    Si un module manque ou l'app crashe au demarrage, edite cryptara.spec
    et mets temporairement  console=False  ->  console=True  pour voir les
    messages d'erreur, recompile, corrige, puis remets console=False.

Note : la configuration et les logs sont ecrits dans
    %APPDATA%\CRYPTARA\
donc l'installation dans Program Files ne pose aucun probleme d'ecriture.

================================================================================
 ETAPE 3 - Creer l'installateur avec Inno Setup
================================================================================
Installer Inno Setup 6 : https://jrsoftware.org/isdl.php

Option A - Interface graphique :
    Ouvrir cryptara.iss dans Inno Setup, cliquer sur "Build".

Option B - Ligne de commande (PowerShell) :
    Inno Setup 7 :
       & "C:\Program Files\Inno Setup 7\ISCC.exe" cryptara.iss
    Inno Setup 6 :
       & "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" cryptara.iss

    Note : dans PowerShell, le & (operateur d'appel) est obligatoire
    quand le chemin est entre guillemets, sinon la commande est traitee
    comme du texte au lieu d'etre executee.

Resultat :
    Output\CRYPTARA_Setup_1.3.6.exe   <- fichier a distribuer

--------------------------------------------------------------------------------
 Remarques
--------------------------------------------------------------------------------
- SmartScreen affichera "editeur inconnu" au 1er lancement chez les
  utilisateurs (normal sans certificat de signature de code payant).
  Cliquer "Informations complementaires" -> "Executer quand meme".

- UPX est volontairement DESACTIVE dans le .spec : compresser les DLL Qt6
  provoque des faux positifs antivirus et parfois des DLL corrompues.

- Pour changer la version affichee dans l'installateur, edite la ligne
  #define MyAppVersion "1.3.6"  dans cryptara.iss

================================================================================
 73 - SDR++ Community
================================================================================

#!/usr/bin/env python3
"""
CRYPTARA - Client VARA sécurisé avec chiffrement AES-256

Acronyme : Cryptographic Radio Yield Protection Transmission Automated Reliable Assistant

Support pour VARA HF, VARA FM et VARA SAT
Avec chiffrement AES-256, Perfect Forward Secrecy et transfert de fichiers sécurisés
"""

import sys
import socket
import threading
import time
import json
import base64
import zlib
import os
import hashlib
import secrets
import subprocess
import configparser
import locale
from datetime import datetime
from pathlib import Path

# Cryptographie
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding, hashes, serialization
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.asymmetric import dh
from cryptography.hazmat.primitives.asymmetric.dh import DHParameterNumbers
from cryptography.hazmat.backends import default_backend

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTextEdit, QLineEdit, QPushButton, QLabel, QGroupBox,
    QTabWidget, QListWidget, QComboBox, QSpinBox, QCheckBox,
    QDialog, QDialogButtonBox, QFormLayout, QMessageBox,
    QStatusBar, QSplitter, QTableWidget, QTableWidgetItem,
    QHeaderView, QMenu, QFileDialog, QProgressDialog, QProgressBar,
    QSizePolicy, QScrollArea
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QObject
from PyQt6.QtGui import QTextCursor, QColor, QFont, QAction, QTextOption


# ============================================================================
# Résolution des chemins (compatible PyInstaller + installation)
# ============================================================================

def resource_path(relative):
    """Résout le chemin d'une ressource embarquée (icône, etc.).

    Compatible avec :
    - Exécution normale du script (.py)
    - Bundle PyInstaller --onefile (ressources extraites dans sys._MEIPASS)
    """
    base = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, relative)


def get_config_dir():
    """Retourne un dossier de configuration inscriptible pour l'utilisateur.

    Nécessaire car une installation dans Program Files (Windows) est protégée
    en écriture. La config et les logs vont donc dans :
    - Windows : %APPDATA%\\CRYPTARA
    - Linux/macOS : ~/.config/CRYPTARA
    """
    if sys.platform == 'win32':
        base = os.environ.get('APPDATA', os.path.expanduser('~'))
    else:
        base = os.path.expanduser('~/.config')
    path = os.path.join(base, 'CRYPTARA')
    os.makedirs(path, exist_ok=True)
    return path


# ============================================================================
# Système de traduction multilingue
# ============================================================================

class Translator:
    """Gestionnaire de traduction FR/EN avec détection automatique de la langue"""
    
    def __init__(self):
        self.current_lang = self.detect_system_language()
        
    def detect_system_language(self):
        """Détecte la langue du système d'exploitation"""
        try:
            # Méthode recommandée (Python 3.11+)
            # Essayer d'abord avec locale.getlocale()
            try:
                system_locale = locale.getlocale()[0]
            except:
                # Fallback pour compatibilité
                import os
                system_locale = os.environ.get('LANG') or os.environ.get('LANGUAGE')
            
            if system_locale:
                # Extraire le code langue (les 2 premiers caractères)
                lang_code = str(system_locale)[:2].lower()
                
                # Si français, retourner 'fr', sinon 'en'
                return 'fr' if lang_code == 'fr' else 'en'
            else:
                return 'en'  # Par défaut anglais
        except:
            return 'en'  # Par défaut anglais en cas d'erreur
    
    def tr(self, key):
        """Traduit une clé selon la langue actuelle"""
        return TRANSLATIONS.get(key, {}).get(self.current_lang, key)


# Dictionnaire de traductions
TRANSLATIONS = {
    # Titre et général
    "app_name": {"fr": "CRYPTARA", "en": "CRYPTARA"},
    "app_description": {"fr": "Client VARA sécurisé avec chiffrement AES-256", 
                       "en": "Secure VARA client with AES-256 encryption"},
    
    # Menus
    "menu_file": {"fr": "Fichier", "en": "File"},
    "menu_settings": {"fr": "Paramètres", "en": "Settings"},
    "menu_qso_log": {"fr": "Journal QSO", "en": "QSO Log"},
    "menu_about": {"fr": "À propos", "en": "About"},
    "menu_quit": {"fr": "Quitter", "en": "Quit"},
    "menu_tools": {"fr": "Outils", "en": "Tools"},
    "menu_help": {"fr": "Aide", "en": "Help"},
    
    # Onglets
    "tab_modem": {"fr": "Modem", "en": "Modem"},
    "tab_auto_responder": {"fr": "Auto-répondeur", "en": "Auto-responder"},
    "tab_interface": {"fr": "Interface", "en": "Interface"},
    
    # Groupes
    "group_connection": {"fr": "Connexion", "en": "Connection"},
    "group_station": {"fr": "Station", "en": "Station"},
    "group_chat": {"fr": "Chat", "en": "Chat"},
    "group_system_log": {"fr": "Journal système", "en": "System Log"},
    "group_file_transfer": {"fr": "Transfert de fichier", "en": "File Transfer"},
    
    # Boutons
    "btn_connect": {"fr": "Connecter au modem", "en": "Connect to Modem"},
    "btn_disconnect": {"fr": "Déconnecter", "en": "Disconnect"},
    "btn_call": {"fr": "Appeler", "en": "Call"},
    "btn_disconnect_station": {"fr": "Déconnecter", "en": "Disconnect"},
    "btn_send": {"fr": "Envoyer", "en": "Send"},
    "btn_file": {"fr": "Fichier", "en": "File"},
    "btn_encryption": {"fr": "Chiffrement", "en": "Encryption"},
    "btn_dh": {"fr": "Diffie-Hellman", "en": "Diffie-Hellman"},
    "btn_settings": {"fr": "Paramètres", "en": "Settings"},
    "btn_cancel": {"fr": "Annuler", "en": "Cancel"},
    "btn_save": {"fr": "Sauvegarder", "en": "Save"},
    "btn_close": {"fr": "Fermer", "en": "Close"},
    "btn_ok": {"fr": "OK", "en": "OK"},
    "btn_yes": {"fr": "OUI", "en": "YES"},
    "btn_no": {"fr": "NON", "en": "NO"},
    "btn_browse": {"fr": "Parcourir", "en": "Browse"},
    "btn_clear_cq": {"fr": "Effacer", "en": "Clear"},
    "btn_save_chat": {"fr": "Sauvegarder le chat", "en": "Save chat"},
    
    # Labels
    "lbl_modem_type": {"fr": "Type de modem :", "en": "Modem type:"},
    "lbl_host": {"fr": "Hôte :", "en": "Host:"},
    "lbl_cmd_port": {"fr": "Port commande :", "en": "Command port:"},
    "lbl_data_port": {"fr": "Port données :", "en": "Data port:"},
    "lbl_mycall": {"fr": "Mon indicatif :", "en": "My callsign:"},
    "lbl_listen": {"fr": "Mode écoute", "en": "Listen mode"},
    "lbl_compression": {"fr": "Compression", "en": "Compression"},
    "lbl_station": {"fr": "Indicatif station :", "en": "Station callsign:"},
    "lbl_message": {"fr": "Message :", "en": "Message:"},
    "lbl_auto_message": {"fr": "Message auto-répondeur :", "en": "Auto-responder message:"},
    "lbl_auto_delay": {"fr": "Délai avant déconnexion (s) :", "en": "Delay before disconnect (s):"},
    "lbl_enable_auto": {"fr": "Activer l'auto-répondeur", "en": "Enable auto-responder"},
    "lbl_auto_disconnect": {"fr": "Déconnecter automatiquement", "en": "Disconnect automatically"},
    "lbl_timestamp": {"fr": "Horodatage des messages", "en": "Message timestamps"},
    "lbl_sound": {"fr": "Notifications sonores", "en": "Sound notifications"},
    "lbl_save_log": {"fr": "Sauvegarder le journal", "en": "Save log"},
    "lbl_vara_hf": {"fr": "VARA HF :", "en": "VARA HF:"},
    "lbl_vara_fm": {"fr": "VARA FM :", "en": "VARA FM:"},
    "lbl_vara_sat": {"fr": "VARA SAT :", "en": "VARA SAT:"},
    "lbl_auto_start": {"fr": "Lancer VARA automatiquement", "en": "Start VARA automatically"},
    
    # Messages de log
    "log_modem_connected": {"fr": "Connecté au modem VARA", "en": "Connected to VARA modem"},
    "log_modem_disconnected": {"fr": "Déconnecté du modem", "en": "Disconnected from modem"},
    "log_calling": {"fr": "Appel de", "en": "Calling"},
    "log_connected_to": {"fr": "Connecté à", "en": "Connected to"},
    "log_disconnected_from": {"fr": "Déconnecté de", "en": "Disconnected from"},
    "log_message_sent": {"fr": "Message envoyé", "en": "Message sent"},
    "log_encryption_enabled": {"fr": "Chiffrement activé", "en": "Encryption enabled"},
    "log_encryption_disabled": {"fr": "Chiffrement désactivé", "en": "Encryption disabled"},
    "log_file_sent": {"fr": "Fichier envoyé", "en": "File sent"},
    "log_file_received": {"fr": "Fichier reçu", "en": "File received"},
    
    # Messages d'erreur
    "error_connection": {"fr": "Erreur de connexion", "en": "Connection error"},
    "error_modem": {"fr": "Erreur modem", "en": "Modem error"},
    "error_callsign": {"fr": "Indicatif invalide", "en": "Invalid callsign"},
    "error_file": {"fr": "Erreur fichier", "en": "File error"},
    "error_file_too_large": {"fr": "Fichier trop volumineux (max 2 MB)", 
                             "en": "File too large (max 2 MB)"},
    
    # Dialogues
    "dlg_settings": {"fr": "Paramètres", "en": "Settings"},
    "dlg_qso_log": {"fr": "Journal QSO", "en": "QSO Log"},
    "dlg_about": {"fr": "À propos de CRYPTARA", "en": "About CRYPTARA"},
    "dlg_password": {"fr": "Mot de passe de chiffrement", "en": "Encryption password"},
    "dlg_select_file": {"fr": "Sélectionner un fichier", "en": "Select a file"},
    "dlg_save_file": {"fr": "Sauvegarder le fichier", "en": "Save file"},
    "dlg_confirm": {"fr": "Confirmation", "en": "Confirmation"},
    "dlg_warning": {"fr": "Avertissement", "en": "Warning"},
    "dlg_error": {"fr": "Erreur", "en": "Error"},
    "dlg_info": {"fr": "Information", "en": "Information"},
    
    # Messages de confirmation
    "msg_quit": {"fr": "Voulez-vous vraiment quitter ?", 
                "en": "Do you really want to quit?"},
    "msg_disconnect": {"fr": "Voulez-vous vous déconnecter ?", 
                      "en": "Do you want to disconnect?"},
    
    # Statut
    "status_ready": {"fr": "Prêt", "en": "Ready"},
    "status_connecting": {"fr": "Connexion en cours...", "en": "Connecting..."},
    "status_connected": {"fr": "Connecté", "en": "Connected"},
    "status_disconnected": {"fr": "Déconnecté", "en": "Disconnected"},
    "status_sending": {"fr": "Envoi en cours...", "en": "Sending..."},
    "status_receiving": {"fr": "Réception en cours...", "en": "Receiving..."},
    
    # Transfert de fichiers
    "file_sending": {"fr": "Envoi de", "en": "Sending"},
    "file_receiving": {"fr": "Réception de", "en": "Receiving"},
    "file_sent": {"fr": "Fichier envoyé :", "en": "File sent:"},
    "file_received": {"fr": "Fichier reçu :", "en": "File received:"},
    "file_encrypted": {"fr": "(chiffré)", "en": "(encrypted)"},
    "file_cancel": {"fr": "Annuler le transfert", "en": "Cancel transfer"},
    "file_cancel_confirm": {"fr": "Voulez-vous vraiment annuler ?", 
                           "en": "Do you really want to cancel?"},
    
    # Chiffrement
    "crypto_setup": {"fr": "Configuration du chiffrement", "en": "Encryption setup"},
    "crypto_password": {"fr": "Entrez le mot de passe de chiffrement :", 
                       "en": "Enter encryption password:"},
    "crypto_share": {"fr": "(Partagez ce mot de passe avec votre interlocuteur de manière sécurisée)", 
                    "en": "(Share this password securely with your contact)"},
    "crypto_enabled": {"fr": "Chiffrement AES-256 activé (PBKDF2 + HMAC)", 
                      "en": "AES-256 encryption enabled (PBKDF2 + HMAC)"},
    "crypto_disabled": {"fr": "Chiffrement désactivé", "en": "Encryption disabled"},
    "crypto_request": {"fr": "souhaite chiffrer, acceptez ?", "en": "wants to encrypt, accept?"},
    "crypto_dh_success": {"fr": "Session Diffie-Hellman établie - Perfect Forward Secrecy activé !", 
                         "en": "Diffie-Hellman session established - Perfect Forward Secrecy enabled!"},
    
    # Auto-répondeur
    "auto_enabled": {"fr": "Auto-répondeur activé", "en": "Auto-responder enabled"},
    "auto_disabled": {"fr": "Auto-répondeur désactivé", "en": "Auto-responder disabled"},
    "auto_responded": {"fr": "Auto-réponse envoyée à", "en": "Auto-response sent to"},
    
    # QSO Log
    "qso_date": {"fr": "Date", "en": "Date"},
    "qso_time": {"fr": "Heure", "en": "Time"},
    "qso_callsign": {"fr": "Indicatif", "en": "Callsign"},
    "qso_freq": {"fr": "Fréquence", "en": "Frequency"},
    "qso_mode": {"fr": "Mode", "en": "Mode"},
    "qso_report": {"fr": "Rapport", "en": "Report"},
    
    # À propos
    "about_title": {"fr": "CRYPTARA", "en": "CRYPTARA"},
    "about_acronym": {"fr": "<b>C</b>ryptographic <b>R</b>adio <b>Y</b>ield <b>P</b>rotection<br>"
                           "<b>T</b>ransmission <b>A</b>utomated <b>R</b>eliable <b>A</b>ssistant",
                     "en": "<b>C</b>ryptographic <b>R</b>adio <b>Y</b>ield <b>P</b>rotection<br>"
                           "<b>T</b>ransmission <b>A</b>utomated <b>R</b>eliable <b>A</b>ssistant"},
    "about_description": {"fr": "Client VARA sécurisé avec chiffrement AES-256",
                         "en": "Secure VARA client with AES-256 encryption"},
    "about_features": {"fr": "Support VARA HF/FM/SAT • Perfect Forward Secrecy • Transfert de fichiers",
                      "en": "VARA HF/FM/SAT Support • Perfect Forward Secrecy • File Transfer"},
    "about_developer": {"fr": "Développé par:", "en": "Developed by:"},
    "about_version": {"fr": "Version:", "en": "Version:"},
    
    # Messages de log supplémentaires
    "log_modem_connected": {"fr": "Connecté au modem VARA", "en": "Connected to VARA modem"},
    "log_modem_disconnected": {"fr": "Déconnecté du modem", "en": "Disconnected from modem"},
    "log_calling": {"fr": "Appel de", "en": "Calling"},
    "log_connected_to": {"fr": "Connecté à", "en": "Connected to"},
    "log_disconnected_from": {"fr": "Déconnecté de", "en": "Disconnected from"},
    "log_encryption_enabled": {"fr": "Chiffrement activé", "en": "Encryption enabled"},
    "log_encryption_disabled": {"fr": "Chiffrement désactivé", "en": "Encryption disabled"},
    "log_message_sent": {"fr": "Message envoyé", "en": "Message sent"},
    "log_file_sent": {"fr": "Fichier envoyé", "en": "File sent"},
    "log_file_received": {"fr": "Fichier reçu", "en": "File received"},
    
    # Erreurs détaillées
    "error_must_be_connected": {"fr": "Vous devez être connecté", "en": "You must be connected"},
    "error_no_file": {"fr": "Aucun fichier sélectionné", "en": "No file selected"},
    "error_sending": {"fr": "Erreur lors de l'envoi", "en": "Error while sending"},
    "error_modem_connection": {"fr": "Impossible de se connecter au modem", "en": "Cannot connect to modem"},
    "error_serialize": {"fr": "Impossible de sérialiser", "en": "Cannot serialize"},
    
    # Confirmations
    "confirm_cancel_transfer": {"fr": "Voulez-vous vraiment annuler", "en": "Do you really want to cancel"},
    "confirm_quit": {"fr": "Voulez-vous quitter", "en": "Do you want to quit"},
    
    # Chiffrement détaillé
    "crypto_password_enter": {"fr": "Entrez le mot de passe de chiffrement", "en": "Enter encryption password"},
    "crypto_password_share": {"fr": "Partagez ce mot de passe", "en": "Share this password"},
    "crypto_dh_established": {"fr": "Session Diffie-Hellman établie", "en": "Diffie-Hellman session established"},
    "crypto_pfs_enabled": {"fr": "Perfect Forward Secrecy activé", "en": "Perfect Forward Secrecy enabled"},
    "crypto_request_title": {"fr": "Demande de chiffrement", "en": "Encryption request"},
    "crypto_wants_encrypt": {"fr": "souhaite chiffrer", "en": "wants to encrypt"},
    "crypto_accept": {"fr": "acceptez ?", "en": "accept?"},
    "crypto_password_title": {"fr": "Mot de passe de chiffrement", "en": "Encryption password"},
    "crypto_password_shared": {"fr": "Entrez le mot de passe partagé", "en": "Enter shared password"},
    "crypto_request_refused": {"fr": "Demande de chiffrement refusée", "en": "Encryption request refused"},
    "crypto_password_not_entered": {"fr": "Mot de passe non saisi", "en": "Password not entered"},
    "crypto_dh_key_sent": {"fr": "Clé publique DH envoyée", "en": "DH public key sent"},
    "crypto_off_notification_sent": {"fr": "Notification de désactivation envoyée", "en": "Deactivation notification sent"},
    "crypto_disabled_by_other": {"fr": "a désactivé le chiffrement", "en": "has disabled encryption"},
    "crypto_disable_question": {"fr": "Souhaitez-vous également le désactiver", "en": "Do you also want to disable it"},
    "crypto_off_ack_sent": {"fr": "Confirmation de désactivation envoyée", "en": "Deactivation confirmation sent"},
    "crypto_kept_active": {"fr": "Chiffrement maintenu actif", "en": "Encryption kept active"},
    "crypto_off_refused_sent": {"fr": "Refus de désactivation envoyé", "en": "Deactivation refusal sent"},
    "crypto_other_wants_keep": {"fr": "souhaite maintenir le chiffrement", "en": "wants to keep encryption"},
    "crypto_desync": {"fr": "Désynchronisation du chiffrement", "en": "Encryption desynchronization"},
    "crypto_reactivate_question": {"fr": "Voulez-vous réactiver le chiffrement", "en": "Do you want to reactivate encryption"},
    "crypto_reactivated": {"fr": "Chiffrement réactivé", "en": "Encryption reactivated"},
    "crypto_reactivated_by_other": {"fr": "a réactivé le chiffrement", "en": "has reactivated encryption"},
    "crypto_reactivate_also": {"fr": "Souhaitez-vous également réactiver", "en": "Do you also want to reactivate"},
    "crypto_on_ack_sent": {"fr": "Confirmation de réactivation envoyée", "en": "Reactivation confirmation sent"},
    "crypto_also_reactivated": {"fr": "a également réactivé le chiffrement", "en": "has also reactivated encryption"},
    "crypto_on_notification_sent": {"fr": "Notification de réactivation envoyée", "en": "Reactivation notification sent"},
    "crypto_sending_request": {"fr": "Envoi d'une demande d'activation du chiffrement", "en": "Sending encryption activation request"},
    "crypto_request_sent": {"fr": "Demande envoyée - En attente de la réponse", "en": "Request sent - Waiting for response"},
    "crypto_generating_dh": {"fr": "Génération de la paire de clés Diffie-Hellman", "en": "Generating Diffie-Hellman key pair"},
    "crypto_waiting_peer_key": {"fr": "En attente de la clé de l'interlocuteur", "en": "Waiting for peer's key"},
    "crypto_request_received": {"fr": "Demande de chiffrement reçue", "en": "Encryption request received"},
    "crypto_generating_our_dh": {"fr": "Génération de notre paire de clés DH", "en": "Generating our DH key pair"},
    "crypto_response_sent_with_key": {"fr": "Réponse envoyée avec notre clé publique", "en": "Response sent with our public key"},
    "crypto_dh_session_established": {"fr": "Session DH établie", "en": "DH session established"},
    "crypto_error_shared_secret": {"fr": "Erreur lors du calcul du secret partagé", "en": "Error computing shared secret"},
    "crypto_response_received": {"fr": "Réponse de chiffrement reçue", "en": "Encryption response received"},
    "crypto_error_dh_session": {"fr": "Erreur lors de l'établissement de la session DH", "en": "Error establishing DH session"},
    "crypto_other_refused": {"fr": "L'autre station a refusé le chiffrement", "en": "The other station refused encryption"},
    "crypto_other_disabled": {"fr": "L'autre station a désactivé le chiffrement", "en": "The other station has disabled encryption"},
    "crypto_other_also_disabled": {"fr": "L'autre station a également désactivé le chiffrement", "en": "The other station has also disabled encryption"},
    "crypto_other_reactivated": {"fr": "L'autre station a réactivé le chiffrement", "en": "The other station has reactivated encryption"},
    "crypto_other_also_reactivated": {"fr": "L'autre station a également réactivé le chiffrement", "en": "The other station has also reactivated encryption"},
    
    # Messages système supplémentaires
    "cancelling": {"fr": "annulation", "en": "cancelling"},
    "open_settings_to_configure": {"fr": "Ouvrez les paramètres pour configurer le chemin de l'exécutable", "en": "Open settings to configure the executable path"},
    
    # Messages de fichiers supplémentaires
    "file_send_cancelled_by_user": {"fr": "Envoi de fichier annulé par l'utilisateur", "en": "File send cancelled by user"},
    "file_other_cancelled": {"fr": "L'autre station a annulé le transfert de fichier", "en": "The other station cancelled the file transfer"},
    
    # Transfert de fichiers détaillé
    "file_select": {"fr": "Sélectionner un fichier", "en": "Select a file"},
    "file_all_files": {"fr": "Tous les fichiers", "en": "All files"},
    "file_metadata_sending": {"fr": "Envoi des métadonnées", "en": "Sending metadata"},
    "file_encrypted_sending": {"fr": "Envoi fichier chiffré", "en": "Sending encrypted file"},
    "file_encrypted_receiving": {"fr": "Réception fichier chiffré", "en": "Receiving encrypted file"},
    "file_cancel_requested": {"fr": "Annulation du transfert demandée", "en": "Transfer cancellation requested"},
    "file_send_cancelled": {"fr": "Envoi de fichier annulé", "en": "File send cancelled"},
    "file_receive_cancelled": {"fr": "Réception de fichier annulée", "en": "File receive cancelled"},
    "file_cancel_notification_sent": {"fr": "Notification d'annulation envoyée", "en": "Cancellation notification sent"},
    "file_cancelled_by_other": {"fr": "a annulé le transfert", "en": "has cancelled the transfer"},
    "file_transfer_cancelled": {"fr": "Transfert annulé", "en": "Transfer cancelled"},
    "file_receive_cancel_title": {"fr": "Annulation de la réception", "en": "Cancel reception"},
    "file_receive_cancel_question": {"fr": "Voulez-vous vraiment annuler la réception", "en": "Do you really want to cancel reception"},
    "file_cancel_transfer_title": {"fr": "Annuler le transfert", "en": "Cancel transfer"},
    "file_send_cancel_question": {"fr": "Voulez-vous vraiment annuler l'envoi", "en": "Do you really want to cancel sending"},
    "file_sending_progress": {"fr": "Envoi en cours", "en": "Sending in progress"},
    "file_data_sent_to_buffer": {"fr": "Données envoyées au buffer VARA", "en": "Data sent to VARA buffer"},
    "file_no_transfer": {"fr": "Aucun transfert en cours", "en": "No transfer in progress"},
    "file_saved": {"fr": "Fichier sauvegardé", "en": "File saved"},
    "file_save": {"fr": "Sauvegarder le fichier", "en": "Save file"},
    
    # Auto-répondeur détaillé
    "auto_response_sent": {"fr": "Auto-réponse envoyée", "en": "Auto-response sent"},
    "auto_disconnect_in": {"fr": "Déconnexion automatique dans", "en": "Auto disconnect in"},
    
    # Statut système
    "version": {"fr": "Version", "en": "Version"},
    "launching": {"fr": "Lancement de", "en": "Launching"},
    "launched_successfully": {"fr": "lancé avec succès", "en": "launched successfully"},
    "launch_failed": {"fr": "Échec du lancement", "en": "Launch failed"},
    "executable_not_found": {"fr": "Exécutable introuvable", "en": "Executable not found"},
    "configure_path": {"fr": "Configurez le chemin", "en": "Configure path"},
    "connection_timeout": {"fr": "Timeout de connexion", "en": "Connection timeout"},
    
    # Dialogues supplémentaires
    "dlg_sync_success": {"fr": "Synchronisation réussie", "en": "Synchronization successful"},
    "dlg_secure_session": {"fr": "Session sécurisée établie", "en": "Secure session established"},
    
    # Textes divers
    "no_message": {"fr": "Aucun message", "en": "No message"},
    "bytes": {"fr": "octets", "en": "bytes"},
    "compressed": {"fr": "compressé", "en": "compressed"},
    "compressed_to": {"fr": "compressé à", "en": "compressed to"},
    "encrypted": {"fr": "chiffré", "en": "encrypted"},
    "remaining": {"fr": "reste", "en": "remaining"},
    "command_sent": {"fr": "Commande envoyée", "en": "Command sent"},
    "response_received": {"fr": "Réponse reçue", "en": "Response received"},
    
    # Clés pour f-strings
    "unknown_modem_type": {"fr": "Type de modem inconnu", "en": "Unknown modem type"},
    "modem_path_not_configured": {"fr": "Chemin de MODEM non configuré", "en": "MODEM path not configured"},
    "calling": {"fr": "Appel de", "en": "Calling"},
    "on": {"fr": "sur", "en": "on"},
    "encrypting_file": {"fr": "Chiffrement du fichier", "en": "Encrypting file"},
    "sending_encrypted_file": {"fr": "Envoi fichier chiffré", "en": "Sending encrypted file"},
    "sending_metadata": {"fr": "Envoi des métadonnées", "en": "Sending metadata"},
    "sending_file_in_chunks": {"fr": "Envoi du fichier en chunks", "en": "Sending file in chunks"},
    "data_sent_to_buffer": {"fr": "Données envoyées au buffer VARA", "en": "Data sent to VARA buffer"},
    "file_sent": {"fr": "Fichier envoyé", "en": "File sent"},
    "error_sending_file": {"fr": "Erreur envoi fichier", "en": "Error sending file"},
    "connecting_to": {"fr": "Connexion en cours vers", "en": "Connecting to"},
    "connected_to": {"fr": "Connecté à", "en": "Connected to"},
    "bandwidth": {"fr": "bandwidth", "en": "bandwidth"},
    "channel": {"fr": "Canal", "en": "Channel"},
    "vara_version": {"fr": "Version VARA", "en": "VARA version"},
    "vara_error": {"fr": "Erreur VARA", "en": "VARA error"},
    "receiving_file": {"fr": "Réception du fichier", "en": "Receiving file"},
    "decrypting_file": {"fr": "Déchiffrement du fichier", "en": "Decrypting file"},
    "file_received": {"fr": "Fichier reçu", "en": "File received"},
    "auto_response_to": {"fr": "Auto-réponse à", "en": "Auto-response to"},
    "auto_disconnect": {"fr": "Déconnexion auto", "en": "Auto disconnect"},
    "seconds": {"fr": "secondes", "en": "seconds"},
    
    # Dialogues MessageBox
    "dlg_dh_exchange_initiated": {"fr": "Échange DH initié", "en": "DH exchange initiated"},
    "msg_dh_key_sent": {"fr": "Votre clé publique Diffie-Hellman a été envoyée", "en": "Your Diffie-Hellman public key has been sent"},
    "msg_waiting_peer_key": {"fr": "En attente de la clé publique de votre interlocuteur", "en": "Waiting for your peer's public key"},
    "dlg_encryption_request": {"fr": "Demande de chiffrement", "en": "Encryption request"},
    "msg_wants_encrypted_session": {"fr": "souhaite établir une session chiffrée", "en": "wants to establish an encrypted session"},
    "msg_activate_encryption": {"fr": "Voulez-vous activer le chiffrement ?", "en": "Do you want to activate encryption?"},
    "msg_same_password": {"fr": "Vous devrez saisir le même mot de passe que votre interlocuteur", "en": "You must enter the same password as your peer"},
    "dlg_encryption_password": {"fr": "Mot de passe de chiffrement", "en": "Encryption password"},
    "msg_enter_shared_password": {"fr": "Entrez le mot de passe partagé avec", "en": "Enter shared password with"},
    "dlg_secure_session_established": {"fr": "Session sécurisée établie", "en": "Secure session established"},
    "msg_encryption_activated_with": {"fr": "Chiffrement activé avec", "en": "Encryption activated with"},
    "msg_dh_session_success": {"fr": "Session Diffie-Hellman établie avec succès", "en": "Diffie-Hellman session successfully established"},
    "dlg_encryption_refused": {"fr": "Refus de chiffrement", "en": "Encryption refused"},
    "msg_refused_encryption": {"fr": "a refusé la demande de chiffrement", "en": "refused the encryption request"},
    "msg_communication_unencrypted": {"fr": "La communication restera non chiffrée", "en": "Communication will remain unencrypted"},
    "dlg_launch_failed": {"fr": "Échec du lancement", "en": "Launch failed"},
    "msg_cannot_launch": {"fr": "Impossible de lancer", "en": "Cannot launch"},
    "dlg_executable_not_found": {"fr": "Exécutable introuvable", "en": "Executable not found"},
    "msg_executable_not_found": {"fr": "L'exécutable n'a pas été trouvé", "en": "Executable not found"},
    "msg_check_path": {"fr": "Vérifiez le chemin dans les paramètres", "en": "Check the path in settings"},
    "dlg_encryption_disabled": {"fr": "Chiffrement désactivé", "en": "Encryption disabled"},
    "dlg_encryption_maintained": {"fr": "Chiffrement maintenu", "en": "Encryption maintained"},
    "msg_encryption_still_active": {"fr": "Le chiffrement reste activé de votre côté", "en": "Encryption remains active on your side"},
    "msg_peer_informed": {"fr": "L'autre station a été informée", "en": "The other station has been informed"},
    "msg_must_agree": {"fr": "Vous devrez vous mettre d'accord", "en": "You must agree"},
    "msg_encryption_reactivated_with": {"fr": "Le chiffrement a été réactivé avec", "en": "Encryption has been reactivated with"},
    
    # setText et labels
    "btn_disconnect_modem": {"fr": "Déconnecter du modem", "en": "Disconnect from modem"},
    "sending": {"fr": "Envoi de", "en": "Sending"},
    "receiving": {"fr": "Réception de", "en": "Receiving"},
    "decrypting": {"fr": "Déchiffrement de", "en": "Decrypting"},
    "decoding": {"fr": "Décodage de", "en": "Decoding"},
    "decompressing": {"fr": "Décompression de", "en": "Decompressing"},
    "saving": {"fr": "Enregistrement", "en": "Saving"},
    
    # Messages finaux - 100% de couverture
    "reactivation_cancelled": {"fr": "Réactivation annulée", "en": "Reactivation cancelled"},
    "continuing_without_encryption": {"fr": "Vous continuez sans chiffrement", "en": "You continue without encryption"},
    "dh_key_received_old_format": {"fr": "Clé publique DH reçue (ancien format)", "en": "DH public key received (old format)"},
    "generating_our_key_pair": {"fr": "Génération de notre paire de clés", "en": "Generating our key pair"},
    "dh_key_sent_response": {"fr": "Clé publique DH envoyée en réponse", "en": "DH public key sent in response"},
    "dh_session_established": {"fr": "Session Diffie-Hellman établie", "en": "Diffie-Hellman session established"},
    "encryption_password_changed": {"fr": "Mot de passe de chiffrement modifié", "en": "Encryption password changed"},
    "send_cancelled_by_user": {"fr": "Envoi annulé par l'utilisateur", "en": "Send cancelled by user"},
    "call_cancelled": {"fr": "Appel annulé", "en": "Call cancelled"},
    "invalid_fileinfo_format": {"fr": "Format FILEINFO invalide", "en": "Invalid FILEINFO format"},
    "invalid_filedata_format": {"fr": "Format FILEDATA invalide", "en": "Invalid FILEDATA format"},
    "invalid_file_format": {"fr": "Format de fichier invalide", "en": "Invalid file format"},
    "receiving_encrypted_file": {"fr": "Réception fichier chiffré", "en": "Receiving encrypted file"},
    "file_decrypted": {"fr": "Fichier déchiffré", "en": "File decrypted"},
    "file_received_saved": {"fr": "Fichier reçu et enregistré", "en": "File received and saved"},
    "auto_responder_enabled": {"fr": "Auto-répondeur activé", "en": "Auto-responder enabled"},
    "delay": {"fr": "délai", "en": "delay"},
    "disconnected_from": {"fr": "Déconnecté de", "en": "Disconnected from"},
    "closing_vara": {"fr": "Fermeture de VARA", "en": "Closing VARA"},
    "error_decoding_data": {"fr": "Erreur décodage données", "en": "Error decoding data"},
    "cannot_decrypt_file": {"fr": "Impossible de déchiffrer le fichier", "en": "Cannot decrypt file"},
    "decompression_error": {"fr": "Erreur de décompression", "en": "Decompression error"},
    "fileinfo_error": {"fr": "Erreur FILEINFO", "en": "FILEINFO error"},
    "file_reception_error": {"fr": "Erreur réception fichier", "en": "File reception error"},
    "settings_save_error": {"fr": "Erreur sauvegarde paramètres", "en": "Settings save error"},
    
    # Dernières clés pour 100%
    "decoding_filename": {"fr": "Décodage du nom de fichier", "en": "Decoding filename"},
    "session": {"fr": "Session", "en": "Session"},
    "no_session": {"fr": "Pas de session", "en": "No session"},
    "dlg_success": {"fr": "Succès", "en": "Success"},
    "log_saved": {"fr": "Journal sauvegardé", "en": "Log saved"},
    "other_station": {"fr": "L'autre station", "en": "The other station"},
    
    # BBS (Bulletin Board System)
    "tab_bbs": {"fr": "BBS", "en": "BBS"},
    "bbs_enable": {"fr": "Activer le BBS (partage de fichiers)", "en": "Enable BBS (file sharing)"},
    "bbs_shared_folder": {"fr": "Dossier partagé:", "en": "Shared folder:"},
    "bbs_folder_placeholder": {"fr": "Sélectionner le dossier à partager", "en": "Select folder to share"},
    "bbs_info": {"fr": "Les fichiers de ce dossier seront accessibles<br>aux stations qui se connectent à vous.", 
                 "en": "Files in this folder will be accessible<br>to stations connecting to you."},
    "bbs_button": {"fr": "BBS", "en": "BBS"},
    "bbs_button_tooltip": {"fr": "Accéder au BBS de la station distante", "en": "Access remote station BBS"},
    "bbs_beacon_sent": {"fr": "BBS activé - Balise envoyée", "en": "BBS enabled - Beacon sent"},
    "bbs_detected": {"fr": "BBS distant détecté - Bouton activé", "en": "Remote BBS detected - Button enabled"},
    "bbs_disabled": {"fr": "BBS désactivé - Requête ignorée", "en": "BBS disabled - Request ignored"},
    "bbs_invalid_folder": {"fr": "Dossier BBS invalide - Requête ignorée", "en": "Invalid BBS folder - Request ignored"},
    "bbs_list_sent": {"fr": "Liste BBS envoyée", "en": "BBS list sent"},
    "bbs_list_error": {"fr": "Erreur liste BBS", "en": "BBS list error"},
    "bbs_list_received": {"fr": "Liste BBS reçue", "en": "BBS list received"},
    "bbs_parse_error": {"fr": "Erreur parsing liste BBS", "en": "BBS list parse error"},
    "bbs_access_denied": {"fr": "Accès refusé", "en": "Access denied"},
    "bbs_file_not_found": {"fr": "Fichier BBS introuvable", "en": "BBS file not found"},
    "bbs_sending_file": {"fr": "Envoi fichier BBS", "en": "Sending BBS file"},
    "bbs_not_active": {"fr": "Le BBS distant n'est pas actif", "en": "Remote BBS is not active"},
    "bbs_requesting_list": {"fr": "Demande liste BBS - Veuillez patienter...", "en": "Requesting BBS list - Please wait..."},
    "bbs_window_title": {"fr": "BBS", "en": "BBS"},
    "bbs_double_click_info": {"fr": "Double-cliquez sur un fichier pour le télécharger", "en": "Double-click a file to download"},
    "bbs_refresh": {"fr": "Rafraîchir", "en": "Refresh"},
    "bbs_refreshing": {"fr": "Rafraîchissement liste BBS...", "en": "Refreshing BBS list..."},
    "bbs_downloading": {"fr": "Téléchargement", "en": "Downloading"},
    "bbs_file_sent": {"fr": "Fichier BBS envoyé", "en": "BBS file sent"},
    "bbs_send_error": {"fr": "Erreur envoi fichier BBS", "en": "BBS file send error"},
    "lbl_files": {"fr": "fichiers", "en": "files"},
    "lbl_file": {"fr": "fichier", "en": "file"},
    "lbl_filename": {"fr": "Nom du fichier", "en": "Filename"},
    "lbl_size": {"fr": "Taille", "en": "Size"},
    
    # Dossier de réception fichiers
    "file_rx_folder": {"fr": "Dossier fichiers reçus:", "en": "Received files folder:"},
    "file_rx_folder_placeholder": {"fr": "Laisser vide pour demander à chaque fichier", 
                                    "en": "Leave empty to ask for each file"},
    "file_rx_folder_browse": {"fr": "Sélectionner le dossier de sauvegarde des fichiers reçus", 
                              "en": "Select folder for received files"},
    "bbs_folder_browse": {"fr": "Sélectionner le dossier BBS à partager", 
                          "en": "Select BBS folder to share"},
    
    # Info station (Paramètres)
    "tab_info_station": {"fr": "Info station", "en": "Station Info"},
    "lbl_station_info": {"fr": "Informations de la station", "en": "Station Information"},
    "lbl_callsign_required": {"fr": "Indicatif: *", "en": "Callsign: *"},
    "lbl_name": {"fr": "Nom:", "en": "Name:"},
    "lbl_firstname": {"fr": "Prénom:", "en": "First Name:"},
    "lbl_qth": {"fr": "QTH:", "en": "QTH:"},
    "lbl_grid_square": {"fr": "Grid Square:", "en": "Grid Square:"},
    "lbl_rig": {"fr": "Rig:", "en": "Rig:"},
    "lbl_antenna": {"fr": "Antenne:", "en": "Antenna:"},
    "placeholder_callsign": {"fr": "Ex: F4JTV", "en": "Ex: W1ABC"},
    "placeholder_name": {"fr": "Ex: Dupont", "en": "Ex: Smith"},
    "placeholder_firstname": {"fr": "Ex: Jean", "en": "Ex: John"},
    "placeholder_qth": {"fr": "Ex: Nice, France", "en": "Ex: New York, USA"},
    "placeholder_grid": {"fr": "Ex: JN33uo", "en": "Ex: FN30as"},
    "placeholder_rig": {"fr": "Ex: Yaesu FT-891", "en": "Ex: Yaesu FT-891"},
    "placeholder_antenna": {"fr": "Ex: Diamond V2000", "en": "Ex: Diamond V2000"},
    "required_field_note": {"fr": "* Champ requis pour la connexion au modem", 
                            "en": "* Required field for modem connection"},
    
    # Macros
    "menu_macros": {"fr": "Macros", "en": "Macros"},
    "dlg_macros_title": {"fr": "Configuration des Macros", "en": "Macro Configuration"},
    "macros_info_title": {"fr": "Configuration des macros F1-F12", 
                          "en": "F1-F12 Macro Configuration"},
    "macros_variables": {"fr": "Variables disponibles: {MYCALL}, {NAME}, {FIRSTNAME}, {QTH}, {GRID}, {RIG}, {ANTENNA}, {CALLSIGN}, {TIME}, {DATE}",
                         "en": "Available variables: {MYCALL}, {NAME}, {FIRSTNAME}, {QTH}, {GRID}, {RIG}, {ANTENNA}, {CALLSIGN}, {TIME}, {DATE}"},
    "macro_placeholder": {"fr": "Exemple: Mon QTH est {QTH}", 
                          "en": "Example: My QTH is {QTH}"},
    "btn_test_macro": {"fr": "Tester F1", "en": "Test F1"},
    "dlg_test_macro": {"fr": "Test de macro", "en": "Macro Test"},
    "test_macro_original": {"fr": "Texte original:", "en": "Original text:"},
    "test_macro_expanded": {"fr": "Après expansion:", "en": "After expansion:"},
    "macros_saved": {"fr": "Macros sauvegardées", "en": "Macros saved"},
    "macro_sent": {"fr": "Macro {0} envoyée", "en": "Macro {0} sent"},
    "macro_empty": {"fr": "Macro {0} vide - configurez-la dans Outils → Macros", 
                    "en": "Macro {0} empty - configure it in Tools → Macros"},
    "macro_no_session": {"fr": "Pas de session active - Macro non envoyée", 
                         "en": "No active session - Macro not sent"},
    
    # Messages divers
    "connection_params_changed": {"fr": "Paramètres de connexion modifiés.\nVeuillez vous déconnecter et reconnecter pour appliquer les changements.",
                                   "en": "Connection parameters changed.\nPlease disconnect and reconnect to apply changes."},
    "restart_required": {"fr": "Redémarrage requis", "en": "Restart Required"},
    
    # Tooltips
    "menu_tooltips": {"fr": "Infobulles", "en": "Tooltips"},
    "tooltips_enabled": {"fr": "✓ Infobulles activées", "en": "✓ Tooltips enabled"},
    "tooltips_disabled": {"fr": "Infobulles désactivées", "en": "Tooltips disabled"},
    
    # Tooltips - Interface principale
    "tt_connect_btn": {"fr": "Se connecter au modem VARA / Se déconnecter", 
                       "en": "Connect to VARA modem / Disconnect"},
    "tt_call_btn": {"fr": "Appeler une station distante (nécessite connexion au modem)", 
                    "en": "Call a remote station (requires modem connection)"},
    "tt_cq_btn": {"fr": "Envoyer un CQ pour annoncer votre présence", 
                  "en": "Send a CQ to announce your presence"},
    "tt_disconnect_btn": {"fr": "Déconnecter la session active avec la station distante", 
                          "en": "Disconnect active session with remote station"},
    "tt_message_input": {"fr": "Saisissez votre message ici (Entrée pour envoyer)", 
                         "en": "Type your message here (Enter to send)"},
    "tt_send_btn": {"fr": "Envoyer le message (ou appuyez sur Entrée)", 
                    "en": "Send message (or press Enter)"},
    "tt_file_btn": {"fr": "Envoyer un fichier à la station distante", 
                    "en": "Send a file to remote station"},
    "tt_crypto_btn": {"fr": "Activer/désactiver le chiffrement AES-256 pour cette session", 
                      "en": "Enable/disable AES-256 encryption for this session"},
    "tt_chat_display": {"fr": "Historique des messages de la conversation", 
                        "en": "Message history of the conversation"},
    "tt_log_display": {"fr": "Journal système avec les événements et statuts", 
                       "en": "System log with events and status"},
    "tt_cq_table": {"fr": "Liste des CQ reçus (double-clic pour se connecter)", 
                    "en": "List of received CQs (double-click to connect)"},
    
    # Tooltips - Paramètres Modem
    "tt_modem_type": {"fr": "Type de modem VARA à utiliser (HF, FM ou SAT)", 
                      "en": "VARA modem type to use (HF, FM or SAT)"},
    "tt_vara_path": {"fr": "Chemin vers l'exécutable VARA sur votre système", 
                     "en": "Path to VARA executable on your system"},
    "tt_auto_start": {"fr": "Lancer automatiquement VARA au démarrage de CRYPTARA", 
                      "en": "Automatically launch VARA when CRYPTARA starts"},
    "tt_host": {"fr": "Adresse IP du modem VARA (localhost pour local)", 
                "en": "VARA modem IP address (localhost for local)"},
    "tt_cmd_port": {"fr": "Port TCP pour les commandes VARA (défaut: 8300)", 
                    "en": "TCP port for VARA commands (default: 8300)"},
    "tt_data_port": {"fr": "Port TCP pour les données VARA (défaut: 8301)", 
                     "en": "TCP port for VARA data (default: 8301)"},
    "tt_listen_mode": {"fr": "Activer le mode écoute au démarrage (permet de recevoir des appels)", 
                       "en": "Enable listen mode at startup (allows receiving calls)"},
    "tt_compression": {"fr": "Activer la compression des données pour optimiser le transfert", 
                       "en": "Enable data compression to optimize transfer"},
    
    # Tooltips - Info station
    "tt_callsign": {"fr": "Votre indicatif radioamateur (requis pour la connexion)", 
                    "en": "Your amateur radio callsign (required for connection)"},
    "tt_name": {"fr": "Votre nom de famille (optionnel, utilisable dans les macros)", 
                "en": "Your last name (optional, usable in macros)"},
    "tt_firstname": {"fr": "Votre prénom (optionnel, utilisable dans les macros)", 
                     "en": "Your first name (optional, usable in macros)"},
    "tt_qth": {"fr": "Votre localisation géographique (optionnel, ex: Nice, France)", 
               "en": "Your geographic location (optional, ex: New York, USA)"},
    "tt_grid": {"fr": "Votre locator Maidenhead (optionnel, 6 caractères, ex: JN33uo)", 
                "en": "Your Maidenhead locator (optional, 6 characters, ex: FN30as)"},
    "tt_rig": {"fr": "Votre transceiver radio (optionnel, utilisable dans les macros)", 
               "en": "Your radio transceiver (optional, usable in macros)"},
    "tt_antenna": {"fr": "Votre antenne (optionnel, utilisable dans les macros)", 
                   "en": "Your antenna (optional, usable in macros)"},
    
    # Tooltips - Auto-répondeur
    "tt_auto_enable": {"fr": "Activer l'auto-répondeur lors des connexions entrantes", 
                       "en": "Enable auto-responder on incoming connections"},
    "tt_auto_message": {"fr": "Message envoyé automatiquement lors d'une connexion entrante", 
                        "en": "Message sent automatically on incoming connection"},
    "tt_auto_disconnect": {"fr": "Se déconnecter automatiquement après l'envoi du message", 
                           "en": "Automatically disconnect after sending message"},
    "tt_auto_delay": {"fr": "Délai en secondes avant l'envoi du message automatique", 
                      "en": "Delay in seconds before sending automatic message"},
    
    # Tooltips - Interface
    "tt_timestamp": {"fr": "Afficher l'horodatage sur chaque message du chat", 
                     "en": "Display timestamp on each chat message"},
    "tt_sound": {"fr": "Jouer un son lors de la réception de messages", 
                 "en": "Play sound on message reception"},
    "tt_save_log": {"fr": "Sauvegarder automatiquement le journal à la fermeture", 
                    "en": "Automatically save log on close"},
    "tt_file_rx_path": {"fr": "Dossier de destination des fichiers reçus", 
                        "en": "Destination folder for received files"},
    
    # Tooltips - BBS
    "tt_bbs_enable": {"fr": "Activer le serveur BBS (Bulletin Board System) lors des connexions", 
                      "en": "Enable BBS (Bulletin Board System) server on connections"},
    "tt_bbs_folder": {"fr": "Dossier contenant les fichiers partagés via le BBS", 
                      "en": "Folder containing files shared via BBS"},
    
    # Tooltips - Macros
    "tt_macro_field": {"fr": "Macro F{0}: message prédéfini avec variables {MYCALL}, {QTH}, etc.", 
                       "en": "Macro F{0}: predefined message with variables {MYCALL}, {QTH}, etc."},
    "tt_test_macro": {"fr": "Tester la macro F1 pour voir le résultat après expansion des variables", 
                      "en": "Test F1 macro to see result after variable expansion"},
}

# Instance globale du traducteur
_translator = Translator()

def tr(key):
    """Fonction de traduction raccourcie"""
    return _translator.tr(key)


class CryptoManager:
    """
    Gestionnaire de chiffrement AES-256 avancé pour les messages
    - PBKDF2 pour la dérivation de clé
    - HMAC-SHA256 pour l'authentification
    - Diffie-Hellman pour Perfect Forward Secrecy
    """
    
    def __init__(self):
        self.master_password = None
        self.session_key = None  # Clé de session (Diffie-Hellman)
        self.enabled = False
        
        # Paramètres PBKDF2
        self.pbkdf2_iterations = 100000  # 100k itérations (recommandé NIST)
        self.salt = None
        
        # Diffie-Hellman
        self.dh_private_key = None
        self.dh_public_key = None
        self.dh_shared_secret = None
        self.dh_established = False
        
    def set_password(self, password):
        """Configurer le mot de passe maître"""
        if not password:
            self.master_password = None
            self.enabled = False
            return
        
        # Normaliser le mot de passe (enlever espaces début/fin)
        password = password.strip()
        
        self.master_password = password
        
        # Afficher un hash pour debug (permet de vérifier que les 2 stations ont le même)
        password_hash = hashlib.sha256(password.encode('utf-8')).hexdigest()[:16]
        print(f"[DEBUG] Mot de passe configuré - Hash: {password_hash}")
        
        # Générer un sel aléatoire pour PBKDF2
        self.salt = secrets.token_bytes(16)
        
    def derive_key(self, password, salt, context=b'message'):
        """
        Dériver une clé avec PBKDF2-HMAC-SHA256
        
        Args:
            password: Mot de passe en texte
            salt: Sel (16 octets)
            context: Contexte pour dérivation (permet plusieurs clés)
        
        Returns:
            Clé de 32 octets (256 bits)
        """
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=32,  # 256 bits
            salt=salt + context,  # Ajouter contexte au sel
            iterations=self.pbkdf2_iterations,
            backend=default_backend()
        )
        
        return kdf.derive(password.encode('utf-8'))
    
    def generate_dh_keypair(self):
        """Générer une paire de clés Diffie-Hellman avec paramètres RFC 3526 Group 14"""
        # Utiliser les paramètres standards RFC 3526 Group 14 (2048 bits)
        # Cela garantit la compatibilité entre les deux stations
        
        # Prime p (2048 bits)
        p = int(
            'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1'
            '29024E088A67CC74020BBEA63B139B22514A08798E3404DD'
            'EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245'
            'E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED'
            'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D'
            'C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F'
            '83655D23DCA3AD961C62F356208552BB9ED529077096966D'
            '670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B'
            'E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9'
            'DE2BCBF6955817183995497CEA956AE515D2261898FA0510'
            '15728E5A8AACAA68FFFFFFFFFFFFFFFF', 16
        )
        g = 2
        
        # Créer les paramètres DH
        pn = dh.DHParameterNumbers(p, g)
        parameters = pn.parameters(default_backend())
        
        # Générer la paire de clés
        self.dh_private_key = parameters.generate_private_key()
        self.dh_public_key = self.dh_private_key.public_key()
        
        return self.serialize_public_key(self.dh_public_key)
    
    def serialize_public_key(self, public_key):
        """Sérialiser la clé publique pour transmission"""
        pem = public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )
        return base64.b64encode(pem).decode('ascii')
    
    def deserialize_public_key(self, pem_b64):
        """Désérialiser une clé publique reçue"""
        pem = base64.b64decode(pem_b64)
        return serialization.load_pem_public_key(pem, backend=default_backend())
    
    def compute_shared_secret(self, peer_public_key_b64):
        """Calculer le secret partagé Diffie-Hellman"""
        if not self.dh_private_key:
            return False
        
        try:
            peer_public_key = self.deserialize_public_key(peer_public_key_b64)
            
            # Calculer le secret partagé
            shared_key = self.dh_private_key.exchange(peer_public_key)
            
            # Dériver la clé de session avec HKDF
            hkdf = HKDF(
                algorithm=hashes.SHA256(),
                length=32,
                salt=None,
                info=b'vara-chat-session-key',
                backend=default_backend()
            )
            
            self.session_key = hkdf.derive(shared_key)
            self.dh_established = True
            
            return True
            
        except Exception as e:
            print(f"Erreur calcul DH: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def enable(self):
        """Activer le chiffrement"""
        if self.master_password:
            self.enabled = True
            return True
        return False
    
    def disable(self):
        """Désactiver le chiffrement"""
        self.enabled = False
    
    def reset_session(self):
        """Réinitialiser la session (nouveau DH)"""
        self.session_key = None
        self.dh_private_key = None
        self.dh_public_key = None
        self.dh_shared_secret = None
        self.dh_established = False
    
    def encrypt(self, plaintext):
        """
        Chiffrer un message avec AES-256-CBC + HMAC-SHA256
        
        Format de sortie:
        - Salt PBKDF2 (16 octets) si pas de session DH
        - IV (16 octets)
        - Message chiffré (variable)
        - HMAC (32 octets)
        """
        if not self.enabled:
            return plaintext
        
        if not self.master_password and not self.session_key:
            return plaintext
        
        try:
            # Choisir la clé : session DH si disponible, sinon master password
            if self.dh_established and self.session_key:
                encryption_key = self.session_key
                use_session = True
                salt = b''  # Pas besoin de salt avec clé de session
                print(f"[DEBUG] Chiffrement MODE SESSION")
            else:
                # Dériver clé depuis master password
                salt = secrets.token_bytes(16)
                encryption_key = self.derive_key(self.master_password, salt, b'encryption')
                use_session = False
                print(f"[DEBUG] Chiffrement MODE PASSWORD")
            
            # Dériver clé HMAC
            if use_session:
                hmac_key = self.derive_key_from_session(self.session_key, b'hmac')
            else:
                hmac_key = self.derive_key(self.master_password, salt, b'hmac')
            
            # Générer un IV aléatoire (16 octets pour AES)
            iv = secrets.token_bytes(16)
            
            # Créer le cipher
            cipher = Cipher(
                algorithms.AES(encryption_key),
                modes.CBC(iv),
                backend=default_backend()
            )
            encryptor = cipher.encryptor()
            
            # Padding PKCS7
            padder = padding.PKCS7(128).padder()
            padded_data = padder.update(plaintext.encode('utf-8')) + padder.finalize()
            
            # Chiffrer
            ciphertext = encryptor.update(padded_data) + encryptor.finalize()
            
            # Calculer HMAC sur IV + ciphertext
            import hmac as hmac_module
            data_to_auth = iv + ciphertext
            hmac_tag = hmac_module.new(hmac_key, data_to_auth, hashlib.sha256).digest()
            
            print(f"[DEBUG] Message: '{plaintext}' ({len(plaintext)} car)")
            print(f"[DEBUG] Padded: {len(padded_data)}B, Cipher: {len(ciphertext)}B")
            print(f"[DEBUG] HMAC: {hmac_tag[:8].hex()}...")
            
            # Format: [salt (0 ou 16)] + IV (16) + ciphertext (variable) + HMAC (32)
            if use_session:
                encrypted_message = b'\x01' + iv + ciphertext + hmac_tag  # \x01 = session key
            else:
                encrypted_message = b'\x00' + salt + iv + ciphertext + hmac_tag  # \x00 = password
            
            print(f"[DEBUG] Taille totale: {len(encrypted_message)} octets")
            
            # Encoder en base64
            encoded = base64.b64encode(encrypted_message).decode('ascii')
            
            return f"<ENC>{encoded}</ENC>"
            
        except Exception as e:
            print(f"Erreur de chiffrement: {e}")
            import traceback
            traceback.print_exc()
            return plaintext
    
    def derive_key_from_session(self, session_key, context):
        """Dériver une clé depuis la session key avec HKDF"""
        hkdf = HKDF(
            algorithm=hashes.SHA256(),
            length=32,
            salt=None,
            info=context,
            backend=default_backend()
        )
        
        return hkdf.derive(session_key)
    
    def decrypt(self, ciphertext):
        """
        Déchiffrer un message AES-256-CBC + vérifier HMAC
        """
        if not self.enabled:
            return ciphertext
        
        # Vérifier si c'est un message chiffré
        if not ciphertext.startswith("<ENC>") or not ciphertext.endswith("</ENC>"):
            return ciphertext
        
        if not self.master_password and not self.session_key:
            return "[Message chiffré - mot de passe non configuré]"
        
        try:
            # Extraire le message encodé
            encoded = ciphertext[5:-6]  # Enlever <ENC> et </ENC>
            encrypted_message = base64.b64decode(encoded)
            
            print(f"[DEBUG] Taille message chiffré: {len(encrypted_message)} octets")
            
            # Lire le flag (premier octet)
            flag = encrypted_message[0]
            print(f"[DEBUG] Flag: 0x{flag:02x} ({'SESSION' if flag == 0x01 else 'PASSWORD'})")
            
            if flag == 0x01:  # Session key
                if not self.dh_established or not self.session_key:
                    print("[DEBUG] Session DH non établie")
                    return "[Message chiffré - session DH non établie]"
                
                # Format: flag (1) + IV (16) + ciphertext (variable) + HMAC (32)
                iv = encrypted_message[1:17]
                hmac_tag = encrypted_message[-32:]
                actual_ciphertext = encrypted_message[17:-32]
                
                print(f"[DEBUG] Mode SESSION - IV: {len(iv)}B, Cipher: {len(actual_ciphertext)}B, HMAC: {len(hmac_tag)}B")
                
                encryption_key = self.session_key
                hmac_key = self.derive_key_from_session(self.session_key, b'hmac')
                
            else:  # Password-based (flag == 0x00)
                # Format: flag (1) + salt (16) + IV (16) + ciphertext (variable) + HMAC (32)
                salt = encrypted_message[1:17]
                iv = encrypted_message[17:33]
                hmac_tag = encrypted_message[-32:]
                actual_ciphertext = encrypted_message[33:-32]
                
                print(f"[DEBUG] Mode PASSWORD - Salt: {len(salt)}B, IV: {len(iv)}B, Cipher: {len(actual_ciphertext)}B, HMAC: {len(hmac_tag)}B")
                
                encryption_key = self.derive_key(self.master_password, salt, b'encryption')
                hmac_key = self.derive_key(self.master_password, salt, b'hmac')
            
            # Vérifier HMAC
            import hmac as hmac_module
            data_to_auth = iv + actual_ciphertext
            expected_hmac = hmac_module.new(hmac_key, data_to_auth, hashlib.sha256).digest()
            
            print(f"[DEBUG] HMAC attendu: {expected_hmac[:8].hex()}...")
            print(f"[DEBUG] HMAC reçu:    {hmac_tag[:8].hex()}...")
            
            if not hmac_module.compare_digest(hmac_tag, expected_hmac):
                print("[DEBUG] ❌ HMAC invalide !")
                return "[Message chiffré - HMAC invalide - mauvais mot de passe ?]"
            
            print("[DEBUG] ✅ HMAC valide")
            
            # Créer le cipher
            cipher = Cipher(
                algorithms.AES(encryption_key),
                modes.CBC(iv),
                backend=default_backend()
            )
            decryptor = cipher.decryptor()
            
            # Déchiffrer
            padded_plaintext = decryptor.update(actual_ciphertext) + decryptor.finalize()
            
            # Enlever le padding
            unpadder = padding.PKCS7(128).unpadder()
            plaintext = unpadder.update(padded_plaintext) + unpadder.finalize()
            
            result = plaintext.decode('utf-8')
            print(f"[DEBUG] ✅ Déchiffrement réussi: '{result}'")
            return result
            
        except Exception as e:
            print(f"[DEBUG] ❌ Erreur de déchiffrement: {e}")
            import traceback
            traceback.print_exc()
            return "[Message chiffré - erreur de déchiffrement]"




class VARAProcessManager:
    """Gestionnaire du processus VARA (lancement/fermeture automatique)"""
    
    def __init__(self):
        self.process = None
        self.modem_type = None
        self.executable_path = None
        
    def start_vara(self, modem_type, executable_path):
        """Lancer VARA"""
        if not executable_path or not os.path.exists(executable_path):
            print(f"[VARA] Exécutable introuvable : {executable_path}")
            return False
        
        self.modem_type = modem_type
        self.executable_path = executable_path
        
        try:
            print(f"[VARA] Lancement de {modem_type} : {executable_path}")
            
            # Lancer VARA en mode caché (sans fenêtre de console sur Windows)
            if sys.platform == 'win32':
                # Windows : utiliser CREATE_NO_WINDOW
                startupinfo = subprocess.STARTUPINFO()
                startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startupinfo.wShowWindow = subprocess.SW_HIDE
                
                self.process = subprocess.Popen(
                    [executable_path],
                    startupinfo=startupinfo,
                    creationflags=subprocess.CREATE_NO_WINDOW
                )
            else:
                # Linux/Mac : utiliser Wine si nécessaire
                if executable_path.lower().endswith('.exe'):
                    # Essayer avec Wine
                    self.process = subprocess.Popen(['wine', executable_path])
                else:
                    self.process = subprocess.Popen([executable_path])
            
            print(f"[VARA] {modem_type} lancé (PID: {self.process.pid})")
            
            # Attendre un peu que VARA démarre
            time.sleep(3)
            
            return True
            
        except Exception as e:
            print(f"[VARA] Erreur de lancement : {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def stop_vara(self):
        """Arrêter VARA"""
        if self.process:
            try:
                print(f"[VARA] Arrêt de {self.modem_type}...")
                
                # Essayer d'abord une fermeture propre
                self.process.terminate()
                
                # Attendre jusqu'à 5 secondes
                try:
                    self.process.wait(timeout=5)
                    print(f"[VARA] {self.modem_type} arrêté proprement")
                except subprocess.TimeoutExpired:
                    # Si pas terminé, forcer
                    print(f"[VARA] Forçage de l'arrêt...")
                    self.process.kill()
                    self.process.wait()
                    print(f"[VARA] {self.modem_type} arrêté (forcé)")
                
                self.process = None
                
            except Exception as e:
                print(f"[VARA] Erreur lors de l'arrêt : {e}")
    
    def is_running(self):
        """Vérifier si VARA est en cours d'exécution"""
        if self.process:
            return self.process.poll() is None
        return False


class VARAModemConfig:
    """Configuration pour les différents modems VARA"""
    
    MODEMS = {
        'VARA HF': {
            'cmd_port': 8300,
            'data_port': 8301,
            'bandwidths': [500, 2300],  # 2750 Hz réservé aux licences police/militaire
            'default_bw': 2300
        },
        'VARA FM': {
            'cmd_port': 8300,
            'data_port': 8301,
            'bandwidths': ['NARROW', 'WIDE'],  # VARA FM utilise NARROW/WIDE
            'default_bw': 'WIDE'
        },
        'VARA SAT': {
            'cmd_port': 8300,
            'data_port': 8301,
            'bandwidths': [500, 2300],
            'default_bw': 2300
        }
    }


class VARASignals(QObject):
    """Signaux Qt pour la communication thread-safe"""
    command_received = pyqtSignal(str)
    data_received = pyqtSignal(bytes)
    connection_status = pyqtSignal(bool)
    ptt_status = pyqtSignal(bool)
    connected_to = pyqtSignal(str)
    disconnected_from = pyqtSignal(str)
    log_message = pyqtSignal(str, str)  # (message, level)


class VARAClient:
    """Client VARA avec support multi-modem"""
    
    def __init__(self, modem_type='VARA HF', host='localhost', cmd_port=None, data_port=None):
        self.modem_type = modem_type
        self.host = host
        self.config = VARAModemConfig.MODEMS[modem_type]
        
        # Utiliser les ports personnalisés s'ils sont fournis, sinon utiliser ceux par défaut
        self.cmd_port = cmd_port if cmd_port is not None else self.config['cmd_port']
        self.data_port = data_port if data_port is not None else self.config['data_port']
        
        self.cmd_socket = None
        self.data_socket = None
        self.connected = False
        self.remote_station = None
        self.session_active = False
        self.channel_busy = False
        self.pending_connect = False
        self.mycall = None  # Pour stocker notre indicatif
        
        self.signals = VARASignals()
        
    def connect(self):
        """Connexion aux ports VARA"""
        try:
            # Socket de commande
            self.cmd_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.cmd_socket.settimeout(5)
            self.cmd_socket.connect((self.host, self.cmd_port))
            
            # Socket de données
            self.data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.data_socket.settimeout(5)
            self.data_socket.connect((self.host, self.data_port))
            
            self.connected = True
            self.signals.connection_status.emit(True)
            self.signals.log_message.emit(
                f"Connecté à {self.modem_type} ({self.host}:{self.cmd_port})", 
                "success"
            )
            
            # Démarrer les threads d'écoute
            threading.Thread(target=self._listen_commands, daemon=True).start()
            threading.Thread(target=self._listen_data, daemon=True).start()
            
            return True
            
        except Exception as e:
            self.signals.log_message.emit(f"Erreur connexion: {e}", "error")
            self.signals.connection_status.emit(False)
            return False
    
    def disconnect(self):
        """Déconnexion du modem"""
        self.connected = False
        if self.cmd_socket:
            try:
                self.cmd_socket.close()
            except:
                pass
        if self.data_socket:
            try:
                self.data_socket.close()
            except:
                pass
        self.signals.connection_status.emit(False)
        self.signals.log_message.emit("Déconnecté du modem", "info")
    
    def _listen_commands(self):
        """Thread d'écoute des commandes"""
        buffer = ""
        while self.connected:
            try:
                data = self.cmd_socket.recv(4096).decode('utf-8', errors='ignore')
                if data:
                    buffer += data
                    while '\r' in buffer or '\n' in buffer:
                        if '\r' in buffer:
                            line, buffer = buffer.split('\r', 1)
                        else:
                            line, buffer = buffer.split('\n', 1)
                        if line.strip():
                            self.signals.command_received.emit(line.strip())
            except socket.timeout:
                continue
            except Exception as e:
                if self.connected:
                    self.signals.log_message.emit(f"Erreur lecture commandes: {e}", "error")
                break
    
    def _listen_data(self):
        """Thread d'écoute des données"""
        while self.connected:
            try:
                data = self.data_socket.recv(4096)
                if data:
                    self.signals.data_received.emit(data)
            except socket.timeout:
                continue
            except Exception as e:
                if self.connected:
                    self.signals.log_message.emit(f"Erreur lecture données: {e}", "error")
                break
    
    def send_command(self, cmd):
        """Envoi d'une commande"""
        if not self.connected:
            return False
        try:
            self.cmd_socket.send(f"{cmd}\r".encode('utf-8'))
            return True
        except Exception as e:
            self.signals.log_message.emit(f"Erreur envoi commande: {e}", "error")
            return False
    
    def send_data(self, data):
        """Envoi de données"""
        if not self.connected:
            return False
        try:
            if isinstance(data, str):
                data = data.encode('utf-8')
            self.data_socket.send(data)
            return True
        except Exception as e:
            self.signals.log_message.emit(f"Erreur envoi données: {e}", "error")
            return False
    
    # Commandes VARA
    def set_mycall(self, callsign):
        self.mycall = callsign  # Stocker pour utilisation dans CONNECT
        return self.send_command(f"MYCALL {callsign}")
    
    def connect_to(self, callsign, bandwidth=None, via1=None, via2=None):
        """
        Commande CONNECT selon le protocole VARA avec digipeaters
        Format: CONNECT MYCALL DESTCALL VIA DIGI1 DIGI2
        
        IMPORTANT: Les digipeaters sont supportés UNIQUEMENT par VARA FM
        
        Paramètres:
            callsign: Indicatif de destination
            bandwidth: Bandwidth en Hz (VARA HF/SAT uniquement)
            via1: Premier digipeater (optionnel, VARA FM uniquement)
            via2: Deuxième digipeater (optionnel, VARA FM uniquement)
        """
        # Récupérer notre MYCALL pour l'utiliser comme source
        mycall = getattr(self, 'mycall', None)
        
        # DEBUG: Log pour vérifier les paramètres
        print(f"[DEBUG] connect_to: modem_type={self.modem_type}, bandwidth={bandwidth}, callsign={callsign}")
        
        # Définir le bandwidth AVANT la connexion (VARA HF UNIQUEMENT)
        # VARA SAT n'utilise PAS la commande BW (bande fixe)
        # IMPORTANT: Attendre un peu pour que VARA prenne en compte le BW
        if bandwidth and self.modem_type == 'VARA HF':
            # Protocole VARA HF: BW{valeur} SANS ESPACE
            print(f"[DEBUG] Envoi commande: BW{bandwidth}")
            self.send_command(f"BW{bandwidth}")
            # Attendre 100ms pour que VARA applique le BW
            import time
            time.sleep(0.1)
        
        # CRITIQUE: Digipeaters supportés UNIQUEMENT par VARA FM
        # Ignorer via1/via2 si ce n'est pas VARA FM
        if self.modem_type != 'VARA FM':
            via1 = None
            via2 = None
        
        # Construire la commande CONNECT
        if mycall:
            # Format avec MYCALL
            cmd = f"CONNECT {mycall} {callsign}"
            
            # Ajouter les digipeaters avec le mot-clé VIA (VARA FM uniquement)
            if via1:
                cmd += f" VIA {via1}"
                if via2:
                    cmd += f" {via2}"
            
            return self.send_command(cmd)
        else:
            # Fallback sans MYCALL
            cmd = f"CONNECT {callsign}"
            
            if via1:
                cmd += f" VIA {via1}"
                if via2:
                    cmd += f" {via2}"
            
            return self.send_command(cmd)
    
    def disconnect_session(self):
        return self.send_command("DISCONNECT")
    
    def listen(self, state=True):
        return self.send_command(f"LISTEN {'ON' if state else 'OFF'}")
    
    def abort(self):
        return self.send_command("ABORT")
    
    def get_version(self):
        return self.send_command("VERSION")
    
    def set_compression(self, state=True):
        return self.send_command(f"COMPRESSION {'ON' if state else 'OFF'}")
    
    def set_cq(self, state=True):
        """Active/désactive CQ (auto-answer)"""
        # Essayer plusieurs commandes selon la version VARA
        # CQ pour VARA HF, AUTOCONNECT pour certaines versions
        if state:
            # Essayer les deux commandes
            self.send_command("CQ ON")
            self.send_command("AUTOCONNECT ON")
        else:
            self.send_command("CQ OFF")
            self.send_command("AUTOCONNECT OFF")
        return True


class SettingsDialog(QDialog):
    """Dialogue de paramètres"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(tr("dlg_settings"))
        self.setModal(True)
        self.resize(500, 400)
        
        layout = QVBoxLayout()
        
        # Onglets
        tabs = QTabWidget()
        
        # Onglet Modem
        modem_tab = QWidget()
        modem_layout = QFormLayout()
        
        self.modem_combo = QComboBox()
        self.modem_combo.addItems(['VARA HF', 'VARA FM', 'VARA SAT'])
        modem_layout.addRow("Type de modem:", self.modem_combo)
        
        # Chemins d'exécutables VARA
        modem_layout.addRow(QLabel("<b>Chemins des exécutables VARA</b>"))
        
        # VARA HF
        hf_layout = QHBoxLayout()
        self.vara_hf_path_edit = QLineEdit()
        self.vara_hf_path_edit.setPlaceholderText("C:\\VARA\\VARA.exe")
        hf_browse_btn = QPushButton("📁")
        hf_browse_btn.setMaximumWidth(40)
        hf_browse_btn.clicked.connect(lambda: self.browse_executable(self.vara_hf_path_edit))
        hf_layout.addWidget(self.vara_hf_path_edit)
        hf_layout.addWidget(hf_browse_btn)
        modem_layout.addRow("VARA HF:", hf_layout)
        
        # VARA FM
        fm_layout = QHBoxLayout()
        self.vara_fm_path_edit = QLineEdit()
        self.vara_fm_path_edit.setPlaceholderText("C:\\VARA FM\\VARAFM.exe")
        fm_browse_btn = QPushButton("📁")
        fm_browse_btn.setMaximumWidth(40)
        fm_browse_btn.clicked.connect(lambda: self.browse_executable(self.vara_fm_path_edit))
        fm_layout.addWidget(self.vara_fm_path_edit)
        fm_layout.addWidget(fm_browse_btn)
        modem_layout.addRow("VARA FM:", fm_layout)
        
        # VARA SAT
        sat_layout = QHBoxLayout()
        self.vara_sat_path_edit = QLineEdit()
        self.vara_sat_path_edit.setPlaceholderText("C:\\VARA SAT\\VARASAT.exe")
        sat_browse_btn = QPushButton("📁")
        sat_browse_btn.setMaximumWidth(40)
        sat_browse_btn.clicked.connect(lambda: self.browse_executable(self.vara_sat_path_edit))
        sat_layout.addWidget(self.vara_sat_path_edit)
        sat_layout.addWidget(sat_browse_btn)
        modem_layout.addRow("VARA SAT:", sat_layout)
        
        # Lancement automatique
        self.auto_start_vara_check = QCheckBox("Lancer VARA automatiquement au démarrage")
        self.auto_start_vara_check.setChecked(True)
        modem_layout.addRow("", self.auto_start_vara_check)
        
        modem_layout.addRow(QLabel(""))  # Séparateur
        
        self.host_edit = QLineEdit("localhost")
        modem_layout.addRow("Hôte:", self.host_edit)
        
        self.cmd_port_spin = QSpinBox()
        self.cmd_port_spin.setRange(1024, 65535)
        self.cmd_port_spin.setValue(8300)
        modem_layout.addRow("Port commandes:", self.cmd_port_spin)
        
        self.data_port_spin = QSpinBox()
        self.data_port_spin.setRange(1024, 65535)
        self.data_port_spin.setValue(8301)
        modem_layout.addRow("Port données:", self.data_port_spin)
        
        self.listen_check = QCheckBox("Mode écoute au démarrage")
        self.listen_check.setChecked(True)
        modem_layout.addRow("", self.listen_check)
        
        self.compression_check = QCheckBox("Compression activée")
        self.compression_check.setChecked(True)
        modem_layout.addRow("", self.compression_check)
        
        modem_tab.setLayout(modem_layout)
        tabs.addTab(modem_tab, "Modem")
        
        # Onglet Info station
        info_tab = QWidget()
        info_layout = QFormLayout()
        
        info_layout.addRow(QLabel(f"<b>{tr('lbl_station_info')}</b>"))
        info_layout.addRow(QLabel(""))  # Séparateur
        
        # Indicatif (requis)
        self.mycall_edit = QLineEdit()
        self.mycall_edit.setPlaceholderText(tr("placeholder_callsign"))
        mycall_label = QLabel(tr("lbl_callsign_required"))
        mycall_label.setStyleSheet("font-weight: bold;")
        info_layout.addRow(mycall_label, self.mycall_edit)
        
        # Nom
        self.operator_name_edit = QLineEdit()
        self.operator_name_edit.setPlaceholderText(tr("placeholder_name"))
        info_layout.addRow(tr("lbl_name"), self.operator_name_edit)
        
        # Prénom
        self.operator_firstname_edit = QLineEdit()
        self.operator_firstname_edit.setPlaceholderText(tr("placeholder_firstname"))
        info_layout.addRow(tr("lbl_firstname"), self.operator_firstname_edit)
        
        # QTH
        self.qth_edit = QLineEdit()
        self.qth_edit.setPlaceholderText(tr("placeholder_qth"))
        info_layout.addRow(tr("lbl_qth"), self.qth_edit)
        
        # Grid Square
        self.grid_square_edit = QLineEdit()
        self.grid_square_edit.setPlaceholderText(tr("placeholder_grid"))
        self.grid_square_edit.setMaxLength(6)
        info_layout.addRow(tr("lbl_grid_square"), self.grid_square_edit)
        
        # Rig
        self.rig_edit = QLineEdit()
        self.rig_edit.setPlaceholderText(tr("placeholder_rig"))
        info_layout.addRow(tr("lbl_rig"), self.rig_edit)
        
        # Antenne
        self.antenna_edit = QLineEdit()
        self.antenna_edit.setPlaceholderText(tr("placeholder_antenna"))
        info_layout.addRow(tr("lbl_antenna"), self.antenna_edit)
        
        info_layout.addRow(QLabel(""))  # Séparateur
        info_layout.addRow(QLabel(f"<i>{tr('required_field_note')}</i>"))
        
        info_tab.setLayout(info_layout)
        tabs.addTab(info_tab, tr("tab_info_station"))
        
        # Onglet Auto-répondeur
        auto_tab = QWidget()
        auto_layout = QVBoxLayout()
        
        self.auto_enabled = QCheckBox(tr("lbl_enable_auto"))
        auto_layout.addWidget(self.auto_enabled)
        
        auto_layout.addWidget(QLabel("Message automatique:"))
        self.auto_message = QTextEdit()
        self.auto_message.setPlaceholderText(
            "Exemple: Bonjour {CALLSIGN}, merci pour l'appel. "
            "Je suis en QRV automatique. 73!"
        )
        self.auto_message.setMaximumHeight(100)
        auto_layout.addWidget(self.auto_message)
        
        self.auto_disconnect = QCheckBox("Déconnecter après réponse")
        self.auto_disconnect.setChecked(True)
        auto_layout.addWidget(self.auto_disconnect)
        
        delay_layout = QHBoxLayout()
        delay_layout.addWidget(QLabel("Délai avant réponse (secondes):"))
        self.auto_delay = QSpinBox()
        self.auto_delay.setRange(1, 60)
        self.auto_delay.setValue(3)
        delay_layout.addWidget(self.auto_delay)
        delay_layout.addStretch()
        auto_layout.addLayout(delay_layout)
        
        auto_layout.addStretch()
        auto_tab.setLayout(auto_layout)
        tabs.addTab(auto_tab, "Auto-répondeur")
        
        # Onglet Interface
        ui_tab = QWidget()
        ui_layout = QFormLayout()
        
        self.timestamp_check = QCheckBox("Afficher horodatage")
        self.timestamp_check.setChecked(True)
        ui_layout.addRow("", self.timestamp_check)
        
        self.sound_check = QCheckBox(tr("lbl_sound"))
        ui_layout.addRow("", self.sound_check)
        
        self.save_log_check = QCheckBox(tr("lbl_save_log"))
        self.save_log_check.setChecked(True)
        ui_layout.addRow("", self.save_log_check)
        
        ui_layout.addRow(QLabel(""))  # Séparateur
        
        # Dossier de sauvegarde des fichiers reçus
        file_rx_layout = QHBoxLayout()
        self.file_rx_path_edit = QLineEdit()
        self.file_rx_path_edit.setPlaceholderText(tr("file_rx_folder_placeholder"))
        file_rx_browse_btn = QPushButton("📁")
        file_rx_browse_btn.setMaximumWidth(40)
        file_rx_browse_btn.clicked.connect(self.browse_file_rx_folder)
        file_rx_layout.addWidget(self.file_rx_path_edit)
        file_rx_layout.addWidget(file_rx_browse_btn)
        ui_layout.addRow(tr("file_rx_folder"), file_rx_layout)
        
        ui_tab.setLayout(ui_layout)
        tabs.addTab(ui_tab, "Interface")
        
        # Onglet BBS (Bulletin Board System)
        bbs_tab = QWidget()
        bbs_layout = QFormLayout()
        
        self.bbs_enabled = QCheckBox(tr("bbs_enable"))
        bbs_layout.addRow("", self.bbs_enabled)
        
        bbs_layout.addRow(QLabel(""))  # Séparateur
        
        # Dossier partagé BBS
        bbs_folder_layout = QHBoxLayout()
        self.bbs_folder_edit = QLineEdit()
        self.bbs_folder_edit.setPlaceholderText(tr("bbs_folder_placeholder"))
        bbs_browse_btn = QPushButton("📁")
        bbs_browse_btn.setMaximumWidth(40)
        bbs_browse_btn.clicked.connect(self.browse_bbs_folder)
        bbs_folder_layout.addWidget(self.bbs_folder_edit)
        bbs_folder_layout.addWidget(bbs_browse_btn)
        bbs_layout.addRow(tr("bbs_shared_folder"), bbs_folder_layout)
        
        bbs_layout.addRow(QLabel(""))  # Séparateur
        bbs_layout.addRow(QLabel(f"<i>{tr('bbs_info')}</i>"))
        
        bbs_tab.setLayout(bbs_layout)
        tabs.addTab(bbs_tab, tr("tab_bbs"))
        
        layout.addWidget(tabs)
        
        # Boutons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | 
            QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        
        self.setLayout(layout)
    
    def browse_executable(self, line_edit):
        """Parcourir pour sélectionner un exécutable"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Sélectionner l'exécutable VARA",
            "",
            "Exécutables (*.exe);;Tous les fichiers (*.*)"
        )
        
        if file_path:
            line_edit.setText(file_path)
    
    def browse_file_rx_folder(self):
        """Parcourir pour sélectionner le dossier de sauvegarde des fichiers reçus"""
        folder_path = QFileDialog.getExistingDirectory(
            self,
            tr("file_rx_folder_browse"),
            ""
        )
        
        if folder_path:
            self.file_rx_path_edit.setText(folder_path)
    
    def browse_bbs_folder(self):
        """Parcourir pour sélectionner le dossier BBS partagé"""
        folder_path = QFileDialog.getExistingDirectory(
            self,
            tr("bbs_folder_browse"),
            ""
        )
        
        if folder_path:
            self.bbs_folder_edit.setText(folder_path)
    
    def apply_tooltips(self, enabled):
        """Appliquer les tooltips sur les éléments de la fenêtre de paramètres"""
        if enabled:
            # Onglet Modem
            self.modem_combo.setToolTip(tr('tt_modem_type'))
            self.vara_hf_path_edit.setToolTip(tr('tt_vara_path'))
            self.vara_fm_path_edit.setToolTip(tr('tt_vara_path'))
            self.vara_sat_path_edit.setToolTip(tr('tt_vara_path'))
            self.auto_start_vara_check.setToolTip(tr('tt_auto_start'))
            self.host_edit.setToolTip(tr('tt_host'))
            self.cmd_port_spin.setToolTip(tr('tt_cmd_port'))
            self.data_port_spin.setToolTip(tr('tt_data_port'))
            self.listen_check.setToolTip(tr('tt_listen_mode'))
            self.compression_check.setToolTip(tr('tt_compression'))
            
            # Onglet Info station
            self.mycall_edit.setToolTip(tr('tt_callsign'))
            self.operator_name_edit.setToolTip(tr('tt_name'))
            self.operator_firstname_edit.setToolTip(tr('tt_firstname'))
            self.qth_edit.setToolTip(tr('tt_qth'))
            self.grid_square_edit.setToolTip(tr('tt_grid'))
            self.rig_edit.setToolTip(tr('tt_rig'))
            self.antenna_edit.setToolTip(tr('tt_antenna'))
            
            # Onglet Auto-répondeur
            self.auto_enabled.setToolTip(tr('tt_auto_enable'))
            self.auto_message.setToolTip(tr('tt_auto_message'))
            self.auto_disconnect.setToolTip(tr('tt_auto_disconnect'))
            self.auto_delay.setToolTip(tr('tt_auto_delay'))
            
            # Onglet Interface
            self.timestamp_check.setToolTip(tr('tt_timestamp'))
            self.sound_check.setToolTip(tr('tt_sound'))
            self.save_log_check.setToolTip(tr('tt_save_log'))
            self.file_rx_path_edit.setToolTip(tr('tt_file_rx_path'))
            
            # Onglet BBS
            self.bbs_enabled.setToolTip(tr('tt_bbs_enable'))
            self.bbs_folder_edit.setToolTip(tr('tt_bbs_folder'))
        else:
            # Supprimer tous les tooltips
            self.modem_combo.setToolTip('')
            self.vara_hf_path_edit.setToolTip('')
            self.vara_fm_path_edit.setToolTip('')
            self.vara_sat_path_edit.setToolTip('')
            self.auto_start_vara_check.setToolTip('')
            self.host_edit.setToolTip('')
            self.cmd_port_spin.setToolTip('')
            self.data_port_spin.setToolTip('')
            self.listen_check.setToolTip('')
            self.compression_check.setToolTip('')
            self.mycall_edit.setToolTip('')
            self.operator_name_edit.setToolTip('')
            self.operator_firstname_edit.setToolTip('')
            self.qth_edit.setToolTip('')
            self.grid_square_edit.setToolTip('')
            self.rig_edit.setToolTip('')
            self.antenna_edit.setToolTip('')
            self.auto_enabled.setToolTip('')
            self.auto_message.setToolTip('')
            self.auto_disconnect.setToolTip('')
            self.auto_delay.setToolTip('')
            self.timestamp_check.setToolTip('')
            self.sound_check.setToolTip('')
            self.save_log_check.setToolTip('')
            self.file_rx_path_edit.setToolTip('')
            self.bbs_enabled.setToolTip('')
            self.bbs_folder_edit.setToolTip('')
    
    def get_settings(self):
        """Récupère les paramètres"""
        return {
            'modem_type': self.modem_combo.currentText(),
            'host': self.host_edit.text(),
            'cmd_port': self.cmd_port_spin.value(),
            'data_port': self.data_port_spin.value(),
            'mycall': self.mycall_edit.text().upper(),
            'operator_name': self.operator_name_edit.text(),
            'operator_firstname': self.operator_firstname_edit.text(),
            'qth': self.qth_edit.text(),
            'grid_square': self.grid_square_edit.text().upper(),
            'rig': self.rig_edit.text(),
            'antenna': self.antenna_edit.text(),
            'listen': self.listen_check.isChecked(),
            'compression': self.compression_check.isChecked(),
            'vara_hf_path': self.vara_hf_path_edit.text(),
            'vara_fm_path': self.vara_fm_path_edit.text(),
            'vara_sat_path': self.vara_sat_path_edit.text(),
            'auto_start_vara': self.auto_start_vara_check.isChecked(),
            'auto_enabled': self.auto_enabled.isChecked(),
            'auto_message': self.auto_message.toPlainText(),
            'auto_disconnect': self.auto_disconnect.isChecked(),
            'auto_delay': self.auto_delay.value(),
            'timestamp': self.timestamp_check.isChecked(),
            'sound': self.sound_check.isChecked(),
            'save_log': self.save_log_check.isChecked(),
            'file_rx_path': self.file_rx_path_edit.text(),
            'bbs_enabled': self.bbs_enabled.isChecked(),
            'bbs_folder': self.bbs_folder_edit.text()
        }
    
    def set_settings(self, settings):
        """Applique les paramètres"""
        self.modem_combo.setCurrentText(settings.get('modem_type', 'VARA HF'))
        self.host_edit.setText(settings.get('host', 'localhost'))
        self.cmd_port_spin.setValue(settings.get('cmd_port', 8300))
        self.data_port_spin.setValue(settings.get('data_port', 8301))
        self.mycall_edit.setText(settings.get('mycall', ''))
        self.operator_name_edit.setText(settings.get('operator_name', ''))
        self.operator_firstname_edit.setText(settings.get('operator_firstname', ''))
        self.qth_edit.setText(settings.get('qth', ''))
        self.grid_square_edit.setText(settings.get('grid_square', ''))
        self.rig_edit.setText(settings.get('rig', ''))
        self.antenna_edit.setText(settings.get('antenna', ''))
        self.vara_hf_path_edit.setText(settings.get('vara_hf_path', ''))
        self.vara_fm_path_edit.setText(settings.get('vara_fm_path', ''))
        self.vara_sat_path_edit.setText(settings.get('vara_sat_path', ''))
        self.auto_start_vara_check.setChecked(settings.get('auto_start_vara', True))
        self.listen_check.setChecked(settings.get('listen', True))
        self.compression_check.setChecked(settings.get('compression', True))
        self.auto_enabled.setChecked(settings.get('auto_enabled', False))
        self.auto_message.setPlainText(settings.get('auto_message', ''))
        self.auto_disconnect.setChecked(settings.get('auto_disconnect', True))
        self.auto_delay.setValue(settings.get('auto_delay', 3))
        self.timestamp_check.setChecked(settings.get('timestamp', True))
        self.sound_check.setChecked(settings.get('sound', False))
        self.save_log_check.setChecked(settings.get('save_log', True))
        self.file_rx_path_edit.setText(settings.get('file_rx_path', ''))
        self.bbs_enabled.setChecked(settings.get('bbs_enabled', False))
        self.bbs_folder_edit.setText(settings.get('bbs_folder', ''))


class MainWindow(QMainWindow):
    """Fenêtre principale de l'application"""
    
    def __init__(self):
        super().__init__()
        self.resize(900, 700)
        
        # Définir l'icône de la fenêtre
        self.set_window_icon()
        
        # Variables
        self.vara = None
        self.vara_process_manager = VARAProcessManager()
        self.settings = self.load_settings()
        self.qso_log = []
        self.auto_responder_active = False
        
        # Mettre à jour le titre avec l'indicatif
        self.update_window_title()
        
        # Chiffrement
        self.crypto = CryptoManager()
        
        # Buffer pour accumulation des messages reçus
        self.rx_buffer = ""
        self.rx_timer = None
        
        # Messages en attente d'ACK (affichés uniquement après confirmation VARA)
        self.pending_messages = []
        
        # Flag pour déconnexion automatique après envoi auto-réponse
        self.auto_disconnect_pending = False
        
        # CQ reçus
        self.received_cqs = []  # Liste des CQ: {callsign, bandwidth, via1, via2, time}
        
        # Transfert de fichiers
        self.receiving_file = False
        self.file_data = b""
        self.file_name = ""
        self.file_size = 0
        self.file_received = 0
        self.file_transfer_active = False
        self.file_transfer_total = 0
        self.file_transfer_sent = 0
        self.file_transfer_progress = 0
        
        # Réception de fichier avec progression
        self.file_rx_active = False
        self.file_rx_name = ""
        self.file_rx_total = 0
        self.file_rx_received = 0
        self.file_rx_start_time = 0
        self.file_rx_encrypted = False
        
        # BBS (Bulletin Board System)
        self.remote_bbs_active = False  # Le BBS distant est-il actif ?
        self.bbs_file_list = []  # Liste des fichiers BBS distants
        
        # Interface
        self.init_ui()
        
        # Auto-connexion si configuré
        if self.settings.get('mycall'):
            # Lancer VARA si activé
            if self.settings.get('auto_start_vara', True):
                QTimer.singleShot(500, self.auto_start_vara)
            else:
                QTimer.singleShot(500, self.connect_to_vara)
    
    def auto_start_vara(self):
        """Lancer VARA automatiquement"""
        modem_type = self.settings.get('modem_type', 'VARA HF')
        
        # Obtenir le chemin de l'exécutable
        path_key = {
            'VARA HF': 'vara_hf_path',
            'VARA FM': 'vara_fm_path',
            'VARA SAT': 'vara_sat_path'
        }.get(modem_type)
        
        if not path_key:
            self.log(f"{tr('unknown_modem_type')}:  {modem_type}", "error")
            return
        
        executable_path = self.settings.get(path_key, '')
        
        if not executable_path:
            self.log(f"⚠️ {tr('modem_path_not_configured').replace('MODEM', modem_type)}", "warning")
            self.log(f"{tr('open_settings_to_configure')}", "info")
            return
        
        if not os.path.exists(executable_path):
            self.log(f"❌ {tr('executable_not_found')} {modem_type}:  {executable_path}", "error")
            QMessageBox.warning(
                self,
                "Exécutable introuvable",
                f"L'exécutable {modem_type} est introuvable :\n{executable_path}\n\n"
                f"Configurez le chemin dans les paramètres."
            )
            return
        
        # Lancer VARA
        self.log(f"🚀 {tr('launching')} {modem_type}...", "info")
        
        if self.vara_process_manager.start_vara(modem_type, executable_path):
            self.log(f"✅ {modem_type} {tr('launched_successfully')}", "success")
            
            # Attendre un peu puis se connecter
            QTimer.singleShot(2000, self.connect_to_vara)
        else:
            self.log(f"❌ {tr('launch_failed')} {modem_type}", "error")
            QMessageBox.critical(
                self,
                "Erreur de lancement",
                f"Impossible de lancer {modem_type}.\n\n"
                f"Vérifiez le chemin de l'exécutable dans les paramètres."
            )
    
    def init_ui(self):
        """Initialisation de l'interface"""
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        
        # Barre d'outils
        toolbar_layout = QHBoxLayout()
        
        self.connect_btn = QPushButton(tr("btn_connect"))
        self.connect_btn.clicked.connect(self.connect_to_vara)
        toolbar_layout.addWidget(self.connect_btn)
        
        toolbar_layout.addWidget(QLabel("Station:"))
        self.station_edit = QLineEdit()
        self.station_edit.setPlaceholderText("Indicatif de la station")
        self.station_edit.setMaximumWidth(150)
        toolbar_layout.addWidget(self.station_edit)
        
        # Digipeaters (VIA)
        toolbar_layout.addWidget(QLabel("Via1:"))
        self.via1_edit = QLineEdit()
        self.via1_edit.setPlaceholderText("Digipeater 1")
        self.via1_edit.setMaximumWidth(100)
        self.via1_edit.setToolTip("Premier digipeater (optionnel)")
        self.via1_edit.setEnabled(False)  # Désactivé par défaut
        toolbar_layout.addWidget(self.via1_edit)
        
        toolbar_layout.addWidget(QLabel("Via2:"))
        self.via2_edit = QLineEdit()
        self.via2_edit.setPlaceholderText("Digipeater 2")
        self.via2_edit.setMaximumWidth(100)
        self.via2_edit.setToolTip("Deuxième digipeater (optionnel)")
        self.via2_edit.setEnabled(False)  # Désactivé par défaut
        toolbar_layout.addWidget(self.via2_edit)
        
        toolbar_layout.addWidget(QLabel("BW:"))
        self.bw_combo = QComboBox()
        self.bw_combo.setMaximumWidth(100)
        toolbar_layout.addWidget(self.bw_combo)
        
        self.call_btn = QPushButton(tr("btn_call"))
        self.call_btn.clicked.connect(self.call_station)
        self.call_btn.setEnabled(False)
        toolbar_layout.addWidget(self.call_btn)
        
        self.disconnect_btn = QPushButton(tr("btn_disconnect"))
        self.disconnect_btn.clicked.connect(self.disconnect_session)
        self.disconnect_btn.setEnabled(False)
        toolbar_layout.addWidget(self.disconnect_btn)
        
        toolbar_layout.addStretch()
        
        # Bouton chiffrement (désactivé par défaut, activé uniquement en session)
        self.crypto_btn = QPushButton("🔒 " + tr("btn_encryption"))
        self.crypto_btn.clicked.connect(self.toggle_crypto)
        self.crypto_btn.setCheckable(True)
        self.crypto_btn.setEnabled(False)  # Désactivé par défaut
        self.crypto_btn.setStyleSheet("QPushButton:checked { background-color: #4CAF50; color: white; font-weight: bold; }")
        toolbar_layout.addWidget(self.crypto_btn)
        
        self.bbs_btn = QPushButton("📁 " + tr("bbs_button"))
        self.bbs_btn.clicked.connect(self.show_bbs_window)
        self.bbs_btn.setEnabled(False)
        self.bbs_btn.setToolTip(tr("bbs_button_tooltip"))
        toolbar_layout.addWidget(self.bbs_btn)
        
        layout.addLayout(toolbar_layout)
        
        # Splitter principal (vertical)
        splitter = QSplitter(Qt.Orientation.Vertical)
        
        # Splitter horizontal pour chat + CQ
        chat_cq_splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # Zone de chat (gauche)
        chat_group = QGroupBox(tr("group_chat"))
        chat_layout = QVBoxLayout()
        
        self.chat_display = QTextEdit()
        self.chat_display.setReadOnly(True)
        self.chat_display.setLineWrapMode(QTextEdit.LineWrapMode.WidgetWidth)
        self.chat_display.setWordWrapMode(QTextOption.WrapMode.WordWrap)
        font = QFont("Monospace", 10)
        self.chat_display.setFont(font)
        # Stretch = 1 : le chat prend tout l'espace disponible
        chat_layout.addWidget(self.chat_display, 1)
        
        # Entrée message
        msg_layout = QHBoxLayout()
        self.msg_input = QLineEdit()
        self.msg_input.setPlaceholderText("Votre message...")
        self.msg_input.returnPressed.connect(self.send_message)
        self.msg_input.setEnabled(False)
        msg_layout.addWidget(self.msg_input)
        
        self.send_btn = QPushButton(tr("btn_send"))
        self.send_btn.clicked.connect(self.send_message)
        self.send_btn.setEnabled(False)
        msg_layout.addWidget(self.send_btn)
        
        self.file_btn = QPushButton("📎 " + tr("btn_file"))
        self.file_btn.clicked.connect(self.send_file)
        self.file_btn.setEnabled(False)
        self.file_btn.setToolTip("Envoyer un fichier")
        msg_layout.addWidget(self.file_btn)
        
        # Stretch = 0 : l'entrée message garde sa hauteur minimale
        chat_layout.addLayout(msg_layout, 0)
        
        # Barre de progression transfert de fichiers
        self.file_progress_widget = QWidget()
        file_progress_layout = QVBoxLayout()
        file_progress_layout.setContentsMargins(0, 5, 0, 5)
        
        self.file_progress_label = QLabel("Aucun transfert en cours")
        self.file_progress_label.setStyleSheet("color: gray; font-style: italic;")
        file_progress_layout.addWidget(self.file_progress_label)
        
        self.file_progress_bar = QProgressBar()
        self.file_progress_bar.setVisible(False)
        self.file_progress_bar.setTextVisible(True)
        self.file_progress_bar.setFormat("%p% - %v/%m octets")
        file_progress_layout.addWidget(self.file_progress_bar)
        
        # Bouton d'annulation de transfert
        # NOTE: Bouton d'annulation supprimé
        # La réception/envoi de fichier ne peut pas être annulée car :
        # - Envoi : fichier déjà dans buffer VARA
        # - Réception : données déjà reçues par VARA
        
        file_progress_layout.addStretch()
        
        self.file_progress_widget.setLayout(file_progress_layout)
        # Stretch = 0 : la barre de progression garde sa hauteur minimale
        chat_layout.addWidget(self.file_progress_widget, 0)
        
        chat_group.setLayout(chat_layout)
        chat_cq_splitter.addWidget(chat_group)
        
        # Panneau CQ (droite)
        cq_group = QGroupBox("📡 CQ Reçus")
        cq_layout = QVBoxLayout()
        
        # Bouton CQ
        cq_button_layout = QHBoxLayout()
        self.cq_btn = QPushButton("📢 Envoyer CQ")
        self.cq_btn.clicked.connect(self.send_cq)
        self.cq_btn.setEnabled(False)
        self.cq_btn.setToolTip("Envoyer un appel CQ sur la fréquence")
        cq_button_layout.addWidget(self.cq_btn)
        
        self.clear_cq_btn = QPushButton("🗑️ " + tr("btn_clear_cq"))
        self.clear_cq_btn.clicked.connect(self.clear_cq_list)
        self.clear_cq_btn.setToolTip("Effacer la liste des CQ reçus")
        cq_button_layout.addWidget(self.clear_cq_btn)
        
        cq_button_layout.addStretch()
        cq_layout.addLayout(cq_button_layout)
        
        # Tableau des CQ
        self.cq_table = QTableWidget()
        self.cq_table.setColumnCount(4)
        self.cq_table.setHorizontalHeaderLabels(["Indicatif", "BW", "Via", "Heure"])
        self.cq_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.cq_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.cq_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        self.cq_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        self.cq_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.cq_table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.cq_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.cq_table.doubleClicked.connect(self.connect_to_cq_station)
        self.cq_table.setToolTip("Double-cliquez sur un CQ pour vous connecter")
        cq_layout.addWidget(self.cq_table)
        
        cq_group.setLayout(cq_layout)
        chat_cq_splitter.addWidget(cq_group)
        
        # Proportions splitter : Chat 70%, CQ 30%
        chat_cq_splitter.setStretchFactor(0, 7)
        chat_cq_splitter.setStretchFactor(1, 3)
        
        # Politique de taille pour expansion verticale
        chat_cq_splitter.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Expanding
        )
        
        # Ajouter le splitter horizontal au splitter vertical
        splitter.addWidget(chat_cq_splitter)
        
        # Journal système
        log_group = QGroupBox(tr("group_system_log"))
        log_layout = QVBoxLayout()
        
        self.log_display = QTextEdit()
        self.log_display.setReadOnly(True)
        # Pas de setMaximumHeight pour permettre l'expansion
        log_layout.addWidget(self.log_display)
        
        log_group.setLayout(log_layout)
        splitter.addWidget(log_group)
        
        # Configuration du splitter vertical
        # Chat et log peuvent tous les deux s'étendre
        splitter.setStretchFactor(0, 3)  # chat_cq_splitter : 75% de l'espace
        splitter.setStretchFactor(1, 1)  # log_group : 25% de l'espace
        splitter.setCollapsible(0, False)  # Chat non collapsible
        splitter.setCollapsible(1, True)   # Log collapsible
        
        # Tailles initiales : Chat 600px, Log 150px
        splitter.setSizes([600, 150])
        
        layout.addWidget(splitter)
        
        # Barre d'état
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        
        self.status_modem = QLabel("⚫ Déconnecté")
        self.status_bar.addWidget(self.status_modem)
        
        self.status_ptt = QLabel("PTT: OFF")
        self.status_bar.addPermanentWidget(self.status_ptt)
        
        self.status_session = QLabel("Pas de session")
        self.status_bar.addPermanentWidget(self.status_session)
        
        # Menu
        menubar = self.menuBar()
        
        file_menu = menubar.addMenu(tr("menu_file"))
        
        save_chat_action = QAction(tr("btn_save_chat"), self)
        save_chat_action.triggered.connect(self.save_chat_to_file)
        file_menu.addAction(save_chat_action)
        
        save_action = QAction("Sauvegarder le journal", self)
        save_action.triggered.connect(self.save_log)
        file_menu.addAction(save_action)
        
        file_menu.addSeparator()
        
        settings_action = QAction(tr("btn_settings"), self)
        settings_action.triggered.connect(self.show_settings)
        file_menu.addAction(settings_action)
        
        file_menu.addSeparator()
        
        quit_action = QAction(tr("menu_quit"), self)
        quit_action.triggered.connect(self.close)
        file_menu.addAction(quit_action)
        
        tools_menu = menubar.addMenu(tr("menu_tools"))
        
        qso_action = QAction(tr("menu_qso_log"), self)
        qso_action.triggered.connect(self.show_qso_log)
        tools_menu.addAction(qso_action)
        
        macros_action = QAction(tr("menu_macros"), self)
        macros_action.triggered.connect(self.show_macros)
        tools_menu.addAction(macros_action)
        
        tools_menu.addSeparator()
        
        # Action pour activer/désactiver les tooltips
        self.tooltips_action = QAction(self)
        self.tooltips_action.triggered.connect(self.toggle_tooltips)
        self.update_tooltips_menu()
        tools_menu.addAction(self.tooltips_action)
        
        help_menu = menubar.addMenu(tr("menu_help"))
        
        about_action = QAction(tr("menu_about"), self)
        about_action.triggered.connect(self.show_about)
        help_menu.addAction(about_action)
        
        # Appliquer les tooltips au démarrage
        self.apply_tooltips()
    
    def connect_to_vara(self):
        """Connexion au modem VARA"""
        if self.vara and self.vara.connected:
            self.vara.disconnect()
            self.vara = None
            self.connect_btn.setText(tr("btn_connect"))
            self.call_btn.setEnabled(False)
            self.cq_btn.setEnabled(False)
            self.status_modem.setText(f"⚫ {tr('status_disconnected')}")
            return
        
        modem_type = self.settings.get('modem_type', 'VARA HF')
        host = self.settings.get('host', 'localhost')
        cmd_port = self.settings.get('cmd_port', 8300)
        data_port = self.settings.get('data_port', 8301)
        
        self.vara = VARAClient(modem_type, host, cmd_port, data_port)
        
        # Connexion des signaux
        self.vara.signals.command_received.connect(self.handle_command)
        self.vara.signals.data_received.connect(self.handle_data)
        self.vara.signals.connection_status.connect(self.handle_connection_status)
        self.vara.signals.log_message.connect(self.log)
        self.vara.signals.ptt_status.connect(self.handle_ptt)
        self.vara.signals.connected_to.connect(self.handle_connected)
        self.vara.signals.disconnected_from.connect(self.handle_disconnected)
        
        if self.vara.connect():
            # Activer le mode CHAT pour optimiser VARA
            # (timing, latence, échanges clavier, S/N reporting)
            self.vara.send_command("CHAT ON")
            
            # Configuration initiale
            mycall = self.settings.get('mycall', '')
            if mycall:
                self.vara.set_mycall(mycall)
            
            self.vara.get_version()
            
            if self.settings.get('listen', True):
                self.vara.listen(True)
            
            # Activer la réception des CQ frames
            self.vara.send_command("LISTEN CQ")
            
            if self.settings.get('compression', True):
                self.vara.set_compression(True)
            
            # NOTE: D'après le protocole VARA officiel, LISTEN ON suffit
            # Il n'y a pas de commande CQ ou AUTOCONNECT pour l'auto-accept
            
            # Mise à jour des bandwidths disponibles
            self.update_bandwidth_combo()
            
            # Activer/désactiver les digipeaters selon le type de modem
            self.update_digipeater_fields()
            
            self.connect_btn.setText(tr("btn_disconnect_modem"))
            self.call_btn.setEnabled(True)
            self.cq_btn.setEnabled(True)
    
    def update_digipeater_fields(self):
        """Active/désactive les champs digipeaters selon le type de modem"""
        if not self.vara:
            return
        
        # Digipeaters supportés UNIQUEMENT par VARA FM
        digipeater_supported = (self.vara.modem_type == 'VARA FM')
        
        self.via1_edit.setEnabled(digipeater_supported)
        self.via2_edit.setEnabled(digipeater_supported)
        
        if digipeater_supported:
            # VARA FM : Activer avec tooltip informatif
            self.via1_edit.setToolTip("Premier digipeater (optionnel)")
            self.via2_edit.setToolTip("Deuxième digipeater (optionnel)")
            self.via1_edit.setPlaceholderText("Digipeater 1")
            self.via2_edit.setPlaceholderText("Digipeater 2")
        else:
            # VARA HF/SAT : Désactiver avec message explicite
            tooltip = f"Digipeaters non supportés par {self.vara.modem_type}\n(Uniquement VARA FM)"
            self.via1_edit.setToolTip(tooltip)
            self.via2_edit.setToolTip(tooltip)
            self.via1_edit.setPlaceholderText("N/A (VARA FM uniquement)")
            self.via2_edit.setPlaceholderText("N/A (VARA FM uniquement)")
            # Vider les champs
            self.via1_edit.clear()
            self.via2_edit.clear()
    
    def update_bandwidth_combo(self):
        """Met à jour la combo des bandwidths"""
        if not self.vara:
            return
        
        self.bw_combo.clear()
        
        # VARA FM et VARA SAT : pas de sélection BW (se configure dans VARA directement)
        if self.vara.modem_type in ['VARA FM', 'VARA SAT']:
            self.bw_combo.setEnabled(False)
            if self.vara.modem_type == 'VARA FM':
                self.bw_combo.setToolTip("Bande passante configurée dans VARA FM")
            else:
                self.bw_combo.setToolTip("Bande passante configurée dans VARA SAT")
            return  # Ne rien ajouter dans la combo
        
        # VARA HF uniquement : choix de la bande passante
        self.bw_combo.setEnabled(True)
        self.bw_combo.setToolTip("Sélectionner la bande passante")
        
        bandwidths = self.vara.config['bandwidths']
        for bw in bandwidths:
            # VARA HF : valeurs numériques en Hz
            self.bw_combo.addItem(f"{bw} Hz", bw)
        
        # Sélectionner le bandwidth par défaut
        default_bw = self.vara.config['default_bw']
        idx = self.bw_combo.findData(default_bw)
        if idx >= 0:
            self.bw_combo.setCurrentIndex(idx)
    
    def call_station(self):
        """Appel d'une station"""
        if not self.vara or not self.vara.connected:
            QMessageBox.warning(self, tr("dlg_error"), "Modem non connecté")
            return
        
        if self.vara.session_active:
            QMessageBox.warning(self, tr("dlg_error"), "Une session est déjà active")
            return
            
        if self.vara.pending_connect:
            QMessageBox.warning(self, tr("dlg_error"), "Un appel est déjà en cours")
            return
        
        mycall = self.settings.get('mycall', '').strip()
        if not mycall:
            QMessageBox.warning(self, tr("dlg_error"), "Veuillez configurer votre indicatif dans les paramètres")
            return
        
        callsign = self.station_edit.text().strip().upper()
        if not callsign:
            QMessageBox.warning(self, tr("dlg_error"), "Veuillez entrer un indicatif")
            return
        
        if callsign == mycall:
            QMessageBox.warning(self, tr("dlg_error"), "Vous ne pouvez pas vous appeler vous-même !")
            return
        
        # Récupérer les digipeaters (optionnels)
        via1 = self.via1_edit.text().strip().upper()
        via2 = self.via2_edit.text().strip().upper()
        
        if self.vara.channel_busy:
            reply = QMessageBox.question(
                self,
                "Canal occupé",
                "Le canal est occupé. Voulez-vous quand même tenter l'appel ?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.No:
                return
        
        bandwidth = self.bw_combo.currentData()
        
        # VARA FM ne supporte pas la commande BW (négocie automatiquement)
        # On passe None pour ne pas envoyer BW
        bw_for_connect = None if self.vara.modem_type == 'VARA FM' else bandwidth
        
        # Construire le message de log avec le chemin
        if via1 and via2:
            path = f"{callsign} via {via1} via {via2}"
        elif via1:
            path = f"{callsign} via {via1}"
        else:
            path = callsign
        
        self.vara.connect_to(callsign, bw_for_connect, via1, via2)
        
        # Message de log adapté selon le type de modem
        if self.vara.modem_type == 'VARA FM':
            self.log(f"{tr('calling')} {path}...", "info")
        else:
            self.log(f"{tr('calling')} {path} {tr('on')} {bandwidth} Hz...", "info")
        
        # Timer de timeout (30 secondes)
        QTimer.singleShot(30000, self.check_connection_timeout)
    
    def disconnect_session(self):
        """Déconnexion de la session"""
        if self.vara:
            self.vara.disconnect_session()
    
    def check_connection_timeout(self):
        """Vérifie si l'appel a timeout"""
        if self.vara and self.vara.pending_connect and not self.vara.session_active:
            self.log(f"{tr('connection_timeout')} - {tr('cancelling')}", "warning")
            self.vara.abort()
            self.vara.pending_connect = False
    
    def send_cq(self):
        """Envoyer un appel CQ"""
        if not self.vara or not self.vara.connected:
            QMessageBox.warning(self, tr("dlg_error"), "Modem non connecté")
            return
        
        if self.vara.session_active:
            QMessageBox.warning(self, tr("dlg_error"), "Impossible d'envoyer un CQ pendant une session active")
            return
        
        mycall = self.settings.get('mycall', '').strip().upper()
        if not mycall:
            QMessageBox.warning(self, tr("dlg_error"), "Veuillez configurer votre indicatif")
            return
        
        # Format CQFRAME selon le type de modem
        if self.vara.modem_type == 'VARA HF':
            # VARA HF: CQFRAME Source BW
            bandwidth = self.bw_combo.currentData()
            cmd = f"CQFRAME {mycall} {bandwidth}"
        
        elif self.vara.modem_type == 'VARA SAT':
            # VARA SAT: CQFRAME Source
            cmd = f"CQFRAME {mycall}"
        
        elif self.vara.modem_type == 'VARA FM':
            # VARA FM: CQFRAME Source Digi1 Digi2
            via1 = self.via1_edit.text().strip().upper()
            via2 = self.via2_edit.text().strip().upper()
            
            cmd = f"CQFRAME {mycall}"
            if via1:
                cmd += f" {via1}"
                if via2:
                    cmd += f" {via2}"
        else:
            self.log(f"Type de modem inconnu: {self.vara.modem_type}", "error")
            return
        
        # Envoyer la commande
        self.vara.send_command(cmd)
        self.log(f"📢 Appel CQ envoyé: {cmd}", "info")
    
    def add_received_cq(self, callsign, bandwidth=None, via1=None, via2=None):
        """Ajouter un CQ reçu au tableau"""
        from datetime import datetime
        
        # Vérifier si ce CQ existe déjà
        for cq in self.received_cqs:
            if cq['callsign'] == callsign:
                # Mettre à jour l'heure
                cq['time'] = datetime.now()
                cq['bandwidth'] = bandwidth
                cq['via1'] = via1
                cq['via2'] = via2
                self.update_cq_table()
                return
        
        # Ajouter le nouveau CQ
        cq_data = {
            'callsign': callsign,
            'bandwidth': bandwidth or '',
            'via1': via1 or '',
            'via2': via2 or '',
            'time': datetime.now()
        }
        self.received_cqs.append(cq_data)
        
        # Limiter à 20 CQ max
        if len(self.received_cqs) > 20:
            self.received_cqs.pop(0)
        
        self.update_cq_table()
        self.log(f"📡 CQ reçu de {callsign}", "info")
    
    def update_cq_table(self):
        """Mettre à jour le tableau des CQ"""
        self.cq_table.setRowCount(0)
        
        # Trier par heure décroissante (plus récent en haut)
        sorted_cqs = sorted(self.received_cqs, key=lambda x: x['time'], reverse=True)
        
        for cq in sorted_cqs:
            row = self.cq_table.rowCount()
            self.cq_table.insertRow(row)
            
            # Colonne 0: Indicatif
            self.cq_table.setItem(row, 0, QTableWidgetItem(cq['callsign']))
            
            # Colonne 1: Bandwidth
            bw_text = str(cq['bandwidth']) if cq['bandwidth'] else '-'
            self.cq_table.setItem(row, 1, QTableWidgetItem(bw_text))
            
            # Colonne 2: Via (digipeaters)
            via_parts = []
            if cq['via1']:
                via_parts.append(cq['via1'])
            if cq['via2']:
                via_parts.append(cq['via2'])
            via_text = ' '.join(via_parts) if via_parts else '-'
            self.cq_table.setItem(row, 2, QTableWidgetItem(via_text))
            
            # Colonne 3: Heure
            time_text = cq['time'].strftime("%H:%M:%S")
            self.cq_table.setItem(row, 3, QTableWidgetItem(time_text))
    
    def connect_to_cq_station(self):
        """Se connecter à une station qui a envoyé un CQ (double-clic)"""
        selected_rows = self.cq_table.selectedItems()
        if not selected_rows:
            return
        
        row = self.cq_table.currentRow()
        if row < 0:
            return
        
        # Récupérer les informations du CQ
        callsign = self.cq_table.item(row, 0).text()
        via_text = self.cq_table.item(row, 2).text()
        
        # Parser les digipeaters
        via1 = ''
        via2 = ''
        if via_text and via_text != '-':
            via_parts = via_text.split()
            if len(via_parts) >= 1:
                via1 = via_parts[0]
            if len(via_parts) >= 2:
                via2 = via_parts[1]
        
        # Demander confirmation
        via_info = f" via {via_text}" if via_text and via_text != '-' else ""
        reply = QMessageBox.question(
            self,
            "Connexion à une station CQ",
            f"Voulez-vous vous connecter à {callsign}{via_info} ?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            # Remplir les champs
            self.station_edit.setText(callsign)
            if via1:
                self.via1_edit.setText(via1)
            if via2:
                self.via2_edit.setText(via2)
            
            # Appeler
            self.call_station()
    
    def clear_cq_list(self):
        """Effacer la liste des CQ reçus"""
        if not self.received_cqs:
            return
        
        reply = QMessageBox.question(
            self,
            "Effacer les CQ",
            "Voulez-vous effacer la liste des CQ reçus ?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            self.received_cqs.clear()
            self.cq_table.setRowCount(0)
            self.log("🗑️ Liste des CQ effacée", "info")
    
    def toggle_crypto(self):
        """Activer/Désactiver le chiffrement"""
        if self.crypto_btn.isChecked():
            # Activer le chiffrement
            if not self.crypto.master_password:
                # Demander le mot de passe
                from PyQt6.QtWidgets import QInputDialog, QLineEdit
                
                password, ok = QInputDialog.getText(
                    self,
                    "Configuration du chiffrement",
                    "Entrez le mot de passe de chiffrement :\n"
                    "(Partagez ce mot de passe avec votre interlocuteur de manière sécurisée)",
                    QLineEdit.EchoMode.Password
                )
                
                if ok and password:
                    self.crypto.set_password(password)
                    self.crypto.enable()
                    self.crypto_btn.setChecked(True)
                    self.log(f"🔒 {tr('crypto_enabled')}", "success")
                    
                    # Si connecté à une station, proposer d'établir une session DH
                    if self.vara and self.vara.session_active:
                        reply = QMessageBox.question(
                            self,
                            "Perfect Forward Secrecy",
                            "Voulez-vous établir une session Diffie-Hellman ?\n\n"
                            "Cela améliore la sécurité avec Perfect Forward Secrecy (PFS).\n"
                            "L'autre station sera invitée à activer le chiffrement.",
                            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                        )
                        
                        if reply == QMessageBox.StandardButton.Yes:
                            self.initiate_crypto_request()
                        else:
                            QMessageBox.information(
                                self,
                                "Chiffrement activé",
                                "🔒 Chiffrement AES-256 activé\n\n"
                                "Mode : PBKDF2 (100k iterations) + HMAC-SHA256\n"
                                "Tous les messages envoyés seront chiffrés.\n"
                                "Votre interlocuteur doit utiliser le même mot de passe."
                            )
                    else:
                        QMessageBox.information(
                            self,
                            "Chiffrement activé",
                            "🔒 Chiffrement AES-256 activé\n\n"
                            "Mode : PBKDF2 (100k iterations) + HMAC-SHA256\n"
                            "Connectez-vous à une station pour établir une session sécurisée."
                        )
                else:
                    self.crypto_btn.setChecked(False)
            else:
                # Clé déjà configurée, juste activer (réactivation)
                self.crypto.enable()
                self.log(f"🔒 {tr('log_encryption_enabled')}", "success")
                
                # Notifier l'autre station de la réactivation
                if self.vara and self.vara.session_active:
                    self.vara.send_data("<CRYPTON></CRYPTON><EOF>")
                    self.log(f"📤 {tr('crypto_on_notification_sent')}", "info")
        else:
            # Désactiver le chiffrement
            self.crypto.disable()
            self.log(f"🔓 {tr('log_encryption_disabled')}", "info")
            
            # Informer l'autre station si connecté
            if self.vara and self.vara.session_active:
                self.vara.send_data("<CRYPTOFF></CRYPTOFF><EOF>")
                self.log(f"📤 {tr('crypto_off_notification_sent')}", "info")
    
    def initiate_crypto_request(self):
        """Envoyer une demande d'activation du chiffrement avec DH"""
        if not self.vara or not self.vara.session_active:
            QMessageBox.warning(self, tr("dlg_error"), "Vous devez être connecté à une station.")
            return
        
        self.log(f"🔑 {tr('crypto_sending_request')}...", "info")
        
        # Générer paire de clés DH
        my_public_key = self.crypto.generate_dh_keypair()
        
        # Envoyer demande d'activation avec clé publique
        # Format: <CRYPTREQ>clé_publique_base64</CRYPTREQ>
        crypto_request = f"<CRYPTREQ>{my_public_key}</CRYPTREQ>"
        self.vara.send_data(crypto_request + "<EOF>")
        
        self.log(f"🔑 {tr('crypto_request_sent')}...", "info")
    
    def initiate_dh_exchange(self):
        """Initier un échange Diffie-Hellman"""
        if not self.vara or not self.vara.session_active:
            QMessageBox.warning(self, tr("dlg_error"), "Vous devez être connecté à une station pour établir une session DH.")
            return
        
        self.log(f"🔑 {tr('crypto_generating_dh')}...", "info")
        
        # Générer paire de clés DH
        my_public_key = self.crypto.generate_dh_keypair()
        
        # Envoyer la clé publique
        dh_message = f"<DHKEY>{my_public_key}</DHKEY>"
        self.vara.send_data(dh_message + "<EOF>")
        
        self.log(f"🔑 {tr('crypto_dh_key_sent')} - {tr('crypto_waiting_peer_key')}...", "info")
        
        QMessageBox.information(
            self,
            tr("dlg_dh_exchange_initiated"),
            f"{tr('msg_dh_key_sent')}\n\n"
            f"{tr('msg_waiting_peer_key')}..."
        )
    
    def handle_crypto_request(self, peer_public_key):
        """Gérer la réception d'une demande d'activation du chiffrement"""
        self.log(f"🔑 {tr('crypto_request_received')}", "info")
        
        # Demander à l'utilisateur s'il veut activer le chiffrement
        reply = QMessageBox.question(
            self,
            tr("dlg_encryption_request"),
            f"{self.vara.remote_station} {tr('msg_wants_encrypted_session')}.\n\n"
            f"{tr('msg_activate_encryption')}\n\n"
            f"{tr('msg_same_password')}",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply != QMessageBox.StandardButton.Yes:
            self.log(f"❌ {tr('crypto_request_refused')}", "warning")
            # Informer l'autre station
            self.vara.send_data("<CRYPTREFUSED></CRYPTREFUSED><EOF>")
            return
        
        # Demander le mot de passe
        from PyQt6.QtWidgets import QInputDialog, QLineEdit
        
        password, ok = QInputDialog.getText(
            self,
            tr("dlg_encryption_password"),
            f"{tr('msg_enter_shared_password')} {self.vara.remote_station}:",
            QLineEdit.EchoMode.Password
        )
        
        if not ok or not password:
            self.log(f"❌ {tr('crypto_password_not_entered')}", "warning")
            self.vara.send_data("<CRYPTREFUSED></CRYPTREFUSED><EOF>")
            return
        
        # Configurer le chiffrement
        self.crypto.set_password(password)
        self.crypto.enable()
        self.crypto_btn.setChecked(True)
        self.log(f"🔒 {tr('log_encryption_enabled')}", "success")
        
        # Générer notre paire de clés DH si pas déjà fait
        if not self.crypto.dh_private_key:
            self.log(f"🔑 {tr('crypto_generating_our_dh')}...", "info")
            my_public_key = self.crypto.generate_dh_keypair()
        else:
            my_public_key = self.crypto.serialize_public_key(self.crypto.dh_public_key)
        
        # Envoyer notre clé publique en réponse
        crypto_response = f"<CRYPTACK>{my_public_key}</CRYPTACK>"
        self.vara.send_data(crypto_response + "<EOF>")
        self.log(f"🔑 {tr('crypto_response_sent_with_key')}", "info")
        
        # Calculer le secret partagé avec la clé reçue
        if self.crypto.compute_shared_secret(peer_public_key):
            self.log(f"✅ {tr('crypto_dh_session_established')} - {tr('crypto_pfs_enabled')}!", "success")
            QMessageBox.information(
                self,
                "Chiffrement activé",
                "✅ Session sécurisée établie !\n\n"
                "Chiffrement AES-256 + Perfect Forward Secrecy activés.\n"
                "Vos communications sont maintenant protégées."
            )
        else:
            self.log(f"❌ {tr('crypto_error_shared_secret')}", "error")
    
    def handle_crypto_ack(self, peer_public_key):
        """Gérer la réception d'un ACK de chiffrement avec clé publique"""
        self.log(f"🔑 {tr('crypto_response_received')}", "info")
        
        # Calculer le secret partagé
        if self.crypto.compute_shared_secret(peer_public_key):
            self.log(f"✅ {tr('crypto_dh_session_established')} - {tr('crypto_pfs_enabled')}!", "success")
            QMessageBox.information(
                self,
                "Session sécurisée établie",
                "✅ Échange Diffie-Hellman réussi !\n\n"
                "Perfect Forward Secrecy (PFS) activé.\n"
                "Les messages utilisent maintenant une clé de session unique.\n\n"
                "Sécurité maximale : même si le mot de passe est compromis,\n"
                "les anciens messages restent sécurisés."
            )
        else:
            self.log(f"❌ {tr('crypto_error_dh_session')}", "error")
            QMessageBox.warning(
                self,
                "Erreur DH",
                "Impossible d'établir la session Diffie-Hellman.\n"
                "Le chiffrement par mot de passe sera utilisé."
            )
    
    def handle_crypto_refused(self):
        """Gérer le refus du chiffrement"""
        self.log(f"❌ {tr('crypto_other_refused')}", "warning")
        QMessageBox.warning(
            self,
            "Chiffrement refusé",
            f"{self.vara.remote_station} a refusé d'activer le chiffrement.\n\n"
            "Vous pouvez continuer en mode non chiffré ou déconnecter."
        )
    
    def handle_crypto_off(self):
        """Gérer la désactivation du chiffrement par l'autre station"""
        self.log(f"🔓 {tr('crypto_other_disabled')}", "info")
        
        # Fenêtre d'information (pas une question)
        msg_box = QMessageBox(self)
        msg_box.setIcon(QMessageBox.Icon.Information)
        msg_box.setWindowTitle("Chiffrement désactivé par l'interlocuteur")
        msg_box.setText(
            f"📢 Information\n\n"
            f"{self.vara.remote_station or 'L\'interlocuteur'} a désactivé le chiffrement."
        )
        msg_box.setInformativeText(
            "Souhaitez-vous également désactiver le chiffrement ?\n\n"
            "• OUI : Le chiffrement sera désactivé des deux côtés\n"
            "• NON : Vous continuerez à chiffrer vos messages"
        )
        msg_box.setStandardButtons(
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        msg_box.setDefaultButton(QMessageBox.StandardButton.No)
        
        reply = msg_box.exec()
        
        if reply == QMessageBox.StandardButton.Yes:
            # L'utilisateur accepte de désactiver
            self.crypto.disable()
            self.crypto_btn.setChecked(False)
            self.log(f"🔓 {tr('log_encryption_disabled')}", "info")
            
            # Confirmer à l'autre station
            if self.vara and self.vara.session_active:
                self.vara.send_data("<CRYPTOFFACK></CRYPTOFFACK><EOF>")
                self.log(f"📤 {tr('crypto_off_ack_sent')}", "info")
        else:
            # L'utilisateur refuse de désactiver
            self.log(f"🔒 {tr('crypto_kept_active')}", "info")
            
            # Informer l'autre station du refus
            if self.vara and self.vara.session_active:
                self.vara.send_data("<CRYPTOFFREFUSED></CRYPTOFFREFUSED><EOF>")
                self.log(f"📤 {tr('crypto_off_refused_sent')}", "info")
            
            QMessageBox.information(
                self,
                "Chiffrement maintenu",
                f"Vous continuez à chiffrer vos messages.\n\n"
                f"{self.vara.remote_station or 'L\'autre station'} a été informé(e) "
                f"que vous souhaitez maintenir le chiffrement."
            )
    
    def handle_crypto_off_ack(self):
        """Gérer la confirmation de désactivation du chiffrement"""
        self.log(f"✅ {tr('crypto_other_also_disabled')}", "success")
        QMessageBox.information(
            self,
            "Synchronisation réussie",
            f"{self.vara.remote_station or 'L\'autre station'} a également désactivé le chiffrement.\n\n"
            f"Vous communiquez maintenant tous les deux en clair."
        )
    
    def handle_crypto_off_refused(self):
        """Gérer le refus de désactivation du chiffrement"""
        self.log(f"⚠️ {tr('crypto_other_wants_keep')}", "warning")
        
        msg_box = QMessageBox(self)
        msg_box.setIcon(QMessageBox.Icon.Warning)
        msg_box.setWindowTitle("Refus de désactivation")
        msg_box.setText(
            f"⚠️ Désynchronisation du chiffrement\n\n"
            f"{self.vara.remote_station or 'L\'autre station'} souhaite maintenir le chiffrement actif."
        )
        msg_box.setInformativeText(
            f"Situation actuelle :\n"
            f"• Vous : Chiffrement DÉSACTIVÉ\n"
            f"• {self.vara.remote_station or 'Interlocuteur'} : Chiffrement ACTIVÉ\n\n"
            f"Conséquences :\n"
            f"• Vos messages en clair seront reçus normalement\n"
            f"• Les messages chiffrés de l'interlocuteur ne seront pas déchiffrables\n\n"
            f"Voulez-vous réactiver le chiffrement ?"
        )
        msg_box.setStandardButtons(
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        msg_box.setDefaultButton(QMessageBox.StandardButton.Yes)
        
        reply = msg_box.exec()
        
        if reply == QMessageBox.StandardButton.Yes:
            # Réactiver le chiffrement
            if not self.crypto.master_password:
                # Demander le mot de passe
                from PyQt6.QtWidgets import QInputDialog, QLineEdit
                
                password, ok = QInputDialog.getText(
                    self,
                    "Mot de passe de chiffrement",
                    f"Entrez le mot de passe partagé avec {self.vara.remote_station or 'l\'interlocuteur'} :",
                    QLineEdit.EchoMode.Password
                )
                
                if ok and password:
                    self.crypto.set_password(password)
                    self.crypto.enable()
                    self.crypto_btn.setChecked(True)
                    self.log(f"🔒 {tr('crypto_reactivated')}", "success")
                else:
                    self.log(f"❌ {tr('reactivation_cancelled')}", "warning")
            else:
                self.crypto.enable()
                self.crypto_btn.setChecked(True)
                self.log(f"🔒 {tr('crypto_reactivated')}", "success")
        else:
            self.log(f"⚠️ {tr('continuing_without_encryption')}", "warning")
    
    def handle_crypto_on(self):
        """Gérer la notification de réactivation du chiffrement par l'autre station"""
        self.log(f"🔒 {tr('crypto_other_reactivated')}", "info")
        
        msg_box = QMessageBox(self)
        msg_box.setIcon(QMessageBox.Icon.Information)
        msg_box.setWindowTitle("Chiffrement réactivé")
        msg_box.setText(
            f"📢 Information\n\n"
            f"{self.vara.remote_station or 'L\'interlocuteur'} a réactivé le chiffrement."
        )
        
        # Vérifier si on a déjà le chiffrement activé
        if self.crypto.enabled:
            msg_box.setInformativeText(
                "Votre chiffrement est déjà activé.\n\n"
                "Vous communiquez tous les deux de manière chiffrée."
            )
            msg_box.setStandardButtons(QMessageBox.StandardButton.Ok)
        else:
            msg_box.setInformativeText(
                "Votre chiffrement est actuellement désactivé.\n\n"
                "Souhaitez-vous également réactiver le chiffrement ?\n\n"
                "• OUI : Chiffrement activé, communication sécurisée\n"
                "• NON : Vous restez en clair (désynchronisé)"
            )
            msg_box.setStandardButtons(
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            msg_box.setDefaultButton(QMessageBox.StandardButton.Yes)
        
        reply = msg_box.exec()
        
        if not self.crypto.enabled and reply == QMessageBox.StandardButton.Yes:
            # Réactiver le chiffrement
            if self.crypto.master_password:
                self.crypto.enable()
                self.crypto_btn.setChecked(True)
                self.log(f"🔒 {tr('crypto_reactivated')}", "success")
                
                # Confirmer à l'autre station
                if self.vara and self.vara.session_active:
                    self.vara.send_data("<CRYPTONACK></CRYPTONACK><EOF>")
                    self.log(f"📤 {tr('crypto_on_ack_sent')}", "info")
            else:
                # Demander le mot de passe
                from PyQt6.QtWidgets import QInputDialog, QLineEdit
                
                password, ok = QInputDialog.getText(
                    self,
                    "Mot de passe de chiffrement",
                    f"Entrez le mot de passe partagé avec {self.vara.remote_station or 'l\'interlocuteur'} :",
                    QLineEdit.EchoMode.Password
                )
                
                if ok and password:
                    self.crypto.set_password(password)
                    self.crypto.enable()
                    self.crypto_btn.setChecked(True)
                    self.log(f"🔒 {tr('log_encryption_enabled')}", "success")
                    
                    # Confirmer à l'autre station
                    if self.vara and self.vara.session_active:
                        self.vara.send_data("<CRYPTONACK></CRYPTONACK><EOF>")
                        self.log(f"📤 {tr('crypto_on_ack_sent')}", "info")
    
    def handle_crypto_on_ack(self):
        """Gérer la confirmation de réactivation du chiffrement"""
        self.log(f"✅ {tr('crypto_other_also_reactivated')}", "success")
        QMessageBox.information(
            self,
            "Synchronisation réussie",
            f"{self.vara.remote_station or 'L\'autre station'} a également réactivé le chiffrement.\n\n"
            f"Vous communiquez maintenant tous les deux de manière chiffrée."
        )
    
    def handle_dh_key(self, peer_public_key):
        """Gérer la réception d'une clé publique DH (ancien protocole - rétrocompatibilité)"""
        self.log(f"🔑 {tr('dh_key_received_old_format')}", "info")
        
        # Si on n'a pas encore de clés, en générer
        if not self.crypto.dh_private_key:
            self.log(f"🔑 {tr('generating_our_key_pair')}...", "info")
            my_public_key = self.crypto.generate_dh_keypair()
            
            # Envoyer notre clé publique SEULEMENT si on n'en a pas déjà envoyé
            dh_message = f"<DHKEY>{my_public_key}</DHKEY>"
            self.vara.send_data(dh_message + "<EOF>")
            self.log(f"🔑 {tr('dh_key_sent_response')}", "info")
        
        # Calculer le secret partagé
        if self.crypto.compute_shared_secret(peer_public_key):
            self.log(f"✅ {tr('dh_session_established')} - {tr('crypto_pfs_enabled')}!", "success")
            
            QMessageBox.information(
                self,
                "Session sécurisée établie",
                "✅ Échange Diffie-Hellman réussi !\n\n"
                "Perfect Forward Secrecy (PFS) activé.\n"
                "Les messages utilisent maintenant une clé de session unique.\n\n"
                "Sécurité maximale : même si le mot de passe est compromis,\n"
                "les anciens messages restent sécurisés."
            )
        else:
            self.log(f"❌ {tr('crypto_error_dh_session')}", "error")
            QMessageBox.warning(
                self,
                "Erreur DH",
                "Impossible d'établir la session Diffie-Hellman.\n"
                "Le chiffrement par mot de passe sera utilisé."
            )
    
    def change_crypto_password(self):
        """Changer le mot de passe de chiffrement"""
        from PyQt6.QtWidgets import QInputDialog, QLineEdit
        
        password, ok = QInputDialog.getText(
            self,
            "Nouveau mot de passe",
            "Entrez le nouveau mot de passe de chiffrement :",
            QLineEdit.EchoMode.Password
        )
        
        if ok and password:
            self.crypto.set_password(password)
            if self.crypto_btn.isChecked():
                self.crypto.enable()
            self.log(f"🔑 {tr('encryption_password_changed')}", "success")
            QMessageBox.information(
                self,
                "Mot de passe modifié",
                "Le mot de passe de chiffrement a été modifié.\n\n"
                "N'oubliez pas de le partager avec votre interlocuteur !"
            )

    def send_message(self):
        """Envoi d'un message"""
        if not self.vara or not self.vara.session_active:
            return
        
        message = self.msg_input.text().strip()
        if not message:
            return
        
        # Chiffrer le message si activé
        message_to_send = self.crypto.encrypt(message) if self.crypto.enabled else message
        
        if self.vara.send_data(message_to_send + "<EOF>"):
            # Stocker le message en attente d'ACK (sera affiché quand BUFFER=0)
            timestamp = datetime.now().strftime("%H:%M:%S")
            mycall = self.settings.get('mycall', 'Moi')
            
            # Stocker pour affichage après ACK
            if self.crypto.enabled:
                pending_msg = f"[{timestamp}] {mycall} 🔒: {message}"
            else:
                pending_msg = f"[{timestamp}] {mycall}: {message}"
            
            # Créer une file d'attente si elle n'existe pas
            if not hasattr(self, 'pending_messages'):
                self.pending_messages = []
            
            self.pending_messages.append(pending_msg)
            self.msg_input.clear()
    
    def send_file(self):
        """Envoi d'un fichier"""
        if not self.vara or not self.vara.session_active:
            return
        
        # Sélection du fichier
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Sélectionner un fichier à envoyer",
            "",
            "Tous les fichiers (*.*)"
        )
        
        if not file_path:
            return
        
        try:
            # Lire le fichier
            with open(file_path, 'rb') as f:
                file_data = f.read()
            
            file_name = os.path.basename(file_path)
            file_size_original = len(file_data)
            
            # Limite de taille (2 MB - pour éviter les timeouts)
            if file_size_original > 2 * 1024 * 1024:
                QMessageBox.warning(
                    self,
                    "Fichier trop volumineux",
                    f"La taille maximale est de 2 MB.\n"
                    f"Taille du fichier : {file_size_original / 1024 / 1024:.2f} MB\n\n"
                    f"💡 Conseil : Compressez le fichier en ZIP avant l'envoi\n"
                    f"pour réduire sa taille."
                )
                return
            
            # Compresser les données avec zlib (niveau 9 = max compression)
            compressed_data = zlib.compress(file_data, level=9)
            file_size_compressed = len(compressed_data)
            
            # Calculer le taux de compression
            compression_ratio = (1 - file_size_compressed / file_size_original) * 100
            
            self.log(
                f"Compression : {file_size_original} → {file_size_compressed} octets "
                f"({compression_ratio:.1f}% de réduction)",
                "info"
            )
            
            # Avertissement pour les gros fichiers
            if file_size_compressed > 500 * 1024:  # > 500 KB compressé
                estimated_time_narrow = int(file_size_compressed / 400)  # ~400 o/s en NARROW
                estimated_time_wide = int(file_size_compressed / 1200)   # ~1200 o/s en WIDE
                
                reply = QMessageBox.question(
                    self,
                    "Fichier volumineux",
                    f"Le fichier compressé fait {file_size_compressed / 1024:.0f} KB.\n\n"
                    f"Temps d'envoi estimé :\n"
                    f"• NARROW : ~{estimated_time_narrow // 60} min {estimated_time_narrow % 60} sec\n"
                    f"• WIDE : ~{estimated_time_wide // 60} min {estimated_time_wide % 60} sec\n\n"
                    f"Voulez-vous continuer ?",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                )
                
                if reply != QMessageBox.StandardButton.Yes:
                    self.log(f"{tr('send_cancelled_by_user')}", "warning")
                    return
            
            # Encoder en base64
            encoded_data = base64.b64encode(compressed_data).decode('ascii')
            
            # Chiffrer les données du fichier si le chiffrement est activé
            if self.crypto.enabled:
                self.log(f"🔒 {tr('encrypting_file')}...", "info")
                # On chiffre les données compressées+base64
                # Cela permet de déchiffrer même si la session change
                encrypted_data = self.crypto.encrypt(encoded_data)
                # Enlever les balises <ENC> car on va les mettre dans FILEDATA
                if encrypted_data.startswith("<ENC>") and encrypted_data.endswith("</ENC>"):
                    encoded_data = encrypted_data[5:-6]  # Juste le base64
                    file_encrypted = True
                else:
                    file_encrypted = False
            else:
                file_encrypted = False
            
            # Encoder le nom de fichier en base64 pour supporter les caractères spéciaux (ç, é, etc.)
            encoded_filename = base64.b64encode(file_name.encode('utf-8')).decode('ascii')
            
            # PHASE 1 : Envoyer les métadonnées d'abord
            # Format: <FILEINFO>chiffré|nom_encodé|taille_originale|taille_compressée|taille_transfert<EOF>
            encrypted_flag = "1" if file_encrypted else "0"
            transfer_data_msg = f"<FILEDATA>{encrypted_flag}|{encoded_filename}|{file_size_original}|{file_size_compressed}|{encoded_data}<EOF>"
            transfer_size = len(transfer_data_msg)
            
            fileinfo_msg = f"<FILEINFO>{encrypted_flag}|{encoded_filename}|{file_size_original}|{file_size_compressed}|{transfer_size}<EOF>"
            
            if file_encrypted:
                self.log(f"🔒 {tr('sending_encrypted_file')}: {file_name}", "info")
            else:
                self.log(f"{tr('sending_metadata')}: {file_name}", "info")
            self.vara.send_data(fileinfo_msg)
            
            # Attendre un peu que les métadonnées arrivent
            time.sleep(0.5)
            
            # PHASE 2 : Envoyer le fichier
            # Format: <FILEDATA>nom_encodé|taille_originale|taille_compressée|données_base64<EOF>
            transfer_msg = transfer_data_msg
            
            # Afficher la barre de progression intégrée
            self.file_progress_bar.setVisible(True)
            self.file_progress_bar.setMaximum(len(transfer_msg))
            self.file_progress_bar.setValue(0)
            self.file_progress_label.setText(f"📤 {tr('sending')} {file_name}...")
            self.file_progress_label.setStyleSheet("color: blue; font-weight: bold;")
            # NOTE: Pas de bouton d'annulation pour l'envoi car le fichier est déjà dans le buffer VARA
            
            # Variables pour le transfert
            self.file_transfer_active = True
            self.file_transfer_total = len(transfer_msg)
            self.file_transfer_progress = 0
            
            self.log(
                f"Envoi du fichier : {file_name} ({file_size_original} octets, "
                f"compressé : {file_size_compressed} octets)",
                "info"
            )
            
            # Envoyer TOUT le fichier d'un coup dans le buffer VARA
            # MAIS en chunks pour éviter de saturer le buffer TCP et causer des timeouts
            transfer_bytes = transfer_msg.encode('utf-8')
            total_bytes = len(transfer_bytes)
            chunk_size = 8192  # 8 KB par chunk
            bytes_sent = 0
            
            self.log(f"{tr('sending_file_in_chunks')} ({total_bytes} {tr('bytes')})", "info")
            
            # Envoyer par chunks
            for i in range(0, total_bytes, chunk_size):
                # Traiter les événements Qt pour garder l'interface réactive
                QApplication.processEvents()
                
                chunk = transfer_bytes[i:i+chunk_size]
                self.vara.data_socket.send(chunk)
                bytes_sent += len(chunk)
                
                # Petite pause pour ne pas saturer
                time.sleep(0.05)
                
                # Mise à jour de l'interface
                if bytes_sent % (chunk_size * 10) == 0:  # Toutes les 10 chunks
                    QApplication.processEvents()
            
            self.log(f"{tr('data_sent_to_buffer')} ({total_bytes} {tr('bytes')})", "info")
            
            # Maintenant surveiller le buffer VARA pour la vraie progression
            start_time = time.time()
            last_progress = 0
            
            while self.file_transfer_active:
                # Traiter les événements Qt
                QApplication.processEvents()
                
                # Mettre à jour la barre avec la progression réelle
                current_progress = self.file_transfer_progress
                
                if current_progress != last_progress:
                    bytes_done = int(self.file_transfer_total * current_progress / 100)
                    self.file_progress_bar.setValue(bytes_done)
                    
                    # Calculer vitesse (pas de temps restant car VARA change de mode dynamiquement)
                    elapsed = time.time() - start_time
                    if elapsed > 1 and current_progress > 0:
                        speed = bytes_done / elapsed
                        
                        self.file_progress_label.setText(
                            f"📤 Envoi de {file_name}: {current_progress}% "
                            f"({speed:.0f} o/s)"
                        )
                    
                    last_progress = current_progress
                
                # Si on a atteint 100%, c'est terminé
                if current_progress >= 100:
                    self.file_transfer_active = False
                    break
                
                # Mettre à jour l'interface
                QApplication.processEvents()
                
                # Petite pause
                time.sleep(0.1)
            
            self.file_transfer_active = False
            self.file_progress_bar.setValue(self.file_transfer_total)
            self.file_progress_label.setText(f"✅ {tr('file_sent')}: {file_name}")
            self.file_progress_label.setStyleSheet("color: green; font-weight: bold;")
            
            # Masquer après 3 secondes
            QTimer.singleShot(3000, lambda: self.file_progress_bar.setVisible(False))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setText(tr("file_no_transfer")))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setStyleSheet("color: gray; font-style: italic;"))
            
            # Afficher dans le chat
            timestamp = datetime.now().strftime("%H:%M:%S")
            mycall = self.settings.get('mycall', 'Moi')
            self.append_chat(
                f"[{timestamp}] {mycall}: 📎 Fichier envoyé : {file_name} "
                f"({file_size_original} octets, compressé à {compression_ratio:.1f}%)",
                "tx"
            )
            
            self.log(f"✓ {tr('file_sent')}: {file_name}", "success")
            
        except Exception as e:
            QMessageBox.critical(
                self,
                tr("dlg_error"),
                f"Impossible d'envoyer le fichier :\n{e}"
            )
            self.log(f"{tr('error_sending_file')}: {e}", "error")


    def handle_command(self, cmd):
        """Traitement des commandes VARA"""
        self.log(f"CMD: {cmd}", "debug")
        
        if cmd.startswith("PTT"):
            state = "ON" in cmd
            self.vara.signals.ptt_status.emit(state)
            
        elif cmd.startswith("PENDING"):
            parts = cmd.split()
            if len(parts) > 1:
                self.vara.pending_connect = True
                self.log(f"{tr('connecting_to')} {parts[1]}...", "info")
                
        elif cmd.startswith("CANCELPENDING"):
            self.vara.pending_connect = False
            self.log(f"{tr('call_cancelled')}", "warning")
                
        elif cmd.startswith("CONNECTED"):
            parts = cmd.split()
            if len(parts) >= 3:
                # Format: CONNECTED Source Destination BW
                source = parts[1]
                destination = parts[2]
                bandwidth = parts[3] if len(parts) > 3 else "?"
                
                # Déterminer qui est la station distante
                # Si on a appelé, la destination est la station distante
                # Si on a été appelé, la source est la station distante
                mycall = self.settings.get('mycall', '').upper()
                
                if source.upper() == mycall:
                    # On est la source (on a appelé) → remote = destination
                    remote = destination
                elif destination.upper() == mycall:
                    # On est la destination (on a été appelé) → remote = source
                    remote = source
                else:
                    # Fallback : prendre le premier qui n'est pas nous
                    remote = source if source.upper() != mycall else destination
                
                self.vara.remote_station = remote
                self.vara.session_active = True
                self.vara.pending_connect = False
                self.log(f"{tr('connected_to')} {remote}, {tr('bandwidth')}: {bandwidth}", "info")
                self.vara.signals.connected_to.emit(remote)
            elif len(parts) > 1:
                # Fallback pour ancien format (sans BW)
                callsign = parts[1]
                self.vara.remote_station = callsign
                self.vara.session_active = True
                self.vara.pending_connect = False
                self.vara.signals.connected_to.emit(callsign)
                
        elif cmd.startswith("DISCONNECTED"):
            self.vara.session_active = False
            self.vara.pending_connect = False
            remote = self.vara.remote_station
            self.vara.remote_station = None
            if remote:
                self.vara.signals.disconnected_from.emit(remote)
        
        elif cmd.startswith("BUSY"):
            self.vara.channel_busy = "TRUE" in cmd or "ON" in cmd
            state = "occupé" if self.vara.channel_busy else "libre"
            self.log(f"{tr('channel')} {state}", "info")
        
        elif cmd.startswith("CQFRAME"):
            # Format VARA selon la doc:
            # VARA HF:  CQFRAME Source BW
            # VARA SAT: CQFRAME Source
            # VARA FM:  CQFRAME Source Digi1 Digi2
            parts = cmd.split()
            
            if len(parts) >= 2:
                callsign = parts[1]  # Source
                bandwidth = None
                via1 = None
                via2 = None
                
                # Parser selon le nombre d'arguments
                if len(parts) >= 3:
                    # Peut être BW (HF) ou Digi1 (FM)
                    # Si c'est un nombre, c'est un BW
                    if parts[2].isdigit() or parts[2] in ['NARROW', 'WIDE']:
                        bandwidth = parts[2]
                    else:
                        # C'est un digipeater (FM)
                        via1 = parts[2]
                        if len(parts) >= 4:
                            via2 = parts[3]
                
                # Ajouter au tableau CQ
                self.add_received_cq(callsign, bandwidth, via1, via2)
                
        elif cmd.startswith("VERSION"):
            version = cmd.replace("VERSION", "").strip()
            self.log(f"{tr('vara_version')}: {version}", "info")
        
        elif cmd.startswith("BUFFER"):
            # BUFFER indique combien d'octets restent à transmettre
            try:
                parts = cmd.split()
                if len(parts) >= 2:
                    bytes_in_buffer = int(parts[1])
                    
                    # Si buffer vide = transmission terminée et ACK reçu
                    if bytes_in_buffer == 0:
                        # Afficher les messages en attente (ils ont été ACK par le destinataire)
                        if hasattr(self, 'pending_messages') and self.pending_messages:
                            for msg in self.pending_messages:
                                self.append_chat(msg, "tx")
                            self.pending_messages.clear()
                        
                        # Si déconnexion auto en attente, déconnecter maintenant
                        if hasattr(self, 'auto_disconnect_pending') and self.auto_disconnect_pending:
                            print("[DEBUG] Auto-déconnexion: BUFFER=0 détecté, déconnexion maintenant")
                            self.auto_disconnect_pending = False
                            # Petit délai pour laisser VARA finaliser
                            QTimer.singleShot(1000, self._auto_disconnect_delayed)
                    
                    # Si on est en train de transférer un fichier, mettre à jour la progression
                    if hasattr(self, 'file_transfer_active') and self.file_transfer_active:
                        if hasattr(self, 'file_transfer_total') and self.file_transfer_total > 0:
                            # Calculer combien on a vraiment transmis (par radio)
                            bytes_transmitted = self.file_transfer_total - bytes_in_buffer
                            percent = int((bytes_transmitted / self.file_transfer_total) * 100)
                            percent = max(0, min(100, percent))  # Entre 0 et 100
                            
                            # Émettre un signal pour mettre à jour la barre
                            if hasattr(self, 'file_transfer_progress'):
                                self.file_transfer_progress = percent
                                
            except (ValueError, IndexError):
                pass
            
        elif cmd.startswith("IAMALIVE"):
            # Heartbeat de VARA - répondre si nécessaire
            pass
        
        elif cmd.startswith("SN "):
            # Rapport Signal/Noise envoyé par VARA en mode CHAT ON
            # Format: SN -10 (valeur en dB)
            # Envoyé pour chaque trame reçue (uniquement si CHAT ON actif)
            try:
                parts = cmd.split()
                if len(parts) >= 2:
                    sn_value = parts[1]
                    # On pourrait l'afficher dans l'interface
                    # Pour l'instant on le log juste
                    print(f"[DEBUG] S/N: {sn_value} dB")
            except:
                pass
            
        elif "WRONG" in cmd or "ERROR" in cmd or "FAIL" in cmd:
            self.log(f"{tr('vara_error')}: {cmd}", "error")
            self.vara.pending_connect = False
    
    def handle_data(self, data):
        """Traitement des données reçues"""
        try:
            message = data.decode('utf-8', errors='ignore')
            
            # Ajouter au buffer
            self.rx_buffer += message
            
            # Si on est en train de recevoir un fichier, mettre à jour la progression
            if self.file_rx_active and self.file_rx_total > 0:
                # Estimer la progression basée sur la taille du buffer
                self.file_rx_received = len(self.rx_buffer)
                percent = min(99, int((self.file_rx_received / self.file_rx_total) * 100))
                
                self.file_progress_bar.setValue(self.file_rx_received)
                
                # Afficher juste le pourcentage et la vitesse
                elapsed = time.time() - self.file_rx_start_time
                if elapsed > 1:
                    speed = self.file_rx_received / elapsed
                    self.file_progress_label.setText(
                        f"📥 Réception de {self.file_rx_name}: {percent}% ({speed:.0f} o/s)"
                    )
                else:
                    self.file_progress_label.setText(
                        f"📥 Réception de {self.file_rx_name}: {percent}%"
                    )
            
            # Vérifier si on a reçu le marqueur de fin <EOF>
            if '<EOF>' in self.rx_buffer:
                # Extraire le message (sans le <EOF>)
                msg, remaining = self.rx_buffer.split('<EOF>', 1)
                
                # Log pour debug
                if msg.startswith('<CRYPT'):
                    print(f"[DEBUG] Message crypto reçu: '{msg}'")
                if msg.startswith('<BBS'):
                    print(f"[DEBUG] Message BBS reçu: '{msg}'")
                
                # Vérifier le type de message
                if msg.startswith('<CRYPTREQ>') and msg.endswith('</CRYPTREQ>'):
                    # Demande d'activation du chiffrement avec clé publique
                    peer_public_key = msg[10:-11]  # Enlever <CRYPTREQ> et </CRYPTREQ>
                    self.handle_crypto_request(peer_public_key)
                elif msg.startswith('<CRYPTACK>') and msg.endswith('</CRYPTACK>'):
                    # Réponse d'activation du chiffrement avec clé publique
                    peer_public_key = msg[10:-11]  # Enlever <CRYPTACK> et </CRYPTACK>
                    self.handle_crypto_ack(peer_public_key)
                elif msg.startswith('<CRYPTREFUSED>') and msg.endswith('</CRYPTREFUSED>'):
                    # Refus du chiffrement
                    self.handle_crypto_refused()
                elif msg.startswith('<CRYPTOFF>') and msg.endswith('</CRYPTOFF>'):
                    # Désactivation du chiffrement
                    self.handle_crypto_off()
                elif msg.startswith('<CRYPTOFFACK>') and msg.endswith('</CRYPTOFFACK>'):
                    # Confirmation de désactivation du chiffrement
                    self.handle_crypto_off_ack()
                elif msg.startswith('<CRYPTOFFREFUSED>') and msg.endswith('</CRYPTOFFREFUSED>'):
                    # Refus de désactivation du chiffrement
                    self.handle_crypto_off_refused()
                elif msg.startswith('<CRYPTON>') and msg.endswith('</CRYPTON>'):
                    # Réactivation du chiffrement
                    self.handle_crypto_on()
                elif msg.startswith('<CRYPTONACK>') and msg.endswith('</CRYPTONACK>'):
                    # Confirmation de réactivation du chiffrement
                    self.handle_crypto_on_ack()
                elif msg.startswith('<DHKEY>') and msg.endswith('</DHKEY>'):
                    # Clé publique Diffie-Hellman (ancien protocole - rétrocompatibilité)
                    peer_public_key = msg[7:-8]  # Enlever <DHKEY> et </DHKEY>
                    self.handle_dh_key(peer_public_key)
                elif msg == '<BBSON>':
                    # La station distante a un BBS actif
                    self.handle_bbs_on()
                elif msg == '<BBSLIST>':
                    # Demande de la liste des fichiers BBS
                    self.handle_bbs_list_request()
                elif msg.startswith('<BBSFILES>') and msg.endswith('</BBSFILES>'):
                    # Réception de la liste des fichiers BBS
                    file_list_json = msg[10:-11]  # Enlever <BBSFILES> et </BBSFILES>
                    self.handle_bbs_files(file_list_json)
                elif msg.startswith('<BBSGET>') and msg.endswith('</BBSGET>'):
                    # Demande de téléchargement d'un fichier BBS
                    filename = msg[8:-9]  # Enlever <BBSGET> et </BBSGET>
                    self.handle_bbs_get_request(filename)
                elif msg.startswith('<FILEINFO>'):
                    # Métadonnées de fichier entrant
                    self.receive_file_info(msg[10:])  # Enlever <FILEINFO>
                elif msg.startswith('<FILEDATA>'):
                    # Données du fichier
                    self.receive_file_data(msg[10:])  # Enlever <FILEDATA>
                elif msg.startswith('<FILE>'):
                    # Ancien format (rétro-compatibilité)
                    self.receive_file(msg[6:])  # Enlever <FILE>
                elif msg:
                    # Message texte normal
                    # Déchiffrer si c'est un message chiffré
                    decrypted_msg = self.crypto.decrypt(msg)
                    
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    remote = self.vara.remote_station or "Station"
                    
                    # Afficher avec indicateur si message chiffré
                    if msg.startswith("<ENC>") and msg.endswith("</ENC>"):
                        if decrypted_msg != msg and not decrypted_msg.startswith("[Message chiffré"):
                            # Déchiffrement réussi
                            self.append_chat(f"[{timestamp}] {remote} 🔒: {decrypted_msg}", "rx")
                        else:
                            # Déchiffrement échoué
                            self.append_chat(f"[{timestamp}] {remote} 🔒: {decrypted_msg}", "rx")
                    else:
                        # Message non chiffré
                        self.append_chat(f"[{timestamp}] {remote}: {decrypted_msg}", "rx")
                
                # Garder le reste dans le buffer
                self.rx_buffer = remaining
                
        except Exception as e:
            self.log(f"{tr('error_decoding_data')}: {e}", "error")
    
    
    
    def receive_file_info(self, info_str):
        """Réception des métadonnées du fichier (Phase 1)"""
        try:
            # Parser: chiffré|nom_encodé|taille_originale|taille_compressée|taille_transfert
            parts = info_str.split('|', 4)
            
            # Compatibilité ancien format (sans flag chiffré)
            if len(parts) == 4:
                encrypted_flag = "0"
                encoded_filename, file_size_original_str, file_size_compressed_str, transfer_size_str = parts
            elif len(parts) == 5:
                encrypted_flag, encoded_filename, file_size_original_str, file_size_compressed_str, transfer_size_str = parts
            else:
                self.log(f"{tr('invalid_fileinfo_format')}", "error")
                return
            
            # Décoder le nom de fichier
            file_name = base64.b64decode(encoded_filename).decode('utf-8')
            file_size_original = int(file_size_original_str)
            file_size_compressed = int(file_size_compressed_str)
            transfer_size = int(transfer_size_str)
            file_encrypted = (encrypted_flag == "1")
            
            # Préparer la réception
            self.file_rx_active = True
            self.file_rx_name = file_name
            self.file_rx_total = transfer_size
            self.file_rx_received = 0
            self.file_rx_start_time = time.time()
            self.file_rx_encrypted = file_encrypted
            
            # Afficher la barre de progression
            self.file_progress_bar.setVisible(True)
            self.file_progress_bar.setMaximum(transfer_size)
            self.file_progress_bar.setValue(0)
            
            if file_encrypted:
                self.file_progress_label.setText(f"📥 🔒 {tr('receiving')} {file_name} ({tr('encrypted')})...")
                self.log(f"🔒 {tr('receiving_encrypted_file')}: {file_name}", "info")
            else:
                self.file_progress_label.setText(f"📥 {tr('receiving')} {file_name}...")
            
            self.file_progress_label.setStyleSheet("color: blue; font-weight: bold;")
            
            self.log(
                f"Réception de {file_name} : {file_size_original} octets "
                f"(compressé : {file_size_compressed}, transfert : {transfer_size})",
                "info"
            )
            
        except Exception as e:
            self.log(f"{tr('fileinfo_error')}: {e}", "error")
    
    def receive_file_data(self, data_str):
        """Réception des données du fichier (Phase 2)"""
        try:
            # Parser: chiffré|nom_encodé|taille_originale|taille_compressée|données_base64
            parts = data_str.split('|', 4)
            
            # Compatibilité ancien format
            if len(parts) == 4:
                encrypted_flag = "0"
                encoded_filename, file_size_original_str, file_size_compressed_str, encoded_data = parts
            elif len(parts) == 5:
                encrypted_flag, encoded_filename, file_size_original_str, file_size_compressed_str, encoded_data = parts
            else:
                self.log(f"{tr('invalid_filedata_format')}", "error")
                self.file_rx_active = False
                self.file_progress_bar.setVisible(False)
                return
            
            file_size_original = int(file_size_original_str)
            file_size_compressed = int(file_size_compressed_str)
            file_encrypted = (encrypted_flag == "1")
            
            # Décoder le nom de fichier
            file_name = base64.b64decode(encoded_filename).decode('utf-8')
            
            # Déchiffrer si nécessaire
            if file_encrypted:
                QApplication.processEvents()
                
                self.file_progress_bar.setValue(int(self.file_rx_total * 0.2))
                self.file_progress_label.setText(f"📥 🔒 {tr('decrypting')} {file_name}...")
                QApplication.processEvents()
                
                # Reconstituer le format <ENC>...</ENC>
                encrypted_msg = f"<ENC>{encoded_data}</ENC>"
                decrypted_data = self.crypto.decrypt(encrypted_msg)
                
                # Vérifier si le déchiffrement a réussi
                if decrypted_data.startswith("[Message chiffré"):
                    self.log(f"❌ {tr('cannot_decrypt_file')}: {decrypted_data}", "error")
                    self.file_rx_active = False
                    self.file_progress_bar.setVisible(False)
                    QMessageBox.critical(
                        self,
                        "Erreur de déchiffrement",
                        f"Impossible de déchiffrer le fichier {file_name}.\n\n"
                        f"Vérifiez que vous avez le même mot de passe que l'expéditeur."
                    )
                    return
                
                # Le résultat déchiffré est le base64 des données compressées
                encoded_data = decrypted_data
                self.log(f"✅ {tr('file_decrypted')}", "success")
            
            # Mettre à jour la progression (décodage)
            QApplication.processEvents()
            
            self.file_progress_bar.setValue(int(self.file_rx_total * 0.3))
            self.file_progress_label.setText(f"📥 {tr('decoding')} {file_name}...")
            QApplication.processEvents()
            
            # Décoder les données base64
            compressed_data = base64.b64decode(encoded_data)
            
            # Vérifier la taille compressée
            if len(compressed_data) != file_size_compressed:
                self.log(
                    f"Taille compressée incorrecte : reçu {len(compressed_data)}, "
                    f"attendu {file_size_compressed}",
                    "error"
                )
                self.file_rx_active = False
                self.file_progress_bar.setVisible(False)
                return
            
            QApplication.processEvents()
            
            self.file_progress_bar.setValue(int(self.file_rx_total * 0.6))
            self.file_progress_label.setText(f"📥 {tr('decompressing')} {file_name}...")
            QApplication.processEvents()
            
            # Décompresser
            try:
                file_data = zlib.decompress(compressed_data)
            except zlib.error as e:
                self.log(f"{tr('decompression_error')}: {e}", "error")
                self.file_rx_active = False
                self.file_progress_bar.setVisible(False)
                return
            
            # Vérifier la taille décompressée
            if len(file_data) != file_size_original:
                self.log(
                    f"Taille décompressée incorrecte : obtenu {len(file_data)}, "
                    f"attendu {file_size_original}",
                    "error"
                )
                self.file_rx_active = False
                self.file_progress_bar.setVisible(False)
                return
            
            # Calculer le taux de compression
            compression_ratio = (1 - file_size_compressed / file_size_original) * 100
            
            self.log(
                f"Décompression : {file_size_compressed} → {file_size_original} octets "
                f"({compression_ratio:.1f}% de réduction)",
                "info"
            )
            
            QApplication.processEvents()
            
            self.file_progress_bar.setValue(int(self.file_rx_total * 0.9))
            self.file_progress_label.setText(f"📥 {tr('saving')}...")
            QApplication.processEvents()
            
            # Vérifier si un chemin de sauvegarde est configuré
            file_rx_path = self.settings.get('file_rx_path', '').strip()
            
            if file_rx_path and os.path.isdir(file_rx_path):
                # Sauvegarder automatiquement dans le dossier configuré
                save_path = os.path.join(file_rx_path, file_name)
                
                # Si le fichier existe déjà, ajouter un numéro
                if os.path.exists(save_path):
                    base, ext = os.path.splitext(file_name)
                    counter = 1
                    while os.path.exists(os.path.join(file_rx_path, f"{base}_{counter}{ext}")):
                        counter += 1
                    save_path = os.path.join(file_rx_path, f"{base}_{counter}{ext}")
            else:
                # Demander où sauvegarder (comportement par défaut)
                save_path, _ = QFileDialog.getSaveFileName(
                    self,
                    "Enregistrer le fichier reçu",
                    file_name,
                    "Tous les fichiers (*.*)"
                )
                
                if not save_path:
                    self.log(f"{tr('file_receive_cancelled')}", "warning")
                    self.file_rx_active = False
                    self.file_progress_bar.setVisible(False)
                    self.file_progress_label.setText(tr("file_no_transfer"))
                    self.file_progress_label.setStyleSheet("color: gray; font-style: italic;")
                    return
            
            # Sauvegarder
            with open(save_path, 'wb') as f:
                f.write(file_data)
            
            self.file_progress_bar.setValue(self.file_rx_total)
            self.file_progress_label.setText(f"✅ {tr('file_received')}: {file_name}")
            self.file_progress_label.setStyleSheet("color: green; font-weight: bold;")
            
            # Masquer après 3 secondes
            QTimer.singleShot(3000, lambda: self.file_progress_bar.setVisible(False))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setText(tr("file_no_transfer")))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setStyleSheet("color: gray; font-style: italic;"))
            
            # Afficher dans le chat
            timestamp = datetime.now().strftime("%H:%M:%S")
            remote = self.vara.remote_station or "Station"
            self.append_chat(
                f"[{timestamp}] {remote}: 📎 Fichier reçu : {file_name} "
                f"({file_size_original} octets, compressé à {compression_ratio:.1f}%)",
                "rx"
            )
            
            self.log(f"✓ {tr('file_received_saved')}: {save_path}", "success")
            
            # Notification
            QMessageBox.information(
                self,
                "Fichier reçu",
                f"Fichier reçu et enregistré :\n{save_path}\n\n"
                f"Taille : {file_size_original} octets\n"
                f"Compression : {compression_ratio:.1f}%"
            )
            
            self.file_rx_active = False
            
        except Exception as e:
            self.log(f"{tr('file_reception_error')}: {e}", "error")
            self.file_rx_active = False
            self.file_progress_bar.setVisible(False)
            QMessageBox.critical(
                self,
                tr("dlg_error"),
                f"Impossible de recevoir le fichier :\n{e}"
            )

    def receive_file(self, file_data_str):
        """Réception d'un fichier"""
        try:
            # Afficher la barre de progression
            self.file_progress_bar.setVisible(True)
            self.file_progress_bar.setMaximum(100)
            self.file_progress_bar.setValue(0)
            self.file_progress_label.setText(f"📥 {tr('receiving_file')}...")
            self.file_progress_label.setStyleSheet("color: blue; font-weight: bold;")
            QApplication.processEvents()
            
            # Parser: nom_encodé|taille_originale|taille_compressée|données_base64
            parts = file_data_str.split('|', 3)
            if len(parts) != 4:
                self.log(f"{tr('invalid_file_format')}", "error")
                self.file_progress_bar.setVisible(False)
                return
            
            encoded_filename, file_size_original_str, file_size_compressed_str, encoded_data = parts
            file_size_original = int(file_size_original_str)
            file_size_compressed = int(file_size_compressed_str)
            
            self.file_progress_bar.setValue(20)
            self.file_progress_label.setText(f"📥 {tr('decoding_filename')}...")
            QApplication.processEvents()
            
            # Décoder le nom de fichier (UTF-8)
            file_name = base64.b64decode(encoded_filename).decode('utf-8')
            
            self.file_progress_bar.setValue(30)
            self.file_progress_label.setText(f"📥 {tr('decoding')} {file_name}...")
            QApplication.processEvents()
            
            # Décoder les données base64
            compressed_data = base64.b64decode(encoded_data)
            
            self.file_progress_bar.setValue(50)
            QApplication.processEvents()
            
            # Vérifier la taille compressée
            if len(compressed_data) != file_size_compressed:
                self.log(
                    f"Taille compressée incorrecte : reçu {len(compressed_data)}, "
                    f"attendu {file_size_compressed}",
                    "error"
                )
                self.file_progress_bar.setVisible(False)
                return
            
            self.file_progress_bar.setValue(60)
            self.file_progress_label.setText(f"📥 {tr('decompressing')} {file_name}...")
            QApplication.processEvents()
            
            # Décompresser
            try:
                file_data = zlib.decompress(compressed_data)
            except zlib.error as e:
                self.log(f"{tr('decompression_error')}: {e}", "error")
                self.file_progress_bar.setVisible(False)
                return
            
            self.file_progress_bar.setValue(80)
            QApplication.processEvents()
            
            # Vérifier la taille décompressée
            if len(file_data) != file_size_original:
                self.log(
                    f"Taille décompressée incorrecte : obtenu {len(file_data)}, "
                    f"attendu {file_size_original}",
                    "error"
                )
                self.file_progress_bar.setVisible(False)
                return
            
            # Calculer le taux de compression
            compression_ratio = (1 - file_size_compressed / file_size_original) * 100
            
            self.log(
                f"Décompression : {file_size_compressed} → {file_size_original} octets "
                f"({compression_ratio:.1f}% de réduction)",
                "info"
            )
            
            self.file_progress_bar.setValue(90)
            self.file_progress_label.setText(f"📥 {tr('saving')}...")
            QApplication.processEvents()
            
            # Vérifier si un chemin de sauvegarde est configuré
            file_rx_path = self.settings.get('file_rx_path', '').strip()
            
            if file_rx_path and os.path.isdir(file_rx_path):
                # Sauvegarder automatiquement dans le dossier configuré
                save_path = os.path.join(file_rx_path, file_name)
                
                # Si le fichier existe déjà, ajouter un numéro
                if os.path.exists(save_path):
                    base, ext = os.path.splitext(file_name)
                    counter = 1
                    while os.path.exists(os.path.join(file_rx_path, f"{base}_{counter}{ext}")):
                        counter += 1
                    save_path = os.path.join(file_rx_path, f"{base}_{counter}{ext}")
            else:
                # Demander où sauvegarder (comportement par défaut)
                save_path, _ = QFileDialog.getSaveFileName(
                    self,
                    "Enregistrer le fichier reçu",
                    file_name,
                    "Tous les fichiers (*.*)"
                )
                
                if not save_path:
                    self.log(f"{tr('file_receive_cancelled')}", "warning")
                    self.file_progress_bar.setVisible(False)
                    self.file_progress_label.setText(tr("file_no_transfer"))
                    self.file_progress_label.setStyleSheet("color: gray; font-style: italic;")
                    return
            
            # Sauvegarder
            with open(save_path, 'wb') as f:
                f.write(file_data)
            
            self.file_progress_bar.setValue(100)
            self.file_progress_label.setText(f"✅ {tr('file_received')}: {file_name}")
            self.file_progress_label.setStyleSheet("color: green; font-weight: bold;")
            
            # Masquer après 3 secondes
            QTimer.singleShot(3000, lambda: self.file_progress_bar.setVisible(False))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setText(tr("file_no_transfer")))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setStyleSheet("color: gray; font-style: italic;"))
            
            # Afficher dans le chat
            timestamp = datetime.now().strftime("%H:%M:%S")
            remote = self.vara.remote_station or "Station"
            self.append_chat(
                f"[{timestamp}] {remote}: 📎 Fichier reçu : {file_name} "
                f"({file_size_original} octets, compressé à {compression_ratio:.1f}%)",
                "rx"
            )
            
            self.log(f"✓ {tr('file_received_saved')}: {save_path}", "success")
            
            # Notification
            QMessageBox.information(
                self,
                "Fichier reçu",
                f"Fichier reçu et enregistré :\n{save_path}\n\n"
                f"Taille : {file_size_original} octets\n"
                f"Compression : {compression_ratio:.1f}%"
            )
            
        except Exception as e:
            self.log(f"{tr('file_reception_error')}: {e}", "error")
            QMessageBox.critical(
                self,
                tr("dlg_error"),
                f"Impossible de recevoir le fichier :\n{e}"
            )
    
    def _trigger_auto_responder_delayed(self):
        """Déclencher l'auto-répondeur avec vérification finale (appelé avec délai)"""
        print(f"[DEBUG] Auto-répondeur delayed: session_active={self.vara.session_active}, already_active={self.auto_responder_active}")
        if self.vara and self.vara.session_active and not self.auto_responder_active:
            print("[DEBUG] Auto-répondeur delayed: Conditions OK, déclenchement")
            self.trigger_auto_responder()
        else:
            print("[DEBUG] Auto-répondeur delayed: Annulation (plus de session ou déjà actif)")
    
    def trigger_auto_responder(self):
        """Déclenche l'auto-répondeur"""
        print("[DEBUG] Auto-répondeur: Déclenchement")
        self.auto_responder_active = True
        delay = self.settings.get('auto_delay', 3)
        
        def send_auto_reply():
            print("[DEBUG] Auto-répondeur: Envoi du message après délai")
            message = self.settings.get('auto_message', '')
            print(f"[DEBUG] Auto-répondeur: Message = '{message}'")
            if message and self.vara.session_active:
                # Remplacer les variables
                remote = self.vara.remote_station or "OM"
                message = message.replace('{CALLSIGN}', remote)
                message = message.replace('{MYCALL}', self.settings.get('mycall', ''))
                message = message.replace('{TIME}', datetime.now().strftime("%H:%M"))
                
                print(f"[DEBUG] Auto-répondeur: Message formaté = '{message}'")
                
                # Chiffrer le message si activé
                message_to_send = self.crypto.encrypt(message) if self.crypto.enabled else message
                
                result = self.vara.send_data(message_to_send + "<EOF>")
                print(f"[DEBUG] Auto-répondeur: send_data result = {result}")
                
                if result:
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    mycall = self.settings.get('mycall', 'Auto')
                    
                    # Stocker pour affichage après ACK
                    if self.crypto.enabled:
                        pending_msg = f"[{timestamp}] {mycall} 🔒 (AUTO): {message}"
                    else:
                        pending_msg = f"[{timestamp}] {mycall} (AUTO): {message}"
                    
                    self.pending_messages.append(pending_msg)
                    
                    # Déconnexion automatique si activée
                    # Au lieu d'un timer, on attend que BUFFER=0 pour déconnecter
                    if self.settings.get('auto_disconnect'):
                        print("[DEBUG] Auto-répondeur: Flag auto_disconnect_pending activé")
                        self.auto_disconnect_pending = True
            else:
                print(f"[DEBUG] Auto-répondeur: Pas d'envoi - message='{message}', session_active={self.vara.session_active}")
            
            self.auto_responder_active = False
        
        QTimer.singleShot(delay * 1000, send_auto_reply)
        self.log(f"{tr('auto_responder_enabled')} ({tr('delay')}: {delay}s)", "info")
    
    def _auto_disconnect_delayed(self):
        """Déconnexion automatique après auto-réponse (avec vérification)"""
        print(f"[DEBUG] Auto-déconnexion: session_active={self.vara.session_active if self.vara else False}")
        if self.vara and self.vara.session_active:
            print("[DEBUG] Auto-déconnexion: Déconnexion maintenant")
            self.disconnect_session()
        else:
            print("[DEBUG] Auto-déconnexion: Annulation (plus de session)")
    
    def handle_connection_status(self, connected):
        """Mise à jour du statut de connexion"""
        if connected:
            self.status_modem.setText(f"🟢 {tr('status_connected')}")
            self.status_modem.setStyleSheet("color: green;")
        else:
            self.status_modem.setText(f"⚫ {tr('status_disconnected')}")
            self.status_modem.setStyleSheet("color: red;")
    
    def handle_ptt(self, active):
        """Mise à jour du statut PTT"""
        if active:
            self.status_ptt.setText("PTT: ON")
            self.status_ptt.setStyleSheet("color: red; font-weight: bold;")
        else:
            self.status_ptt.setText("PTT: OFF")
            self.status_ptt.setStyleSheet("color: gray;")
    
    def handle_connected(self, callsign):
        """Gestion de la connexion à une station"""
        self.log(f"✓ {tr('connected_to')} {callsign}", "success")
        self.status_session.setText(f"{tr('session')}: {callsign}")
        self.status_session.setStyleSheet("color: green; font-weight: bold;")
        self.msg_input.setEnabled(True)
        self.send_btn.setEnabled(True)
        self.file_btn.setEnabled(True)
        self.disconnect_btn.setEnabled(True)
        self.call_btn.setEnabled(False)
        self.cq_btn.setEnabled(False)  # Pas de CQ pendant une session
        self.crypto_btn.setEnabled(True)  # Activer le chiffrement en session
        self.crypto_btn.setEnabled(True)  # Activer le chiffrement
        
        # Envoyer la balise BBSON si le BBS est activé
        # Attendre 500ms pour que la connexion soit bien établie
        # NE PAS envoyer BBSON si "Déconnecter après réponse" est coché
        auto_disconnect = self.settings.get('auto_disconnect', True)
        bbs_enabled = self.settings.get('bbs_enabled', False)
        bbs_folder = self.settings.get('bbs_folder', '').strip()
        print(f"[DEBUG] BBS check: enabled={bbs_enabled}, folder='{bbs_folder}', auto_disconnect={auto_disconnect}")
        
        if bbs_enabled and not auto_disconnect:
            if bbs_folder and os.path.isdir(bbs_folder):
                print(f"[DEBUG] BBS: Programmation envoi BBSON dans 500ms")
                QTimer.singleShot(500, self._send_bbson)
            else:
                print(f"[DEBUG] BBS: Dossier invalide ou vide")
        else:
            if auto_disconnect:
                print(f"[DEBUG] BBS: Désactivé car auto_disconnect est activé")
            else:
                print(f"[DEBUG] BBS: Désactivé")
        
        # Déclencher l'auto-répondeur si activé
        # Attendre 1000ms pour laisser le temps à BBSON d'être envoyé
        auto_enabled = self.settings.get('auto_enabled', False)
        print(f"[DEBUG] Auto-répondeur check: enabled={auto_enabled}, already_active={self.auto_responder_active}")
        
        if auto_enabled and not self.auto_responder_active:
            print(f"[DEBUG] Auto-répondeur: Programmation dans 1000ms")
            QTimer.singleShot(1000, self._trigger_auto_responder_delayed)
        else:
            print(f"[DEBUG] Auto-répondeur: Non activé ou déjà actif")
        
        # Ajouter au journal QSO
        self.qso_log.append({
            'callsign': callsign,
            'start_time': datetime.now(),
            'end_time': None,
            'messages': []
        })
    
    def handle_disconnected(self, callsign):
        """Gestion de la déconnexion"""
        self.log(f"✗ {tr('disconnected_from')} {callsign}", "info")
        self.status_session.setText(tr("no_session"))
        self.status_session.setStyleSheet("color: gray;")
        self.msg_input.setEnabled(False)
        self.send_btn.setEnabled(False)
        self.file_btn.setEnabled(False)
        self.disconnect_btn.setEnabled(False)
        self.call_btn.setEnabled(True)
        self.cq_btn.setEnabled(True)  # Réactiver CQ après déconnexion
        
        # Désactiver et décocher le bouton chiffrement
        self.crypto_btn.setEnabled(False)
        if self.crypto_btn.isChecked():
            self.crypto_btn.setChecked(False)
        # Réinitialiser le chiffrement
        if self.crypto.enabled:
            self.crypto.disable()
        
        # Désactiver le bouton BBS à la déconnexion
        self.remote_bbs_active = False
        self.bbs_btn.setEnabled(False)
        self.bbs_file_list = []
        
        # Réinitialiser l'auto-répondeur
        self.auto_responder_active = False
        self.auto_disconnect_pending = False
        
        # Finaliser le QSO dans le journal
        if self.qso_log and self.qso_log[-1]['callsign'] == callsign:
            self.qso_log[-1]['end_time'] = datetime.now()
    
    def append_chat(self, text, msg_type="info"):
        """Ajoute un message au chat"""
        cursor = self.chat_display.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        
        # Couleurs selon le type
        if msg_type == "tx":
            color = QColor(0, 100, 200)  # Bleu
        elif msg_type == "rx":
            color = QColor(0, 150, 0)    # Vert
            
            # Notification sonore pour les messages reçus
            if self.settings.get('sound', False):
                QApplication.beep()
        else:
            color = QColor(0, 0, 0)      # Noir
        
        format = cursor.charFormat()
        format.setForeground(color)
        cursor.setCharFormat(format)
        cursor.insertText(text + "\n")
        
        self.chat_display.setTextCursor(cursor)
        self.chat_display.ensureCursorVisible()
    
    def log(self, message, level="info"):
        """Ajoute un message au journal"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        
        cursor = self.log_display.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        
        # Couleurs selon le niveau
        colors = {
            'debug': QColor(128, 128, 128),
            'info': QColor(0, 0, 0),
            'success': QColor(0, 150, 0),
            'warning': QColor(200, 100, 0),
            'error': QColor(200, 0, 0)
        }
        
        format = cursor.charFormat()
        format.setForeground(colors.get(level, QColor(0, 0, 0)))
        cursor.setCharFormat(format)
        cursor.insertText(f"[{timestamp}] {message}\n")
        
        self.log_display.setTextCursor(cursor)
        self.log_display.ensureCursorVisible()
    
    def set_dialog_icon(self, dialog):
        """Définit l'icône pour une fenêtre de dialogue"""
        from PyQt6.QtGui import QIcon
        
        # Essayer .ico d'abord (Windows), puis .png
        # resource_path gère l'exécution normale ET le bundle PyInstaller
        for name in ('icon.ico', 'icon.png'):
            icon_path = resource_path(name)
            if os.path.exists(icon_path):
                dialog.setWindowIcon(QIcon(icon_path))
                return
    
    def show_settings(self):
        """Affiche le dialogue de paramètres"""
        dialog = SettingsDialog(self)
        self.set_dialog_icon(dialog)  # Ajouter l'icône
        dialog.set_settings(self.settings)
        dialog.apply_tooltips(self.settings.get('tooltips_enabled', True))
        
        # Sauvegarder le type de modem actuel
        old_modem_type = self.settings.get('modem_type', 'VARA HF')
        
        if dialog.exec() == QDialog.DialogCode.Accepted:
            new_settings = dialog.get_settings()
            new_modem_type = new_settings.get('modem_type', 'VARA HF')
            
            # Détecter changement de type de modem
            modem_changed = (old_modem_type != new_modem_type)
            
            # Détecter changement de paramètres critiques nécessitant reconnexion
            critical_params_changed = False
            if self.vara and self.vara.connected:
                # Paramètres qui nécessitent une reconnexion
                critical_params = ['host', 'cmd_port', 'data_port', 'mycall']
                for param in critical_params:
                    if self.settings.get(param) != new_settings.get(param):
                        critical_params_changed = True
                        break
            
            self.settings = new_settings
            self.save_settings()
            
            # Mettre à jour le titre si l'indicatif a changé
            self.update_window_title()
            
            # Gérer le changement de modem
            if modem_changed:
                # 1. D'abord déconnecter le modem si connecté
                if self.vara and self.vara.connected:
                    self.log(f"Déconnexion de {old_modem_type}...", "info")
                    self.vara.disconnect()
                    self.vara = None
                
                # 2. Ensuite fermer le processus VARA
                if self.vara_process_manager:
                    self.log(f"Fermeture de {old_modem_type}...", "info")
                    self.vara_process_manager.stop_vara()
                    self.vara_process_manager = None
                
                # Mettre à jour l'interface
                self.connect_btn.setText(tr("btn_connect"))
                self.call_btn.setEnabled(False)
                self.cq_btn.setEnabled(False)
                self.status_modem.setText(f"⚫ {tr('status_disconnected')}")
                
                # Obtenir le chemin de l'exécutable
                path_key = {
                    'VARA HF': 'vara_hf_path',
                    'VARA FM': 'vara_fm_path',
                    'VARA SAT': 'vara_sat_path'
                }.get(new_modem_type)
                
                # Créer le nouveau VARAProcessManager (sans arguments)
                self.vara_process_manager = VARAProcessManager()
                
                # TOUJOURS lancer le nouveau modem lors d'un changement de type
                # (ignorer le paramètre auto_start dans ce cas)
                self.log(f"Démarrage de {new_modem_type}...", "info")
                QTimer.singleShot(1000, self.auto_start_vara)
            
            elif critical_params_changed:
                # Paramètres critiques changés (host, ports, indicatif)
                QMessageBox.information(
                    self,
                    tr("restart_required"),
                    tr("connection_params_changed")
                )
    
    
    def set_window_icon(self):
        """Définir l'icône de la fenêtre"""
        from PyQt6.QtGui import QIcon
        
        # resource_path gère l'exécution normale ET le bundle PyInstaller.
        # On tente aussi le dossier de l'exe en secours (icône fournie à côté).
        possible_paths = [
            resource_path('icon.ico'),
            resource_path('icon.png'),
            os.path.join(os.path.dirname(sys.executable), 'icon.ico'),
        ]
        
        for icon_path in possible_paths:
            if os.path.exists(icon_path):
                try:
                    self.setWindowIcon(QIcon(icon_path))
                    return
                except Exception as e:
                    print(f"Erreur chargement icône {icon_path}: {e}")
        
        # Si aucune icône trouvée, continuer sans icône
        print("Icône icon.ico non trouvée")
    
    def update_window_title(self):
        """Met à jour le titre de la fenêtre avec l'indicatif"""
        mycall = self.settings.get('mycall', '')
        if mycall:
            self.setWindowTitle(f"CRYPTARA - {mycall}")
        else:
            self.setWindowTitle("CRYPTARA")
    
    def save_chat_to_file(self):
        """Sauvegarder le contenu du chat dans un fichier texte"""
        # Récupérer le contenu du chat
        chat_content = self.chat_display.toPlainText()
        
        if not chat_content.strip():
            QMessageBox.information(
                self,
                "Chat vide",
                "Le chat est vide, rien à sauvegarder."
            )
            return
        
        # Demander le nom du fichier
        filename, _ = QFileDialog.getSaveFileName(
            self,
            "Sauvegarder le chat",
            f"chat_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt",
            "Fichiers texte (*.txt);;Tous les fichiers (*)"
        )
        
        if filename:
            try:
                with open(filename, 'w', encoding='utf-8') as f:
                    # En-tête
                    f.write("=== CRYPTARA Chat ===\n")
                    f.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    f.write(f"Station: {self.settings.get('mycall', 'N/A')}\n\n")
                    # Contenu
                    f.write(chat_content)
                
                self.log(f"💾 Chat sauvegardé: {filename}", "success")
                QMessageBox.information(
                    self,
                    "Sauvegarde réussie",
                    f"Le chat a été sauvegardé dans:\n{filename}"
                )
            except Exception as e:
                self.log(f"❌ Erreur sauvegarde chat: {e}", "error")
                QMessageBox.critical(
                    self,
                    "Erreur",
                    f"Impossible de sauvegarder le chat:\n{e}"
                )
    
    def show_qso_log(self):
        """Affiche le journal des QSO"""
        dialog = QDialog(self)
        self.set_dialog_icon(dialog)  # Ajouter l'icône
        dialog.setWindowTitle(tr("dlg_qso_log"))
        dialog.resize(600, 400)
        
        layout = QVBoxLayout()
        
        table = QTableWidget()
        table.setColumnCount(4)
        table.setHorizontalHeaderLabels(['Indicatif', 'Début', 'Fin', 'Durée'])
        table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        
        for qso in self.qso_log:
            row = table.rowCount()
            table.insertRow(row)
            
            table.setItem(row, 0, QTableWidgetItem(qso['callsign']))
            table.setItem(row, 1, QTableWidgetItem(
                qso['start_time'].strftime("%Y-%m-%d %H:%M:%S")
            ))
            
            if qso['end_time']:
                table.setItem(row, 2, QTableWidgetItem(
                    qso['end_time'].strftime("%Y-%m-%d %H:%M:%S")
                ))
                
                duration = qso['end_time'] - qso['start_time']
                minutes = int(duration.total_seconds() / 60)
                seconds = int(duration.total_seconds() % 60)
                table.setItem(row, 3, QTableWidgetItem(f"{minutes}m {seconds}s"))
            else:
                table.setItem(row, 2, QTableWidgetItem("En cours"))
                table.setItem(row, 3, QTableWidgetItem("-"))
        
        layout.addWidget(table)
        
        close_btn = QPushButton(tr("btn_close"))
        close_btn.clicked.connect(dialog.accept)
        layout.addWidget(close_btn)
        
        dialog.setLayout(layout)
        dialog.exec()
    
    def show_macros(self):
        """Affiche la fenêtre de configuration des macros"""
        dialog = QDialog(self)
        self.set_dialog_icon(dialog)
        dialog.setWindowTitle(tr("dlg_macros_title"))
        dialog.resize(700, 600)
        
        layout = QVBoxLayout()
        
        # Instructions
        info_label = QLabel(
            f"<b>{tr('macros_info_title')}</b><br>"
            f"<i>{tr('macros_variables')}</i>"
        )
        layout.addWidget(info_label)
        
        # Scroll area pour les 12 macros
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll_widget = QWidget()
        scroll_layout = QFormLayout()
        
        # Stocker les QLineEdit pour récupérer les valeurs
        macro_edits = []
        
        for i in range(1, 13):
            label = QLabel(f"<b>F{i}:</b>")
            edit = QLineEdit()
            edit.setPlaceholderText(tr("macro_placeholder"))
            
            # Tooltip si activé
            if self.settings.get('tooltips_enabled', True):
                edit.setToolTip(tr('tt_macro_field').replace('{0}', str(i)))
            else:
                edit.setToolTip('')
            
            # Charger la macro existante
            macro_key = f'macro_f{i}'
            edit.setText(self.settings.get(macro_key, ''))
            
            macro_edits.append(edit)
            scroll_layout.addRow(label, edit)
        
        scroll_widget.setLayout(scroll_layout)
        scroll.setWidget(scroll_widget)
        layout.addWidget(scroll)
        
        # Boutons
        button_layout = QHBoxLayout()
        
        test_btn = QPushButton(tr("btn_test_macro"))
        test_btn.clicked.connect(lambda: self._test_macro(macro_edits[0].text()))
        if self.settings.get('tooltips_enabled', True):
            test_btn.setToolTip(tr('tt_test_macro'))
        else:
            test_btn.setToolTip('')
        button_layout.addWidget(test_btn)
        
        button_layout.addStretch()
        
        save_btn = QPushButton(tr("btn_save"))
        save_btn.clicked.connect(lambda: self._save_macros(macro_edits, dialog))
        button_layout.addWidget(save_btn)
        
        cancel_btn = QPushButton(tr("btn_cancel"))
        cancel_btn.clicked.connect(dialog.reject)
        button_layout.addWidget(cancel_btn)
        
        layout.addLayout(button_layout)
        
        dialog.setLayout(layout)
        dialog.exec()
    
    def _save_macros(self, macro_edits, dialog):
        """Sauvegarde les macros"""
        for i, edit in enumerate(macro_edits, start=1):
            macro_key = f'macro_f{i}'
            self.settings[macro_key] = edit.text()
        
        self.save_settings()
        self.log(f"💾 {tr('macros_saved')}", "success")
        dialog.accept()
    
    def _test_macro(self, macro_text):
        """Teste une macro en l'affichant avec les variables remplacées"""
        expanded = self._expand_macro(macro_text)
        QMessageBox.information(
            self,
            tr("dlg_test_macro"),
            f"{tr('test_macro_original')}\n{macro_text}\n\n"
            f"{tr('test_macro_expanded')}\n{expanded}"
        )
    
    def _expand_macro(self, macro_text):
        """Remplace les variables dans une macro"""
        if not macro_text:
            return ""
        
        # Récupérer les infos de la station
        mycall = self.settings.get('mycall', '')
        name = self.settings.get('operator_name', '')
        firstname = self.settings.get('operator_firstname', '')
        qth = self.settings.get('qth', '')
        grid = self.settings.get('grid_square', '')
        rig = self.settings.get('rig', '')
        antenna = self.settings.get('antenna', '')
        
        # Récupérer l'indicatif distant
        callsign = self.vara.remote_station if (self.vara and self.vara.remote_station) else ''
        
        # Date et heure
        now = datetime.now()
        time_str = now.strftime("%H:%M")
        date_str = now.strftime("%Y-%m-%d")
        
        # Remplacer les variables
        expanded = macro_text
        expanded = expanded.replace('{MYCALL}', mycall)
        expanded = expanded.replace('{NAME}', name)
        expanded = expanded.replace('{FIRSTNAME}', firstname)
        expanded = expanded.replace('{QTH}', qth)
        expanded = expanded.replace('{GRID}', grid)
        expanded = expanded.replace('{RIG}', rig)
        expanded = expanded.replace('{ANTENNA}', antenna)
        expanded = expanded.replace('{CALLSIGN}', callsign)
        expanded = expanded.replace('{TIME}', time_str)
        expanded = expanded.replace('{DATE}', date_str)
        
        return expanded
    
    def keyPressEvent(self, event):
        """Gestion des touches de fonction F1-F12 pour les macros"""
        from PyQt6.QtCore import Qt
        
        # Touches F1 à F12
        f_keys = {
            Qt.Key.Key_F1: 'macro_f1',
            Qt.Key.Key_F2: 'macro_f2',
            Qt.Key.Key_F3: 'macro_f3',
            Qt.Key.Key_F4: 'macro_f4',
            Qt.Key.Key_F5: 'macro_f5',
            Qt.Key.Key_F6: 'macro_f6',
            Qt.Key.Key_F7: 'macro_f7',
            Qt.Key.Key_F8: 'macro_f8',
            Qt.Key.Key_F9: 'macro_f9',
            Qt.Key.Key_F10: 'macro_f10',
            Qt.Key.Key_F11: 'macro_f11',
            Qt.Key.Key_F12: 'macro_f12'
        }
        
        if event.key() in f_keys:
            # Vérifier qu'on est connecté
            if not self.vara or not self.vara.session_active:
                self.log(f"⚠️ {tr('macro_no_session')}", "warning")
                return
            
            # Récupérer la macro
            macro_key = f_keys[event.key()]
            macro_text = self.settings.get(macro_key, '')
            
            if not macro_text:
                f_num = macro_key.replace('macro_f', 'F')
                self.log(f"⚠️ {tr('macro_empty').replace('{0}', f_num)}", "warning")
                return
            
            # Remplacer les variables
            expanded_text = self._expand_macro(macro_text)
            
            # Envoyer le message
            self.send_message(expanded_text)
            
            f_num = macro_key.replace('macro_f', 'F')
            self.log(f"📤 {tr('macro_sent').replace('{0}', f_num)}", "info")
        else:
            # Appeler le gestionnaire parent pour les autres touches
            super().keyPressEvent(event)
    
    def toggle_tooltips(self):
        """Activer/désactiver les tooltips"""
        current = self.settings.get('tooltips_enabled', True)
        self.settings['tooltips_enabled'] = not current
        self.save_settings()
        self.update_tooltips_menu()
        self.apply_tooltips()
        
        status = tr('tooltips_enabled') if self.settings['tooltips_enabled'] else tr('tooltips_disabled')
        self.log(f"ℹ️ {status}", "info")
    
    def update_tooltips_menu(self):
        """Mettre à jour le texte du menu tooltips"""
        if self.settings.get('tooltips_enabled', True):
            self.tooltips_action.setText(tr('tooltips_enabled'))
        else:
            self.tooltips_action.setText(tr('tooltips_disabled'))
    
    def apply_tooltips(self):
        """Appliquer ou supprimer les tooltips sur tous les éléments"""
        enabled = self.settings.get('tooltips_enabled', True)
        
        if enabled:
            # Interface principale
            self.connect_btn.setToolTip(tr('tt_connect_btn'))
            self.call_btn.setToolTip(tr('tt_call_btn'))
            self.cq_btn.setToolTip(tr('tt_cq_btn'))
            self.disconnect_btn.setToolTip(tr('tt_disconnect_btn'))
            self.msg_input.setToolTip(tr('tt_message_input'))
            self.send_btn.setToolTip(tr('tt_send_btn'))
            self.file_btn.setToolTip(tr('tt_file_btn'))
            self.crypto_btn.setToolTip(tr('tt_crypto_btn'))
            self.chat_display.setToolTip(tr('tt_chat_display'))
            self.log_display.setToolTip(tr('tt_log_display'))
            self.cq_table.setToolTip(tr('tt_cq_table'))
        else:
            # Supprimer tous les tooltips
            self.connect_btn.setToolTip('')
            self.call_btn.setToolTip('')
            self.cq_btn.setToolTip('')
            self.disconnect_btn.setToolTip('')
            self.msg_input.setToolTip('')
            self.send_btn.setToolTip('')
            self.file_btn.setToolTip('')
            self.crypto_btn.setToolTip('')
            self.chat_display.setToolTip('')
            self.log_display.setToolTip('')
            self.cq_table.setToolTip('')
    
    def save_log(self):
        """Sauvegarde le journal"""
        # Écrire dans un dossier inscriptible (pas Program Files)
        log_name = f"cryptara_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        filename = os.path.join(get_config_dir(), log_name)
        
        try:
            with open(filename, 'w', encoding='utf-8') as f:
                f.write("=== CRYPTARA Log ===\n")
                f.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Station: {self.settings.get('mycall', 'N/A')}\n\n")
                f.write(self.chat_display.toPlainText())
            
            QMessageBox.information(self, tr("dlg_success"), f"{tr('log_saved')}: {filename}")
        except Exception as e:
            QMessageBox.critical(self, tr("dlg_error"), f"Erreur sauvegarde: {e}")
    
    def show_about(self):
        """Affiche la boîte À propos"""
        QMessageBox.about(
            self,
            "À propos de CRYPTARA",
            "<h3>CRYPTARA</h3>"
            "<p><b>C</b>ryptographic <b>R</b>adio <b>Y</b>ield <b>P</b>rotection<br>"
            "<b>T</b>ransmission <b>A</b>utomated <b>R</b>eliable <b>A</b>ssistant</p>"
            "<p>Client VARA sécurisé avec chiffrement AES-256</p>"
            "<p>Support VARA HF/FM/SAT • Perfect Forward Secrecy • Transfert de fichiers</p>"
            "<p><b>Version:</b> 1.3.1</p>"
            "<p>73! 🔒📻</p>"
        )
    
    def load_settings(self):
        """Charge les paramètres depuis le fichier .conf"""
        # Config dans un dossier utilisateur inscriptible (%APPDATA% / ~/.config)
        config_file = os.path.join(get_config_dir(), 'cryptara_settings.conf')
        
        config = configparser.ConfigParser()
        
        # Valeurs par défaut
        defaults = {
            'modem_type': 'VARA HF',
            'host': 'localhost',
            'cmd_port': '8300',
            'data_port': '8301',
            'mycall': '',
            'operator_name': '',
            'operator_firstname': '',
            'qth': '',
            'grid_square': '',
            'rig': '',
            'antenna': '',
            'listen': 'True',
            'compression': 'True',
            'vara_hf_path': '',
            'vara_fm_path': '',
            'vara_sat_path': '',
            'auto_start_vara': 'True',
            'auto_enabled': 'False',
            'auto_message': '',
            'auto_disconnect': 'True',
            'auto_delay': '3',
            'timestamp': 'True',
            'sound': 'False',
            'save_log': 'True',
            'tooltips_enabled': 'True',
            'file_rx_path': '',
            'bbs_enabled': 'False',
            'bbs_folder': '',
            # Macros F1-F12
            'macro_f1': '',
            'macro_f2': '',
            'macro_f3': '',
            'macro_f4': '',
            'macro_f5': '',
            'macro_f6': '',
            'macro_f7': '',
            'macro_f8': '',
            'macro_f9': '',
            'macro_f10': '',
            'macro_f11': '',
            'macro_f12': ''
        }
        
        if os.path.exists(config_file):
            try:
                config.read(config_file, encoding='utf-8')
                
                # Charger depuis le fichier
                settings = {}
                if 'CRYPTARA' in config:
                    for key in defaults:
                        value = config.get('CRYPTARA', key, fallback=defaults[key])
                        
                        # Convertir les types
                        if key in ['cmd_port', 'data_port', 'auto_delay']:
                            settings[key] = int(value)
                        elif key in ['listen', 'compression', 'auto_start_vara', 'auto_enabled', 'auto_disconnect', 'timestamp', 'sound', 'save_log', 'bbs_enabled', 'tooltips_enabled']:
                            settings[key] = value.lower() == 'true'
                        else:
                            settings[key] = value
                    
                    # Charger aussi les macros (F1-F12) même si absentes des defaults
                    for i in range(1, 13):
                        macro_key = f'macro_f{i}'
                        settings[macro_key] = config.get('CRYPTARA', macro_key, fallback='')
                else:
                    # Section manquante, utiliser les défauts
                    settings = self._convert_defaults(defaults)
                
                return settings
                
            except Exception as e:
                print(f"Erreur lors du chargement des paramètres : {e}")
        
        # Retourner les défauts
        return self._convert_defaults(defaults)
    
    def _convert_defaults(self, defaults):
        """Convertir les défauts depuis strings"""
        settings = {}
        for key, value in defaults.items():
            if key in ['cmd_port', 'data_port', 'auto_delay']:
                settings[key] = int(value)
            elif key in ['listen', 'compression', 'auto_start_vara', 'auto_enabled', 'auto_disconnect', 'timestamp', 'sound', 'save_log', 'bbs_enabled']:
                settings[key] = value.lower() == 'true'
            else:
                settings[key] = value
        return settings
    
    def save_settings(self):
        """Sauvegarde les paramètres dans le fichier .conf"""
        # Config dans un dossier utilisateur inscriptible (%APPDATA% / ~/.config)
        config_file = os.path.join(get_config_dir(), 'cryptara_settings.conf')
        
        config = configparser.ConfigParser()
        config['CRYPTARA'] = {}
        
        # Convertir tous les paramètres en strings
        for key, value in self.settings.items():
            config['CRYPTARA'][key] = str(value)
        
        try:
            with open(config_file, 'w', encoding='utf-8') as f:
                config.write(f)
            print(f"[CONFIG] Paramètres sauvegardés dans {config_file}")
        except Exception as e:
            self.log(f"{tr('settings_save_error')}: {e}", "error")
    
    def closeEvent(self, event):
        """Gestion de la fermeture"""
        # Déconnecter du modem si connecté
        if self.vara and self.vara.connected:
            self.vara.disconnect()
        
        # Fermer VARA si lancé par le script
        if self.vara_process_manager.is_running():
            self.log(f"🛑 {tr('closing_vara')}...", "info")
            self.vara_process_manager.stop_vara()
        
        # Sauvegarder les paramètres
        self.save_settings()
        event.accept()

    # ==================== BBS (Bulletin Board System) ====================
    
    def _send_bbson(self):
        """Envoyer la balise BBSON (appelé avec délai)"""
        if self.vara and self.vara.session_active:
            self.vara.send_data("<BBSON><EOF>".encode())
            self.log(f"📁 {tr('bbs_beacon_sent')}", "info")
    
    def handle_bbs_on(self):
        """La station distante a activé le BBS"""
        print("[DEBUG] BBS: Balise <BBSON> reçue")
        self.remote_bbs_active = True
        self.bbs_btn.setEnabled(True)
        self.log(f"📁 {tr('bbs_detected')}", "info")
    
    def handle_bbs_list_request(self):
        """La station distante demande la liste des fichiers BBS"""
        if not self.settings.get('bbs_enabled', False):
            self.log(f"⚠️ {tr('bbs_disabled')}", "warning")
            return
        
        bbs_folder = self.settings.get('bbs_folder', '').strip()
        if not bbs_folder or not os.path.isdir(bbs_folder):
            self.log(f"⚠️ {tr('bbs_invalid_folder')}", "warning")
            return
        
        # Lister les fichiers du dossier BBS
        file_list = []
        try:
            for filename in os.listdir(bbs_folder):
                filepath = os.path.join(bbs_folder, filename)
                if os.path.isfile(filepath):
                    size = os.path.getsize(filepath)
                    file_list.append({'name': filename, 'size': size})
            
            # Envoyer la liste en JSON
            import json
            file_list_json = json.dumps(file_list)
            response = f"<BBSFILES>{file_list_json}</BBSFILES><EOF>"
            self.vara.send_data(response.encode())
            self.log(f"📁 {tr('bbs_list_sent')} ({len(file_list)} {tr('lbl_files')})", "info")
        
        except Exception as e:
            self.log(f"❌ {tr('bbs_list_error')}: {e}", "error")
    
    def handle_bbs_files(self, file_list_json):
        """Réception de la liste des fichiers BBS"""
        try:
            import json
            self.bbs_file_list = json.loads(file_list_json)
            self.log(f"📁 {tr('bbs_list_received')} ({len(self.bbs_file_list)} {tr('lbl_files')})", "info")
            
            # Ouvrir automatiquement la fenêtre BBS
            QTimer.singleShot(100, self._show_bbs_window_delayed)
        
        except Exception as e:
            self.log(f"❌ {tr('bbs_parse_error')}: {e}", "error")
            self.bbs_file_list = []
    
    def handle_bbs_get_request(self, filename):
        """La station distante demande un fichier BBS"""
        if not self.settings.get('bbs_enabled', False):
            self.log(f"⚠️ {tr('bbs_disabled')}", "warning")
            return
        
        bbs_folder = self.settings.get('bbs_folder', '').strip()
        if not bbs_folder or not os.path.isdir(bbs_folder):
            self.log(f"⚠️ {tr('bbs_invalid_folder')}", "warning")
            return
        
        # Vérifier que le fichier existe et est dans le dossier BBS
        filepath = os.path.join(bbs_folder, filename)
        
        # Sécurité : vérifier qu'on ne sort pas du dossier BBS
        if not os.path.abspath(filepath).startswith(os.path.abspath(bbs_folder)):
            self.log(f"⚠️ {tr('bbs_access_denied')}: {filename}", "warning")
            return
        
        if not os.path.isfile(filepath):
            self.log(f"⚠️ {tr('bbs_file_not_found')}: {filename}", "warning")
            return
        
        # Envoyer le fichier
        self.log(f"📤 {tr('bbs_sending_file')}: {filename}", "info")
        self.send_file_path(filepath)
    
    def show_bbs_window(self):
        """Afficher la fenêtre BBS avec la liste des fichiers"""
        if not self.remote_bbs_active:
            QMessageBox.warning(self, tr("bbs_window_title"), tr("bbs_not_active"))
            return
        
        # Si pas de liste, la demander et attendre
        if not self.bbs_file_list:
            self.vara.send_data("<BBSLIST><EOF>".encode())
            self.log(f"📁 {tr('bbs_requesting_list')}", "info")
            # Ne PAS ouvrir la fenêtre maintenant
            # Elle s'ouvrira automatiquement quand la liste sera reçue
            return
        
        # Ouvrir la fenêtre avec la liste
        self._show_bbs_window_delayed()
    
    def _show_bbs_window_delayed(self):
        """Afficher la fenêtre BBS (appelé après réception de la liste)"""
        # Créer la fenêtre BBS
        dialog = QDialog(self)
        self.set_dialog_icon(dialog)
        dialog.setWindowTitle(f"📁 {tr('bbs_window_title')} - {self.vara.remote_station}")
        dialog.resize(600, 400)
        
        layout = QVBoxLayout()
        
        # Instructions
        info_label = QLabel(tr("bbs_double_click_info"))
        info_label.setStyleSheet("color: gray; font-style: italic;")
        layout.addWidget(info_label)
        
        # Tableau des fichiers
        table = QTableWidget()
        table.setColumnCount(2)
        table.setHorizontalHeaderLabels([tr("lbl_filename"), tr("lbl_size")])
        table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        
        # Remplir le tableau
        table.setRowCount(len(self.bbs_file_list))
        for i, file_info in enumerate(self.bbs_file_list):
            # Nom
            name_item = QTableWidgetItem(file_info['name'])
            table.setItem(i, 0, name_item)
            
            # Taille formatée
            size = file_info['size']
            if size < 1024:
                size_str = f"{size} o"
            elif size < 1024 * 1024:
                size_str = f"{size / 1024:.1f} Ko"
            else:
                size_str = f"{size / (1024 * 1024):.1f} Mo"
            
            size_item = QTableWidgetItem(size_str)
            table.setItem(i, 1, size_item)
        
        # Double-clic pour télécharger
        table.doubleClicked.connect(lambda: self._download_bbs_file(table, dialog))
        
        layout.addWidget(table)
        
        # Bouton Rafraîchir et Fermer
        button_layout = QHBoxLayout()
        refresh_btn = QPushButton("🔄 " + tr("bbs_refresh"))
        refresh_btn.clicked.connect(lambda: self._refresh_bbs_list(dialog))
        button_layout.addWidget(refresh_btn)
        
        button_layout.addStretch()
        
        close_btn = QPushButton(tr("btn_close"))
        close_btn.clicked.connect(dialog.close)
        button_layout.addWidget(close_btn)
        
        layout.addLayout(button_layout)
        
        dialog.setLayout(layout)
        dialog.exec()
    
    def _download_bbs_file(self, table, dialog):
        """Télécharger le fichier sélectionné"""
        current_row = table.currentRow()
        if current_row < 0:
            return
        
        filename = table.item(current_row, 0).text()
        
        # Demander le fichier
        request = f"<BBSGET>{filename}</BBSGET><EOF>"
        self.vara.send_data(request.encode())
        self.log(f"📥 {tr('bbs_downloading')}: {filename}", "info")
        
        # Fermer la fenêtre BBS
        dialog.close()
    
    def _refresh_bbs_list(self, dialog):
        """Rafraîchir la liste BBS"""
        self.bbs_file_list = []
        self.vara.send_data("<BBSLIST><EOF>".encode())
        self.log(f"📁 {tr('bbs_refreshing')}", "info")
        
        # Fermer et rouvrir après 1 seconde
        dialog.close()
        QTimer.singleShot(1000, self.show_bbs_window)
    
    def send_file_path(self, filepath):
        """Envoyer un fichier depuis un chemin (utilisé par BBS)"""
        if not self.vara or not self.vara.session_active:
            self.log("⚠️ Pas de session active", "warning")
            return
        
        try:
            # Lire le fichier
            with open(filepath, 'rb') as f:
                file_data = f.read()
            
            file_name = os.path.basename(filepath)
            file_size_original = len(file_data)
            
            # Compresser
            compressed_data = zlib.compress(file_data, level=9)
            file_size_compressed = len(compressed_data)
            
            compression_ratio = (1 - file_size_compressed / file_size_original) * 100
            self.log(
                f"Compression : {file_size_original} → {file_size_compressed} octets "
                f"({compression_ratio:.1f}% de réduction)",
                "info"
            )
            
            # Encoder en base64
            encoded_data = base64.b64encode(compressed_data).decode('ascii')
            
            # Chiffrer si activé
            if self.crypto.enabled:
                self.log(f"🔒 {tr('encrypting_file')}...", "info")
                encrypted_data = self.crypto.encrypt(encoded_data)
                if encrypted_data.startswith("<ENC>") and encrypted_data.endswith("</ENC>"):
                    encoded_data = encrypted_data[5:-6]
                    file_encrypted = True
                else:
                    file_encrypted = False
            else:
                file_encrypted = False
            
            # Encoder le nom de fichier
            encoded_filename = base64.b64encode(file_name.encode('utf-8')).decode('ascii')
            
            # Métadonnées
            encrypted_flag = "1" if file_encrypted else "0"
            transfer_data_msg = f"<FILEDATA>{encrypted_flag}|{encoded_filename}|{file_size_original}|{file_size_compressed}|{encoded_data}<EOF>"
            transfer_size = len(transfer_data_msg)
            
            fileinfo_msg = f"<FILEINFO>{encrypted_flag}|{encoded_filename}|{file_size_original}|{file_size_compressed}|{transfer_size}<EOF>"
            
            self.log(f"{tr('sending_metadata')}: {file_name}", "info")
            self.vara.send_data(fileinfo_msg)
            
            # Attendre
            time.sleep(0.5)
            
            # Afficher barre de progression
            self.file_progress_bar.setVisible(True)
            self.file_progress_bar.setMaximum(transfer_size)
            self.file_progress_bar.setValue(0)
            self.file_progress_label.setText(f"📤 Envoi de {file_name}...")
            self.file_progress_label.setStyleSheet("color: blue; font-weight: bold;")
            
            # Variables pour le transfert
            self.file_transfer_active = True
            self.file_transfer_total = transfer_size
            self.file_transfer_progress = 0
            
            # Envoyer par chunks
            transfer_bytes = transfer_data_msg.encode('utf-8')
            total_bytes = len(transfer_bytes)
            chunk_size = 8192  # 8 KB par chunk
            bytes_sent = 0
            
            for i in range(0, total_bytes, chunk_size):
                QApplication.processEvents()
                chunk = transfer_bytes[i:i+chunk_size]
                self.vara.data_socket.send(chunk)
                bytes_sent += len(chunk)
                time.sleep(0.05)
                
                if bytes_sent % (chunk_size * 10) == 0:
                    QApplication.processEvents()
            
            # Surveiller la progression
            start_time = time.time()
            last_progress = 0
            
            while self.file_transfer_active:
                QApplication.processEvents()
                
                current_progress = self.file_transfer_progress
                
                if current_progress != last_progress:
                    bytes_done = int(self.file_transfer_total * current_progress / 100)
                    self.file_progress_bar.setValue(bytes_done)
                    
                    elapsed = time.time() - start_time
                    if elapsed > 1 and current_progress > 0:
                        speed = bytes_done / elapsed
                        self.file_progress_label.setText(
                            f"📤 Envoi de {file_name}: {current_progress}% ({speed:.0f} o/s)"
                        )
                    
                    last_progress = current_progress
                
                if current_progress >= 100:
                    self.file_transfer_active = False
                    break
                
                QApplication.processEvents()
                time.sleep(0.1)
                
                # Timeout après 5 minutes
                if time.time() - start_time > 300:
                    self.log("⚠️ Timeout envoi fichier", "warning")
                    break
            
            # Terminé
            self.file_progress_bar.setValue(transfer_size)
            self.file_progress_label.setText(f"✅ {tr('file_sent')}: {file_name}")
            self.file_progress_label.setStyleSheet("color: green; font-weight: bold;")
            
            # Masquer après 3 secondes
            QTimer.singleShot(3000, lambda: self.file_progress_bar.setVisible(False))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setText(tr("file_no_transfer")))
            QTimer.singleShot(3000, lambda: self.file_progress_label.setStyleSheet("color: gray; font-style: italic;"))
            
            self.log(f"✅ {tr('bbs_file_sent')}: {file_name}", "success")
        
        except Exception as e:
            self.log(f"❌ {tr('bbs_send_error')}: {e}", "error")
            import traceback
            traceback.print_exc()
            self.file_transfer_active = False


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("CRYPTARA")
    
    # Appliquer le style Fusion par défaut (ou depuis arguments)
    # Vérifie si --style est passé en argument
    if '--style' not in sys.argv:
        # Style Fusion par défaut pour un look moderne et uniforme
        app.setStyle('Fusion')
    # Sinon QApplication gère automatiquement l'argument --style
    
    window = MainWindow()
    window.show()
    
    sys.exit(app.exec())


if __name__ == '__main__':
    main()

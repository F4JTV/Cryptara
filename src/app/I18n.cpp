// I18n.cpp - the catalogue.  Keys follow the Python version's TRANSLATIONS
// where it had one; the texts it hard-coded in French have keys here too.
#include "I18n.h"

#include <QHash>
#include <QLocale>

namespace {

struct Entry
{
    const char *key;
    const char *en;
    const char *fr;
};

// clang-format off
const Entry kCatalogue[] = {
    // ---- menus
    {"menu_file", "File", "Fichier"},
    {"menu_tools", "Tools", "Outils"},
    {"menu_help", "Help", "Aide"},
    {"menu_quit", "Quit", "Quitter"},
    {"menu_qso_log", "QSO Log", "Journal QSO"},
    {"menu_macros", "Macros", "Macros"},
    {"menu_about", "About", "À propos"},
    {"menu_theme", "Theme", "Thème"},
    {"menu_save_log", "Save log", "Sauvegarder le journal"},
    {"menu_copy", "Copy", "Copier"},
    {"menu_copy_all", "Copy all", "Tout copier"},
    {"btn_save_chat", "Save chat", "Sauvegarder le chat"},
    {"btn_settings", "Settings", "Paramètres"},
    {"tooltips_enabled", "✓ Tooltips enabled", "✓ Infobulles activées"},
    {"tooltips_disabled", "Tooltips disabled", "Infobulles désactivées"},

    // ---- main window
    {"btn_connect", "Connect to Modem", "Connecter au modem"},
    {"btn_disconnect_modem", "Disconnect from modem", "Déconnecter du modem"},
    {"btn_cancel_connect", "Cancel connection", "Annuler la connexion"},
    {"lbl_station_short", "Station:", "Station :"},
    {"placeholder_station", "Station callsign", "Indicatif de la station"},
    {"placeholder_digi1", "Digipeater 1", "Digipeater 1"},
    {"placeholder_digi2", "Digipeater 2", "Digipeater 2"},
    {"via_na", "N/A (VARA FM only)", "N/A (VARA FM uniquement)"},
    {"tt_via1", "First digipeater (optional)", "Premier digipeater (optionnel)"},
    {"tt_via2", "Second digipeater (optional)", "Deuxième digipeater (optionnel)"},
    {"tt_via_unsupported", "Digipeaters not supported by %1\n(VARA FM only)",
                           "Digipeaters non supportés par %1\n(Uniquement VARA FM)"},
    {"tt_bw_select", "Select the bandwidth", "Sélectionner la bande passante"},
    {"tt_bw_fm", "Bandwidth configured in VARA FM", "Bande passante configurée dans VARA FM"},
    {"tt_bw_sat", "Bandwidth configured in VARA SAT", "Bande passante configurée dans VARA SAT"},
    {"btn_call", "Call", "Appeler"},
    {"btn_disconnect", "Disconnect", "Déconnecter"},
    {"btn_encryption", "Encryption", "Chiffrement"},
    {"bbs_button", "BBS", "BBS"},
    {"bbs_button_tooltip", "Access remote station BBS", "Accéder au BBS de la station distante"},
    {"group_chat", "Chat", "Chat"},
    {"group_cq", "📡 Received CQ", "📡 CQ Reçus"},
    {"group_system_log", "System Log", "Journal système"},
    {"placeholder_message", "Your message...", "Votre message..."},
    {"btn_send", "Send", "Envoyer"},
    {"btn_file", "File", "Fichier"},
    {"btn_send_cq", "Send CQ", "Envoyer CQ"},
    {"btn_clear_cq", "Clear", "Effacer"},
    {"tt_clear_cq", "Clear the list of received CQs", "Effacer la liste des CQ reçus"},
    {"col_callsign", "Callsign", "Indicatif"},
    {"col_time", "Time", "Heure"},
    {"col_start", "Start", "Début"},
    {"col_end", "End", "Fin"},
    {"col_duration", "Duration", "Durée"},
    {"file_select", "Select a file to send", "Sélectionner un fichier à envoyer"},
    {"file_all_files", "All files", "Tous les fichiers"},
    {"status_connected", "Connected", "Connecté"},
    {"status_disconnected", "Disconnected", "Déconnecté"},
    {"status_connecting", "Connecting...", "Connexion en cours..."},
    {"session", "Session", "Session"},
    {"no_session", "No session", "Pas de session"},
    {"file_no_transfer", "No transfer in progress", "Aucun transfert en cours"},

    // ---- tooltips, main window (Python tt_*)
    {"tt_connect_btn", "Connect to VARA modem / Disconnect", "Se connecter au modem VARA / Se déconnecter"},
    {"tt_call_btn", "Call a remote station (requires modem connection)",
                    "Appeler une station distante (nécessite connexion au modem)"},
    {"tt_cq_btn", "Send a CQ to announce your presence", "Envoyer un CQ pour annoncer votre présence"},
    {"tt_disconnect_btn", "Disconnect active session with remote station",
                          "Déconnecter la session active avec la station distante"},
    {"tt_message_input", "Type your message here (Enter to send)", "Saisissez votre message ici (Entrée pour envoyer)"},
    {"tt_send_btn", "Send message (or press Enter)", "Envoyer le message (ou appuyez sur Entrée)"},
    {"tt_file_btn", "Send a file to remote station", "Envoyer un fichier à la station distante"},
    {"tt_crypto_btn", "Enable/disable AES-256 encryption for this session",
                      "Activer/désactiver le chiffrement AES-256 pour cette session"},
    {"tt_chat_display", "Message history of the conversation", "Historique des messages de la conversation"},
    {"tt_log_display", "System log with events and status", "Journal système avec les événements et statuts"},
    {"tt_cq_table", "List of received CQs (double-click to connect)",
                    "Liste des CQ reçus (double-clic pour se connecter)"},

    // ---- settings dialog
    {"dlg_settings", "Settings", "Paramètres"},
    {"tab_modem", "Modem", "Modem"},
    {"tab_info_station", "Station Info", "Info station"},
    {"tab_auto_responder", "Auto-responder", "Auto-répondeur"},
    {"tab_interface", "Interface", "Interface"},
    {"tab_bbs", "BBS", "BBS"},
    {"lbl_modem_type", "Modem type:", "Type de modem :"},
    {"lbl_vara_paths", "VARA executable paths", "Chemins des exécutables VARA"},
    {"lbl_auto_start", "Start VARA automatically at startup", "Lancer VARA automatiquement au démarrage"},
    {"lbl_host", "Host:", "Hôte :"},
    {"lbl_cmd_port", "Command port:", "Port commandes :"},
    {"lbl_data_port", "Data port:", "Port données :"},
    {"lbl_listen", "Listen mode at startup", "Mode écoute au démarrage"},
    {"lbl_compression", "Compression enabled", "Compression activée"},
    {"dlg_select_vara", "Select the VARA executable", "Sélectionner l'exécutable VARA"},
    {"filter_exe", "Executables (*.exe)", "Exécutables (*.exe)"},
    {"lbl_station_info", "Station Information", "Informations de la station"},
    {"lbl_callsign_required", "Callsign: *", "Indicatif : *"},
    {"lbl_name", "Name:", "Nom :"},
    {"lbl_firstname", "First Name:", "Prénom :"},
    {"lbl_qth", "QTH:", "QTH :"},
    {"lbl_grid_square", "Grid Square:", "Locator :"},
    {"lbl_rig", "Rig:", "Rig :"},
    {"lbl_antenna", "Antenna:", "Antenne :"},
    {"placeholder_callsign", "e.g. N0CALL", "Ex : N0CALL"},
    {"placeholder_name", "e.g. Smith", "Ex : Dupont"},
    {"placeholder_firstname", "e.g. John", "Ex : Jean"},
    {"placeholder_qth", "e.g. New York, USA", "Ex : Nice, France"},
    {"placeholder_grid", "e.g. JN33uo", "Ex : JN33uo"},
    {"placeholder_rig", "e.g. Yaesu FT-891", "Ex : Yaesu FT-891"},
    {"placeholder_antenna", "e.g. Diamond V2000", "Ex : Diamond V2000"},
    {"required_field_note", "* Required field for modem connection", "* Champ requis pour la connexion au modem"},
    {"lbl_enable_auto", "Enable auto-responder", "Activer l'auto-répondeur"},
    {"lbl_auto_message", "Automatic message:", "Message automatique :"},
    {"placeholder_auto", "Example: Hello {CALLSIGN}, thanks for the call. I am in automatic QRV mode. 73!",
                         "Exemple : Bonjour {CALLSIGN}, merci pour l'appel. Je suis en QRV automatique. 73!"},
    {"lbl_auto_disconnect", "Disconnect after reply", "Déconnecter après réponse"},
    {"lbl_auto_delay", "Delay before reply (seconds):", "Délai avant réponse (secondes) :"},
    {"lbl_timestamp", "Show timestamps", "Afficher l'horodatage"},
    {"lbl_sound", "Sound notifications", "Notifications sonores"},
    {"lbl_save_log", "Save the log on closing", "Sauvegarder le journal à la fermeture"},
    {"file_rx_folder", "Received files folder:", "Dossier fichiers reçus :"},
    {"file_rx_folder_placeholder", "Leave empty to ask for each file", "Laisser vide pour demander à chaque fichier"},
    {"file_rx_folder_browse", "Select folder for received files",
                              "Sélectionner le dossier de sauvegarde des fichiers reçus"},
    {"lbl_theme", "Theme:", "Thème :"},
    {"theme_light", "Light", "Clair"},
    {"theme_dark", "Dark", "Sombre"},
    {"theme_red", "Red (night)", "Rouge (nuit)"},
    {"theme_amber", "Amber", "Ambre"},
    {"lbl_language", "Language:", "Langue :"},
    {"lang_auto", "System language", "Langue du système"},
    {"bbs_enable", "Enable BBS (file sharing)", "Activer le BBS (partage de fichiers)"},
    {"bbs_shared_folder", "Shared folder:", "Dossier partagé :"},
    {"bbs_folder_placeholder", "Select folder to share", "Sélectionner le dossier à partager"},
    {"bbs_info", "Files in this folder will be accessible\nto stations connecting to you.",
                 "Les fichiers de ce dossier seront accessibles\naux stations qui se connectent à vous."},
    {"bbs_folder_browse", "Select BBS folder to share", "Sélectionner le dossier BBS à partager"},
    {"btn_ok", "OK", "OK"},
    {"btn_cancel", "Cancel", "Annuler"},
    {"btn_yes", "Yes", "Oui"},
    {"btn_no", "No", "Non"},
    {"btn_close", "Close", "Fermer"},
    {"btn_save", "Save", "Sauvegarder"},
    {"btn_browse", "Browse", "Parcourir"},

    // ---- tooltips, settings (Python tt_*)
    {"tt_modem_type", "VARA modem type to use (HF, FM or SAT)", "Type de modem VARA à utiliser (HF, FM ou SAT)"},
    {"tt_vara_path", "Path to VARA executable on your system", "Chemin vers l'exécutable VARA sur votre système"},
    {"tt_auto_start", "Automatically launch VARA when CRYPTARA starts",
                      "Lancer automatiquement VARA au démarrage de CRYPTARA"},
    {"tt_host", "VARA modem IP address (localhost for local)", "Adresse IP du modem VARA (localhost pour local)"},
    {"tt_cmd_port", "TCP port for VARA commands (default: 8300)", "Port TCP pour les commandes VARA (défaut : 8300)"},
    {"tt_data_port", "TCP port for VARA data (default: 8301)", "Port TCP pour les données VARA (défaut : 8301)"},
    {"tt_listen_mode", "Enable listen mode at startup (allows receiving calls)",
                       "Activer le mode écoute au démarrage (permet de recevoir des appels)"},
    {"tt_compression", "Enable data compression to optimize transfer",
                       "Activer la compression des données pour optimiser le transfert"},
    {"tt_callsign", "Your amateur radio callsign (required for connection)",
                    "Votre indicatif radioamateur (requis pour la connexion)"},
    {"tt_name", "Your last name (optional, usable in macros)", "Votre nom de famille (optionnel, utilisable dans les macros)"},
    {"tt_firstname", "Your first name (optional, usable in macros)", "Votre prénom (optionnel, utilisable dans les macros)"},
    {"tt_qth", "Your geographic location (optional, e.g. New York, USA)",
               "Votre localisation géographique (optionnel, ex : Nice, France)"},
    {"tt_grid", "Your Maidenhead locator (optional, 6 characters, e.g. FN30as)",
                "Votre locator Maidenhead (optionnel, 6 caractères, ex : JN33uo)"},
    {"tt_rig", "Your radio transceiver (optional, usable in macros)",
               "Votre transceiver radio (optionnel, utilisable dans les macros)"},
    {"tt_antenna", "Your antenna (optional, usable in macros)", "Votre antenne (optionnel, utilisable dans les macros)"},
    {"tt_auto_enable", "Enable auto-responder on incoming connections",
                       "Activer l'auto-répondeur lors des connexions entrantes"},
    {"tt_auto_message", "Message sent automatically on incoming connection",
                        "Message envoyé automatiquement lors d'une connexion entrante"},
    {"tt_auto_disconnect", "Automatically disconnect after sending message",
                           "Se déconnecter automatiquement après l'envoi du message"},
    {"tt_auto_delay", "Delay in seconds before sending automatic message",
                      "Délai en secondes avant l'envoi du message automatique"},
    {"tt_timestamp", "Display timestamp on each chat message", "Afficher l'horodatage sur chaque message du chat"},
    {"tt_sound", "Play sound on message reception", "Jouer un son lors de la réception de messages"},
    {"tt_save_log", "Automatically save log on close", "Sauvegarder automatiquement le journal à la fermeture"},
    {"tt_file_rx_path", "Destination folder for received files", "Dossier de destination des fichiers reçus"},
    {"tt_bbs_enable", "Enable BBS (Bulletin Board System) server on connections",
                      "Activer le serveur BBS (Bulletin Board System) lors des connexions"},
    {"tt_bbs_folder", "Folder containing files shared via BBS", "Dossier contenant les fichiers partagés via le BBS"},
    {"tt_theme", "Colours of the window; red keeps your night vision", "Couleurs de la fenêtre ; le rouge préserve la vision de nuit"},
    {"tt_language", "Language of the interface", "Langue de l'interface"},

    // ---- macros dialog
    {"dlg_macros_title", "Macro Configuration", "Configuration des Macros"},
    {"macros_info_title", "F1-F12 Macro Configuration", "Configuration des macros F1-F12"},
    {"macros_variables", "Available variables: {MYCALL}, {NAME}, {FIRSTNAME}, {QTH}, {GRID}, {RIG}, {ANTENNA}, {CALLSIGN}, {TIME}, {DATE}",
                         "Variables disponibles : {MYCALL}, {NAME}, {FIRSTNAME}, {QTH}, {GRID}, {RIG}, {ANTENNA}, {CALLSIGN}, {TIME}, {DATE}"},
    {"macro_placeholder", "Example: My QTH is {QTH}", "Exemple : Mon QTH est {QTH}"},
    {"btn_test_macro", "Test F1", "Tester F1"},
    {"tt_macro_field", "Macro F%1: predefined message with variables {MYCALL}, {QTH}, etc.",
                       "Macro F%1 : message prédéfini avec variables {MYCALL}, {QTH}, etc."},
    {"tt_test_macro", "Test F1 macro to see result after variable expansion",
                      "Tester la macro F1 pour voir le résultat après expansion des variables"},
    {"dlg_test_macro", "Macro Test", "Test de macro"},
    {"test_macro_original", "Original text:", "Texte original :"},
    {"test_macro_expanded", "After expansion:", "Après expansion :"},
    {"macros_saved", "Macros saved", "Macros sauvegardées"},
    {"macro_sent", "Macro %1 sent", "Macro %1 envoyée"},
    {"macro_empty", "Macro %1 empty - configure it in Tools → Macros", "Macro %1 vide - configurez-la dans Outils → Macros"},
    {"macro_no_session", "No active session - Macro not sent", "Pas de session active - Macro non envoyée"},

    // ---- QSO log, BBS, about dialogs
    {"dlg_qso_log", "QSO Log", "Journal QSO"},
    {"qso_in_progress", "In progress", "En cours"},
    {"bbs_window_title", "BBS", "BBS"},
    {"bbs_double_click_info", "Double-click a file to download", "Double-cliquez sur un fichier pour le télécharger"},
    {"bbs_refresh", "Refresh", "Rafraîchir"},
    {"lbl_filename", "Filename", "Nom du fichier"},
    {"lbl_size", "Size", "Taille"},
    {"unit_bytes", "B", "o"},
    {"unit_kb", "KB", "Ko"},
    {"unit_mb", "MB", "Mo"},
    {"dlg_about", "About CRYPTARA", "À propos de CRYPTARA"},
    {"about_description", "Secure VARA client with AES-256 encryption", "Client VARA sécurisé avec chiffrement AES-256"},
    {"about_features", "VARA HF/FM/SAT Support • Perfect Forward Secrecy • File Transfer",
                       "Support VARA HF/FM/SAT • Perfect Forward Secrecy • Transfert de fichiers"},
    {"about_version", "Version:", "Version :"},
    {"about_crypto", "Cryptography:", "Cryptographie :"},

    // ---- log and status messages
    {"log_modem_disconnected", "Disconnected from modem", "Déconnecté du modem"},
    {"connected_to_modem", "Connected to %1 (%2:%3)", "Connecté à %1 (%2:%3)"},
    {"error_connection", "Connection error", "Erreur de connexion"},
    {"modem_link_lost", "Connection to the modem lost", "Connexion au modem perdue"},
    {"connected_to", "Connected to", "Connecté à"},
    {"disconnected_from", "Disconnected from", "Déconnecté de"},
    {"connecting_to", "Connecting to", "Connexion en cours vers"},
    {"call_cancelled", "Call cancelled", "Appel annulé"},
    {"calling", "Calling", "Appel de"},
    {"on", "on", "sur"},
    {"bandwidth", "bandwidth", "bande passante"},
    {"channel", "Channel", "Canal"},
    {"busy", "busy", "occupé"},
    {"free", "free", "libre"},
    {"vara_version", "VARA version", "Version VARA"},
    {"vara_error", "VARA error", "Erreur VARA"},
    {"connection_timeout", "Connection timeout", "Timeout de connexion"},
    {"cancelling", "cancelling", "annulation"},
    {"cq_sent", "CQ call sent", "Appel CQ envoyé"},
    {"cq_received_from", "CQ received from", "CQ reçu de"},
    {"cq_cleared", "CQ list cleared", "Liste des CQ effacée"},
    {"unknown_modem_type", "Unknown modem type", "Type de modem inconnu"},
    {"modem_path_not_configured", "%1 path not configured", "Chemin de %1 non configuré"},
    {"open_settings_to_configure", "Open settings to configure the executable path",
                                   "Ouvrez les paramètres pour configurer le chemin de l'exécutable"},
    {"executable_not_found", "Executable not found", "Exécutable introuvable"},
    {"launching", "Launching", "Lancement de"},
    {"launched_successfully", "launched successfully", "lancé avec succès"},
    {"launch_failed", "Launch failed", "Échec du lancement"},
    {"closing_vara", "Closing VARA", "Fermeture de VARA"},
    {"disconnecting_from", "Disconnecting from %1...", "Déconnexion de %1..."},
    {"closing_modem", "Closing %1...", "Fermeture de %1..."},
    {"starting_modem", "Starting %1...", "Démarrage de %1..."},
    {"settings_save_error", "Settings save error", "Erreur sauvegarde paramètres"},
    {"settings_load_error", "Settings read error", "Erreur lecture paramètres"},
    {"session_ended", "session ended", "session terminée"},
    {"me", "Me", "Moi"},
    {"other_station", "The other station", "L'autre station"},
    {"your_peer", "Your peer", "L'interlocuteur"},
    {"auto_responder_enabled", "Auto-responder enabled", "Auto-répondeur activé"},
    {"delay", "delay", "délai"},
    {"chat_saved", "Chat saved", "Chat sauvegardé"},
    {"chat_save_error", "Cannot save the chat", "Impossible de sauvegarder le chat"},
    {"log_saved", "Log saved", "Journal sauvegardé"},
    {"save_error", "Save error", "Erreur sauvegarde"},

    // ---- encryption, log
    {"crypto_enabled", "AES-256 encryption enabled (PBKDF2 + HMAC)", "Chiffrement AES-256 activé (PBKDF2 + HMAC)"},
    {"log_encryption_enabled", "Encryption enabled", "Chiffrement activé"},
    {"log_encryption_disabled", "Encryption disabled", "Chiffrement désactivé"},
    {"crypto_on_notification_sent", "Reactivation notification sent", "Notification de réactivation envoyée"},
    {"crypto_off_notification_sent", "Deactivation notification sent", "Notification de désactivation envoyée"},
    {"crypto_sending_request", "Sending encryption activation request", "Envoi d'une demande d'activation du chiffrement"},
    {"crypto_request_sent", "Request sent - Waiting for response", "Demande envoyée - En attente de la réponse"},
    {"crypto_request_received", "Encryption request received", "Demande de chiffrement reçue"},
    {"crypto_request_refused", "Encryption request refused", "Demande de chiffrement refusée"},
    {"crypto_password_not_entered", "Password not entered", "Mot de passe non saisi"},
    {"crypto_generating_our_dh", "Generating our DH key pair", "Génération de notre paire de clés DH"},
    {"crypto_response_sent_with_key", "Response sent with our public key", "Réponse envoyée avec notre clé publique"},
    {"crypto_dh_session_established", "DH session established", "Session DH établie"},
    {"crypto_pfs_enabled", "Perfect Forward Secrecy enabled", "Perfect Forward Secrecy activé"},
    {"crypto_error_shared_secret", "Error computing shared secret", "Erreur lors du calcul du secret partagé"},
    {"crypto_error_dh_keygen", "Could not generate a Diffie-Hellman key pair", "Impossible de générer une paire de clés Diffie-Hellman"},
    {"crypto_response_received", "Encryption response received", "Réponse de chiffrement reçue"},
    {"crypto_error_dh_session", "Error establishing DH session", "Erreur lors de l'établissement de la session DH"},
    {"crypto_other_refused", "The other station refused encryption", "L'autre station a refusé le chiffrement"},
    {"crypto_other_disabled", "The other station has disabled encryption", "L'autre station a désactivé le chiffrement"},
    {"crypto_off_ack_sent", "Deactivation confirmation sent", "Confirmation de désactivation envoyée"},
    {"crypto_kept_active", "Encryption kept active", "Chiffrement maintenu actif"},
    {"crypto_off_refused_sent", "Deactivation refusal sent", "Refus de désactivation envoyé"},
    {"crypto_other_also_disabled", "The other station has also disabled encryption",
                                   "L'autre station a également désactivé le chiffrement"},
    {"crypto_other_wants_keep", "The other station wants to keep encryption", "L'autre station souhaite maintenir le chiffrement"},
    {"continuing_without_encryption", "You continue without encryption", "Vous continuez sans chiffrement"},
    {"crypto_reactivated", "Encryption reactivated", "Chiffrement réactivé"},
    {"reactivation_cancelled", "Reactivation cancelled", "Réactivation annulée"},
    {"crypto_other_reactivated", "The other station has reactivated encryption", "L'autre station a réactivé le chiffrement"},
    {"crypto_on_ack_sent", "Reactivation confirmation sent", "Confirmation de réactivation envoyée"},
    {"crypto_other_also_reactivated", "The other station has also reactivated encryption",
                                      "L'autre station a également réactivé le chiffrement"},
    {"dh_key_received_old_format", "DH public key received (old format)", "Clé publique DH reçue (ancien format)"},
    {"generating_our_key_pair", "Generating our key pair", "Génération de notre paire de clés"},
    {"dh_key_sent_response", "DH public key sent in response", "Clé publique DH envoyée en réponse"},
    {"dh_session_established", "Diffie-Hellman session established", "Session Diffie-Hellman établie"},
    {"encryption_failed_not_sent", "Encryption failed - nothing was sent", "Échec du chiffrement - rien n'a été envoyé"},
    {"dec_no_password", "[Encrypted message - password not configured]", "[Message chiffré - mot de passe non configuré]"},
    {"dec_no_session", "[Encrypted message - DH session not established]", "[Message chiffré - session DH non établie]"},
    {"dec_bad_hmac", "[Encrypted message - invalid HMAC - wrong password?]", "[Message chiffré - HMAC invalide - mauvais mot de passe ?]"},
    {"dec_disabled", "[Encrypted message - encryption is off]", "[Message chiffré - chiffrement désactivé]"},
    {"dec_failed", "[Encrypted message - decryption error]", "[Message chiffré - erreur de déchiffrement]"},

    // ---- encryption, dialogs
    {"crypto_setup", "Encryption setup", "Configuration du chiffrement"},
    {"crypto_password_prompt", "Enter the encryption password:\n(Share this password securely with your contact)",
                               "Entrez le mot de passe de chiffrement :\n(Partagez ce mot de passe avec votre interlocuteur de manière sécurisée)"},
    {"pfs_question", "Do you want to establish a Diffie-Hellman session?\n\nThis improves security with Perfect Forward Secrecy (PFS).\nThe other station will be invited to enable encryption.",
                     "Voulez-vous établir une session Diffie-Hellman ?\n\nCela améliore la sécurité avec Perfect Forward Secrecy (PFS).\nL'autre station sera invitée à activer le chiffrement."},
    {"dlg_encryption_enabled", "Encryption enabled", "Chiffrement activé"},
    {"msg_encryption_enabled_session", "🔒 AES-256 encryption enabled\n\nMode: PBKDF2 (100k iterations) + HMAC-SHA256\nAll messages sent will be encrypted.\nYour peer must use the same password.",
                                       "🔒 Chiffrement AES-256 activé\n\nMode : PBKDF2 (100k iterations) + HMAC-SHA256\nTous les messages envoyés seront chiffrés.\nVotre interlocuteur doit utiliser le même mot de passe."},
    {"msg_encryption_enabled_no_session", "🔒 AES-256 encryption enabled\n\nMode: PBKDF2 (100k iterations) + HMAC-SHA256\nConnect to a station to establish a secure session.",
                                          "🔒 Chiffrement AES-256 activé\n\nMode : PBKDF2 (100k iterations) + HMAC-SHA256\nConnectez-vous à une station pour établir une session sécurisée."},
    {"must_be_connected_station", "You must be connected to a station.", "Vous devez être connecté à une station."},
    {"dlg_encryption_request", "Encryption request", "Demande de chiffrement"},
    {"msg_wants_encrypted_session", "%1 wants to establish an encrypted session.\n\nDo you want to activate encryption?\n\nYou must enter the same password as your peer",
                                    "%1 souhaite établir une session chiffrée.\n\nVoulez-vous activer le chiffrement ?\n\nVous devrez saisir le même mot de passe que votre interlocuteur"},
    {"dlg_encryption_password", "Encryption password", "Mot de passe de chiffrement"},
    {"msg_enter_shared_password", "Enter the password shared with %1:", "Entrez le mot de passe partagé avec %1 :"},
    {"msg_secure_session_ready", "✅ Secure session established!\n\nAES-256 encryption + Perfect Forward Secrecy enabled.\nYour communications are now protected.",
                                 "✅ Session sécurisée établie !\n\nChiffrement AES-256 + Perfect Forward Secrecy activés.\nVos communications sont maintenant protégées."},
    {"dlg_secure_session_established", "Secure session established", "Session sécurisée établie"},
    {"msg_dh_success_long", "✅ Diffie-Hellman exchange successful!\n\nPerfect Forward Secrecy (PFS) enabled.\nMessages now use a unique session key.\n\nMaximum security: even if the password is compromised,\nolder messages remain secure.",
                            "✅ Échange Diffie-Hellman réussi !\n\nPerfect Forward Secrecy (PFS) activé.\nLes messages utilisent maintenant une clé de session unique.\n\nSécurité maximale : même si le mot de passe est compromis,\nles anciens messages restent sécurisés."},
    {"dlg_dh_error", "DH error", "Erreur DH"},
    {"msg_dh_failed_password_used", "Cannot establish the Diffie-Hellman session.\nPassword encryption will be used.",
                                    "Impossible d'établir la session Diffie-Hellman.\nLe chiffrement par mot de passe sera utilisé."},
    {"dlg_encryption_refused", "Encryption refused", "Chiffrement refusé"},
    {"msg_peer_refused_encryption", "%1 refused to enable encryption.\n\nYou can continue unencrypted or disconnect.",
                                    "%1 a refusé d'activer le chiffrement.\n\nVous pouvez continuer en mode non chiffré ou déconnecter."},
    {"dlg_encryption_disabled_by_peer", "Encryption disabled by your peer", "Chiffrement désactivé par l'interlocuteur"},
    {"msg_peer_disabled_encryption", "📢 Information\n\n%1 has disabled encryption.", "📢 Information\n\n%1 a désactivé le chiffrement."},
    {"msg_also_disable_question", "Do you also want to disable encryption?\n\n• YES: encryption will be disabled on both sides\n• NO: you will keep encrypting your messages",
                                  "Souhaitez-vous également désactiver le chiffrement ?\n\n• OUI : le chiffrement sera désactivé des deux côtés\n• NON : vous continuerez à chiffrer vos messages"},
    {"dlg_encryption_maintained", "Encryption maintained", "Chiffrement maintenu"},
    {"msg_keep_encrypting", "You keep encrypting your messages.\n\n%1 has been told that you want to keep encryption.",
                            "Vous continuez à chiffrer vos messages.\n\n%1 a été informé(e) que vous souhaitez maintenir le chiffrement."},
    {"dlg_sync_success", "Synchronization successful", "Synchronisation réussie"},
    {"msg_both_clear", "%1 has also disabled encryption.\n\nYou are both communicating in clear now.",
                       "%1 a également désactivé le chiffrement.\n\nVous communiquez maintenant tous les deux en clair."},
    {"dlg_deactivation_refused", "Deactivation refused", "Refus de désactivation"},
    {"msg_desync", "⚠️ Encryption desynchronization\n\n%1 wants to keep encryption active.",
                   "⚠️ Désynchronisation du chiffrement\n\n%1 souhaite maintenir le chiffrement actif."},
    {"msg_desync_details", "Current situation:\n• You: encryption OFF\n• %1: encryption ON\n\nConsequences:\n• Your clear messages will be received normally\n• Your peer's encrypted messages cannot be decrypted\n\nDo you want to reactivate encryption?",
                           "Situation actuelle :\n• Vous : chiffrement DÉSACTIVÉ\n• %1 : chiffrement ACTIVÉ\n\nConséquences :\n• Vos messages en clair seront reçus normalement\n• Les messages chiffrés de l'interlocuteur ne seront pas déchiffrables\n\nVoulez-vous réactiver le chiffrement ?"},
    {"dlg_encryption_reactivated", "Encryption reactivated", "Chiffrement réactivé"},
    {"msg_peer_reactivated", "📢 Information\n\n%1 has reactivated encryption.", "📢 Information\n\n%1 a réactivé le chiffrement."},
    {"msg_already_encrypting", "Your encryption is already active.\n\nYou are both communicating encrypted.",
                               "Votre chiffrement est déjà activé.\n\nVous communiquez tous les deux de manière chiffrée."},
    {"msg_also_reactivate_question", "Your encryption is currently disabled.\n\nDo you also want to reactivate encryption?\n\n• YES: encryption enabled, secure communication\n• NO: you stay in clear (desynchronized)",
                                     "Votre chiffrement est actuellement désactivé.\n\nSouhaitez-vous également réactiver le chiffrement ?\n\n• OUI : chiffrement activé, communication sécurisée\n• NON : vous restez en clair (désynchronisé)"},
    {"msg_both_encrypted", "%1 has also reactivated encryption.\n\nYou are both communicating encrypted now.",
                           "%1 a également réactivé le chiffrement.\n\nVous communiquez maintenant tous les deux de manière chiffrée."},

    // ---- calls, CQ
    {"dlg_error", "Error", "Erreur"},
    {"dlg_success", "Success", "Succès"},
    {"modem_not_connected", "Modem not connected", "Modem non connecté"},
    {"session_already_active", "A session is already active", "Une session est déjà active"},
    {"call_already_in_progress", "A call is already in progress", "Un appel est déjà en cours"},
    {"configure_callsign", "Please configure your callsign in the settings", "Veuillez configurer votre indicatif dans les paramètres"},
    {"enter_callsign", "Please enter a callsign", "Veuillez entrer un indicatif"},
    {"cannot_call_yourself", "You cannot call yourself!", "Vous ne pouvez pas vous appeler vous-même !"},
    {"channel_busy_title", "Channel busy", "Canal occupé"},
    {"channel_busy_question", "The channel is busy. Do you still want to try the call?",
                              "Le canal est occupé. Voulez-vous quand même tenter l'appel ?"},
    {"no_cq_in_session", "Cannot send a CQ during an active session", "Impossible d'envoyer un CQ pendant une session active"},
    {"cq_connect_title", "Connect to a CQ station", "Connexion à une station CQ"},
    {"cq_connect_question", "Do you want to connect to %1?", "Voulez-vous vous connecter à %1 ?"},
    {"cq_clear_title", "Clear CQs", "Effacer les CQ"},
    {"cq_clear_question", "Do you want to clear the list of received CQs?", "Voulez-vous effacer la liste des CQ reçus ?"},
    {"restart_required", "Restart Required", "Redémarrage requis"},
    {"connection_params_changed", "Connection parameters changed.\nPlease disconnect and reconnect to apply changes.",
                                  "Paramètres de connexion modifiés.\nVeuillez vous déconnecter et reconnecter pour appliquer les changements."},
    {"dlg_executable_not_found", "Executable not found", "Exécutable introuvable"},
    {"msg_executable_missing", "The %1 executable cannot be found:\n%2\n\nConfigure the path in the settings.",
                               "L'exécutable %1 est introuvable :\n%2\n\nConfigurez le chemin dans les paramètres."},
    {"dlg_launch_failed", "Launch error", "Erreur de lancement"},
    {"msg_cannot_launch", "Cannot launch %1.\n\nCheck the executable path in the settings.",
                          "Impossible de lancer %1.\n\nVérifiez le chemin de l'exécutable dans les paramètres."},

    // ---- files
    {"no_active_session", "No active session", "Pas de session active"},
    {"transfer_already_running", "A file transfer is already in progress", "Un transfert de fichier est déjà en cours"},
    {"transfer_interrupted", "File transfer interrupted", "Transfert de fichier interrompu"},
    {"cannot_send_file", "Cannot send the file:", "Impossible d'envoyer le fichier :"},
    {"error_sending_file", "Error sending file", "Erreur envoi fichier"},
    {"dlg_file_too_large", "File too large", "Fichier trop volumineux"},
    {"msg_file_too_large", "The maximum size is 2 MB.\nFile size: %1 MB\n\n💡 Tip: compress the file as ZIP before sending\nto reduce its size.",
                           "La taille maximale est de 2 MB.\nTaille du fichier : %1 MB\n\n💡 Conseil : compressez le fichier en ZIP avant l'envoi\npour réduire sa taille."},
    {"compression_log", "Compression: %1 → %2 bytes (%3% reduction)", "Compression : %1 → %2 octets (%3% de réduction)"},
    {"decompression_log", "Decompression: %1 → %2 bytes (%3% reduction)", "Décompression : %1 → %2 octets (%3% de réduction)"},
    {"dlg_large_file", "Large file", "Fichier volumineux"},
    {"msg_large_file", "The compressed file is %1 KB.\n\nEstimated sending time:\n• NARROW: ~%2 min %3 sec\n• WIDE: ~%4 min %5 sec\n\nDo you want to continue?",
                       "Le fichier compressé fait %1 KB.\n\nTemps d'envoi estimé :\n• NARROW : ~%2 min %3 sec\n• WIDE : ~%4 min %5 sec\n\nVoulez-vous continuer ?"},
    {"send_cancelled_by_user", "Send cancelled by user", "Envoi annulé par l'utilisateur"},
    {"encrypting_file", "Encrypting file", "Chiffrement du fichier"},
    {"sending_encrypted_file", "Sending encrypted file", "Envoi fichier chiffré"},
    {"sending_metadata", "Sending metadata", "Envoi des métadonnées"},
    {"sending", "Sending", "Envoi de"},
    {"sending_file_log", "Sending file: %1 (%2 bytes, compressed: %3 bytes)", "Envoi du fichier : %1 (%2 octets, compressé : %3 octets)"},
    {"sending_file_in_chunks", "Sending file in chunks", "Envoi du fichier en chunks"},
    {"data_sent_to_buffer", "Data sent to VARA buffer", "Données envoyées au buffer VARA"},
    {"bytes", "bytes", "octets"},
    {"bytes_per_second", "B/s", "o/s"},
    {"file_sent", "File sent", "Fichier envoyé"},
    {"file_send_timeout", "File send timeout", "Timeout envoi fichier"},
    {"chat_file_sent", "File sent: %1 (%2 bytes, compressed by %3%)", "Fichier envoyé : %1 (%2 octets, compressé à %3%)"},
    {"chat_file_received", "File received: %1 (%2 bytes, compressed by %3%)", "Fichier reçu : %1 (%2 octets, compressé à %3%)"},
    {"receiving", "Receiving", "Réception de"},
    {"receiving_file", "Receiving file", "Réception du fichier"},
    {"receiving_encrypted_file", "Receiving encrypted file", "Réception fichier chiffré"},
    {"receiving_file_log", "Receiving %1: %2 bytes (compressed: %3, transfer: %4)",
                           "Réception de %1 : %2 octets (compressé : %3, transfert : %4)"},
    {"encrypted", "encrypted", "chiffré"},
    {"decrypting", "Decrypting", "Déchiffrement de"},
    {"decoding", "Decoding", "Décodage de"},
    {"decompressing", "Decompressing", "Décompression de"},
    {"saving", "Saving", "Enregistrement"},
    {"invalid_fileinfo_format", "Invalid FILEINFO format", "Format FILEINFO invalide"},
    {"invalid_filedata_format", "Invalid FILEDATA format", "Format FILEDATA invalide"},
    {"invalid_file_format", "Invalid file format", "Format de fichier invalide"},
    {"fileinfo_error", "FILEINFO error", "Erreur FILEINFO"},
    {"cannot_decrypt_file", "Cannot decrypt file", "Impossible de déchiffrer le fichier"},
    {"dlg_decryption_error", "Decryption error", "Erreur de déchiffrement"},
    {"msg_cannot_decrypt_file", "Cannot decrypt file %1.\n\nCheck that you have the same password as the sender.",
                                "Impossible de déchiffrer le fichier %1.\n\nVérifiez que vous avez le même mot de passe que l'expéditeur."},
    {"file_decrypted", "File decrypted", "Fichier déchiffré"},
    {"wrong_compressed_size", "Wrong compressed size: received %1, expected %2", "Taille compressée incorrecte : reçu %1, attendu %2"},
    {"wrong_original_size", "Wrong decompressed size: got %1, expected %2", "Taille décompressée incorrecte : obtenu %1, attendu %2"},
    {"decompression_error", "Decompression error", "Erreur de décompression"},
    {"corrupted_data", "corrupted data", "données corrompues"},
    {"save_received_file", "Save the received file", "Enregistrer le fichier reçu"},
    {"file_receive_cancelled", "File receive cancelled", "Réception de fichier annulée"},
    {"file_reception_error", "File reception error", "Erreur réception fichier"},
    {"cannot_receive_file", "Cannot receive the file:", "Impossible de recevoir le fichier :"},
    {"file_received", "File received", "Fichier reçu"},
    {"file_received_saved", "File received and saved", "Fichier reçu et enregistré"},
    {"dlg_file_received", "File received", "Fichier reçu"},
    {"msg_file_received", "File received and saved:\n%1\n\nSize: %2 bytes\nCompression: %3%",
                          "Fichier reçu et enregistré :\n%1\n\nTaille : %2 octets\nCompression : %3%"},

    // ---- BBS
    {"bbs_beacon_sent", "BBS enabled - Beacon sent", "BBS activé - Balise envoyée"},
    {"bbs_detected", "Remote BBS detected - Button enabled", "BBS distant détecté - Bouton activé"},
    {"bbs_disabled", "BBS disabled - Request ignored", "BBS désactivé - Requête ignorée"},
    {"bbs_invalid_folder", "Invalid BBS folder - Request ignored", "Dossier BBS invalide - Requête ignorée"},
    {"bbs_list_sent", "BBS list sent", "Liste BBS envoyée"},
    {"bbs_list_received", "BBS list received", "Liste BBS reçue"},
    {"bbs_parse_error", "BBS list parse error", "Erreur parsing liste BBS"},
    {"bbs_access_denied", "Access denied", "Accès refusé"},
    {"bbs_file_not_found", "BBS file not found", "Fichier BBS introuvable"},
    {"bbs_sending_file", "Sending BBS file", "Envoi fichier BBS"},
    {"bbs_not_active", "Remote BBS is not active", "Le BBS distant n'est pas actif"},
    {"bbs_requesting_list", "Requesting BBS list - Please wait...", "Demande liste BBS - Veuillez patienter..."},
    {"bbs_refreshing", "Refreshing BBS list...", "Rafraîchissement liste BBS..."},
    {"bbs_downloading", "Downloading", "Téléchargement"},
    {"bbs_file_sent", "BBS file sent", "Fichier BBS envoyé"},
    {"bbs_send_error", "BBS file send error", "Erreur envoi fichier BBS"},
    {"lbl_files", "files", "fichiers"},

    // ---- chat and log files
    {"dlg_empty_chat", "Empty chat", "Chat vide"},
    {"msg_empty_chat", "The chat is empty, nothing to save.", "Le chat est vide, rien à sauvegarder."},
    {"save_chat_title", "Save chat", "Sauvegarder le chat"},
    {"dlg_save_success", "Saved", "Sauvegarde réussie"},
    {"msg_chat_saved_to", "The chat has been saved to:", "Le chat a été sauvegardé dans :"},
};
// clang-format on

const QHash<QByteArray, const Entry *> &index()
{
    static const QHash<QByteArray, const Entry *> table = [] {
        QHash<QByteArray, const Entry *> t;
        for (const Entry &e : kCatalogue)
            t.insert(QByteArray(e.key), &e);
        return t;
    }();
    return table;
}

} // namespace

I18n &I18n::instance()
{
    static I18n self;
    return self;
}

I18n::I18n()
{
    m_lang = systemLanguage();
    rebuild();
}

QString I18n::systemLanguage()
{
    // The Python version: French if the locale starts with "fr", else English.
    return QLocale::system().language() == QLocale::French ? QStringLiteral("fr") : QStringLiteral("en");
}

void I18n::setPreference(const QString &preference)
{
    const QString lang = (preference == QLatin1String("en") || preference == QLatin1String("fr"))
                             ? preference
                             : systemLanguage();
    if (lang == m_lang)
        return;
    m_lang = lang;
    rebuild();
    emit changed();
}

void I18n::rebuild()
{
    const bool fr = m_lang == QLatin1String("fr");
    m_strings.clear();
    for (const Entry &e : kCatalogue)
        m_strings.insert(QString::fromLatin1(e.key), QString::fromUtf8(fr ? e.fr : e.en));
}

QString I18n::t(const char *key) const
{
    const Entry *e = index().value(QByteArray(key));
    if (!e)
        return QString::fromLatin1(key);
    return QString::fromUtf8(m_lang == QLatin1String("fr") ? e->fr : e->en);
}

QStringList I18n::keys()
{
    QStringList list;
    for (const Entry &e : kCatalogue)
        list.append(QString::fromLatin1(e.key));
    return list;
}

bool I18n::hasKey(const QString &key)
{
    return index().contains(key.toLatin1());
}

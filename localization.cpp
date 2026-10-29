#include "localization.h"

#include <string>
#include <unordered_map>

namespace {

struct Entry {
    const char* key;
    const char* text[kLanguageCount];
};

const Entry kEntries[] = {
    { "welcome.1", { "Welcome", "Bienvenido", "Willkommen", "Benvenuto" } },
    { "welcome.2", { "to", "a", "bei", "in" } },

    { "dialog.select_folder", {
        "Select the folder that contains the 3D model.",
        "Selecciona la carpeta que contiene el modelo 3D.",
        "Wähle den Ordner mit dem 3D-Modell aus.",
        "Seleziona la cartella che contiene il modello 3D."
    } },

    { "asset.current", { "Current Asset", "Recurso actual", "Aktuelles Asset", "Asset corrente" } },
    { "btn.upload", { "Upload Asset", "Subir recurso", "Asset hochladen", "Carica asset" } },
    { "btn.settings", { "Settings", "Configuración", "Einstellungen", "Impostazioni" } },

    { "opt.params", { "Optimization Parameters", "Parámetros de optimización", "Optimierungsparameter", "Parametri di ottimizzazione" } },
    { "opt.target", { "Target Triangle Count", "Triángulos objetivo", "Ziel-Dreiecksanzahl", "Triangoli obiettivo" } },
    { "opt.reduction", { "Reduction Percentage", "Porcentaje de reducción", "Reduktion in Prozent", "Percentuale di riduzione" } },
    { "opt.max_error", { "Maximum Deformation Error", "Error máximo de deformación", "Maximaler Verformungsfehler", "Errore massimo di deformazione" } },
    { "opt.prevent_flip", { "Prevent Normal Flipping", "Evitar inversión de normales", "Normalenumkehr verhindern", "Evita inversione delle normali" } },
    { "opt.preserve_edges", { "Preserve Hard Edges", "Preservar aristas duras", "Harte Kanten erhalten", "Mantieni spigoli vivi" } },
    { "opt.edge_angle", { "Hard Edge Angle", "Ángulo de arista dura", "Winkel harter Kanten", "Angolo spigoli vivi" } },
    { "opt.lock_boundary", { "Lock Boundary Edges", "Bloquear aristas de borde", "Randkanten sperren", "Blocca bordi di contorno" } },
    { "opt.preserve_uv", { "Preserve UV Mapping", "Preservar mapeo UV", "UV-Mapping erhalten", "Mantieni mappatura UV" } },

    { "btn.optimize", { "Optimize Model", "Optimizar modelo", "Modell optimieren", "Ottimizza modello" } },
    { "btn.remesh", { "Remesh Model", "Remesh del modelo", "Modell neu vernetzen", "Rigenera mesh" } },
    { "btn.save", { "Save Optimized Model", "Guardar modelo optimizado", "Optimiertes Modell speichern", "Salva modello ottimizzato" } },

    { "save.ok", { "Saved successfully.", "Guardado correctamente.", "Erfolgreich gespeichert.", "Salvato con successo." } },
    { "save.failed", { "Save failed", "Error al guardar", "Speichern fehlgeschlagen", "Salvataggio non riuscito" } },

    { "opt.note.cloud_ok", { "Optimized in the cloud.", "Optimizado en la nube.", "In der Cloud optimiert.", "Ottimizzato nel cloud." } },
    { "opt.note.cloud_fallback", {
        "Cloud unavailable, optimized locally instead.",
        "Nube no disponible, se optimizó de forma local.",
        "Cloud nicht verfügbar, stattdessen lokal optimiert.",
        "Cloud non disponibile, ottimizzato in locale."
    } },
    { "opt.running", { "Optimizing...", "Optimizando...", "Optimierung läuft...", "Ottimizzazione in corso..." } },
    { "btn.ok", { "OK", "Aceptar", "OK", "OK" } },
    { "timeout.title", { "Optimization stopped", "Optimización detenida", "Optimierung gestoppt", "Ottimizzazione interrotta" } },
    { "timeout.text", {
        "The optimization was stopped because the mesh took too long to process. We recommend activating Speedigon in Settings to process it faster in the cloud.",
        "La optimización se detuvo porque la malla tardó demasiado en procesarse. Te recomendamos activar Speedigon en la configuración para procesarla más rápido en la nube.",
        "Die Optimierung wurde gestoppt, weil die Verarbeitung des Netzes zu lange gedauert hat. Wir empfehlen, Speedigon in den Einstellungen zu aktivieren, um es in der Cloud schneller zu verarbeiten.",
        "L'ottimizzazione è stata interrotta perché la mesh ha richiesto troppo tempo per essere elaborata. Ti consigliamo di attivare Speedigon nelle impostazioni per elaborarla più velocemente nel cloud."
    } },
    { "timeout.text_speedigon", {
        "The optimization was stopped because the mesh took too long to process, even with Speedigon active. Try again later or with a lighter mesh.",
        "La optimización se detuvo porque la malla tardó demasiado en procesarse, incluso con Speedigon activo. Inténtalo más tarde o con una malla más ligera.",
        "Die Optimierung wurde gestoppt, weil die Verarbeitung des Netzes selbst mit aktivem Speedigon zu lange gedauert hat. Versuche es später erneut oder mit einem leichteren Netz.",
        "L'ottimizzazione è stata interrotta perché la mesh ha richiesto troppo tempo per essere elaborata, anche con Speedigon attivo. Riprova più tardi o con una mesh più leggera."
    } },
    { "limit.title", { "Triangle limit reached", "Límite de triángulos alcanzado", "Dreieckslimit erreicht", "Limite di triangoli raggiunto" } },
    { "limit.text", {
        "This model has too many triangles to be optimized without Speedigon. Activate Speedigon in Settings to remove the triangle limit.",
        "Este modelo tiene demasiados triángulos para optimizarse sin Speedigon. Activa Speedigon en la configuración para eliminar el límite de triángulos.",
        "Dieses Modell hat zu viele Dreiecke, um ohne Speedigon optimiert zu werden. Aktiviere Speedigon in den Einstellungen, um das Dreieckslimit aufzuheben.",
        "Questo modello ha troppi triangoli per essere ottimizzato senza Speedigon. Attiva Speedigon nelle impostazioni per eliminare il limite di triangoli."
    } },
    { "limit.model_triangles", { "Triangles in this model", "Triángulos del modelo", "Dreiecke im Modell", "Triangoli nel modello" } },
    { "limit.local_limit", { "Limit without Speedigon", "Límite sin Speedigon", "Limit ohne Speedigon", "Limite senza Speedigon" } },
    { "timeout.open_settings", { "Open Settings", "Abrir configuración", "Einstellungen öffnen", "Apri impostazioni" } },
    { "opt.note.restored", { "Original model restored.", "Modelo original restaurado.", "Originalmodell wiederhergestellt.", "Modello originale ripristinato." } },

    { "info.title", { "Asset Information", "Información del recurso", "Asset-Informationen", "Informazioni sull'asset" } },
    { "info.loading", { "Loading...", "Cargando...", "Wird geladen...", "Caricamento..." } },
    { "info.original", { "Original", "Original", "Original", "Originale" } },
    { "info.optimized", { "Optimized", "Optimizado", "Optimiert", "Ottimizzato" } },
    { "info.reduction", { "Reduction", "Reducción", "Reduktion", "Riduzione" } },
    { "info.vertices", { "Vertices", "Vértices", "Eckpunkte", "Vertici" } },
    { "info.edges", { "Edges", "Aristas", "Kanten", "Spigoli" } },
    { "info.memory", { "Memory", "Memoria", "Speicher", "Memoria" } },
    { "info.vertices_removed", { "Vertices Removed", "Vértices eliminados", "Entfernte Eckpunkte", "Vertici rimossi" } },
    { "info.edges_removed", { "Edges Removed", "Aristas eliminadas", "Entfernte Kanten", "Spigoli rimossi" } },
    { "info.memory_reduced", { "Memory Reduced", "Memoria reducida", "Speicher reduziert", "Memoria ridotta" } },
    { "info.invalid", {
        "Invalid file! Try uploading a folder with a .glTF or .glb file instead.",
        "¡Archivo inválido! Intenta subir una carpeta con un archivo .glTF o .glb.",
        "Ungültige Datei! Lade stattdessen einen Ordner mit einer .glTF- oder .glb-Datei hoch.",
        "File non valido! Prova a caricare una cartella con un file .glTF o .glb."
    } },

    { "settings.title", { "Settings", "Configuración", "Einstellungen", "Impostazioni" } },
    { "settings.account", { "Manage Account", "Gestionar cuenta", "Konto verwalten", "Gestisci account" } },
    { "settings.language", { "Language", "Idioma", "Sprache", "Lingua" } },
    { "settings.speedigon_activate", { "Activate Speedigon", "Activar Speedigon", "Speedigon aktivieren", "Attiva Speedigon" } },
    { "settings.speedigon_deactivate", { "Deactivate Speedigon", "Desactivar Speedigon", "Speedigon deaktivieren", "Disattiva Speedigon" } },
    { "settings.close", { "Close", "Cerrar", "Schließen", "Chiudi" } },

    { "account.title", { "Account", "Cuenta", "Konto", "Account" } },
    { "account.login", { "Log In", "Iniciar sesión", "Anmelden", "Accedi" } },
    { "account.create", { "Create Account", "Crear cuenta", "Konto erstellen", "Crea account" } },
    { "account.username", { "Username", "Nombre de usuario", "Benutzername", "Nome utente" } },
    { "account.password", { "Password", "Contraseña", "Passwort", "Password" } },
    { "account.confirm", { "Confirm Password", "Confirmar contraseña", "Passwort bestätigen", "Conferma password" } },
    { "account.signed_in_as", { "Signed in as", "Sesión iniciada como", "Angemeldet als", "Connesso come" } },
    { "account.signed_in_ok", { "Signed in successfully.", "Sesión iniciada correctamente.", "Erfolgreich angemeldet.", "Accesso effettuato con successo." } },
    { "account.logout", { "Log Out", "Cerrar sesión", "Abmelden", "Esci" } },
    { "account.refresh", { "Refresh Status", "Actualizar estado", "Status aktualisieren", "Aggiorna stato" } },
    { "account.working", { "Please wait...", "Espera un momento...", "Bitte warten...", "Attendere prego..." } },
    { "account.speedigon_owned", { "Speedigon: purchased", "Speedigon: adquirido", "Speedigon: erworben", "Speedigon: acquistato" } },
    { "account.speedigon_not_owned", { "Speedigon: not purchased", "Speedigon: no adquirido", "Speedigon: nicht erworben", "Speedigon: non acquistato" } },

    { "speedigon.need_account", {
        "You need an account to use Speedigon. Create one or log in to continue.",
        "Necesitas una cuenta para usar Speedigon. Crea una o inicia sesión para continuar.",
        "Für Speedigon benötigst du ein Konto. Erstelle eines oder melde dich an, um fortzufahren.",
        "Ti serve un account per usare Speedigon. Creane uno o accedi per continuare."
    } },
    { "speedigon.about_title", { "About Speedigon", "Acerca de Speedigon", "Über Speedigon", "Informazioni su Speedigon" } },
    { "speedigon.about_text", {
        "Speedigon is an optional extension that uses a CPU cloud service to process meshes that require more computing power, and it also removes the triangle limit a mesh can have.",
        "Speedigon es una extensión opcional que usa un servicio en la nube de CPU para procesar mallas que requieren más cómputo, y además elimina el límite de triángulos que puede tener la malla.",
        "Speedigon ist eine optionale Erweiterung, die einen CPU-basierten Cloud-Dienst nutzt, um Netze zu verarbeiten, die mehr Rechenleistung benötigen, und zusätzlich das Dreieckslimit des Netzes aufhebt.",
        "Speedigon è un'estensione opzionale che usa un servizio cloud basato su CPU per elaborare mesh che richiedono più calcolo e, inoltre, elimina il limite di triangoli della mesh."
    } },
    { "speedigon.price", { "Price: $5 USD", "Precio: $5 USD", "Preis: 5 USD", "Prezzo: 5 USD" } },
    { "speedigon.buy", { "Buy Now", "Comprar ahora", "Jetzt kaufen", "Acquista ora" } },
    { "speedigon.active_label", { "Speedigon active", "Speedigon activo", "Speedigon aktiv", "Speedigon attivo" } },

    { "err.invalid_credentials", {
        "Incorrect username or password.",
        "Nombre de usuario o contraseña incorrectos.",
        "Benutzername oder Passwort ist falsch.",
        "Nome utente o password errati."
    } },
    { "err.username_taken", {
        "That username is already taken.",
        "Ese nombre de usuario ya está en uso.",
        "Dieser Benutzername ist bereits vergeben.",
        "Questo nome utente è già in uso."
    } },
    { "err.invalid_input", {
        "Invalid username (3-32 letters, numbers, . _ -) or password (minimum 8 characters).",
        "Nombre de usuario inválido (3-32 letras, números, . _ -) o contraseña inválida (mínimo 8 caracteres).",
        "Ungültiger Benutzername (3-32 Buchstaben, Zahlen, . _ -) oder ungültiges Passwort (mindestens 8 Zeichen).",
        "Nome utente non valido (3-32 lettere, numeri, . _ -) o password non valida (minimo 8 caratteri)."
    } },
    { "err.password_mismatch", {
        "Passwords do not match.",
        "Las contraseñas no coinciden.",
        "Die Passwörter stimmen nicht überein.",
        "Le password non coincidono."
    } },
    { "err.unauthorized", {
        "Your session expired. Please log in again.",
        "Tu sesión expiró. Inicia sesión de nuevo.",
        "Deine Sitzung ist abgelaufen. Bitte melde dich erneut an.",
        "La sessione è scaduta. Accedi di nuovo."
    } },
    { "err.not_owned", {
        "Speedigon has not been purchased on this account.",
        "Speedigon no ha sido adquirido en esta cuenta.",
        "Speedigon wurde mit diesem Konto nicht erworben.",
        "Speedigon non è stato acquistato con questo account."
    } },
    { "err.busy", {
        "The cloud service is busy. Try again in a moment.",
        "El servicio en la nube está ocupado. Inténtalo en un momento.",
        "Der Cloud-Dienst ist ausgelastet. Versuche es gleich noch einmal.",
        "Il servizio cloud è occupato. Riprova tra un momento."
    } },
    { "err.not_implemented", {
        "The cloud optimizer is not available yet.",
        "El optimizador en la nube aún no está disponible.",
        "Der Cloud-Optimierer ist noch nicht verfügbar.",
        "L'ottimizzatore cloud non è ancora disponibile."
    } },
    { "err.not_configured", {
        "The cloud service is not configured yet.",
        "El servicio en la nube aún no está configurado.",
        "Der Cloud-Dienst ist noch nicht konfiguriert.",
        "Il servizio cloud non è ancora configurato."
    } },
    { "err.network", {
        "Could not reach the server.",
        "No se pudo conectar con el servidor.",
        "Der Server ist nicht erreichbar.",
        "Impossibile raggiungere il server."
    } },
    { "err.server_error", {
        "Server error. Try again later.",
        "Error del servidor. Inténtalo más tarde.",
        "Serverfehler. Versuche es später erneut.",
        "Errore del server. Riprova più tardi."
    } },
    { "err.unknown", {
        "Something went wrong.",
        "Algo salió mal.",
        "Etwas ist schiefgelaufen.",
        "Qualcosa è andato storto."
    } }
};

const char* const kCodes[kLanguageCount] = { "en", "es", "de", "it" };
const char* const kNames[kLanguageCount] = { "English", "Español", "Deutsch", "Italiano" };

Language currentLanguage = Language::English;

const std::unordered_map<std::string, const Entry*>& table() {
    static std::unordered_map<std::string, const Entry*> map;

    if (map.empty()) {
        for (const auto& entry : kEntries) {
            map[entry.key] = &entry;
        }
    }

    return map;
}

}

void setLanguage(Language language) {
    currentLanguage = language;
}

Language getLanguage() {
    return currentLanguage;
}

const char* languageCode(Language language) {
    return kCodes[static_cast<int>(language)];
}

const char* languageNativeName(Language language) {
    return kNames[static_cast<int>(language)];
}

Language languageFromCode(const std::string& code, bool* valid) {
    for (int i = 0; i < kLanguageCount; ++i) {
        if (code == kCodes[i]) {
            if (valid) {
                *valid = true;
            }

            return static_cast<Language>(i);
        }
    }

    if (valid) {
        *valid = false;
    }

    return Language::English;
}

const char* tr(const char* key) {
    const auto& map = table();
    auto it = map.find(key);

    if (it == map.end()) {
        return key;
    }

    const char* text = it->second->text[static_cast<int>(currentLanguage)];

    if (text && text[0]) {
        return text;
    }

    return it->second->text[0];
}
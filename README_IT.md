# UniversalShellExt

Shell Extension COM generica per Windows che aggiunge una voce personalizzabile al menu contestuale di cartelle.

## Funzionalità

- ✅ **Menu Contestuale Personalizzabile**:
  - Voce nel menu contestuale per cartelle (click destro su cartella)
  - Voce nel menu contestuale per sfondo cartella (click destro su area vuota)
  - Testo, icona e comando completamente configurabili via registro
- ✅ **Configurazione Flessibile**:
  - Tutti i parametri letti dal registro di Windows
  - Supporto per argomenti personalizzati con placeholder `%1`
  - Icona estratta automaticamente dall'eseguibile
- ✅ **Sicurezza e Compatibilità**:
  - Supporto UAC elevation automatico se necessario
  - Compatibile con Windows 10/11 (x64)
  - Accesso registro WOW64 corretto (64-bit)
  - Thread-safe

## File del Progetto

| File | Descrizione |
|------|-------------|
| `UniversalShellExt.cpp` | Codice sorgente C++ della Shell Extension COM |
| `UniversalShellExt.def` | Definizioni esportazioni DLL |
| `UniversalShellExt.rc` | Risorse della DLL (Version Info) |
| `build.py` | Script Python per compilazione, firma e registrazione |

## Configurazione

La DLL legge la configurazione dal registro di Windows:

### Chiavi di Registro

**Percorso:** `HKEY_LOCAL_MACHINE\SOFTWARE\UniversalShellExt`

*(Fallback: `HKEY_CURRENT_USER\SOFTWARE\UniversalShellExt`)*

| Valore | Tipo | Descrizione | Esempio |
|--------|------|-------------|---------|
| `Command` | REG_SZ | Percorso completo dell'eseguibile | `N:\VSCode.exe` |
| `Arguments` | REG_SZ | Argomenti da passare (`%1` = path selezionato) | `"%1"` |
| `MenuText` | REG_SZ | Testo visualizzato nel menu | `Apri con VSCode` |
| `Icon` | REG_SZ | Percorso icona (exe o ico) | `N:\VSCode.exe` |

### Template Configurazione

```reg
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\UniversalShellExt]
"Command"="N:\\VSCode.exe"
"Arguments"="\"%1\""
"MenuText"="Apri con VSCode Portable"
"Icon"="N:\\VSCode.exe"
```

### Valori Default

Se non configurati:
- `Arguments` → `"%1"` (passa il percorso selezionato)
- `MenuText` → `"Open with Application"`
- `Icon` → usa lo stesso percorso di `Command`

## Identificatori

| Tipo | Valore |
|------|--------|
| **CLSID** | `{5D849F86-F2A9-41FA-8A24-E01CD6D6696C}` |
| **Nome COM** | `Universal Portable Shell Extension` |

## Compilazione

### Prerequisiti
- **Visual Studio 2022** (con workload "Desktop development with C++")
- **Windows SDK** (per `signtool.exe` e `rc.exe`)
- **Python 3.x**
- **Certificato di firma** (opzionale, per firma digitale)

### Build Script

```powershell
# Build completo (compila, copia, firma, registra)
python build.py

# Solo pulizia
python build.py --clean

# Solo deregistrazione
python build.py --unregister

# Build senza firma
python build.py --no-sign
```

### Output

La DLL compilata viene copiata in:
```
plugins\UniversalShellExt64.dll
```

## Registrazione

### Metodo Consigliato (tramite Launcher)

Il launcher `VSCode.exe` gestisce automaticamente la registrazione:

```powershell
N:\VSCode.exe /register           # Registra la shell extension
N:\VSCode.exe /unregister         # Rimuove la shell extension
N:\VSCode.exe /register /silent   # Modalità silenziosa
```

### Metodo Manuale (regsvr32)

Eseguire come **Amministratore**:

```cmd
:: Registrazione
regsvr32 "N:\Code\Workspace\Launchers\plugins\UniversalShellExt64.dll"

:: Rimozione
regsvr32 /u "N:\Code\Workspace\Launchers\plugins\UniversalShellExt64.dll"
```

## Chiavi di Registro Create

Durante la registrazione (`DllRegisterServer`):

| Chiave | Scopo |
|--------|-------|
| `HKCR\CLSID\{5D849F86-...}` | Registrazione classe COM |
| `HKCR\CLSID\{5D849F86-...}\InprocServer32` | Percorso DLL |
| `HKCR\Directory\shellex\ContextMenuHandlers\UniversalShellExt` | Handler per cartelle |
| `HKCR\Directory\Background\shellex\ContextMenuHandlers\...` | Handler per sfondo |
| `HKLM\...\Shell Extensions\Approved` | Whitelist Microsoft |

## Come Funziona

1. **Inizializzazione**: Quando l'utente fa click destro, Explorer carica la DLL
2. **Configurazione**: La DLL legge `Command`, `Arguments`, `MenuText`, `Icon` dal registro
3. **Menu**: Inserisce la voce nel menu contestuale con l'icona configurata
4. **Esecuzione**: Al click, sostituisce `%1` con il percorso e lancia il comando
5. **UAC**: Se il comando richiede elevazione, la richiede automaticamente

## Troubleshooting

### La voce non appare nel menu
1. Verificare che la DLL sia registrata (`regsvr32`)
2. Controllare che le chiavi di registro siano configurate
3. Riavviare Explorer (`taskkill /F /IM explorer.exe && explorer.exe`)

### Errore di registrazione
- Eseguire come Amministratore
- Verificare che la DLL sia firmata (Windows potrebbe bloccare DLL non firmate)

### Il comando non viene eseguito
- Verificare il percorso in `Command` sia corretto
- Controllare che `Arguments` contenga `%1` per ricevere il percorso

## Versioni

| Versione | Note |
|----------|------|
| **1.0.0** | Prima release - Shell extension generica configurabile via registro |

## Licenza

Uso interno - emulab.it

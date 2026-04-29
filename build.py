#!/usr/bin/env python3
"""
Build script per UniversalShellExt64.dll
Compila, copia, firma e registra la shell extension generica.
Richiede Visual Studio 2022 e Windows SDK.
"""

import subprocess
import shutil
import sys
import os
from pathlib import Path
import winreg

# =============================================================================
# Configurazione
# =============================================================================

# Percorsi
SCRIPT_DIR = Path(__file__).parent.resolve()
SOURCE_FILE = SCRIPT_DIR / "UniversalShellExt.cpp"
DEF_FILE = SCRIPT_DIR / "UniversalShellExt.def"
RC_FILE = SCRIPT_DIR / "UniversalShellExt.rc"
RES_FILE = SCRIPT_DIR / "UniversalShellExt.res"

OUTPUT_DIR = SCRIPT_DIR / "bin" / "x64"
OUTPUT_DLL = OUTPUT_DIR / "UniversalShellExt64.dll"

# Destinazione finale (cartella plugins)
DEST_DLL = SCRIPT_DIR.parent.parent / "plugins" / "UniversalShellExt64.dll"

# Visual Studio
VS_PATH = Path("C:/Program Files/Microsoft Visual Studio/2022/Community")
VCVARS_BAT = VS_PATH / "VC/Auxiliary/Build/vcvars64.bat"

# Signtool
SIGNTOOL = Path("C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/signtool.exe")
CERT_THUMBPRINT = "DFF03ABA0E0156D8E7823A893F9588530A364410"
TIMESTAMP_URL = "http://timestamp.digicert.com"

# Librerie
LIBS = [
    "ole32.lib",
    "uuid.lib", 
    "shell32.lib",
    "user32.lib",
    "gdi32.lib",
    "advapi32.lib",
    "shlwapi.lib"
]

CLSID_STRING = "{5D849F86-F2A9-41FA-8A24-E01CD6D6696C}"

def run_command(cmd, shell=False, check=True):
    print(f"  > {cmd if isinstance(cmd, str) else ' '.join(cmd)}")
    result = subprocess.run(cmd, shell=shell, capture_output=True, text=True)
    if result.stdout: print(result.stdout)
    if result.stderr: print(result.stderr)
    if check and result.returncode != 0:
        print(f"ERRORE: Comando fallito con codice {result.returncode}")
        sys.exit(1)
    return result

def check_prerequisites():
    print("\n=== Verifica prerequisiti ===")
    errors = []
    if not SOURCE_FILE.exists(): errors.append(f"Manca sorgente: {SOURCE_FILE}")
    if not DEF_FILE.exists(): errors.append(f"Manca DEF: {DEF_FILE}")
    if not VCVARS_BAT.exists(): errors.append(f"Manca VS: {VCVARS_BAT}")
    if not SIGNTOOL.exists(): errors.append(f"Manca Signtool: {SIGNTOOL}")
    if errors:
        for e in errors: print(f"  ERRORE: {e}")
        sys.exit(1)
    print("  Prerequisiti OK")

def update_version():
    print("\n=== Incremento Versione ===")
    import re
    
    if not RC_FILE.exists():
        print("  ERRORE: .rc non trovato!")
        return

    content = RC_FILE.read_text(encoding="utf-8")
    
    # Cerca VER_FILEVERSION 1,0,0,X
    # Regex per catturare i primi 3 numeri e l'ultimo
    pattern = r'(#define VER_FILEVERSION\s+\d+,\d+,\d+,)(\d+)'
    match = re.search(pattern, content)
    
    if match:
        prefix = match.group(1)
        build_num = int(match.group(2))
        new_build_num = build_num + 1
        
        # Sostituisci VER_FILEVERSION
        new_content = re.sub(pattern, f'{prefix}{new_build_num}', content)
        
        # Aggiorna anche VER_FILEVERSION_STR "1.0.0.X\0"
        str_pattern = r'(#define VER_FILEVERSION_STR\s+"[^"]*?)(\d+)(\\0")'
        # Nota: questa regex assume che la stringa finisca col numero da incrementare. 
        # Se è "1.0.0.0\0", cattura "1.0.0." come gruppo 1, "0" come 2.
        
        # Miglior approccio: ricostruire la stringa in base al nuovo numero
        # Ma dobbiamo parsare i primi 3 numeri per essere sicuri
        v_parts = prefix.strip().replace("#define VER_FILEVERSION", "").strip().split(',')
        if len(v_parts) >= 3:
            major, minor, patch = v_parts[0], v_parts[1], v_parts[2]
            new_ver_str = f'{major}.{minor}.{patch}.{new_build_num}'
            
            # Sostituisci la stringa intera con gestione corretta di virgolette e backslash
            # La regex deve catturare tutto fino ai doppi apici finali
            new_content = re.sub(r'(#define VER_FILEVERSION_STR\s+)"(.*?)"', f'#define VER_FILEVERSION_STR         "{new_ver_str}\\\\0"', new_content)
            
            # Aggiorna anche VER_PRODUCTVERSION (spesso uguale)
            new_content = re.sub(r'(#define VER_PRODUCTVERSION\s+).*', f'#define VER_PRODUCTVERSION          {major},{minor},{patch},{new_build_num}', new_content)
            
            # Aggiorna VER_PRODUCTVERSION_STR (spesso solo 3 cifre, ma qui mettiamo 4 per coerenza o lasciamo stare?)
            # Nel file attuale è "1.0.0\0". Se vogliamo incrementare anche quello:
            # new_content = re.sub(r'#define VER_PRODUCTVERSION_STR\s+".*?"', f'#define VER_PRODUCTVERSION_STR      "{major}.{minor}.{patch}\\0"', new_content)
            
            RC_FILE.write_text(new_content, encoding="utf-8")
            print(f"  Versione aggiornata a: {new_ver_str}")
        else:
             print("  Non riesco a parsare la versione corrente.")
    else:
        print("  Pattern versione non trovato.")

def clean_build():
    print("\n=== Clean build ===")
    to_remove = [
        SCRIPT_DIR / "bin",
        SCRIPT_DIR / "Release",
        SCRIPT_DIR / "Debug",
        SCRIPT_DIR / "x64",
        SCRIPT_DIR / "UniversalShellExt64.dll",
        SCRIPT_DIR / "UniversalShellExt64.exp",
        SCRIPT_DIR / "UniversalShellExt64.lib",
        SCRIPT_DIR / "UniversalShellExt64.pdb",
        SCRIPT_DIR / "UniversalShellExt.obj",
        RES_FILE,
    ]
    for path in to_remove:
        if path.exists():
            if path.is_dir(): 
                try: shutil.rmtree(path)
                except: pass
            else: 
                try: path.unlink()
                except: pass
    print("  Clean completato")

def kill_explorer():
    print("\n=== Termina Explorer ===")
    subprocess.run(["taskkill", "/F", "/IM", "explorer.exe"], capture_output=True, check=False)
    import time
    time.sleep(2)

def start_explorer():
    print("\n=== Riavvia Explorer ===")
    subprocess.Popen(["explorer.exe"])
    import time
    time.sleep(2)

def unregister_dll():
    print("\n=== Deregistra DLL ===")
    if DEST_DLL.exists():
        run_command(["regsvr32", "/u", "/s", str(DEST_DLL)], check=False)
        print("  DLL deregistrata")
    else:
        print("  DLL non trovata, skip")

def compile_dll():
    print("\n=== Compilazione DLL ===")
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    
    # Compile Resource
    rc_cmd = f'''call "{VCVARS_BAT}" && cd /D "{SCRIPT_DIR}" && rc.exe /fo"{RES_FILE.name}" "{RC_FILE.name}"'''
    subprocess.run(f'cmd /c "{rc_cmd}"', shell=True, check=True)
    
    # Compile DLL
    libs_str = " ".join(LIBS)
    compile_cmd = f'''call "{VCVARS_BAT}" && cd /D "{SCRIPT_DIR}" && cl.exe /LD /EHsc /O2 /DUNICODE /D_UNICODE "{SOURCE_FILE.name}" "{RES_FILE.name}" /Fe:"{OUTPUT_DLL}" /link /DEF:"{DEF_FILE.name}" {libs_str}'''
    
    result = subprocess.run(f'cmd /c "{compile_cmd}"', shell=True, capture_output=True, text=True)
    if result.stdout:
         for line in result.stdout.split('\n'):
            if line.strip() and not line.startswith('**') and not line.startswith('['):
                print(f"  {line}")
    if result.returncode != 0 or not OUTPUT_DLL.exists():
        print("ERRORE: Compilazione fallita")
        sys.exit(1)
    
    print(f"  Compilata: {OUTPUT_DLL}")

def copy_dll():
    print("\n=== Copia DLL ===")
    shutil.copy2(OUTPUT_DLL, DEST_DLL)
    print(f"  Copiata in: {DEST_DLL}")

def sign_dll():
    print("\n=== Firma DLL ===")
    cmd = [str(SIGNTOOL), "sign", "/sha1", CERT_THUMBPRINT, "/fd", "SHA256", "/t", TIMESTAMP_URL, str(DEST_DLL)]
    run_command(cmd)
    print("  Firma OK")

def register_dll():
    print("\n=== Registra DLL ===")
    result = run_command(["regsvr32", "/s", str(DEST_DLL)], check=False)
    if result.returncode == 0:
        print("  Registrazione OK")
    else:
        print(f"  FALLITA: {result.returncode} (Serve Admin?)")

def verify_registration():
    print("\n=== Verifica Registrazione ===")
    try:
        key_path = fr"SOFTWARE\Classes\CLSID\{CLSID_STRING}\InprocServer32"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path) as key:
            val, _ = winreg.QueryValueEx(key, "")
            print(f"  CLSID registrato: {val}")
    except FileNotFoundError:
        print("  ERRORE: CLSID non trovato")
        return False
        
    try:
        key_path = r"SOFTWARE\Classes\Directory\shellex\ContextMenuHandlers\UniversalShellExt"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path) as key:
            val, _ = winreg.QueryValueEx(key, "")
            print(f"  Handler directory registrato: {val}")
    except FileNotFoundError:
        print("  ERRORE: Handler non trovato")
        return False
    return True

def main():
    print("UniversalShellExt Builder")
    args = sys.argv[1:]
    
    if "--clean" in args:
        clean_build()
        return
        
    if "--unregister" in args:
        unregister_dll()
        return

    # STEP 0: Clean pre-build
    clean_build()

    update_version()
    check_prerequisites()
    unregister_dll()
    kill_explorer()
    compile_dll()
    copy_dll()
    
    if "--no-sign" not in args:
        sign_dll()
        
    start_explorer()
    register_dll()
    verify_registration()
    clean_build()
    
    print("\nDONE!")
    print(f"DLL: {DEST_DLL}")
    print("Ricorda di configurare HKLM\\Software\\UniversalShellExt")

if __name__ == "__main__":
    main()

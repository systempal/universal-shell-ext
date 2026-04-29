# UniversalShellExt

Generic COM Shell Extension for Windows that adds a customizable entry to the folder context menu.

## Features

- ✅ **Customizable Context Menu**:
  - Context menu entry for folders (right-click on folder)
  - Context menu entry for folder background (right-click on empty area)
  - Text, icon and command fully configurable via registry
- ✅ **Flexible Configuration**:
  - All parameters read from the Windows registry
  - Support for custom arguments with `%1` placeholder
  - Icon automatically extracted from the executable
- ✅ **Security and Compatibility**:
  - Automatic UAC elevation support when required
  - Compatible with Windows 10/11 (x64)
  - Correct WOW64 registry access (64-bit)
  - Thread-safe

## Project Files

| File | Description |
|------|-------------|
| `UniversalShellExt.cpp` | C++ source code of the COM Shell Extension |
| `UniversalShellExt.def` | DLL export definitions |
| `UniversalShellExt.rc` | DLL resources (Version Info) |
| `build.py` | Python script for build, signing and registration |

## Configuration

The DLL reads its configuration from the Windows registry:

### Registry Keys

**Path:** `HKEY_LOCAL_MACHINE\SOFTWARE\UniversalShellExt`

*(Fallback: `HKEY_CURRENT_USER\SOFTWARE\UniversalShellExt`)*

| Value | Type | Description | Example |
|-------|------|-------------|---------|
| `Command` | REG_SZ | Full path to the executable | `N:\VSCode.exe` |
| `Arguments` | REG_SZ | Arguments to pass (`%1` = selected path) | `"%1"` |
| `MenuText` | REG_SZ | Text displayed in the menu | `Open with VSCode` |
| `Icon` | REG_SZ | Icon path (exe or ico) | `N:\VSCode.exe` |

### Configuration Template

```reg
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\UniversalShellExt]
"Command"="N:\\VSCode.exe"
"Arguments"="\"%1\""
"MenuText"="Open with VSCode Portable"
"Icon"="N:\\VSCode.exe"
```

### Default Values

If not configured:
- `Arguments` → `"%1"` (passes the selected path)
- `MenuText` → `"Open with Application"`
- `Icon` → uses the same path as `Command`

## Identifiers

| Type | Value |
|------|-------|
| **CLSID** | `{5D849F86-F2A9-41FA-8A24-E01CD6D6696C}` |
| **COM Name** | `Universal Portable Shell Extension` |

## Build

### Prerequisites
- **Visual Studio 2022** (with "Desktop development with C++" workload)
- **Windows SDK** (for `signtool.exe` and `rc.exe`)
- **Python 3.x**
- **Signing certificate** (optional, for digital signing)

### Build Script

```powershell
# Full build (compile, copy, sign, register)
python build.py

# Clean only
python build.py --clean

# Unregister only
python build.py --unregister

# Build without signing
python build.py --no-sign
```

### Output

The compiled DLL is copied to:
```
plugins\UniversalShellExt64.dll
```

## Registration

### Recommended Method (via Launcher)

The `VSCode.exe` launcher automatically manages registration:

```powershell
N:\VSCode.exe /register           # Register the shell extension
N:\VSCode.exe /unregister         # Remove the shell extension
N:\VSCode.exe /register /silent   # Silent mode
```

### Manual Method (regsvr32)

Run as **Administrator**:

```cmd
regsvr32 "plugins\UniversalShellExt64.dll"
```

## Registry Keys Created

During registration (`DllRegisterServer`):

| Key | Purpose |
|-----|---------|
| `HKCR\CLSID\{5D849F86-...}` | COM class registration |
| `HKCR\CLSID\{5D849F86-...}\InprocServer32` | DLL path |
| `HKCR\Directory\shellex\ContextMenuHandlers\UniversalShellExt` | Handler for folders |
| `HKCR\Directory\Background\shellex\ContextMenuHandlers\...` | Handler for folder background |
| `HKLM\...\Shell Extensions\Approved` | Microsoft whitelist |

## How It Works

1. **Initialization**: When the user right-clicks, Explorer loads the DLL
2. **Configuration**: The DLL reads `Command`, `Arguments`, `MenuText`, `Icon` from the registry
3. **Menu**: Inserts the entry in the context menu with the configured icon
4. **Execution**: On click, replaces `%1` with the path and launches the command
5. **UAC**: If the command requires elevation, requests it automatically

## Troubleshooting

### Entry does not appear in the menu
1. Verify the DLL is registered (`regsvr32`)
2. Check that the registry keys are configured
3. Restart Explorer (`taskkill /F /IM explorer.exe && explorer.exe`)

### Registration error
- Run as Administrator
- Verify the DLL is signed (Windows may block unsigned DLLs)

### Command is not executed
- Verify the path in `Command` is correct
- Check that `Arguments` contains `%1` to receive the path

## Versions

| Version | Notes |
|---------|-------|
| **1.0.0** | First release — generic shell extension configurable via registry |

## License

Internal use — emulab.it

---

*See [README_IT.md](README_IT.md) for the Italian version.*

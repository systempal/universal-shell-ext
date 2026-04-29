// UniversalShellExt.cpp - Generic Shell Extension
// Configurable via Registry
// Version: 1.1.0

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <new>

#pragma comment(lib, "shlwapi.lib")

// DLL Version
#define DLL_VERSION L"1.1.0"

// GUID for UniversalShellExt
// {5D849F86-F2A9-41FA-8A24-E01CD6D6696C}
static const CLSID CLSID_UniversalShellExt = 
{ 0x5d849f86, 0xf2a9, 0x41fa, { 0x8a, 0x24, 0xe0, 0x1c, 0xd6, 0xd6, 0x69, 0x6c } };

// CLSID as string
#define CLSID_STRING L"{5D849F86-F2A9-41FA-8A24-E01CD6D6696C}"

// Registry Configuration - now per-CLSID
#define REG_KEY_BASE      L"Software\\UniversalShellExt\\"
#define REG_VALUE_COMMAND L"Command"
#define REG_VALUE_ARGS    L"Arguments"
#define REG_VALUE_ICON    L"Icon"
#define REG_VALUE_TEXT    L"MenuText"
#define REG_VALUE_ENABLE_LOG L"EnableLog"

// Global Variables
HINSTANCE g_hInst = NULL;
LONG g_cDllRef = 0;

// Internal Helper for File Logging
void WriteLogFile(const wchar_t* buffer) {
    FILE* f;
    if (_wfopen_s(&f, L"C:\\Users\\Simone\\.gemini\\shellext_debug.log", L"a") == 0) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buffer);
        fclose(f);
    }
}

// Global Logger (Legacy/Global scope)
void WriteLog(const wchar_t* format, ...) {
    // Global logging is now MINIMAL or DISABLED by default unless needed.
    // Uncomment for deep debugging of DLL loading.
    // wchar_t buffer[1024];
    // va_list args;
    // va_start(args, format);
    // _vswprintf_p(buffer, 1024, format, args);
    // va_end(args);
    // WriteLogFile(buffer);
}

// Configuration Cache globals removed.
// They are now member variables of the class.

//==============================================================================
// Helper: Icon to Bitmap
//==============================================================================

//==============================================================================
// Helper: Icon to Bitmap
//==============================================================================
HBITMAP IconToBitmap(HICON hIcon)
{
    if (!hIcon) return NULL;
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return NULL;
    
    BITMAP bm;
    GetObject(iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask, sizeof(bm), &bm);
    
    int width = bm.bmWidth;
    int height = bm.bmHeight;
    
    if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
    if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* pBits;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    
    if (hBitmap)
    {
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        RECT rc = {0, 0, width, height};
        FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); // Transparent assumes 32bit alpha
        DrawIconEx(hdcMem, 0, 0, hIcon, width, height, 0, NULL, DI_NORMAL);
        SelectObject(hdcMem, hOldBitmap);
    }
    
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    return hBitmap;
}

//==============================================================================
// Class UniversalShellExt
//==============================================================================
class UniversalShellExt : public IShellExtInit, public IContextMenu
{
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] = 
        {
            QITABENT(UniversalShellExt, IContextMenu),
            QITABENT(UniversalShellExt, IShellExtInit),
            { 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    // IShellExtInit
    IFACEMETHODIMP Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID)
    {
        LPOLESTR szCLSID = NULL;
        StringFromCLSID(m_clsid, &szCLSID);
        Log(L"UniversalShellExt::Initialize called. CLSID: %s", szCLSID ? szCLSID : L"Unknown");
        if (szCLSID) CoTaskMemFree(szCLSID);

        if (pDataObj == NULL) {
            Log(L"Initialize - Error: pDataObj is NULL");
            return E_INVALIDARG;
        }
        
        FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stm;
        
        if (SUCCEEDED(pDataObj->GetData(&fe, &stm)))
        {
            UINT nFiles = DragQueryFile((HDROP)stm.hGlobal, 0xFFFFFFFF, NULL, 0);
            Log(L"Initialize - Files selected: %d", nFiles);
            if (nFiles > 0)
            {
                DragQueryFile((HDROP)stm.hGlobal, 0, m_szSelectedPath, MAX_PATH);
                Log(L"Initialize - Selected Path: %s", m_szSelectedPath);
            }
            ReleaseStgMedium(&stm);
        }
        else if (pidlFolder != NULL)
        {
            SHGetPathFromIDList(pidlFolder, m_szSelectedPath);
            Log(L"Initialize - From PIDL: %s", m_szSelectedPath);
        }
        else {
             Log(L"Initialize - Failed to get path data.");
        }
        return S_OK;
    }

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
    {
        LPOLESTR szCLSID = NULL;
        StringFromCLSID(m_clsid, &szCLSID);
        Log(L"UniversalShellExt::QueryContextMenu called for CLSID: %s", szCLSID ? szCLSID : L"Unknown");
        Log(L"QueryContextMenu - indexMenu: %d, idCmdFirst: %d, idCmdLast: %d, uFlags: 0x%X", indexMenu, idCmdFirst, idCmdLast, uFlags);
        if (szCLSID) CoTaskMemFree(szCLSID);

        if (uFlags & CMF_DEFAULTONLY) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        
        LoadConfiguration(); // Load config into member variables

        // Fixed position at top
        UINT targetPosition = 0;
        
        Log(L"QueryContextMenu - indexMenu: %d, idCmdFirst: %d, idCmdLast: %d", indexMenu, idCmdFirst, idCmdLast);
        Log(L"QueryContextMenu - Using fixed position: %d", targetPosition);
        Log(L"QueryContextMenu called. MenuText: %s", m_szMenuText);
    
        // Dont show if no command configured
        if (m_szCommand[0] == L'\0') {
            Log(L"QueryContextMenu - Error: Command is empty!");
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        HICON hIcon = NULL;
        HBITMAP hBitmap = NULL;
        
        // Extract icon
        if (m_szIcon[0] != L'\0')
        {
             Log(L"QueryContextMenu - Icon configured: %s", m_szIcon);
             // Try to find index if comma exists e.g. "dll,1"
             // Simple extraction for now
             ExtractIconEx(m_szIcon, 0, NULL, &hIcon, 1);
        }
        
        if (hIcon)
        {
            hBitmap = IconToBitmap(hIcon);
            DestroyIcon(hIcon);
        }
        
        if (m_hMenuBitmap) { DeleteObject(m_hMenuBitmap); m_hMenuBitmap = NULL; }

        MENUITEMINFO mii = { sizeof(mii) };
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_BITMAP;
        mii.wID = idCmdFirst;
        mii.dwTypeData = m_szMenuText;
        mii.fState = MFS_ENABLED;
        mii.hbmpItem = hBitmap;
        
        if (!InsertMenuItem(hMenu, targetPosition, TRUE, &mii))
        {
            if (hBitmap) DeleteObject(hBitmap);
            Log(L"QueryContextMenu - InsertMenuItem FAILED. Error: %d", GetLastError());
            return HRESULT_FROM_WIN32(GetLastError());
        }
        else
        {
             Log(L"QueryContextMenu - InsertMenuItem Success. ID: %d", idCmdFirst);
        }
        
        m_hMenuBitmap = hBitmap;
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 1);
    }

    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici)
    {
        LPOLESTR szCLSID = NULL;
        StringFromCLSID(m_clsid, &szCLSID);
        Log(L"UniversalShellExt::InvokeCommand called for CLSID: %s", szCLSID ? szCLSID : L"Unknown");
        
// Debug MessageBox removed for production

        
        // Debug Box (Temporary - UNCOMMENTED FOR DEBUG)
        // MessageBox(NULL, L"InvokeCommand Called!", L"Debug", MB_OK);

        bool isMatch = false;

        // 1. Check for Integer Offset (Standard Right-Click)
        if (HIWORD(pici->lpVerb) == 0)
        {
            // We only add 1 item, so offset 0 is the only valid one.
            if (LOWORD(pici->lpVerb) == 0) isMatch = true;
        }
        // 2. Check for String Verb (Programmatic/Windows 11)
        else
        {
             LPCSTR pszVerb = pici->lpVerb;
             // Compare with our CLSID string (which we return in GetCommandString)
             // Need to convert our WCHAR CLSID to MultiByte for comparison
             if (szCLSID) {
                 // Construct EXPECTED verb: Verb_XXXX (without braces)
                 WCHAR szVerbW[128];
                 StringCchPrintf(szVerbW, 128, L"Verb_%s", szCLSID + 1); // Skip '{'
                 size_t lenW = 0;
                 StringCchLength(szVerbW, 128, &lenW);
                 if (lenW > 0 && szVerbW[lenW-1] == L'}') szVerbW[lenW-1] = L'\0';

                 // Convert to ANSI for comparison
                 int lenA = WideCharToMultiByte(CP_ACP, 0, szVerbW, -1, NULL, 0, NULL, NULL);
                 char* pszVerbA = new (std::nothrow) char[lenA];
                 if (pszVerbA) {
                     WideCharToMultiByte(CP_ACP, 0, szVerbW, -1, pszVerbA, lenA, NULL, NULL);
                     
                     if (lstrcmpiA(pszVerb, pszVerbA) == 0) isMatch = true;
                     else if (lstrcmpiA(pszVerb, "UniversalShellExt") == 0) isMatch = true; // Fallback

                     delete[] pszVerbA;
                 }
             }
        }

        if (szCLSID) CoTaskMemFree(szCLSID);

        if (!isMatch) {
             Log(L"InvokeCommand - Verb mismatch. Rejecting.");
             return E_INVALIDARG;
        }
        
        // If command is empty, try reloading config (fallback)
        if (m_szCommand[0] == L'\0') {
             Log(L"InvokeCommand - Command empty, reloading...");
             LoadConfiguration();
        }
        
        // Check if command file exists
        if (m_szCommand[0] == L'\0' || GetFileAttributes(m_szCommand) == INVALID_FILE_ATTRIBUTES)
        {
            WCHAR szMsg[MAX_PATH + 128];
            if (m_szCommand[0] == L'\0')
            {
                StringCchCopy(szMsg, ARRAYSIZE(szMsg), L"Nessun comando configurato.\n\nConfigurare la chiave di registro:\nHKLM\\SOFTWARE\\UniversalShellExt\\Command");
            }
            else
            {
                StringCchPrintf(szMsg, ARRAYSIZE(szMsg), L"File non trovato:\n\n%s\n\nVerificare la configurazione nel registro.", m_szCommand);
            }
            Log(L"InvokeCommand - Error: %s", szMsg);
            MessageBox(NULL, szMsg, L"UniversalShellExt - Errore", MB_OK | MB_ICONERROR);
            return E_FAIL;
        }
        
        // Prepare Parameters
        // Replace %1 in m_szArguments with m_szSelectedPath (quoted)
        WCHAR szArgsExpanded[MAX_PATH * 2];
        WCHAR szPathQuoted[MAX_PATH + 2];
        StringCchPrintf(szPathQuoted, MAX_PATH+2, L"\"%s\"", m_szSelectedPath);
        
        // Simple Replace of %1
        // Note: This is a basic implementation.
        // It searches for %1 and replaces it.
        
        // Copy arguments to buffer
        WCHAR *pFound = wcsstr(m_szArguments, L"%1");
        if (pFound)
        {
             size_t prefixLen = pFound - m_szArguments;
             wcsncpy_s(szArgsExpanded, ARRAYSIZE(szArgsExpanded), m_szArguments, prefixLen);
             szArgsExpanded[prefixLen] = L'\0';
             StringCchCat(szArgsExpanded, ARRAYSIZE(szArgsExpanded), szPathQuoted); // Insert Path (quoted)
             StringCchCat(szArgsExpanded, ARRAYSIZE(szArgsExpanded), pFound + 2);
        }
        else
        {
             // No %1 found, just use arguments as is.
             // The provided snippet had a different logic here, appending the path if no %1.
             // Sticking to the original logic for now, which means if no %1, path is not appended.
             StringCchCopy(szArgsExpanded, ARRAYSIZE(szArgsExpanded), m_szArguments);
        }

        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = m_szCommand;
        sei.lpParameters = szArgsExpanded;
        sei.nShow = SW_SHOWNORMAL;

        // Set Working Directory to the folder containing the executable
        WCHAR szDir[MAX_PATH];
        StringCchCopy(szDir, MAX_PATH, m_szCommand);
        PathRemoveFileSpec(szDir); // Requires shlwapi.h and linking shlwapi.lib
        sei.lpDirectory = szDir;
        
        Log(L"InvokeCommand - Executing: %s Params: %s Dir: %s", sei.lpFile, sei.lpParameters, sei.lpDirectory);

        if (!ShellExecuteEx(&sei))
        {
            DWORD lastError = GetLastError();
            Log(L"InvokeCommand - ShellExecuteEx failed with error: %d", lastError);
            if (lastError == ERROR_ELEVATION_REQUIRED)
            {
                sei.lpVerb = L"runas";
                Log(L"InvokeCommand - Retrying with 'runas' verb.");
                ShellExecuteEx(&sei);
            } else {
                MessageBox(NULL, L"Failed to launch command.", L"Error", MB_ICONERROR);
                return E_FAIL;
            }
        }
        
        if (sei.hProcess) CloseHandle(sei.hProcess);
        Log(L"InvokeCommand - Success");
        return S_OK;
    }

    IFACEMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType, UINT *pReserved, CHAR *pszName, UINT cchMax)
    {
        Log(L"GetCommandString called. uType: %d", uType);
        if (idCmd != 0) return E_INVALIDARG;
        
        LoadConfiguration(); // Reload config

        switch (uType)
        {
        case GCS_HELPTEXTW:
            StringCchCopyW((LPWSTR)pszName, cchMax, m_szMenuText);
            return S_OK;

        case GCS_HELPTEXTA:
            {
                int len = WideCharToMultiByte(CP_ACP, 0, m_szMenuText, -1, NULL, 0, NULL, NULL);
                if (len <= cchMax) WideCharToMultiByte(CP_ACP, 0, m_szMenuText, -1, pszName, cchMax, NULL, NULL);
            }
            return S_OK;

        case GCS_VERBW:
        case GCS_VALIDATEW:
            {
                LPOLESTR szCLSID = NULL;
                StringFromCLSID(m_clsid, &szCLSID);
                if (szCLSID)
                {
                    // Sanitize Verb: replace {CLSID} with Verb_CLSID (no braces)
                    WCHAR szVerb[128];
                    StringCchPrintf(szVerb, 128, L"Verb_%s", szCLSID + 1); // Skip '{'
                    size_t len = 0;
                    StringCchLength(szVerb, 128, &len);
                    if (len > 0 && szVerb[len-1] == L'}') szVerb[len-1] = L'\0';

                    Log(L"GetCommandString - Returning VERB: %s", szVerb);
                    
                    if (uType == GCS_VERBW && pszName) {
                         StringCchCopyW((LPWSTR)pszName, cchMax, szVerb);
                    }
                    CoTaskMemFree(szCLSID);
                    return S_OK;
                }
                return E_FAIL;
            }

        case GCS_VERBA:
        case GCS_VALIDATEA:
             // Return "UniversalShellExt" for legacy ANSI, but maybe we should return Verb_GUID here too if possible?
             // Sticking to simplified "Valid" response for now to minimize charset risk.
             // If Verb is requested (ANSI), we return the generic string.
             if (uType == GCS_VERBA && pszName) {
                 StringCchCopyA(pszName, cchMax, "UniversalShellExt");
             }
             return S_OK;

        default:
            Log(L"GetCommandString - Unknown uType: %d", uType);
            return E_INVALIDARG;
        }
    }

    UniversalShellExt(REFCLSID clsid) : m_cRef(1), m_hMenuBitmap(NULL), m_clsid(clsid) 
    {
        InterlockedIncrement(&g_cDllRef);
        
        // Stringify CLSID for log
        LPOLESTR szCLSID = NULL;
        StringFromCLSID(m_clsid, &szCLSID);
        Log(L"UniversalShellExt::Constructor - CLSID: %s", szCLSID ? szCLSID : L"Unknown");
        if (szCLSID) CoTaskMemFree(szCLSID);

        m_szSelectedPath[0] = L'\0';
        m_szCommand[0] = L'\0';
        m_szArguments[0] = L'\0';
        m_szIcon[0] = L'\0';
        m_szMenuText[0] = L'\0';
        m_bEnableLog = FALSE;
    }

protected:
    ~UniversalShellExt()
    {
        LPOLESTR szCLSID = NULL;
        StringFromCLSID(m_clsid, &szCLSID);
        Log(L"UniversalShellExt::Destructor - CLSID: %s", szCLSID ? szCLSID : L"Unknown");
        if (szCLSID) CoTaskMemFree(szCLSID);

        if (m_hMenuBitmap) DeleteObject(m_hMenuBitmap);
        InterlockedDecrement(&g_cDllRef);
    }

private:
    LONG m_cRef;
    WCHAR m_szSelectedPath[MAX_PATH];
    HBITMAP m_hMenuBitmap;
    CLSID m_clsid;
    // Member variables to store configuration for this instance
    WCHAR m_szCommand[MAX_PATH];
    WCHAR m_szArguments[MAX_PATH];
    WCHAR m_szIcon[MAX_PATH];
    WCHAR m_szMenuText[128];
    BOOL m_bEnableLog;

    // Member helper for logging
    void Log(const wchar_t* format, ...) {
        if (!m_bEnableLog) return;
        
        wchar_t buffer[1024];
        va_list args;
        va_start(args, format);
        _vswprintf_p(buffer, 1024, format, args);
        va_end(args);
        
        WriteLogFile(buffer);
    }

    // Internal LoadConfiguration method
    void LoadConfiguration()
    {
        m_szCommand[0] = L'\0';
        m_szArguments[0] = L'\0';
        m_szIcon[0] = L'\0';
        m_szMenuText[0] = L'\0';
        
        // Build registry path: Software\UniversalShellExt\{CLSID}
        WCHAR szKeyPath[256];
        WCHAR szClsid[64];
        StringFromGUID2(m_clsid, szClsid, ARRAYSIZE(szClsid));
        StringCchPrintf(szKeyPath, ARRAYSIZE(szKeyPath), L"%s%s", REG_KEY_BASE, szClsid);
        
        HKEY hKey;
        DWORD dwSize;
        DWORD dwType = REG_SZ;
        
        // Try to open key from HKLM (64-bit view)
        LSTATUS status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyPath, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
        if (status != ERROR_SUCCESS)
        {
            status = RegOpenKeyEx(HKEY_CURRENT_USER, szKeyPath, 0, KEY_READ, &hKey);
        }
        
        if (status == ERROR_SUCCESS)
        {
            // Read Command
            dwSize = sizeof(m_szCommand);
            RegQueryValueEx(hKey, REG_VALUE_COMMAND, NULL, &dwType, (LPBYTE)m_szCommand, &dwSize);
            
            // Read Arguments
            dwSize = sizeof(m_szArguments);
            RegQueryValueEx(hKey, REG_VALUE_ARGS, NULL, &dwType, (LPBYTE)m_szArguments, &dwSize);
            
            // Read Icon
            dwSize = sizeof(m_szIcon);
            RegQueryValueEx(hKey, REG_VALUE_ICON, NULL, &dwType, (LPBYTE)m_szIcon, &dwSize);
            
            // Read MenuText
            dwSize = sizeof(m_szMenuText);
            dwSize = sizeof(m_szMenuText);
            RegQueryValueEx(hKey, REG_VALUE_TEXT, NULL, &dwType, (LPBYTE)m_szMenuText, &dwSize);
            
            // Read EnableLog
            DWORD dwLog = 0;
            dwSize = sizeof(dwLog);
            DWORD dwTypeDw = REG_DWORD;
            if (RegQueryValueEx(hKey, REG_VALUE_ENABLE_LOG, NULL, &dwTypeDw, (LPBYTE)&dwLog, &dwSize) == ERROR_SUCCESS) {
                 m_bEnableLog = (dwLog != 0);
            }
            
            RegCloseKey(hKey);
        }
        
        // Defaults
        if (m_szCommand[0] == L'\0') {
            // Nothing to run?
        }
        
        if (m_szArguments[0] == L'\0') {
            StringCchCopy(m_szArguments, MAX_PATH, L"\"%1\"");
        }
        
        if (m_szIcon[0] == L'\0') {
            // Default icon to command exe
            StringCchCopy(m_szIcon, MAX_PATH, m_szCommand);
        }
        
        if (m_szMenuText[0] == L'\0') {
            StringCchCopy(m_szMenuText, 128, L"Open with Application");
        }
    }
};

//==============================================================================
// Class Factory
//==============================================================================
class ClassFactory : public IClassFactory
{
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        static const QITAB qit[] = { QITABENT(ClassFactory, IClassFactory), { 0 } };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release()
    {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
    {
        if (pUnkOuter != NULL) return CLASS_E_NOAGGREGATION;
        UniversalShellExt *pExt = new (std::nothrow) UniversalShellExt(m_clsid);
        if (pExt == NULL) return E_OUTOFMEMORY;
        HRESULT hr = pExt->QueryInterface(riid, ppv);
        pExt->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if (fLock) InterlockedIncrement(&g_cDllRef);
        else InterlockedDecrement(&g_cDllRef);
        return S_OK;
    }
    ClassFactory(const CLSID& clsid) : m_cRef(1), m_clsid(clsid) { }
protected:
    ~ClassFactory() { }
private:
    LONG m_cRef;
    CLSID m_clsid;
};

//==============================================================================
// DLL Exports
//==============================================================================

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    LPOLESTR szCLSID = NULL;
    StringFromCLSID(rclsid, &szCLSID);
    WriteLog(L"DllGetClassObject - Requesting CLSID: %s", szCLSID ? szCLSID : L"Unknown");
    if (szCLSID) CoTaskMemFree(szCLSID);

    *ppv = NULL;
    if (!IsEqualIID(riid, IID_IClassFactory))
        return CLASS_E_CLASSNOTAVAILABLE;

    ClassFactory* pFactory = new (std::nothrow) ClassFactory(rclsid); // Pass CLSID to factory
    if (!pFactory)
        return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void) { return g_cDllRef > 0 ? S_FALSE : S_OK; }

HRESULT SetRegistryValue(HKEY hKeyRoot, LPCWSTR pszSubKey, LPCWSTR pszValueName, LPCWSTR pszValue)
{
    HKEY hKey;
    HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyEx(hKeyRoot, pszSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL));
    if (SUCCEEDED(hr))
    {
        if (pszValue != NULL)
        {
            DWORD cbData = (lstrlen(pszValue) + 1) * sizeof(WCHAR);
            hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey, pszValueName, 0, REG_SZ, (const BYTE*)pszValue, cbData));
        }
        RegCloseKey(hKey);
    }
    return hr;
}

STDAPI DllRegisterServer(void)
{
    WCHAR szModulePath[MAX_PATH];
    if (!GetModuleFileName(g_hInst, szModulePath, ARRAYSIZE(szModulePath))) return HRESULT_FROM_WIN32(GetLastError());

    HRESULT hr = S_OK;
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, L"CLSID\\" CLSID_STRING, NULL, L"Universal Portable Shell Extension");
    if (FAILED(hr)) return hr;
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, L"CLSID\\" CLSID_STRING L"\\InprocServer32", NULL, szModulePath);
    if (FAILED(hr)) return hr;
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, L"CLSID\\" CLSID_STRING L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;
    
    // Register Context Menu Handler
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, L"Directory\\shellex\\ContextMenuHandlers\\UniversalShellExt", NULL, CLSID_STRING);
    if (FAILED(hr)) return hr;
    hr = SetRegistryValue(HKEY_CLASSES_ROOT, L"Directory\\Background\\shellex\\ContextMenuHandlers\\UniversalShellExt", NULL, CLSID_STRING);
    if (FAILED(hr)) return hr;

    // Approve
    hr = SetRegistryValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", CLSID_STRING, L"Universal Shell Extension");
    return hr;
}

STDAPI DllUnregisterServer(void)
{
    RegDeleteTree(HKEY_CLASSES_ROOT, L"CLSID\\" CLSID_STRING);
    RegDeleteTree(HKEY_CLASSES_ROOT, L"Directory\\shellex\\ContextMenuHandlers\\UniversalShellExt");
    RegDeleteTree(HKEY_CLASSES_ROOT, L"Directory\\Background\\shellex\\ContextMenuHandlers\\UniversalShellExt");
    
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
    {
        RegDeleteValue(hKey, CLSID_STRING);
        RegCloseKey(hKey);
    }
    return S_OK;
}

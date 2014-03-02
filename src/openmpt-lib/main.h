#pragma once

#ifndef __AFXWIN_H__
    #error include 'stdafx.h' before including this file for PCH
#endif

#include "stdafx.h"
#include "resource.h"
#include "legacy_soundlib/Sndfile.h"
#include <windows.h>
#include "tracker/constants.hpp"

class CModDoc;
class CVstPluginManager;


/////////////////////////////////////////////////////////////////////////////
// 16-colors DIB
typedef struct MODPLUGDIB
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[16];
    LPBYTE lpDibBits;
} MODPLUGDIB, *LPMODPLUGDIB;


typedef struct MIDILIBSTRUCT
{
    LPTSTR MidiMap[128*2];        // 128 instruments + 128 percussions
} MIDILIBSTRUCT, *LPMIDILIBSTRUCT;


#define MAX_DLS_BANKS    100 //rewbs.increaseMaxDLSBanks

class CDLSBank;


typedef struct MPTCHORD
{
    uint8_t key;
    uint8_t notes[3];
} MPTCHORD, *PMPTCHORD;


typedef struct DRAGONDROP
{
    CModDoc *pModDoc;
    uint32_t dwDropType;
    uint32_t dwDropItem;
    LPARAM lDropParam;
} DRAGONDROP, *LPDRAGONDROP;

enum {
    DRAGONDROP_NOTHING=0,        // |------< Drop Type >-------------|--< dwDropItem >---|--< lDropParam >---|
    DRAGONDROP_DLS,              // | Instrument from a DLS bank     | DLS Bank #        | DLS Instrument    |
    DRAGONDROP_SAMPLE,           // | Sample from a song             | Sample #          | NULL              |
    DRAGONDROP_INSTRUMENT,       // | Instrument from a song         | Instrument #      | NULL              |
    DRAGONDROP_SOUNDFILE,        // | File from instrument library   | ?                 | pszFileName       |
    DRAGONDROP_MIDIINSTR,        // | File from midi library         | Midi Program/Perc | pszFileName       |
    DRAGONDROP_PATTERN,          // | Pattern from a song            |      Pattern #    | NULL              |
    DRAGONDROP_ORDER,            // | Pattern index in a song        |       Order #     | NULL              |
    DRAGONDROP_SONG,             // | Song file (mod/s3m/xm/it)      | 0                 | pszFileName       |
    DRAGONDROP_SEQUENCE          // | Sequence (a set of orders)     |    Sequence #     | NULL              |
};


/////////////////////////////////////////////////////////////////////////////
// File dialog (open/save) results
struct FileDlgResult
{
    std::string workingDirectory;        // working directory. will include filename, so beware.
    std::string first_file;              // for some convenience, this will keep the first filename of the filenames vector.
    std::vector <std::string> filenames; // all selected filenames in one vector.
    std::string extension;               // extension used. beware of this when multiple files can be selected!
    bool abort;                          // no selection has been made.
};


class CTrackApp: public CWinApp {
    friend class CMainFrame;
// static data
protected:
    static UINT m_nDefaultDocType;
    static LPMIDILIBSTRUCT glpMidiLibrary;
    static BOOL m_nProject;

public:
    static MEMORYSTATUS gMemStatus;

protected:
    CMultiDocTemplate *m_pModTemplate;
    CVstPluginManager *m_pPluginManager;
    BOOL m_bInitialized;
    BOOL m_bLayer3Present; //XXXih: trace this!
    BOOL m_bDebugMode;
    uint32_t m_dwTimeStarted, m_dwLastPluginIdleCall;
    HANDLE m_hAlternateResourceHandle;
    // Default macro configuration
    MODMIDICFG m_MidiCfg;
    static TCHAR m_szExePath[_MAX_PATH];
    TCHAR m_szConfigDirectory[_MAX_PATH];
    TCHAR m_szConfigFileName[_MAX_PATH];
    TCHAR m_szPluginCacheFileName[_MAX_PATH];
    TCHAR m_szStringsFileName[_MAX_PATH];
    static bool m_bPortableMode;

public:
    CTrackApp();

public:
    //XXXih: qt hurrr
    virtual BOOL Run() override;

public:
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    static BOOL IsProject() { return m_nProject; }
    static VOID SetAsProject(BOOL n) { m_nProject = n; }
// -! NEW_FEATURE#0023

    static LPCTSTR GetAppDirPath() {return m_szExePath;} // Returns '\'-ended executable directory path.
    static UINT GetDefaultDocType() { return m_nDefaultDocType; }
    static VOID SetDefaultDocType(UINT n) { m_nDefaultDocType = n; }
    static LPMIDILIBSTRUCT GetMidiLibrary() { return glpMidiLibrary; }
    static void RegisterExtensions();

    static FileDlgResult ShowOpenSaveFileDialog(const bool load, const std::string defaultExtension, const std::string defaultFilename, const std::string extFilter, const std::string workingDirectory = "", const bool allowMultiSelect = false, int *filterIndex = nullptr);

public:
    CDocTemplate *GetModDocTemplate() const { return m_pModTemplate; }
    CVstPluginManager *GetPluginManager() const { return m_pPluginManager; }
    void GetDefaultMidiMacro(MODMIDICFG *pcfg) const { *pcfg = m_MidiCfg; }
    void SetDefaultMidiMacro(const MODMIDICFG *pcfg) { m_MidiCfg = *pcfg; }
    void LoadChords(PMPTCHORD pChords);
    void SaveChords(PMPTCHORD pChords);
    BOOL CanEncodeLayer3() const { return m_bLayer3Present; }
    BOOL IsDebug() const { return m_bDebugMode; }
    LPCSTR GetConfigFileName() const { return m_szConfigFileName; }
    static bool IsPortableMode() { return m_bPortableMode; }
    LPCSTR GetPluginCacheFileName() const { return m_szPluginCacheFileName; }
    LPCSTR GetConfigPath() const { return m_szConfigDirectory; }
    void SetupPaths();

// Localized strings
public:
    VOID ImportLocalizedStrings();
    BOOL GetLocalizedString(LPCSTR pszName, LPSTR pszStr, UINT cbSize);

public:
    BOOL deprecated_InitializeDXPlugins();
    BOOL deprecated_UninitializeDXPlugins();

// Overrides
public:
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CTrackApp)
    public:
    virtual BOOL InitInstance();
    virtual int ExitInstance();
    virtual BOOL OnIdle(LONG lCount);
    //}}AFX_VIRTUAL

// Implementation

    //{{AFX_MSG(CTrackApp)
    afx_msg void OnFileNew();
    afx_msg void OnFileNewMOD();
    afx_msg void OnFileNewS3M();
    afx_msg void OnFileNewXM();
    afx_msg void OnFileNewIT();
    afx_msg void OnFileNewMPT();
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    afx_msg void OnFileNewITProject();
// -! NEW_FEATURE#0023

    afx_msg void OnFileOpen();
    afx_msg void OnAppAbout();
    afx_msg void OnHelpSearch();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
    virtual LRESULT ProcessWndProcException(CException* e, const MSG* pMsg);

private:
    static void LoadRegistryDLS();

    #ifdef WIN32        // Legacy stuff
    bool MoveConfigFile(TCHAR sFileName[_MAX_PATH], TCHAR sSubDir[_MAX_PATH] = "", TCHAR sNewFileName[_MAX_PATH] = "");
    #endif
};


class CButtonEx: public CButton {
protected:
    MODPLUGDIB m_Dib;
    RECT m_srcRect;
    BOOL m_bPushed;

public:
    CButtonEx() { m_Dib.lpDibBits = NULL; m_bPushed = FALSE; }
    BOOL Init(const LPMODPLUGDIB pDib, COLORREF colorkey=RGB(0,128,128));
    BOOL SetSourcePos(int x, int y=0, int cx=16, int cy=15);
    BOOL AlignButton(UINT nIdPrev, int dx=0);
    BOOL AlignButton(const CWnd &wnd, int dx=0) { return AlignButton(wnd.m_hWnd, dx); }
    BOOL AlignButton(HWND hwnd, int dx=0);
    BOOL GetPushState() const { return m_bPushed; }
    void SetPushState(BOOL bPushed);

protected:
    //{{AFX_VIRTUAL(CButtonEx)
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    //}}AFX_VIRTUAL
    //{{AFX_MSG(CButtonEx)
    afx_msg BOOL OnEraseBkgnd(CDC *) { return TRUE; }
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP();
};

extern CTrackApp theApp;

class CMappedFile {
protected:
    CFile m_File;
    HANDLE m_hFMap;
    LPVOID m_lpData;

public:
    CMappedFile();
    virtual ~CMappedFile();

public:
    BOOL Open(LPCSTR lpszFileName);
    void Close();
    uint32_t GetLength();
    LPBYTE Lock(uint32_t dwMaxLen=0);
    BOOL Unlock();
};


//////////////////////////////////////////////////////////////////
// More Bitmap Helpers

#define FASTBMP_XSHIFT                    13        // 4K pixels
#define FASTBMP_MAXWIDTH            (1 << FASTBMP_XSHIFT)
#define FASTBMP_MAXHEIGHT            16

typedef struct MODPLUGFASTDIB
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[256];
    uint8_t DibBits[FASTBMP_MAXWIDTH*FASTBMP_MAXHEIGHT];
} MODPLUGFASTDIB, *LPMODPLUGFASTDIB;

class CFastBitmap {
protected:
    MODPLUGFASTDIB m_Dib;
    UINT m_nTextColor, m_nBkColor;
    LPMODPLUGDIB m_pTextDib;
    uint8_t m_nBlendOffset;
    uint8_t m_n4BitPalette[16];

public:
    CFastBitmap() {}

public:
    void Init(LPMODPLUGDIB lpTextDib=NULL);
    void Blit(HDC hdc, int x, int y, int cx, int cy);
    void Blit(HDC hdc, LPCRECT lprc) { Blit(hdc, lprc->left, lprc->top, lprc->right-lprc->left, lprc->bottom-lprc->top); }
    void SetTextColor(int nText, int nBk=-1) { m_nTextColor = nText; if (nBk >= 0) m_nBkColor = nBk; }
    void SetTextBkColor(UINT nBk) { m_nBkColor = nBk; }
    void SetColor(UINT nIndex, COLORREF cr);
    void SetAllColors(UINT nBaseIndex, UINT nColors, COLORREF *pcr);
    void TextBlt(int x, int y, int cx, int cy, int srcx, int srcy, LPMODPLUGDIB lpdib=NULL);
    void SetBlendMode(uint8_t nBlendOfs) { m_nBlendOffset = nBlendOfs; }
    void SetBlendColor(COLORREF cr);
};


///////////////////////////////////////////////////
// 4-bit DIB Drawing functions
void DibBlt(HDC hdc, int x, int y, int sizex, int sizey, int srcx, int srcy, LPMODPLUGDIB lpdib);
LPMODPLUGDIB LoadDib(LPCSTR lpszName);
RGBQUAD rgb2quad(COLORREF c);

// Other bitmap functions
void DrawBitmapButton(HDC hdc, LPRECT lpRect, LPMODPLUGDIB lpdib, int srcx, int srcy, BOOL bPushed);
void DrawButtonRect(HDC hdc, LPRECT lpRect, LPCSTR lpszText=NULL, BOOL bDisabled=FALSE, BOOL bPushed=FALSE, uint32_t dwFlags=(DT_CENTER|DT_VCENTER));

// Misc functions
class CVstPlugin;
void Log(LPCSTR fmt, ...);
UINT MsgBox(UINT nStringID, CWnd *p=NULL, LPCSTR lpszTitle=NULL, UINT n=MB_OK);
void ErrorBox(UINT nStringID, CWnd*p=NULL);

// Helper function declarations.
void AddPluginNamesToCombobox(CComboBox& CBox, SNDMIXPLUGIN* plugarray, const bool librarynames = false);
void AddPluginParameternamesToCombobox(CComboBox& CBox, SNDMIXPLUGIN& plugarray);
void AddPluginParameternamesToCombobox(CComboBox& CBox, CVstPlugin& plug);

// Append note names in range [noteStart, noteEnd] to given combobox. Index starts from 0.
void AppendNotesToControl(CComboBox& combobox, const modplug::tracker::note_t noteStart, const modplug::tracker::note_t noteEnd);

// Append note names to combobox. If pSndFile != nullprt, appends only notes that are
// available in the module type. If nInstr is given, instrument specific note names are used instead of
// default note names.
void AppendNotesToControlEx(CComboBox& combobox, const module_renderer* const pSndFile = nullptr, const modplug::tracker::instrumentindex_t nInstr = MAX_INSTRUMENTS);

// Returns note name(such as "C-5") of given note. Regular notes are in range [1,MAX_NOTE].
LPCTSTR GetNoteStr(const modplug::tracker::note_t);

///////////////////////////////////////////////////
// Tables

extern const uint8_t gEffectColors[modplug::tracker::CmdMax];
extern const uint8_t gVolEffectColors[modplug::tracker::VolCmdMax];
extern const LPCSTR szNoteNames[12];
extern const LPCTSTR szDefaultNoteNames[modplug::tracker::NoteCount];
//const LPCTSTR szSpecialNoteNames[NOTE_MAX_SPECIAL - NOTE_MIN_SPECIAL + 1] = {TEXT("PCs"), TEXT("PC"), TEXT("~~"), TEXT("^^"), TEXT("==")};

const LPCTSTR szSpecialNoteNames[(size_t)(modplug::tracker::NoteMaxSpecial - modplug::tracker::NoteMinSpecial) + 1] = {
    TEXT("PCs"),
    TEXT("PC"),
    TEXT("Note Fade"),
    TEXT("Note Cut"),
    TEXT("Note Off")
};
const LPCTSTR szSpecialNoteShortDesc[(size_t)(modplug::tracker::NoteMaxSpecial - modplug::tracker::NoteMinSpecial) + 1] = {
    TEXT("Param Control (Smooth)"),
    TEXT("Param Control"),
    TEXT("Note Fade"),
    TEXT("Note Cut"),
    TEXT("Note Off")
};

static_assert(
    modplug::tracker::NoteMaxSpecial - modplug::tracker::NoteMinSpecial + 1 == CountOf(szSpecialNoteNames),
    "special note arrays must include a descriptive string for every note");
static_assert(
    CountOf(szSpecialNoteShortDesc) == CountOf(szSpecialNoteNames),
    "sizeof(szSpecialNoteShortDesc) must equal sizeof(szSpecialNoteNames)");

// Defined in load_mid.cpp
extern const LPCSTR szMidiProgramNames[128];
extern const LPCSTR szMidiPercussionNames[61]; // notes 25..85
extern const LPCSTR szMidiGroupNames[17];            // 16 groups + Percussions

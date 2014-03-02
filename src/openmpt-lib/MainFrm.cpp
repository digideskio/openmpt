// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"

#include <iostream>

#include "MainFrm.h"
#include "moddoc.h"
#include "childfrm.h"
#include "mpdlgs.h"
#include "moptions.h"
#include "vstplug.h"
#include "mainfrm.h"
// -> CODE#0015
// -> DESC="channels management dlg"
#include "globals.h"
#include "ChannelManagerDlg.h"
#include "MIDIMappingDialog.h"
// -! NEW_FEATURE#0015
#include <direct.h>
#include "version.h"
#include "ctrl_pat.h"

#include "gui/qt5/mfc_root.hpp"
#include "gui/qt5/config_dialog.hpp"

#include "pervasives/pervasives.hpp"
using namespace modplug::pervasives;
using namespace modplug::tracker;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAINFRAME_REGKEY_BASE            "Software\\Olivier Lapicque\\"
#define MAINFRAME_REGKEY_DEFAULT    "ModPlug Tracker"
#define MAINFRAME_REGEXT_WINDOW            "\\Window"
#define MAINFRAME_REGEXT_SETTINGS    "\\Settings"

#define MPTTIMER_PERIOD            200

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
    //{{AFX_MSG_MAP(CMainFrame)
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_CREATE()
    ON_WM_RBUTTONDOWN()
    ON_COMMAND(ID_VIEW_OPTIONS,                            OnViewOptions)
    ON_COMMAND(ID_VIEW_SOOPERSETUP,         display_config_editor)

// -> CODE#0002
// -> DESC="list box to choose VST plugin presets (programs)"
    ON_COMMAND(ID_PLUGIN_SETUP,                            OnPluginManager)
// -! NEW_FEATURE#0002

// -> CODE#0015
// -> DESC="channels management dlg"
    ON_COMMAND(ID_CHANNEL_MANAGER,                    OnChannelManager)
// -! NEW_FEATURE#0015
    ON_COMMAND(ID_VIEW_MIDIMAPPING,                    OnViewMIDIMapping)
    //ON_COMMAND(ID_HELP,                                    CMDIFrameWnd::OnHelp)
    ON_COMMAND(ID_VIEW_SONGPROPERTIES,            OnSongProperties)
    ON_COMMAND(ID_HELP_FINDER,                            CMDIFrameWnd::OnHelpFinder)
    ON_COMMAND(ID_CONTEXT_HELP,                            CMDIFrameWnd::OnContextHelp)
    ON_COMMAND(ID_DEFAULT_HELP,                            CMDIFrameWnd::OnHelpFinder)
    ON_COMMAND(ID_NEXTOCTAVE,                            OnNextOctave)
    ON_COMMAND(ID_PREVOCTAVE,                            OnPrevOctave)
    ON_COMMAND(ID_MIDI_RECORD,                            OnMidiRecord)
    ON_COMMAND(ID_PANIC,                                    OnPanic)
    ON_COMMAND(ID_PLAYER_PAUSE,                            OnPlayerPause)
    ON_COMMAND_EX(IDD_TREEVIEW,                            OnBarCheck)
    ON_CBN_SELCHANGE(IDC_COMBO_BASEOCTAVE,    OnOctaveChanged)
    ON_UPDATE_COMMAND_UI(ID_MIDI_RECORD,    OnUpdateMidiRecord)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_TIME,    OnUpdateTime)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_USER,    OnUpdateUser)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_INFO,    OnUpdateInfo)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_XINFO,OnUpdateXInfo) //rewbs.xinfo
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_CPU,  OnUpdateCPU)
    ON_UPDATE_COMMAND_UI(IDD_TREEVIEW,            OnUpdateControlBarMenu)
    ON_MESSAGE(WM_MOD_UPDATEPOSITION,            OnUpdatePosition)
    ON_MESSAGE(WM_MOD_INVALIDATEPATTERNS,    OnInvalidatePatterns)
    ON_MESSAGE(WM_MOD_SPECIALKEY,                    OnSpecialKey)
    //}}AFX_MSG_MAP
    ON_WM_INITMENU()
    ON_WM_KILLFOCUS() //rewbs.fix3116
    ON_WM_MOUSEWHEEL()
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

// Static
static uint32_t gsdwTotalSamples = 0;

// Globals
UINT CMainFrame::gnPatternSpacing = 0;
BOOL CMainFrame::gbPatternRecord = TRUE;
BOOL CMainFrame::gbPatternVUMeters = FALSE;
BOOL CMainFrame::gbPatternPluginNames = TRUE;
uint32_t CMainFrame::gdwNotificationType = MPTNOTIFY_DEFAULT;
UINT CMainFrame::m_nLastOptionsPage = 0;
BOOL CMainFrame::gbMdiMaximize = FALSE;
bool CMainFrame::gbShowHackControls = false;
//rewbs.varWindowSize
LONG CMainFrame::glGeneralWindowHeight = 178;
LONG CMainFrame::glPatternWindowHeight = 152;
LONG CMainFrame::glSampleWindowHeight = 188;
LONG CMainFrame::glInstrumentWindowHeight = 300;
LONG CMainFrame::glCommentsWindowHeight = 288;
LONG CMainFrame::glGraphWindowHeight = 288; //rewbs.graph
//end rewbs.varWindowSize
LONG CMainFrame::glTreeWindowWidth = 160;
LONG CMainFrame::glTreeSplitRatio = 128;
HHOOK CMainFrame::ghKbdHook = NULL;
CString CMainFrame::gcsPreviousVersion = "";
CString CMainFrame::gcsInstallGUID = "";

uint32_t CMainFrame::gnHotKeyMask = 0;
// Audio Setup
//rewbs.resamplerConf
long CMainFrame::glVolumeRampInSamples = 0;
long CMainFrame::glVolumeRampOutSamples = 42;
double CMainFrame::gdWFIRCutoff = 0.97;
uint8_t  CMainFrame::gbWFIRType = 7; //WFIR_KAISER4T;
//end rewbs.resamplerConf
UINT CMainFrame::gnAutoChordWaitTime = 60;

int CMainFrame::gnPlugWindowX = 243;
int CMainFrame::gnPlugWindowY = 273;
int CMainFrame::gnPlugWindowWidth = 370;
int CMainFrame::gnPlugWindowHeight = 332;
uint32_t CMainFrame::gnPlugWindowLast = 0;

uint32_t CMainFrame::gnMsgBoxVisiblityFlags = UINT32_MAX;

CRITICAL_SECTION CMainFrame::m_csAudio;
HANDLE CMainFrame::m_hNotifyThread = NULL;
DWORD CMainFrame::m_dwNotifyThreadId = 0;
HANDLE CMainFrame::m_hNotifyWakeUp = NULL;
LONG CMainFrame::slSampleSize = 2;
LONG CMainFrame::sdwSamplesPerSec = 44100;
LONG CMainFrame::sdwAudioBufferSize = MAX_AUDIO_BUFFERSIZE;
uint32_t CMainFrame::deprecated_m_dwQuality = 0;
uint32_t CMainFrame::m_nSrcMode = SRCMODE_LINEAR;
uint32_t CMainFrame::m_nPreAmp = 128;
uint32_t CMainFrame::gbLoopSong = TRUE;
LONG CMainFrame::m_nWaveDevice = 0;
LONG CMainFrame::m_nMidiDevice = 0;
LONG CMainFrame::gnLVuMeter = 0;
LONG CMainFrame::gnRVuMeter = 0;
// Midi Setup
uint32_t CMainFrame::m_dwMidiSetup = MIDISETUP_RECORDVELOCITY|MIDISETUP_RECORDNOTEOFF;
// Pattern Setup
uint32_t CMainFrame::m_dwPatternSetup = PATTERN_PLAYNEWNOTE | PATTERN_EFFECTHILIGHT
                                   | PATTERN_SMALLFONT | PATTERN_CENTERROW
                                   | PATTERN_DRAGNDROPEDIT | PATTERN_FLATBUTTONS | PATTERN_NOEXTRALOUD
                                   | PATTERN_2NDHIGHLIGHT | PATTERN_STDHIGHLIGHT /*| PATTERN_HILITETIMESIGS*/
                                   | PATTERN_SHOWPREVIOUS | PATTERN_CONTSCROLL | PATTERN_SYNCMUTE | PATTERN_AUTODELAY | PATTERN_NOTEFADE;
uint32_t CMainFrame::m_nRowSpacing = 16;    // primary highlight (measures)
uint32_t CMainFrame::m_nRowSpacing2 = 4;    // secondary highlight (beats)
UINT CMainFrame::m_nSampleUndoMaxBuffer = 0;    // Real sample buffer undo size will be set later.

// GDI
HICON CMainFrame::m_hIcon = NULL;
HFONT CMainFrame::m_hGUIFont = NULL;
HFONT CMainFrame::m_hFixedFont = NULL;
HFONT CMainFrame::m_hLargeFixedFont = NULL;
HPEN CMainFrame::penDarkGray = NULL;
HPEN CMainFrame::penScratch = NULL; //rewbs.fxVis
HPEN CMainFrame::penGray00 = NULL;
HPEN CMainFrame::penGray33 = NULL;
HPEN CMainFrame::penGray40 = NULL;
HPEN CMainFrame::penGray55 = NULL;
HPEN CMainFrame::penGray80 = NULL;
HPEN CMainFrame::penGray99 = NULL;
HPEN CMainFrame::penGraycc = NULL;
HPEN CMainFrame::penGrayff = NULL; //end rewbs.fxVis
HPEN CMainFrame::penLightGray = NULL;
HPEN CMainFrame::penBlack = NULL;
HPEN CMainFrame::penWhite = NULL;
HPEN CMainFrame::penHalfDarkGray = NULL;
HPEN CMainFrame::penSample = NULL;
HPEN CMainFrame::penEnvelope = NULL;
HPEN CMainFrame::penEnvelopeHighlight = NULL;
HPEN CMainFrame::penSeparator = NULL;
HBRUSH CMainFrame::brushGray = NULL;
HBRUSH CMainFrame::brushBlack = NULL;
HBRUSH CMainFrame::brushWhite = NULL;
HBRUSH CMainFrame::brushText = NULL;
//CBrush *CMainFrame::pbrushBlack = NULL;//rewbs.envRowGrid
//CBrush *CMainFrame::pbrushWhite = NULL;//rewbs.envRowGrid

HBRUSH CMainFrame::brushHighLight = NULL;
HBRUSH CMainFrame::brushHighLightRed = NULL;
HBRUSH CMainFrame::brushWindow = NULL;
HBRUSH CMainFrame::brushYellow = NULL;
HCURSOR CMainFrame::curDragging = NULL;
HCURSOR CMainFrame::curArrow = NULL;
HCURSOR CMainFrame::curNoDrop = NULL;
HCURSOR CMainFrame::curNoDrop2 = NULL;
HCURSOR CMainFrame::curVSplit = NULL;
LPMODPLUGDIB CMainFrame::bmpPatterns = NULL;
LPMODPLUGDIB CMainFrame::bmpNotes = NULL;
LPMODPLUGDIB CMainFrame::bmpVUMeters = NULL;
LPMODPLUGDIB CMainFrame::bmpVisNode = NULL;
LPMODPLUGDIB CMainFrame::bmpVisPcNode = NULL;
HPEN CMainFrame::gpenVuMeter[NUM_VUMETER_PENS*2];
COLORREF CMainFrame::rgbCustomColors[MAX_MODCOLORS] =
    {
        RGB(0xFF, 0xFF, 0xFF), RGB(0x00, 0x00, 0x00), RGB(0xC0, 0xC0, 0xC0), RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x00), RGB(0xFF, 0xFF, 0xFF), 0x0000FF,
        RGB(0xFF, 0xFF, 0x80), RGB(0x00, 0x00, 0x00), RGB(0xE0, 0xE8, 0xE0),
        // Effect Colors
        RGB(0x00, 0x00, 0x80), RGB(0x00, 0x80, 0x80), RGB(0x00, 0x80, 0x00), RGB(0x00, 0x80, 0x80), RGB(0x80, 0x80, 0x00), RGB(0x80, 0x00, 0x00), RGB(0x00, 0x00, 0xFF),
        // VU-Meters
        RGB(0x00, 0xC8, 0x00), RGB(0xFF, 0xC8, 0x00), RGB(0xE1, 0x00, 0x00),
        // Channel separators
        GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_BTNHIGHLIGHT),
        // Blend colour
        GetSysColor(COLOR_BTNFACE),
        // Dodgy commands
        RGB(0xC0, 0x00, 0x00),
    };

// Directory Arrays (Default + Last)
TCHAR CMainFrame::m_szDefaultDirectory[NUM_DIRS][_MAX_PATH] = {0};
TCHAR CMainFrame::m_szWorkingDirectory[NUM_DIRS][_MAX_PATH] = {0};
TCHAR CMainFrame::m_szKbdFile[_MAX_PATH] = "";                    //rewbs.customKeys
// Directory to INI setting translation
const TCHAR CMainFrame::m_szDirectoryToSettingsName[NUM_DIRS][32] =
{
    _T("Songs_Directory"), _T("Samples_Directory"), _T("Instruments_Directory"), _T("Plugins_Directory"), _T("Plugin_Presets_Directory"), _T("Export_Directory"), _T("")
};


static UINT indicators[] =
{
    ID_SEPARATOR,           // status line indicator
    ID_INDICATOR_XINFO,            //rewbs.xinfo
    ID_INDICATOR_INFO,
    ID_INDICATOR_USER,
    ID_INDICATOR_TIME,
    ID_INDICATOR_CPU
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction
//#include <direct.h>
CMainFrame::CMainFrame() :
    pa_auto_system(),
    pa_system(portaudio::System::instance()),
    global_config(pa_system)
{
    m_bModTreeHasFocus = false;    //rewbs.customKeys
    m_pNoteMapHasFocus = nullptr;    //rewbs.customKeys
    m_pOrderlistHasFocus = nullptr;
    m_bOptionsLocked = false;    //rewbs.customKeys

    m_pJustModifiedDoc = nullptr;
    m_pModPlaying = nullptr;
    m_hFollowSong = NULL;
    m_hWndMidi = NULL;
    renderer = nullptr;
    m_dwStatus = 0;
    m_dwElapsedTime = 0;
    m_dwTimeSec = 0;
    m_dwNotifyType = 0;
    m_nTimer = 0;
    m_nAvgMixChn = m_nMixChn = 0;
    m_szUserText[0] = 0;
    m_szInfoText[0] = 0;
    m_szXInfoText[0]= 0;    //rewbs.xinfo

    for(UINT i = 0; i < NUM_DIRS; i++)
    {
        if (i == DIR_TUNING) // Hack: Tuning folder is set already so don't reset it.
            continue;
        MemsetZero(m_szDefaultDirectory[i]);
        MemsetZero(m_szWorkingDirectory[i]);
    }

    m_dTotalCPU=0;
    MemsetZero(gpenVuMeter);

    // Default chords
    MemsetZero(Chords);
    for (UINT ichord=0; ichord<3*12; ichord++)
    {
        Chords[ichord].key = (uint8_t)ichord;
        Chords[ichord].notes[0] = 0;
        Chords[ichord].notes[1] = 0;
        Chords[ichord].notes[2] = 0;
        // Major Chords
        if (ichord < 12)
        {
            Chords[ichord].notes[0] = (uint8_t)(ichord+5);
            Chords[ichord].notes[1] = (uint8_t)(ichord+8);
            Chords[ichord].notes[2] = (uint8_t)(ichord+11);
        } else
        // Minor Chords
        if (ichord < 24)
        {
            Chords[ichord].notes[0] = (uint8_t)(ichord-8);
            Chords[ichord].notes[1] = (uint8_t)(ichord-4);
            Chords[ichord].notes[2] = (uint8_t)(ichord-1);
        }
    }


    auto &default_output = pa_system.defaultOutputDevice();

    modplug::audioio::paudio_settings_t stream_settings;
    stream_settings.latency  = default_output.defaultLowOutputLatency();
    stream_settings.host_api = default_output.hostApi().typeId();
    stream_settings.device   = default_output.index();
    stream_settings.sample_rate   = 44100.0;
    stream_settings.channels      = 2;
    stream_settings.buffer_length = 512;
    global_config.change_audio_settings(stream_settings);
    DEBUG_FUNC("init stream_settings = %s", debug_json_dump(
        modplug::audioio::json_of_paudio_settings(stream_settings, pa_system)
    ).c_str());
    stream = std::make_shared<modplug::audioio::paudio>(stream_settings, pa_system, *this);

    // Create Audio Critical Section
    MemsetZero(m_csAudio);
    InitializeCriticalSection(&m_csAudio);

    m_csRegKey.Format("%s%s", MAINFRAME_REGKEY_BASE, MAINFRAME_REGKEY_DEFAULT);
    m_csRegSettings.Format("%s%s", m_csRegKey, MAINFRAME_REGEXT_SETTINGS);
    m_csRegWindow.Format("%s%s", m_csRegKey, MAINFRAME_REGEXT_WINDOW);

    CString storedVersion = GetPrivateProfileCString("Version", "Version", "", theApp.GetConfigFileName());
    // If version number stored in INI is 1.17.02.40 or later, always load setting from INI file.
    // If it isn't, try loading from Registry first, then from the INI file.
    if (storedVersion >= "1.17.02.40" || !LoadRegistrySettings())
    {
        LoadIniSettings();
    }

    //Loading static tunings here - probably not the best place to do that but anyway.
    module_renderer::LoadStaticTunings();
}

void CMainFrame::LoadIniSettings()
//--------------------------------
{
    CString iniFile = theApp.GetConfigFileName();
    //CHAR collectedString[INIBUFFERSIZE];
    MptVersion::VersionNum vIniVersion;

    gcsPreviousVersion = GetPrivateProfileCString("Version", "Version", "", iniFile);
    if(gcsPreviousVersion == "")
        vIniVersion = MptVersion::num;
    else
        vIniVersion = MptVersion::ToNum(gcsPreviousVersion);

    gcsInstallGUID = GetPrivateProfileCString("Version", "InstallGUID", "", iniFile);
    if(gcsInstallGUID == "")
    {
        // No GUID found in INI file - generate one.
        GUID guid;
        CoCreateGuid(&guid);
        uint8_t* Str;
        UuidToString((UUID*)&guid, &Str);
        gcsInstallGUID.Format("%s", (LPTSTR)Str);
        RpcStringFree(&Str);
    }

    gbMdiMaximize = GetPrivateProfileLong("Display", "MDIMaximize", true, iniFile);
    glTreeWindowWidth = GetPrivateProfileLong("Display", "MDITreeWidth", 160, iniFile);
    glTreeSplitRatio = GetPrivateProfileLong("Display", "MDITreeRatio", 128, iniFile);
    glGeneralWindowHeight = GetPrivateProfileLong("Display", "MDIGeneralHeight", 178, iniFile);
    glPatternWindowHeight = GetPrivateProfileLong("Display", "MDIPatternHeight", 152, iniFile);
    glSampleWindowHeight = GetPrivateProfileLong("Display", "MDISampleHeight", 188, iniFile);
    glInstrumentWindowHeight = GetPrivateProfileLong("Display", "MDIInstrumentHeight", 300, iniFile);
    glCommentsWindowHeight = GetPrivateProfileLong("Display", "MDICommentsHeight", 288, iniFile);
    glGraphWindowHeight = GetPrivateProfileLong("Display", "MDIGraphHeight", 288, iniFile); //rewbs.graph
    gnPlugWindowX = GetPrivateProfileInt("Display", "PlugSelectWindowX", 243, iniFile);
    gnPlugWindowY = GetPrivateProfileInt("Display", "PlugSelectWindowY", 273, iniFile);
    gnPlugWindowWidth = GetPrivateProfileInt("Display", "PlugSelectWindowWidth", 370, iniFile);
    gnPlugWindowHeight = GetPrivateProfileInt("Display", "PlugSelectWindowHeight", 332, iniFile);
    gnPlugWindowLast = GetPrivateProfileDWord("Display", "PlugSelectWindowLast", 0, iniFile);
    gnMsgBoxVisiblityFlags = GetPrivateProfileDWord("Display", "MsgBoxVisibilityFlags", UINT32_MAX, iniFile);

    CHAR s[16];
    for (int ncol = 0; ncol < MAX_MODCOLORS; ncol++)
    {
        wsprintf(s, "Color%02d", ncol);
        rgbCustomColors[ncol] = GetPrivateProfileDWord("Display", s, rgbCustomColors[ncol], iniFile);
    }

    uint32_t defaultDevice = 0; //XXXih: portaudio

    m_nWaveDevice = GetPrivateProfileLong("Sound Settings", "WaveDevice", defaultDevice, iniFile);
    deprecated_m_dwQuality = GetPrivateProfileDWord("Sound Settings", "Quality", 0, iniFile);
    m_nSrcMode = GetPrivateProfileDWord("Sound Settings", "SrcMode", SRCMODE_POLYPHASE, iniFile);

    m_nPreAmp = GetPrivateProfileDWord("Sound Settings", "PreAmp", 128, iniFile);
    module_renderer::m_nStereoSeparation = GetPrivateProfileLong("Sound Settings", "StereoSeparation", 128, iniFile);
    module_renderer::m_nMaxMixChannels = GetPrivateProfileLong("Sound Settings", "MixChannels", MAX_VIRTUAL_CHANNELS, iniFile);
    gbWFIRType = static_cast<uint8_t>(GetPrivateProfileDWord("Sound Settings", "XMMSModplugResamplerWFIRType", 7, iniFile));
    gdWFIRCutoff = static_cast<double>(GetPrivateProfileLong("Sound Settings", "ResamplerWFIRCutoff", 97, iniFile))/100.0;
    //XXXih: kill the old registry entry or something
    glVolumeRampInSamples = GetPrivateProfileLong("Sound Settings", "VolumeRampInSamples", 0, iniFile);
    glVolumeRampOutSamples = GetPrivateProfileLong("Sound Settings", "VolumeRampOutSamples", 42, iniFile);

    m_dwMidiSetup = GetPrivateProfileDWord("MIDI Settings", "MidiSetup", m_dwMidiSetup, iniFile);
    m_nMidiDevice = GetPrivateProfileDWord("MIDI Settings", "MidiDevice", m_nMidiDevice, iniFile);

    m_dwPatternSetup = GetPrivateProfileDWord("Pattern Editor", "PatternSetup", m_dwPatternSetup, iniFile);
    if(vIniVersion < MAKE_VERSION_NUMERIC(1,17,02,50))
        m_dwPatternSetup |= PATTERN_NOTEFADE;
    if(vIniVersion < MAKE_VERSION_NUMERIC(1,17,03,01))
        m_dwPatternSetup |= PATTERN_RESETCHANNELS;
    if(vIniVersion < MAKE_VERSION_NUMERIC(1,19,00,07))
        m_dwPatternSetup &= ~0x800;                                    // this was previously deprecated and is now used for something else
    if(vIniVersion < MptVersion::num)
        m_dwPatternSetup &= ~(0x200000|0x400000|0x10000000);    // various deprecated old options

    m_nRowSpacing = GetPrivateProfileDWord("Pattern Editor", "RowSpacing", 16, iniFile);
    m_nRowSpacing2 = GetPrivateProfileDWord("Pattern Editor", "RowSpacing2", 4, iniFile);
    gbLoopSong = GetPrivateProfileDWord("Pattern Editor", "LoopSong", true, iniFile);
    gnPatternSpacing = GetPrivateProfileDWord("Pattern Editor", "Spacing", 1, iniFile);
    gbPatternVUMeters = GetPrivateProfileDWord("Pattern Editor", "VU-Meters", false, iniFile);
    gbPatternPluginNames = GetPrivateProfileDWord("Pattern Editor", "Plugin-Names", true, iniFile);
    gbPatternRecord = GetPrivateProfileDWord("Pattern Editor", "Record", true, iniFile);
    gnAutoChordWaitTime = GetPrivateProfileDWord("Pattern Editor", "AutoChordWaitTime", 60, iniFile);
    COrderList::s_nDefaultMargins = static_cast<uint8_t>(GetPrivateProfileInt("Pattern Editor", "DefaultSequenceMargins", 2, iniFile));
    gbShowHackControls = (0 != GetPrivateProfileDWord("Misc", "ShowHackControls", 0, iniFile));
    module_renderer::s_DefaultPlugVolumeHandling = static_cast<uint8_t>(GetPrivateProfileInt("Misc", "DefaultPlugVolumeHandling", PLUGIN_VOLUMEHANDLING_IGNORE, iniFile));
    if(module_renderer::s_DefaultPlugVolumeHandling > 2) module_renderer::s_DefaultPlugVolumeHandling = PLUGIN_VOLUMEHANDLING_IGNORE;

    m_nSampleUndoMaxBuffer = GetPrivateProfileLong("Sample Editor" , "UndoBufferSize", m_nSampleUndoMaxBuffer >> 20, iniFile);
    m_nSampleUndoMaxBuffer = bad_max(1, m_nSampleUndoMaxBuffer) << 20;

    TCHAR szPath[_MAX_PATH] = "";
    for(size_t i = 0; i < NUM_DIRS; i++)
    {
        if(m_szDirectoryToSettingsName[i][0] == 0)
            continue;

        GetPrivateProfileString("Paths", m_szDirectoryToSettingsName[i], GetDefaultDirectory(static_cast<Directory>(i)), szPath, CountOf(szPath), iniFile);
        RelativePathToAbsolute(szPath);
        SetDefaultDirectory(szPath, static_cast<Directory>(i), false);

    }
    GetPrivateProfileString("Paths", "Key_Config_File", m_szKbdFile, m_szKbdFile, INIBUFFERSIZE, iniFile);
    RelativePathToAbsolute(m_szKbdFile);
}

bool CMainFrame::LoadRegistrySettings()
//-------------------------------------
{

    HKEY key;
    uint32_t dwREG_DWORD = REG_DWORD;
    uint32_t dwREG_SZ = REG_SZ;
    uint32_t dwDWORDSize = sizeof(UINT);
    uint32_t dwCRSIZE = sizeof(COLORREF);

    if (RegOpenKeyEx(HKEY_CURRENT_USER,    m_csRegWindow, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        uint32_t d = 0;
        registry_query_value(key, "Maximized", NULL, &dwREG_DWORD, (LPBYTE)&d, &dwDWORDSize);
        if (d) theApp.m_nCmdShow = SW_SHOWMAXIMIZED;
        registry_query_value(key, "MDIMaximize", NULL, &dwREG_DWORD, (LPBYTE)&gbMdiMaximize, &dwDWORDSize);
        registry_query_value(key, "MDITreeWidth", NULL, &dwREG_DWORD, (LPBYTE)&glTreeWindowWidth, &dwDWORDSize);
        registry_query_value(key, "MDIGeneralHeight", NULL, &dwREG_DWORD, (LPBYTE)&glGeneralWindowHeight, &dwDWORDSize);
        registry_query_value(key, "MDIPatternHeight", NULL, &dwREG_DWORD, (LPBYTE)&glPatternWindowHeight, &dwDWORDSize);
        registry_query_value(key, "MDISampleHeight", NULL, &dwREG_DWORD,  (LPBYTE)&glSampleWindowHeight, &dwDWORDSize);
        registry_query_value(key, "MDIInstrumentHeight", NULL, &dwREG_DWORD,  (LPBYTE)&glInstrumentWindowHeight, &dwDWORDSize);
        registry_query_value(key, "MDICommentsHeight", NULL, &dwREG_DWORD,  (LPBYTE)&glCommentsWindowHeight, &dwDWORDSize);
        registry_query_value(key, "MDIGraphHeight", NULL, &dwREG_DWORD,  (LPBYTE)&glGraphWindowHeight, &dwDWORDSize); //rewbs.graph
        registry_query_value(key, "MDITreeRatio", NULL, &dwREG_DWORD, (LPBYTE)&glTreeSplitRatio, &dwDWORDSize);
        // Colors
        for (int ncol=0; ncol<MAX_MODCOLORS; ncol++)
        {
            CHAR s[64];
            wsprintf(s, "Color%02d", ncol);
            registry_query_value(key, s, NULL, &dwREG_DWORD, (LPBYTE)&rgbCustomColors[ncol], &dwCRSIZE);
        }
        RegCloseKey(key);
    }

    if (RegOpenKeyEx(HKEY_CURRENT_USER,    m_csRegKey, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        registry_query_value(key, "Quality", NULL, &dwREG_DWORD, (LPBYTE)&deprecated_m_dwQuality, &dwDWORDSize);
        registry_query_value(key, "SrcMode", NULL, &dwREG_DWORD, (LPBYTE)&m_nSrcMode, &dwDWORDSize);
        registry_query_value(key, "PreAmp", NULL, &dwREG_DWORD, (LPBYTE)&m_nPreAmp, &dwDWORDSize);

        CHAR sPath[_MAX_PATH] = "";
        uint32_t dwSZSIZE = sizeof(sPath);
        registry_query_value(key, "Songs_Directory", NULL, &dwREG_SZ, (LPBYTE)sPath, &dwSZSIZE);
        SetDefaultDirectory(sPath, DIR_MODS);
        dwSZSIZE = sizeof(sPath);
        registry_query_value(key, "Samples_Directory", NULL, &dwREG_SZ, (LPBYTE)sPath, &dwSZSIZE);
        SetDefaultDirectory(sPath, DIR_SAMPLES);
        dwSZSIZE = sizeof(sPath);
        registry_query_value(key, "Instruments_Directory", NULL, &dwREG_SZ, (LPBYTE)sPath, &dwSZSIZE);
        SetDefaultDirectory(sPath, DIR_INSTRUMENTS);
        dwSZSIZE = sizeof(sPath);
        registry_query_value(key, "Plugins_Directory", NULL, &dwREG_SZ, (LPBYTE)sPath, &dwSZSIZE);
        SetDefaultDirectory(sPath, DIR_PLUGINS);
        dwSZSIZE = sizeof(m_szKbdFile);
        registry_query_value(key, "Key_Config_File", NULL, &dwREG_SZ, (LPBYTE)m_szKbdFile, &dwSZSIZE);

        registry_query_value(key, "StereoSeparation", NULL, &dwREG_DWORD, (LPBYTE)&module_renderer::m_nStereoSeparation, &dwDWORDSize);
        registry_query_value(key, "MixChannels", NULL, &dwREG_DWORD, (LPBYTE)&module_renderer::m_nMaxMixChannels, &dwDWORDSize);
        registry_query_value(key, "WaveDevice", NULL, &dwREG_DWORD, (LPBYTE)&m_nWaveDevice, &dwDWORDSize);
        registry_query_value(key, "MidiSetup", NULL, &dwREG_DWORD, (LPBYTE)&m_dwMidiSetup, &dwDWORDSize);
        registry_query_value(key, "MidiDevice", NULL, &dwREG_DWORD, (LPBYTE)&m_nMidiDevice, &dwDWORDSize);
        registry_query_value(key, "PatternSetup", NULL, &dwREG_DWORD, (LPBYTE)&m_dwPatternSetup, &dwDWORDSize);
            m_dwPatternSetup &= ~(0x800|0x200000|0x400000);    // various deprecated old options
            m_dwPatternSetup |= PATTERN_NOTEFADE; // Set flag to maintain old behaviour (was changed in 1.17.02.50).
            m_dwPatternSetup |= PATTERN_RESETCHANNELS; // Set flag to reset channels on loop was changed in 1.17.03.01).
        registry_query_value(key, "RowSpacing", NULL, &dwREG_DWORD, (LPBYTE)&m_nRowSpacing, &dwDWORDSize);
        registry_query_value(key, "RowSpacing2", NULL, &dwREG_DWORD, (LPBYTE)&m_nRowSpacing2, &dwDWORDSize);
        registry_query_value(key, "LoopSong", NULL, &dwREG_DWORD, (LPBYTE)&gbLoopSong, &dwDWORDSize);

        //rewbs.resamplerConf
        dwDWORDSize = sizeof(gbWFIRType);
        registry_query_value(key, "XMMSModplugResamplerWFIRType", NULL, &dwREG_DWORD, (LPBYTE)&gbWFIRType, &dwDWORDSize);
        dwDWORDSize = sizeof(gdWFIRCutoff);
        registry_query_value(key, "ResamplerWFIRCutoff", NULL, &dwREG_DWORD, (LPBYTE)&gdWFIRCutoff, &dwDWORDSize);
        dwDWORDSize = sizeof(glVolumeRampInSamples);
        registry_query_value(key, "VolumeRampInSamples", NULL, &dwREG_DWORD, (LPBYTE)&glVolumeRampInSamples, &dwDWORDSize);
        dwDWORDSize = sizeof(glVolumeRampOutSamples);
        registry_query_value(key, "VolumeRampOutSamples", NULL, &dwREG_DWORD, (LPBYTE)&glVolumeRampOutSamples, &dwDWORDSize);

        //end rewbs.resamplerConf
        //rewbs.autochord
        dwDWORDSize = sizeof(gnAutoChordWaitTime);
        registry_query_value(key, "AutoChordWaitTime", NULL, &dwREG_DWORD, (LPBYTE)&gnAutoChordWaitTime, &dwDWORDSize);
        //end rewbs.autochord

        dwDWORDSize = sizeof(gnPlugWindowX);
        registry_query_value(key, "PlugSelectWindowX", NULL, &dwREG_DWORD, (LPBYTE)&gnPlugWindowX, &dwDWORDSize);
        dwDWORDSize = sizeof(gnPlugWindowY);
        registry_query_value(key, "PlugSelectWindowY", NULL, &dwREG_DWORD, (LPBYTE)&gnPlugWindowY, &dwDWORDSize);
        dwDWORDSize = sizeof(gnPlugWindowWidth);
        registry_query_value(key, "PlugSelectWindowWidth", NULL, &dwREG_DWORD, (LPBYTE)&gnPlugWindowWidth, &dwDWORDSize);
        dwDWORDSize = sizeof(gnPlugWindowHeight);
        registry_query_value(key, "PlugSelectWindowHeight", NULL, &dwREG_DWORD, (LPBYTE)&gnPlugWindowHeight, &dwDWORDSize);
        dwDWORDSize = sizeof(gnPlugWindowLast);
        registry_query_value(key, "PlugSelectWindowLast", NULL, &dwREG_DWORD, (LPBYTE)&gnPlugWindowLast, &dwDWORDSize);

        RegCloseKey(key);
    } else
    {
        return false;
    }

    if (RegOpenKeyEx(HKEY_CURRENT_USER,    m_csRegSettings, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        // Version
        dwDWORDSize = sizeof(uint32_t);
        uint32_t dwPreviousVersion;
        registry_query_value(key, "Version", NULL, &dwREG_DWORD, (LPBYTE)&dwPreviousVersion, &dwDWORDSize);
        gcsPreviousVersion = MptVersion::ToStr(dwPreviousVersion);
        RegCloseKey(key);
    }

    gnPatternSpacing = theApp.GetProfileInt("Pattern Editor", "Spacing", 0);
    gbPatternVUMeters = theApp.GetProfileInt("Pattern Editor", "VU-Meters", 0);
    gbPatternPluginNames = theApp.GetProfileInt("Pattern Editor", "Plugin-Names", 1);

    return true;
}


VOID CMainFrame::Initialize() {
    ui_root = std::unique_ptr<modplug::gui::qt5::mfc_root>(
        new modplug::gui::qt5::mfc_root(global_config, *this));
    config_dialog = new modplug::gui::qt5::config_dialog(
        global_config, ui_root.get());
    ui_root->mainwindow.show();

    //Adding version number to the frame title
    CString title = GetTitle();
    title += CString(" ") + MptVersion::str;
    #ifdef DEBUG
        title += CString(" DEBUG");
    #endif
    #ifdef NO_VST
        title += " NO_VST";
    #endif
    SetTitle(title);
    OnUpdateFrameTitle(false);

    // Load Chords
    theApp.LoadChords(Chords);
    // Default directory location
    for(UINT i = 0; i < NUM_DIRS; i++)
    {
        _tcscpy(m_szWorkingDirectory[i], m_szDefaultDirectory[i]);
    }
    if (m_szDefaultDirectory[DIR_MODS][0]) SetCurrentDirectory(m_szDefaultDirectory[DIR_MODS]);

    // Create Audio Thread
    stream->start();
    //XXXih: portaudio
    m_hNotifyWakeUp = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hNotifyThread = CreateThread(NULL, 0, NotifyThread, NULL, 0, &m_dwNotifyThreadId);
    // Setup timer
    OnUpdateUser(NULL);
    m_nTimer = SetTimer(1, MPTTIMER_PERIOD, NULL);

//rewbs: reduce to normal priority during debug for easier hang debugging
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

    // Initialize Audio Mixer
    //XXXih: UpdateAudioParameters(TRUE);
}


CMainFrame::~CMainFrame()
//-----------------------
{
    DeleteCriticalSection(&m_csAudio);

    CChannelManagerDlg::DestroySharedInstance();
    module_renderer::DeleteStaticdata();
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
//-----------------------------------------------------
{
    if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1) return -1;
    // Load resources
    m_hIcon = theApp.LoadIcon(IDR_MAINFRAME);
    m_ImageList.Create(IDB_IMAGELIST, 16, 0, RGB(0,128,128));
    m_hGUIFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    m_hFixedFont = ::CreateFont(12,5, 0,0, 300,
                            FALSE, FALSE, FALSE,
                            OEM_CHARSET, OUT_RASTER_PRECIS,
                            CLIP_DEFAULT_PRECIS, DRAFT_QUALITY,
                            FIXED_PITCH | FF_MODERN, "");
    m_hLargeFixedFont = ::CreateFont(18,8, 0,0, 400,
                            FALSE, FALSE, FALSE,
                            OEM_CHARSET, OUT_RASTER_PRECIS,
                            CLIP_DEFAULT_PRECIS, DRAFT_QUALITY,
                            FIXED_PITCH | FF_MODERN, "");
    if (m_hGUIFont == NULL) m_hGUIFont = (HFONT)GetStockObject(ANSI_VAR_FONT);
    brushBlack = (HBRUSH)::GetStockObject(BLACK_BRUSH);
    brushWhite = (HBRUSH)::GetStockObject(WHITE_BRUSH);
    brushText = ::CreateSolidBrush(GetSysColor(COLOR_BTNTEXT));
    brushGray = ::CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    penLightGray = ::CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNHIGHLIGHT));
    penDarkGray = ::CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW));
    penScratch = ::CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW));
    penGray00 = ::CreatePen(PS_SOLID,0, RGB(0x00, 0x00, 0x00));
    penGray33 = ::CreatePen(PS_SOLID,0, RGB(0x33, 0x33, 0x33));
    penGray40 = ::CreatePen(PS_SOLID,0, RGB(0x40, 0x40, 0x40));
    penGray55 = ::CreatePen(PS_SOLID,0, RGB(0x55, 0x55, 0x55));
    penGray80 = ::CreatePen(PS_SOLID,0, RGB(0x80, 0x80, 0x80));
    penGray99 = ::CreatePen(PS_SOLID,0, RGB(0x99, 0x99, 0x99));
    penGraycc = ::CreatePen(PS_SOLID,0, RGB(0xcc, 0xcc, 0xcc));
    penGrayff = ::CreatePen(PS_SOLID,0, RGB(0xff, 0xff, 0xff));

    penHalfDarkGray = ::CreatePen(PS_DOT, 0, GetSysColor(COLOR_BTNSHADOW));
    penBlack = (HPEN)::GetStockObject(BLACK_PEN);
    penWhite = (HPEN)::GetStockObject(WHITE_PEN);



    // Cursors
    curDragging = theApp.LoadCursor(IDC_DRAGGING);
    curArrow = theApp.LoadStandardCursor(IDC_ARROW);
    curNoDrop = theApp.LoadCursor(IDC_NODROP);
    curNoDrop2 = theApp.LoadCursor(IDC_NODRAG);
    curVSplit = theApp.LoadCursor(AFX_IDC_HSPLITBAR);
    // bitmaps
    bmpPatterns = LoadDib(MAKEINTRESOURCE(IDB_PATTERNS));
    bmpNotes = LoadDib(MAKEINTRESOURCE(IDB_PATTERNVIEW));
    bmpVUMeters = LoadDib(MAKEINTRESOURCE(IDB_VUMETERS));
    bmpVisNode = LoadDib(MAKEINTRESOURCE(IDB_VISNODE));
    bmpVisPcNode = LoadDib(MAKEINTRESOURCE(IDB_VISPCNODE));
    UpdateColors();
    // Toolbars
    EnableDocking(CBRS_ALIGN_ANY);
    if (!m_wndToolBar.Create(this)) return -1;
    if (!m_wndStatusBar.Create(this)) return -1;
    m_wndStatusBar.SetIndicators(indicators, CountOf(indicators));
    m_wndToolBar.Init(this);

    if(m_dwPatternSetup & PATTERN_MIDIRECORD) OnMidiRecord();

    return 0;
}


BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
//------------------------------------------------
{
    return CMDIFrameWnd::PreCreateWindow(cs);
}


BOOL CMainFrame::DestroyWindow()
//------------------------------
{
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    // Uninstall Keyboard Hook
    if (ghKbdHook)
    {
        UnhookWindowsHookEx(ghKbdHook);
        ghKbdHook = NULL;
    }
    // Kill Timer
    if (m_nTimer)
    {
        KillTimer(m_nTimer);
        m_nTimer = 0;
    }
    if (shMidiIn) midiCloseDevice();
    if (m_hNotifyThread != NULL)
    {
        if(TerminateThread(m_hNotifyThread, 0)) m_hNotifyThread = NULL;
    }
    // Delete bitmaps
    if (bmpPatterns)
    {
        delete bmpPatterns;
        bmpPatterns = NULL;
    }
    if (bmpNotes)
    {
        delete bmpNotes;
        bmpNotes = NULL;
    }
    if (bmpVUMeters)
    {
        delete bmpVUMeters;
        bmpVUMeters = NULL;
    }
    if (bmpVisNode)
    {
        delete bmpVisNode;
        bmpVisNode = NULL;
    }
    if (bmpVisPcNode)
    {
        delete bmpVisPcNode;
        bmpVisPcNode = NULL;
    }

    // Kill GDI Objects
    DeleteGDIObject(brushGray);
    DeleteGDIObject(penLightGray);
    DeleteGDIObject(penDarkGray);
    DeleteGDIObject(penSample);
    DeleteGDIObject(penEnvelope);
    DeleteGDIObject(penEnvelopeHighlight);
    DeleteGDIObject(m_hFixedFont);
    DeleteGDIObject(m_hLargeFixedFont);
    DeleteGDIObject(penScratch);
    DeleteGDIObject(penGray00);
    DeleteGDIObject(penGray33);
    DeleteGDIObject(penGray40);
    DeleteGDIObject(penGray55);
    DeleteGDIObject(penGray80);
    DeleteGDIObject(penGray99);
    DeleteGDIObject(penGraycc);
    DeleteGDIObject(penGrayff);

    for (UINT i=0; i<NUM_VUMETER_PENS*2; i++)
    {
        if (gpenVuMeter[i])
        {
            DeleteObject(gpenVuMeter[i]);
            gpenVuMeter[i] = NULL;
        }
    }

    return CMDIFrameWnd::DestroyWindow();
}


void CMainFrame::OnClose()
//------------------------
{
    CChildFrame *pMDIActive = (CChildFrame *)MDIGetActive();

    BeginWaitCursor();
    if (m_dwStatus & MODSTATUS_PLAYING) PauseMod();
    if (pMDIActive) pMDIActive->SavePosition(TRUE);
    // Save Settings
    SaveIniSettings();

    EndWaitCursor();
    CMDIFrameWnd::OnClose();
}

void CMainFrame::SaveIniSettings()
//--------------------------------
{
    CString iniFile = theApp.GetConfigFileName();

    CString version = MptVersion::str;
    WritePrivateProfileString("Version", "Version", version, iniFile);
    WritePrivateProfileString("Version", "InstallGUID", gcsInstallGUID, iniFile);

    WINDOWPLACEMENT wpl;
    wpl.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(&wpl);
    WritePrivateProfileStruct("Display", "WindowPlacement", &wpl, sizeof(WINDOWPLACEMENT), iniFile);

    WritePrivateProfileLong("Display", "MDIMaximize", gbMdiMaximize, iniFile);
    WritePrivateProfileLong("Display", "MDITreeWidth", glTreeWindowWidth, iniFile);
    WritePrivateProfileLong("Display", "MDITreeRatio", glTreeSplitRatio, iniFile);
    WritePrivateProfileLong("Display", "MDIGeneralHeight", glGeneralWindowHeight, iniFile);
    WritePrivateProfileLong("Display", "MDIPatternHeight", glPatternWindowHeight, iniFile);
    WritePrivateProfileLong("Display", "MDISampleHeight", glSampleWindowHeight, iniFile);
    WritePrivateProfileLong("Display", "MDIInstrumentHeight", glInstrumentWindowHeight, iniFile);
    WritePrivateProfileLong("Display", "MDICommentsHeight", glCommentsWindowHeight, iniFile);
    WritePrivateProfileLong("Display", "MDIGraphHeight", glGraphWindowHeight, iniFile); //rewbs.graph
    WritePrivateProfileLong("Display", "PlugSelectWindowX", gnPlugWindowX, iniFile);
    WritePrivateProfileLong("Display", "PlugSelectWindowY", gnPlugWindowY, iniFile);
    WritePrivateProfileLong("Display", "PlugSelectWindowWidth", gnPlugWindowWidth, iniFile);
    WritePrivateProfileLong("Display", "PlugSelectWindowHeight", gnPlugWindowHeight, iniFile);
    WritePrivateProfileLong("Display", "PlugSelectWindowLast", gnPlugWindowLast, iniFile);
    WritePrivateProfileDWord("Display", "MsgBoxVisibilityFlags", gnMsgBoxVisiblityFlags, iniFile);

    CHAR s[16];
    for (int ncol = 0; ncol < MAX_MODCOLORS; ncol++)
    {
        wsprintf(s, "Color%02d", ncol);
        WritePrivateProfileDWord("Display", s, rgbCustomColors[ncol], iniFile);
    }

    WritePrivateProfileLong("Sound Settings", "WaveDevice", m_nWaveDevice, iniFile);
    WritePrivateProfileDWord("Sound Settings", "Quality", deprecated_m_dwQuality, iniFile);
    WritePrivateProfileDWord("Sound Settings", "SrcMode", m_nSrcMode, iniFile);
    WritePrivateProfileDWord("Sound Settings", "PreAmp", m_nPreAmp, iniFile);
    WritePrivateProfileLong("Sound Settings", "StereoSeparation", module_renderer::m_nStereoSeparation, iniFile);
    WritePrivateProfileLong("Sound Settings", "MixChannels", module_renderer::m_nMaxMixChannels, iniFile);
    WritePrivateProfileDWord("Sound Settings", "XMMSModplugResamplerWFIRType", gbWFIRType, iniFile);
    WritePrivateProfileLong("Sound Settings", "ResamplerWFIRCutoff", static_cast<int>(gdWFIRCutoff*100+0.5), iniFile);
    WritePrivateProfileLong("Sound Settings", "VolumeRampInSamples", glVolumeRampInSamples, iniFile);
    WritePrivateProfileLong("Sound Settings", "VolumeRampOutSamples", glVolumeRampOutSamples, iniFile);

    WritePrivateProfileDWord("MIDI Settings", "MidiSetup", m_dwMidiSetup, iniFile);
    WritePrivateProfileDWord("MIDI Settings", "MidiDevice", m_nMidiDevice, iniFile);

    WritePrivateProfileDWord("Pattern Editor", "PatternSetup", m_dwPatternSetup, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "RowSpacing", m_nRowSpacing, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "RowSpacing2", m_nRowSpacing2, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "LoopSong", gbLoopSong, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "Spacing", gnPatternSpacing, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "VU-Meters", gbPatternVUMeters, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "Plugin-Names", gbPatternPluginNames, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "Record", gbPatternRecord, iniFile);
    WritePrivateProfileDWord("Pattern Editor", "AutoChordWaitTime", gnAutoChordWaitTime, iniFile);

    // Write default paths
    const bool bConvertPaths = theApp.IsPortableMode();
    TCHAR szPath[_MAX_PATH] = "";
    for(size_t i = 0; i < NUM_DIRS; i++)
    {
        if(m_szDirectoryToSettingsName[i][0] == 0)
            continue;

        _tcscpy(szPath, GetDefaultDirectory(static_cast<Directory>(i)));
        if(bConvertPaths)
        {
            AbsolutePathToRelative(szPath);
        }
        WritePrivateProfileString("Paths", m_szDirectoryToSettingsName[i], szPath, iniFile);

    }
    // Obsolete, since we always write to Keybindings.mkb now. Older versions of OpenMPT 1.18+ will look for this file if this entry is missing, so this is kind of backwards compatible.
    WritePrivateProfileString("Paths", "Key_Config_File", NULL, iniFile);

    theApp.SaveChords(Chords);

    RemoveControlBar(&m_wndStatusBar); //Remove statusbar so that its state won't get saved.
    SaveBarState("Toolbars");
    AddControlBar(&m_wndStatusBar); //Restore statusbar to mainframe.
}

bool CMainFrame::WritePrivateProfileLong(const CString section, const CString key, const long value, const CString iniFile)
{
    CHAR valueBuffer[INIBUFFERSIZE];
    wsprintf(valueBuffer, "%li", value);
    return (WritePrivateProfileString(section, key, valueBuffer, iniFile) != 0);
}


long CMainFrame::GetPrivateProfileLong(const CString section, const CString key, const long defaultValue, const CString iniFile)
{
    CHAR defaultValueBuffer[INIBUFFERSIZE];
    wsprintf(defaultValueBuffer, "%li", defaultValue);

    CHAR valueBuffer[INIBUFFERSIZE];
    GetPrivateProfileString(section, key, defaultValueBuffer, valueBuffer, INIBUFFERSIZE, iniFile);

    return atol(valueBuffer);
}


bool CMainFrame::WritePrivateProfileDWord(const CString section, const CString key, const uint32_t value, const CString iniFile)
{
    CHAR valueBuffer[INIBUFFERSIZE];
    wsprintf(valueBuffer, "%lu", value);
    return (WritePrivateProfileString(section, key, valueBuffer, iniFile) != 0);
}

uint32_t CMainFrame::GetPrivateProfileDWord(const CString section, const CString key, const uint32_t defaultValue, const CString iniFile)
{
    CHAR defaultValueBuffer[INIBUFFERSIZE];
    wsprintf(defaultValueBuffer, "%lu", defaultValue);

    CHAR valueBuffer[INIBUFFERSIZE];
    GetPrivateProfileString(section, key, defaultValueBuffer, valueBuffer, INIBUFFERSIZE, iniFile);
    return static_cast<uint32_t>(atol(valueBuffer));
}

bool CMainFrame::WritePrivateProfileCString(const CString section, const CString key, const CString value, const CString iniFile)
{
    return (WritePrivateProfileString(section, key, value, iniFile) != 0);
}

CString CMainFrame::GetPrivateProfileCString(const CString section, const CString key, const CString defaultValue, const CString iniFile)
{
    CHAR defaultValueBuffer[INIBUFFERSIZE];
    strcpy(defaultValueBuffer, defaultValue);
    CHAR valueBuffer[INIBUFFERSIZE];
    GetPrivateProfileString(section, key, defaultValueBuffer, valueBuffer, INIBUFFERSIZE, iniFile);
    return valueBuffer;
}



BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
//---------------------------------------------
{
    if ((pMsg->message == WM_RBUTTONDOWN) || (pMsg->message == WM_NCRBUTTONDOWN))
    {
        CWnd* pWnd = CWnd::FromHandlePermanent(pMsg->hwnd);
        CControlBar* pBar = NULL;
        HWND hwnd = (pWnd) ? pWnd->m_hWnd : NULL;

        if ((hwnd) && (pMsg->message == WM_RBUTTONDOWN)) pBar = DYNAMIC_DOWNCAST(CControlBar, pWnd);
        if ((pBar != NULL) || ((pMsg->message == WM_NCRBUTTONDOWN) && (pMsg->wParam == HTMENU)))
        {
            CMenu Menu;
            CPoint pt;

            GetCursorPos(&pt);
            if (Menu.LoadMenu(IDR_TOOLBARS))
            {
                CMenu* pSubMenu = Menu.GetSubMenu(0);
                if (pSubMenu!=NULL) pSubMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,this);
            }
        }
    }
    return CMDIFrameWnd::PreTranslateMessage(pMsg);
}


void CMainFrame::OnUpdateFrameTitle(BOOL bAddToTitle)
//---------------------------------------------------
{
    if ((GetStyle() & FWS_ADDTOTITLE) == 0)    return;     // leave it alone!

    CMDIChildWnd* pActiveChild = NULL;
    CDocument* pDocument = GetActiveDocument();
    if (bAddToTitle &&
      (pActiveChild = MDIGetActive()) != NULL &&
      (pActiveChild->GetStyle() & WS_MAXIMIZE) == 0 &&
      (pDocument != NULL ||
       (pDocument = pActiveChild->GetActiveDocument()) != NULL))
    {
        TCHAR szText[256+_MAX_PATH];
        lstrcpy(szText, pDocument->GetTitle());
        if (pDocument->IsModified()) lstrcat(szText, "*");
        UpdateFrameTitleForDocument(szText);
    } else
    {
        LPCTSTR lpstrTitle = NULL;
        CString strTitle;

        if (pActiveChild != NULL)
        {
            strTitle = pActiveChild->GetTitle();
            if (!strTitle.IsEmpty())
                lpstrTitle = strTitle;
        }
        UpdateFrameTitleForDocument(lpstrTitle);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame Sound Library

//static BOOL gbStopSent = FALSE;
BOOL gbStopSent = FALSE;

void Terminate_NotifyThread()
//----------------------------------------------
{
    //TODO: Why does this not get called.
    AfxMessageBox("Notify thread terminated unexpectedly. Attempting to shut down audio device");
    CMainFrame* pMainFrame = CMainFrame::GetMainFrame();
    exit(-1);
}

// Notify thread
DWORD WINAPI CMainFrame::NotifyThread(LPVOID)
//-------------------------------------------
{
    CMainFrame *pMainFrm;

    set_terminate(Terminate_NotifyThread);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    for (;;)
    {
        WaitForSingleObject(CMainFrame::m_hNotifyWakeUp, 1000);
        pMainFrm = (CMainFrame *)theApp.m_pMainWnd;
        if ((pMainFrm) && (pMainFrm->IsPlaying()))
        {
            MPTNOTIFICATION *pnotify = NULL;
            uint32_t dwLatency = 0;

            for (UINT i=0; i<MAX_UPDATE_HISTORY; i++)
            {
                MPTNOTIFICATION *p = &pMainFrm->NotifyBuffer[i];
                if ((p->dwType & MPTNOTIFY_PENDING)
                 && (!(pMainFrm->m_dwStatus & MODSTATUS_BUSY)))
                {
                    if (p->dwLatency >= dwLatency)
                    {
                        if (pnotify) pnotify->dwType = 0;
                        pnotify = p;
                    } else
                    {
                        p->dwType = 0;
                    }
                }
            }
            if (pnotify)
            {
                pMainFrm->m_dwStatus |= MODSTATUS_BUSY;
                //XXXih: JUICY FRUITS
                pMainFrm->PostMessage(WM_MOD_UPDATEPOSITION, 0, (LPARAM)pnotify);
            }
        }
    }
    // Commented the two lines below as those caused "warning C4702: unreachable code"
    //ExitThread(0);
    //return 0;
}


void CMainFrame::CalcStereoVuMeters(int *pMix, unsigned long nSamples, unsigned long nChannels)
//---------------------------------------------------------------------------------------------
{
    const int * const p = pMix;
    int lmax = gnLVuMeter, rmax = gnRVuMeter;
    if (nChannels > 1)
    {
        for (UINT i=0; i<nSamples; i+=nChannels)
        {
            int vl = p[i];
            int vr = p[i+1];
            if (vl < 0) vl = -vl;
            if (vr < 0) vr = -vr;
            if (vl > lmax) lmax = vl;
            if (vr > rmax) rmax = vr;
        }
    } else
    {
        for (UINT i=0; i<nSamples; i++)
        {
            int vl = p[i];
            if (vl < 0) vl = -vl;
            if (vl > lmax) lmax = vl;
        }
        rmax = lmax;
    }
    gnLVuMeter = lmax;
    gnRVuMeter = rmax;
}


BOOL CMainFrame::DoNotification(uint32_t dwSamplesRead, uint32_t dwLatency)
//-------------------------------------------------------------------
{
    auto &stream_settings = global_config.audio_settings();
    m_dwElapsedTime += (dwSamplesRead * 1000) / stream_settings.sample_rate;
    gsdwTotalSamples += dwSamplesRead;
    if (!renderer) return FALSE;
    if (m_nMixChn < renderer->m_nMixStat) m_nMixChn++;
    if (m_nMixChn > renderer->m_nMixStat) m_nMixChn--;
    if (!(m_dwNotifyType & MPTNOTIFY_TYPEMASK)) return FALSE;
    // Notify Client
    for (UINT i=0; i<MAX_UPDATE_HISTORY; i++)
    {
        MPTNOTIFICATION *p = &NotifyBuffer[i];
        if ((p->dwType & MPTNOTIFY_TYPEMASK)
         && (!(p->dwType & MPTNOTIFY_PENDING))
         && (gsdwTotalSamples >= p->dwLatency))
        {
            p->dwType |= MPTNOTIFY_PENDING;
            SetEvent(m_hNotifyWakeUp);
        }
    }
    if (!renderer) return FALSE;
    // Add an entry to the notification history
    for (UINT j=0; j<MAX_UPDATE_HISTORY; j++)
    {
        MPTNOTIFICATION *p = &NotifyBuffer[j];
        if (!(p->dwType & MPTNOTIFY_TYPEMASK))
        {
            p->dwType = m_dwNotifyType;
            uint32_t d = dwLatency / slSampleSize;
            p->dwLatency = gsdwTotalSamples + d;
            p->nOrder = renderer->m_nCurrentPattern;
            p->nRow = renderer->m_nRow;
            p->nPattern = renderer->m_nPattern;
            if (m_dwNotifyType & MPTNOTIFY_SAMPLE)
            {
                UINT nSmp = m_dwNotifyType & 0xFFFF;
                for (UINT k=0; k<MAX_VIRTUAL_CHANNELS; k++)
                {
                    modplug::tracker::modchannel_t *pChn = &renderer->Chn[k];
                    p->dwPos[k] = 0;
                    if ((nSmp) && (nSmp <= renderer->m_nSamples) && (pChn->length)
                     && (pChn->sample_data) && (pChn->sample_data == renderer->Samples[nSmp].sample_data.generic)
                     && ((!bitset_is_set(pChn->flags, vflag_ty::NoteFade)) || (pChn->nFadeOutVol)))
                    {
                        p->dwPos[k] = MPTNOTIFY_POSVALID | (uint32_t)(pChn->sample_position);
                    }
                }
            } else
            if (m_dwNotifyType & (MPTNOTIFY_VOLENV|MPTNOTIFY_PANENV|MPTNOTIFY_PITCHENV))
            {
                UINT nIns = m_dwNotifyType & 0xFFFF;
                for (UINT k=0; k<MAX_VIRTUAL_CHANNELS; k++)
                {
                    modplug::tracker::modchannel_t *pChn = &renderer->Chn[k];
                    p->dwPos[k] = 0;
                    if ((nIns) && (nIns <= renderer->m_nInstruments) && (pChn->length)
                     && (pChn->instrument) && (pChn->instrument == renderer->Instruments[nIns])
                     && ((!bitset_is_set(pChn->flags, vflag_ty::NoteFade)) || (pChn->nFadeOutVol)))
                    {
                        if (m_dwNotifyType & MPTNOTIFY_PITCHENV)
                        {
                            if (bitset_is_set(pChn->flags, vflag_ty::PitchEnvOn)) p->dwPos[k] = MPTNOTIFY_POSVALID | (uint32_t)(pChn->pitch_envelope.position);
                        } else
                        if (m_dwNotifyType & MPTNOTIFY_PANENV)
                        {
                            if (bitset_is_set(pChn->flags, vflag_ty::PanEnvOn)) p->dwPos[k] = MPTNOTIFY_POSVALID | (uint32_t)(pChn->panning_envelope.position);
                        } else
                        {
                            if (bitset_is_set(pChn->flags, vflag_ty::VolEnvOn)) p->dwPos[k] = MPTNOTIFY_POSVALID | (uint32_t)(pChn->volume_envelope.position);
                        }
                    }
                }
            } else
            if (m_dwNotifyType & (MPTNOTIFY_VUMETERS))
            {
                for (UINT k=0; k<MAX_VIRTUAL_CHANNELS; k++)
                {
                    modplug::tracker::modchannel_t *pChn = &renderer->Chn[k];
                    UINT vul = pChn->nLeftVU;
                    UINT vur = pChn->nRightVU;
                    p->dwPos[k] = (vul << 8) | (vur);
                }
            } else
            if (m_dwNotifyType & MPTNOTIFY_MASTERVU)
            {
                uint32_t lVu = (gnLVuMeter >> 11);
                uint32_t rVu = (gnRVuMeter >> 11);
                if (lVu > 0x10000) lVu = 0x10000;
                if (rVu > 0x10000) rVu = 0x10000;
                p->dwPos[0] = lVu;
                p->dwPos[1] = rVu;
                uint32_t dwVuDecay = _muldiv(dwSamplesRead, 120000, stream_settings.sample_rate) + 1;
                if (lVu >= dwVuDecay) gnLVuMeter = (lVu - dwVuDecay) << 11; else gnLVuMeter = 0;
                if (rVu >= dwVuDecay) gnRVuMeter = (rVu - dwVuDecay) << 11; else gnRVuMeter = 0;
            }
            return TRUE;
        }
    }

    return FALSE;
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
    CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
    CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame static helpers


void CMainFrame::UpdateColors()
//-----------------------------
{
    if (bmpPatterns)
    {
        bmpPatterns->bmiColors[7] = rgb2quad(GetSysColor(COLOR_BTNFACE));
    }
    if (bmpVUMeters)
    {
        bmpVUMeters->bmiColors[7] = rgb2quad(GetSysColor(COLOR_BTNFACE));
        bmpVUMeters->bmiColors[8] = rgb2quad(GetSysColor(COLOR_BTNSHADOW));
        bmpVUMeters->bmiColors[15] = rgb2quad(GetSysColor(COLOR_BTNHIGHLIGHT));
        bmpVUMeters->bmiColors[10] = rgb2quad(rgbCustomColors[MODCOLOR_VUMETER_LO]);
        bmpVUMeters->bmiColors[11] = rgb2quad(rgbCustomColors[MODCOLOR_VUMETER_MED]);
        bmpVUMeters->bmiColors[9] = rgb2quad(rgbCustomColors[MODCOLOR_VUMETER_HI]);
        bmpVUMeters->bmiColors[2] = rgb2quad((rgbCustomColors[MODCOLOR_VUMETER_LO] >> 1) & 0x7F7F7F);
        bmpVUMeters->bmiColors[3] = rgb2quad((rgbCustomColors[MODCOLOR_VUMETER_MED] >> 1) & 0x7F7F7F);
        bmpVUMeters->bmiColors[1] = rgb2quad((rgbCustomColors[MODCOLOR_VUMETER_HI] >> 1) & 0x7F7F7F);
    }
    if (penSample) DeleteObject(penSample);
    penSample = ::CreatePen(PS_SOLID, 0, rgbCustomColors[MODCOLOR_SAMPLE]);
    if (penEnvelope) DeleteObject(penEnvelope);
    penEnvelope = ::CreatePen(PS_SOLID, 0, rgbCustomColors[MODCOLOR_ENVELOPES]);
    if (penEnvelopeHighlight) DeleteObject(penEnvelopeHighlight);
    penEnvelopeHighlight = ::CreatePen(PS_SOLID, 0, RGB(0xFF, 0xFF, 0x00));

    for (UINT i=0; i<NUM_VUMETER_PENS*2; i++)
    {
        int r0,g0,b0, r1,g1,b1;
        int r, g, b;
        int y;

        if (gpenVuMeter[i])
        {
            DeleteObject(gpenVuMeter[i]);
            gpenVuMeter[i] = NULL;
        }
        y = (i >= NUM_VUMETER_PENS) ? (i-NUM_VUMETER_PENS) : i;
        if (y < (NUM_VUMETER_PENS/2))
        {
            r0 = GetRValue(rgbCustomColors[MODCOLOR_VUMETER_LO]);
            g0 = GetGValue(rgbCustomColors[MODCOLOR_VUMETER_LO]);
            b0 = GetBValue(rgbCustomColors[MODCOLOR_VUMETER_LO]);
            r1 = GetRValue(rgbCustomColors[MODCOLOR_VUMETER_MED]);
            g1 = GetGValue(rgbCustomColors[MODCOLOR_VUMETER_MED]);
            b1 = GetBValue(rgbCustomColors[MODCOLOR_VUMETER_MED]);
        } else
        {
            y -= (NUM_VUMETER_PENS/2);
            r0 = GetRValue(rgbCustomColors[MODCOLOR_VUMETER_MED]);
            g0 = GetGValue(rgbCustomColors[MODCOLOR_VUMETER_MED]);
            b0 = GetBValue(rgbCustomColors[MODCOLOR_VUMETER_MED]);
            r1 = GetRValue(rgbCustomColors[MODCOLOR_VUMETER_HI]);
            g1 = GetGValue(rgbCustomColors[MODCOLOR_VUMETER_HI]);
            b1 = GetBValue(rgbCustomColors[MODCOLOR_VUMETER_HI]);
        }
        r = r0 + ((r1 - r0) * y) / (NUM_VUMETER_PENS/2);
        g = g0 + ((g1 - g0) * y) / (NUM_VUMETER_PENS/2);
        b = b0 + ((b1 - b0) * y) / (NUM_VUMETER_PENS/2);
        if (i >= NUM_VUMETER_PENS)
        {
            r = (r*2)/5;
            g = (g*2)/5;
            b = (b*2)/5;
        }
        gpenVuMeter[i] = CreatePen(PS_SOLID, 0, RGB(r, g, b));
    }
    // Sequence window
    {
        COLORREF crBkgnd = GetSysColor(COLOR_WINDOW);
        if (brushHighLight) DeleteObject(brushHighLight);
        brushHighLight = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
        if (brushHighLightRed) DeleteObject(brushHighLightRed);
        brushHighLightRed = CreateSolidBrush(RGB(0xFF,0x00,0x00));
        if (brushYellow) DeleteObject(brushYellow);
        brushYellow = CreateSolidBrush(RGB(0xFF,0xFF,0x00));

        if (brushWindow) DeleteObject(brushWindow);
        brushWindow = CreateSolidBrush(crBkgnd);
        if (penSeparator) DeleteObject(penSeparator);
        penSeparator = CreatePen(PS_SOLID, 0, RGB(GetRValue(crBkgnd)/2, GetGValue(crBkgnd)/2, GetBValue(crBkgnd)/2));
    }
}


VOID CMainFrame::GetKeyName(LONG lParam, LPSTR pszName, UINT cbSize)
//------------------------------------------------------------------
{
    pszName[0] = (char)cbSize;
    if ((cbSize > 0) && (lParam))
    {
        GetKeyNameText(lParam, pszName, cbSize);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame operations

UINT CMainFrame::GetBaseOctave()
//------------------------------
{
    return m_wndToolBar.GetBaseOctave();
}


void CMainFrame::SetPreAmp(UINT n)
//--------------------------------
{
    m_nPreAmp = n;
    if (renderer) renderer->SetMasterVolume(m_nPreAmp, true);
}


BOOL CMainFrame::ResetNotificationBuffer(HWND hwnd)
//-------------------------------------------------
{
    if ((!hwnd) || (m_hFollowSong == hwnd))
    {
        memset(NotifyBuffer, 0, sizeof(NotifyBuffer));
        gsdwTotalSamples = 0;
        return TRUE;
    }
    return FALSE;
}


BOOL CMainFrame::PlayMod(CModDoc *pModDoc, HWND hPat, uint32_t dwNotifyType)
//-----------------------------------------------------------------------
{
    DEBUG_FUNC("");
    if (!pModDoc) {
        DEBUG_FUNC("pModDoc == 0");
        return FALSE;
    }
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    if ((!pSndFile) || (!pSndFile->GetType())) {
        DEBUG_FUNC("pSndFile == 0 or pSndFile->GetType == 0");
        return FALSE;
    }
    const bool bPaused = pSndFile->IsPaused();
    const bool bPatLoop = (pSndFile->m_dwSongFlags & SONG_PATTERNLOOP) ? true : false;
    pSndFile->ResetChannels();
    // Select correct bidi loop mode when playing a module.
    pSndFile->SetupITBidiMode();

    if ((renderer) || (m_dwStatus & MODSTATUS_PLAYING)) PauseMod();
    renderer = pSndFile;
    m_pModPlaying = pModDoc;
    m_hFollowSong = hPat;
    m_dwNotifyType = dwNotifyType;
    if (m_dwNotifyType & MPTNOTIFY_MASTERVU) {
        gnLVuMeter = gnRVuMeter = 0;
        module_renderer::sound_mix_callback = CalcStereoVuMeters;
    } else {
        module_renderer::sound_mix_callback = NULL;
    }

    //XXXih: dumb
    ui_root->update_audio_settings();
    gbStopSent = FALSE;
    pSndFile->SetRepeatCount((gbLoopSong) ? -1 : 0);
    //XXXih: dumb ^

    m_nMixChn = m_nAvgMixChn = 0;
    gsdwTotalSamples = 0;
    if (!bPatLoop)
    {
        if (bPaused)
        {
            pSndFile->m_dwSongFlags |= SONG_PAUSED;
        } else
        {
            pModDoc->SetPause(FALSE);
            //rewbs.fix3185: removed this check so play position stays on last pattern if song ends and loop is off.
            //Otherwise play from cursor screws up.
            //if (pSndFile->GetCurrentPos() + 2 >= pSndFile->GetMaxPosition()) pSndFile->SetCurrentPos(0);

            // Tentative fix for http://bugs.openmpt.org/view.php?id=11 - Moved following line out of any condition checks
            //pSndFile->SetRepeatCount((gbLoopSong) ? -1 : 0);
        }
    }

    renderer->SetMasterVolume(m_nPreAmp, true);
    renderer->InitPlayer(TRUE);
    MemsetZero(NotifyBuffer);
    m_dwStatus |= MODSTATUS_PLAYING;
    m_wndToolBar.SetCurrentSong(renderer);
    DEBUG_FUNC("end");
    //if (gpSoundDevice) gpSoundDevice->Start();
    return TRUE;
}


BOOL CMainFrame::PauseMod(CModDoc *pModDoc)
//-----------------------------------------
{
    if ((pModDoc) && (pModDoc != m_pModPlaying)) return FALSE;
    if (m_dwStatus & MODSTATUS_PLAYING)
    {
        m_dwStatus &= ~MODSTATUS_PLAYING;

        BEGIN_CRITICAL();
        renderer->SuspendPlugins();     //rewbs.VSTCompliance
        END_CRITICAL();

        m_nMixChn = m_nAvgMixChn = 0;
        Sleep(1);
        if (m_hFollowSong)
        {
            MPTNOTIFICATION mn;
            MemsetZero(mn);
            mn.dwType = MPTNOTIFY_STOP;
            ::SendMessage(m_hFollowSong, WM_MOD_UPDATEPOSITION, 0, (LPARAM)&mn);
        }
    }
    if (m_pModPlaying)
    {
        m_pModPlaying->SetPause(TRUE);
    }
    if (renderer)
    {
        //m_pSndFile->LoopPattern(-1);
        //Commented above line - why loop should be disabled when pausing?

        renderer->m_dwSongFlags &= ~SONG_PAUSED;
        if (renderer == &m_WaveFile)
        {
            renderer = NULL;
            m_WaveFile.Destroy();
        } else
        {
            for (UINT i=renderer->m_nChannels; i<MAX_VIRTUAL_CHANNELS; i++)
            {
                if (!(renderer->Chn[i].parent_channel))
                {
                    renderer->Chn[i].sample_position = renderer->Chn[i].fractional_sample_position = renderer->Chn[i].length = 0;
                }
            }
        }

    }
    m_pModPlaying = NULL;
    renderer = NULL;
    m_hFollowSong = NULL;
    m_wndToolBar.SetCurrentSong(NULL);
    return TRUE;
}


BOOL CMainFrame::StopMod(CModDoc *pModDoc)
//----------------------------------------
{
    if ((pModDoc) && (pModDoc != m_pModPlaying)) return FALSE;
    CModDoc *pPlay = m_pModPlaying;
    module_renderer *pSndFile = renderer;
    PauseMod();
    if (pPlay) pPlay->SetPause(FALSE);
    if (pSndFile) pSndFile->SetCurrentPos(0);
    m_dwElapsedTime = 0;
    return TRUE;
}


BOOL CMainFrame::PlaySoundFile(module_renderer *pSndFile)
//--------------------------------------------------
{
    DEBUG_FUNC("");
    ui_root->update_audio_settings();

    if (renderer) PauseMod(NULL);
    if ((!pSndFile) || (!pSndFile->GetType())) {
        DEBUG_FUNC("pSndFile == 0 or pSndFile->GetType == 0");
        return FALSE;
    }
    renderer = pSndFile;

    gsdwTotalSamples = 0;
    renderer->SetMasterVolume(m_nPreAmp, true);
    renderer->InitPlayer(TRUE);
    m_dwStatus |= MODSTATUS_PLAYING;
    DEBUG_FUNC("CMainFrame::PlaySoundFile: end");
    return TRUE;
}


BOOL CMainFrame::PlaySoundFile(LPCSTR lpszFileName, UINT nNote)
//-------------------------------------------------------------
{
    return FALSE;
}


BOOL CMainFrame::PlaySoundFile(module_renderer *pSong, UINT nInstrument, UINT nSample, UINT nNote)
//-------------------------------------------------------------------------------------------
{
    return FALSE;
}


BOOL CMainFrame::StopSoundFile(module_renderer *pSndFile)
//--------------------------------------------------
{
    if ((pSndFile) && (pSndFile != renderer)) return FALSE;
    PauseMod(NULL);
    return TRUE;
}


BOOL CMainFrame::SetFollowSong(CModDoc *pDoc, HWND hwnd, BOOL bFollowSong, uint32_t dwType)
//--------------------------------------------------------------------------------------
{
    if ((!pDoc) || (pDoc != m_pModPlaying)) return FALSE;
    if (bFollowSong)
    {
        m_hFollowSong = hwnd;
    } else
    {
        if (hwnd == m_hFollowSong) m_hFollowSong = NULL;
    }
    if (dwType) m_dwNotifyType = dwType;
    if (m_dwNotifyType & MPTNOTIFY_MASTERVU)
    {
        module_renderer::sound_mix_callback = CalcStereoVuMeters;
    } else
    {
        gnLVuMeter = gnRVuMeter = 0;
        module_renderer::sound_mix_callback = NULL;
    }
    return TRUE;
}



BOOL CMainFrame::SetupPlayer(uint32_t q, uint32_t srcmode, BOOL bForceUpdate)
//---------------------------------------------------------------------
{
    if ((q != deprecated_m_dwQuality) || (srcmode != m_nSrcMode) || (bForceUpdate))
    {
        m_nSrcMode = srcmode;
        deprecated_m_dwQuality = q;
        BEGIN_CRITICAL();
        module_renderer::deprecated_SetResamplingMode(m_nSrcMode);
        END_CRITICAL();
        PostMessage(WM_MOD_INVALIDATEPATTERNS, HINT_MPTSETUP);
    }
    return TRUE;
}


BOOL CMainFrame::SetupDirectories(LPCTSTR szModDir, LPCTSTR szSampleDir, LPCTSTR szInstrDir, LPCTSTR szVstDir, LPCTSTR szPresetDir)
//---------------------------------------------------------------------------------------------------------------------------------
{
    // will also set working directory
    SetDefaultDirectory(szModDir, DIR_MODS);
    SetDefaultDirectory(szSampleDir, DIR_SAMPLES);
    SetDefaultDirectory(szInstrDir, DIR_INSTRUMENTS);
    SetDefaultDirectory(szVstDir, DIR_PLUGINS);
    SetDefaultDirectory(szPresetDir, DIR_PLUGINPRESETS);
    return TRUE;
}

BOOL CMainFrame::SetupMiscOptions()
//---------------------------------
{
    if (CMainFrame::m_dwPatternSetup & PATTERN_MUTECHNMODE)
        module_renderer::deprecated_global_sound_setup_bitmask |= SNDMIX_MUTECHNMODE;
    else
        module_renderer::deprecated_global_sound_setup_bitmask &= ~SNDMIX_MUTECHNMODE;

    m_wndToolBar.EnableFlatButtons(m_dwPatternSetup & PATTERN_FLATBUTTONS);

    UpdateTree(NULL, HINT_MPTOPTIONS);
    UpdateAllViews(HINT_MPTOPTIONS, NULL);
    return true;
}


BOOL CMainFrame::SetupMidi(uint32_t d, LONG n)
//-----------------------------------------
{
    m_dwMidiSetup = d;
    m_nMidiDevice = n;
    return TRUE;
}


void CMainFrame::UpdateAllViews(uint32_t dwHint, CObject *pHint)
//-----------------------------------------------------------
{
    CDocTemplate *pDocTmpl = theApp.GetModDocTemplate();
    if (pDocTmpl)
    {
        POSITION pos = pDocTmpl->GetFirstDocPosition();
        CDocument *pDoc;
        while ((pos != NULL) && ((pDoc = pDocTmpl->GetNextDoc(pos)) != NULL))
        {
            pDoc->UpdateAllViews(NULL, dwHint, pHint);
        }
    }
}


VOID CMainFrame::SetUserText(LPCSTR lpszText)
//-------------------------------------------
{
    if (lpszText[0] | m_szUserText[0])
    {
        strcpy(m_szUserText, lpszText);
        OnUpdateUser(NULL);
    }
}


VOID CMainFrame::SetInfoText(LPCSTR lpszText)
//-------------------------------------------
{
    if (lpszText[0] | m_szInfoText[0])
    {
        strcpy(m_szInfoText, lpszText);
        OnUpdateInfo(NULL);
    }
}

//rewbs.xinfo
VOID CMainFrame::SetXInfoText(LPCSTR lpszText)
//-------------------------------------------
{
    if (lpszText[0] | m_szXInfoText[0])
    {
        strcpy(m_szXInfoText, lpszText);
        OnUpdateInfo(NULL);
    }
}
//end rewbs.xinfo

VOID CMainFrame::SetHelpText(LPCSTR lpszText)
//-------------------------------------------
{
    m_wndStatusBar.SetPaneText(0, lpszText);
}


VOID CMainFrame::OnDocumentCreated(CModDoc *pModDoc)
//--------------------------------------------------
{
}


VOID CMainFrame::OnDocumentClosed(CModDoc *pModDoc)
//-------------------------------------------------
{
    if (pModDoc == m_pModPlaying) PauseMod();

    // Make sure that OnTimer() won't try to set the closed document modified anymore.
    if (pModDoc == m_pJustModifiedDoc) m_pJustModifiedDoc = 0;

}


VOID CMainFrame::UpdateTree(CModDoc *pModDoc, uint32_t lHint, CObject *pHint)
//------------------------------------------------------------------------
{
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


void CMainFrame::OnViewOptions()
//------------------------------
{
    if (m_bOptionsLocked)    //rewbs.customKeys
        return;

    CPropertySheet dlg("OpenMPT Setup", this, m_nLastOptionsPage);
    COptionsGeneral general;
    COptionsColors colors;
    CMidiSetupDlg mididlg(m_dwMidiSetup, m_nMidiDevice);
    dlg.AddPage(&general);
    dlg.AddPage(&colors);
    dlg.AddPage(&mididlg);
    m_bOptionsLocked=true;    //rewbs.customKeys
    dlg.DoModal();
    m_bOptionsLocked=false;    //rewbs.customKeys
}

void CMainFrame::display_config_editor() {
    DEBUG_FUNC("thread id = %x", GetCurrentThreadId());
    config_dialog->show();
}


void CMainFrame::OnSongProperties()
//---------------------------------
{
    CModDoc* pModDoc = GetActiveDoc();
    if(pModDoc) pModDoc->SongProperties();
}


// -> CODE#0002
// -> DESC="list box to choose VST plugin presets (programs)"
void CMainFrame::OnPluginManager()
//--------------------------------
{
#ifndef NO_VST
    int nPlugslot=-1;
    CModDoc* pModDoc = GetActiveDoc();

    if (pModDoc)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        //Find empty plugin slot
        for (int nPlug=0; nPlug<MAX_MIXPLUGINS; nPlug++)
        {
            PSNDMIXPLUGIN pCandidatePlugin = &pSndFile->m_MixPlugins[nPlug];
            if (pCandidatePlugin->pMixPlugin == NULL)
            {
                nPlugslot=nPlug;
                break;
            }
        }
    }
    CSelectPluginDlg dlg(GetActiveDoc(), nPlugslot, this);
    dlg.DoModal();
    if (pModDoc)
    {
        //Refresh views
        pModDoc->UpdateAllViews(NULL, HINT_MIXPLUGINS|HINT_MODTYPE);
        //Refresh Controls
        CChildFrame *pActiveChild = (CChildFrame *)MDIGetActive();
        pActiveChild->ForceRefresh();
    }
#endif // NO_VST
}
// -! NEW_FEATURE#0002


// -> CODE#0015
// -> DESC="channels management dlg"
void CMainFrame::OnChannelManager()
//---------------------------------
{
    if(GetActiveDoc() && CChannelManagerDlg::sharedInstance())
    {
        if(CChannelManagerDlg::sharedInstance()->IsDisplayed())
            CChannelManagerDlg::sharedInstance()->Hide();
        else
        {
            CChannelManagerDlg::sharedInstance()->SetDocument(NULL);
            CChannelManagerDlg::sharedInstance()->Show();
        }
    }
}
// -! NEW_FEATURE#0015

void CMainFrame::OnTimer(UINT)
//----------------------------
{
    // Display Time in status bar
    uint32_t dwTime = m_dwElapsedTime / 1000;
    if (dwTime != m_dwTimeSec)
    {
        m_dwTimeSec = dwTime;
        m_nAvgMixChn = m_nMixChn;
        OnUpdateTime(NULL);
    }
    // Idle Time Check
    uint32_t curTime = timeGetTime();
    m_wndToolBar.SetCurrentSong(renderer);

    // Ensure the modified flag gets set in the WinMain thread, even if modification
    // originated from Audio Thread (access to CWnd is not thread safe).
    // Flaw: if 2 docs are modified in between Timer ticks (very rare), one mod will be lost.
    /*CModDoc* pModDoc = GetActiveDoc();
    if (pModDoc && pModDoc->m_bModifiedChanged) {
        pModDoc->SetModifiedFlag(pModDoc->m_bDocModified);
        pModDoc->m_bModifiedChanged=false;
    }*/
    if (m_pJustModifiedDoc)
    {
        m_pJustModifiedDoc->SetModified(true);
        m_pJustModifiedDoc = NULL;
    }

}


CModDoc *CMainFrame::GetActiveDoc()
//---------------------------------
{
    CMDIChildWnd *pMDIActive = MDIGetActive();
    if (pMDIActive)
    {
        CView *pView = pMDIActive->GetActiveView();
        if (pView) return (CModDoc *)pView->GetDocument();
    }
    return NULL;
}

//rewbs.customKeys
CView *CMainFrame::GetActiveView()
//---------------------------------
{
    CMDIChildWnd *pMDIActive = MDIGetActive();
    if (pMDIActive)
    {
        return pMDIActive->GetActiveView();
    }

    return NULL;
}
//end rewbs.customKeys

void CMainFrame::SwitchToActiveView()
//-----------------------------------
{
    CMDIChildWnd *pMDIActive = MDIGetActive();
    if (pMDIActive)
    {
        CView *pView = pMDIActive->GetActiveView();
        if (pView)
        {
            pMDIActive->SetActiveView(pView);
            pView->SetFocus();
        }
    }
}


void CMainFrame::OnUpdateTime(CCmdUI *)
//-------------------------------------
{
    CHAR s[64];
    wsprintf(s, "%d:%02d:%02d",
        m_dwTimeSec / 3600, (m_dwTimeSec / 60) % 60, (m_dwTimeSec % 60));
    if ((renderer) && (!(renderer->IsPaused())))
    {
        if (renderer != &m_WaveFile)
        {
            UINT nPat = renderer->m_nPattern;
            if(nPat < renderer->Patterns.Size())
            {
                if (nPat < 10) strcat(s, " ");
                if (nPat < 100) strcat(s, " ");
                wsprintf(s+strlen(s), " [%d]", nPat);
            }
        }
        wsprintf(s+strlen(s), " %dch", m_nAvgMixChn);
    }
    m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_TIME), s, TRUE);
}


void CMainFrame::OnUpdateUser(CCmdUI *)
//-------------------------------------
{
    m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_USER), m_szUserText, TRUE);
}


void CMainFrame::OnUpdateInfo(CCmdUI *)
//-------------------------------------
{
    m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_INFO), m_szInfoText, TRUE);
}


void CMainFrame::OnUpdateCPU(CCmdUI *)
//-------------------------------------
{
/*    CString s;
    double totalCPUPercent = m_dTotalCPU*100;
    UINT intPart = static_cast<int>(totalCPUPercent);
    UINT decPart = static_cast<int>(totalCPUPercent-intPart)*100;
    s.Format("%d.%d%%", intPart, decPart);
    m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_CPU), s, TRUE);*/
}

//rewbs.xinfo
void CMainFrame::OnUpdateXInfo(CCmdUI *)
//-------------------------------------
{
    m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_XINFO), m_szXInfoText, TRUE);
}
//end rewbs.xinfo

void CMainFrame::OnPlayerPause()
//------------------------------
{
    if (m_pModPlaying)
    {
        m_pModPlaying->OnPlayerPause();
    } else
    {
        PauseMod();
    }
}


LRESULT CMainFrame::OnInvalidatePatterns(WPARAM wParam, LPARAM)
//-------------------------------------------------------------
{
    UpdateAllViews(wParam, NULL);
    return TRUE;
}


LRESULT CMainFrame::OnUpdatePosition(WPARAM, LPARAM lParam)
//---------------------------------------------------------
{
    MPTNOTIFICATION *pnotify = (MPTNOTIFICATION *)lParam;
    if (pnotify)
    {
        //Log("OnUpdatePosition: row=%d time=%lu\n", pnotify->nRow, pnotify->dwLatency);
        if ((m_pModPlaying) && (renderer))
        {
            if (m_hFollowSong) {
                ::SendMessage(m_hFollowSong, WM_MOD_UPDATEPOSITION, 0, lParam);
            }
            //XXXih: JUICY FRUITS
            //XXXih: not correct
            auto child = MDIGetActive();
            if (child) {
                ::SendMessage(child->m_hWnd, WM_MOD_UPDATEPOSITION, 0, lParam);
            }
        }
        pnotify->dwType = 0;
    }
    m_dwStatus &= ~MODSTATUS_BUSY;
    return 0;
}


void CMainFrame::OnPanic()
//------------------------
{
    // "Panic button." At the moment, it just resets all VSTi and sample notes.
    if(m_pModPlaying)
        m_pModPlaying->OnPanic();
}


void CMainFrame::OnPrevOctave()
//-----------------------------
{
    UINT n = GetBaseOctave();
    if (n > MIN_BASEOCTAVE) m_wndToolBar.SetBaseOctave(n-1);
// -> CODE#0009
// -> DESC="instrument editor note play & octave change"
//    SwitchToActiveView();
// -! BEHAVIOUR_CHANGE#0009
}


void CMainFrame::OnNextOctave()
//-----------------------------
{
    UINT n = GetBaseOctave();
    if (n < MAX_BASEOCTAVE) m_wndToolBar.SetBaseOctave(n+1);
// -> CODE#0009
// -> DESC="instrument editor note play & octave change"
//    SwitchToActiveView();
// -! BEHAVIOUR_CHANGE#0009
}


void CMainFrame::OnOctaveChanged()
//--------------------------------
{
    SwitchToActiveView();
}


void CMainFrame::OnRButtonDown(UINT, CPoint pt)
//---------------------------------------------
{
    CMenu Menu;

    ClientToScreen(&pt);
    if (Menu.LoadMenu(IDR_TOOLBARS))
    {
        CMenu* pSubMenu = Menu.GetSubMenu(0);
        if (pSubMenu!=NULL) pSubMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,this);
    }
}


LRESULT CMainFrame::OnSpecialKey(WPARAM /*vKey*/, LPARAM)
//---------------------------------------------------
{
/*    CMDIChildWnd *pMDIActive = MDIGetActive();
    CView *pView = NULL;
    if (pMDIActive) pView = pMDIActive->GetActiveView();
    switch(vKey)
    {
    case VK_RMENU:
        if (pView) pView->PostMessage(WM_COMMAND, ID_PATTERN_RESTART, 0);
        break;
    case VK_RCONTROL:
        if (pView) pView->PostMessage(WM_COMMAND, ID_PLAYER_PLAY, 0);
        break;
    }
*/
    return 0;
}


void CMainFrame::OnInitMenu(CMenu* pMenu)
//---------------------------------------
{
    CMDIFrameWnd::OnInitMenu(pMenu);

}

//end rewbs.VSTTimeInfo
long CMainFrame::GetSampleRate()
//------------------------------
{
    return module_renderer::GetSampleRate();
}

long CMainFrame::GetTotalSampleCount()
//------------------------------------
{
    if (GetModPlaying())
        return GetModPlaying()->GetSoundFile()->m_lTotalSampleCount;
    return 0;
}

double CMainFrame::GetApproxBPM()
//-------------------------------
{
    module_renderer *pSndFile = NULL;

    pSndFile = GetActiveDoc()->GetSoundFile();
    if (pSndFile) {
        return pSndFile->GetCurrentBPM();
    }
    return 0;
}

BOOL CMainFrame::InitRenderer(module_renderer* pSndFile)
//-------------------------------------------------
{
    BEGIN_CRITICAL();
    pSndFile->m_bIsRendering=true;
    pSndFile->SuspendPlugins();
    pSndFile->ResumePlugins();
    END_CRITICAL();
    m_dwStatus |= MODSTATUS_RENDERING;
    m_pModPlaying = GetActiveDoc();
    return true;
}

BOOL CMainFrame::StopRenderer(module_renderer* pSndFile)
//-------------------------------------------------
{
    m_dwStatus &= ~MODSTATUS_RENDERING;
    m_pModPlaying = NULL;
    BEGIN_CRITICAL();
    pSndFile->SuspendPlugins();
    pSndFile->m_bIsRendering=false;
    END_CRITICAL();
    return true;
}
//end rewbs.VSTTimeInfo


//rewbs.fix3116
void CMainFrame::OnKillFocus(CWnd* pNewWnd)
//-----------------------------------------
{
    CMDIFrameWnd::OnKillFocus(pNewWnd);
}
//end rewbs.fix3116

void CMainFrame::OnShowWindow(BOOL bShow, UINT /*nStatus*/)
//---------------------------------------------------------
{
    static bool firstShow = true;
    if (bShow && !IsWindowVisible() && firstShow)
    {
        firstShow = false;
        WINDOWPLACEMENT wpl;
        if (GetPrivateProfileStruct("Display", "WindowPlacement", &wpl, sizeof(WINDOWPLACEMENT), theApp.GetConfigFileName()))
        {
            SetWindowPlacement(&wpl);
        }
    }
}


void CMainFrame::OnViewMIDIMapping()
//----------------------------------
{
    CModDoc* pModDoc = GetActiveDoc();
    module_renderer* pSndFile = (pModDoc) ? pModDoc->GetSoundFile() : nullptr;
    if(!pSndFile) return;

    const HWND oldMIDIRecondWnd = GetMidiRecordWnd();
    CMIDIMappingDialog dlg(this, *pSndFile);
    dlg.DoModal();
    SetMidiRecordWnd(oldMIDIRecondWnd);
}


void CMainFrame::OnViewEditHistory()
//----------------------------------
{
    CModDoc* pModDoc = GetActiveDoc();
    if(pModDoc != nullptr)
    {
        pModDoc->OnViewEditHistory();
    }
}


/////////////////////////////////////////////
//Misc helper functions
/////////////////////////////////////////////

void AddPluginNamesToCombobox(CComboBox& CBox, SNDMIXPLUGIN* plugarray, const bool librarynames)
//----------------------------------------------------------------------------------------------
{
#ifndef NO_VST
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        PSNDMIXPLUGIN p = &plugarray[iPlug];
        CString str;
        str.Preallocate(80);
        str.Format("FX%d: ", iPlug+1);
        const int size0 = str.GetLength();
        str += (librarynames) ? p->GetLibraryName() : p->GetName();
        if(str.GetLength() <= size0) str += "undefined";

        CBox.SetItemData(CBox.AddString(str), iPlug + 1);
    }
#endif // NO_VST
}

void AddPluginParameternamesToCombobox(CComboBox& CBox, SNDMIXPLUGIN& plug)
//-------------------------------------------------------------------------
{
    if(plug.pMixPlugin)
        AddPluginParameternamesToCombobox(CBox, *(CVstPlugin *)plug.pMixPlugin);
}

void AddPluginParameternamesToCombobox(CComboBox& CBox, CVstPlugin& plug)
//-----------------------------------------------------------------------
{
    char s[72], sname[64];
    const PlugParamIndex nParams = plug.GetNumParameters();
    for (PlugParamIndex i = 0; i < nParams; i++)
    {
        plug.GetParamName(i, sname, sizeof(sname));
        wsprintf(s, "%02d: %s", i, sname);
        CBox.SetItemData(CBox.AddString(s), i);
    }
}


// retrieve / set default directory from given string and store it our setup variables
void CMainFrame::SetDirectory(const LPCTSTR szFilenameFrom, Directory dir, TCHAR (&directories)[NUM_DIRS][_MAX_PATH], bool bStripFilename)
//----------------------------------------------------------------------------------------------------------------------------------------
{
    TCHAR szPath[_MAX_PATH], szDir[_MAX_DIR];

    if(bStripFilename)
    {
        _tsplitpath(szFilenameFrom, szPath, szDir, 0, 0);
        _tcscat(szPath, szDir);
    }
    else
    {
        _tcscpy(szPath, szFilenameFrom);
    }

    TCHAR szOldDir[sizeof(directories[dir])]; // for comparison
    _tcscpy(szOldDir, directories[dir]);

    _tcscpy(directories[dir], szPath);

    // When updating default directory, also update the working directory.
    if(szPath[0] && directories == m_szDefaultDirectory)
    {
        if(_tcscmp(szOldDir, szPath) != 0) // update only if default directory has changed
            SetWorkingDirectory(szPath, dir);
    }
}

void CMainFrame::SetDefaultDirectory(const LPCTSTR szFilenameFrom, Directory dir, bool bStripFilename)
//----------------------------------------------------------------------------------------------------
{
    SetDirectory(szFilenameFrom, dir, m_szDefaultDirectory, bStripFilename);
}


void CMainFrame::SetWorkingDirectory(const LPCTSTR szFilenameFrom, Directory dir, bool bStripFilename)
//----------------------------------------------------------------------------------------------------
{
    SetDirectory(szFilenameFrom, dir, m_szWorkingDirectory, bStripFilename);
}


LPCTSTR CMainFrame::GetDefaultDirectory(Directory dir)
//----------------------------------------------------
{
    return m_szDefaultDirectory[dir];
}


LPCTSTR CMainFrame::GetWorkingDirectory(Directory dir)
//----------------------------------------------------
{
    return m_szWorkingDirectory[dir];
}


// Convert an absolute path to a path that's relative to OpenMPT's directory.
// Paths are relative to the executable path.
// nLength specifies the maximum number of character that can be written into szPath,
// including the trailing null char.
template <size_t nLength>
void CMainFrame::AbsolutePathToRelative(TCHAR (&szPath)[nLength])
//---------------------------------------------------------------
{
    static_assert(nLength >= 3, "can't apply AbsolutePathToRelative to a buffer of length < 3");

    if(_tcslen(szPath) == 0)
        return;

    const size_t nStrLength = nLength - 1;    // "usable" length, i.e. not including the null char.
    TCHAR szExePath[nLength], szTempPath[nLength];
    _tcsncpy(szExePath, theApp.GetAppDirPath(), nStrLength);
    SetNullTerminator(szExePath);

    // Path is OpenMPT's directory or a sub directory ("C:\OpenMPT\Somepath" => ".\Somepath")
    if(!_tcsncicmp(szExePath, szPath, _tcslen(szExePath)))
    {
        _tcscpy(szTempPath, _T(".\\"));    // ".\"
        _tcsncat(szTempPath, &szPath[_tcslen(szExePath)], nStrLength - 2);    // "Somepath"
        _tcscpy(szPath, szTempPath);
    } else
    // Path is on the same drive as OpenMPT ("C:\Somepath" => "\Somepath")
    if(!_tcsncicmp(szExePath, szPath, 1))
    {
        _tcsncpy(szTempPath, &szPath[2], nStrLength);    // "\Somepath"
        _tcscpy(szPath, szTempPath);
    }
    SetNullTerminator(szPath);
}


// Convert a relative path to an absolute path.
// Paths are relative to the executable path.
// nLength specifies the maximum number of character that can be written into szPath,
// including the trailing null char.
template <size_t nLength>
void CMainFrame::RelativePathToAbsolute(TCHAR (&szPath)[nLength])
//---------------------------------------------------------------
{
    static_assert(nLength >= 3, "can't apply dsf.sdfsdklfsdfds");

    if(_tcslen(szPath) == 0)
        return;

    const size_t nStrLength = nLength - 1;    // "usable" length, i.e. not including the null char.
    TCHAR szExePath[nLength], szTempPath[nLength] = _T("");
    _tcsncpy(szExePath, theApp.GetAppDirPath(), nStrLength);
    SetNullTerminator(szExePath);

    // Path is on the same drive as OpenMPT ("\Somepath\" => "C:\Somepath\")
    if(!_tcsncicmp(szPath, _T("\\"), 1))
    {
        _tcsncat(szTempPath, szExePath, 2);    // "C:"
        _tcsncat(szTempPath, szPath, nStrLength - 2);    // "\Somepath\"
        _tcscpy(szPath, szTempPath);
    } else
    // Path is OpenMPT's directory or a sub directory (".\Somepath\" => "C:\OpenMPT\Somepath\")
    if(!_tcsncicmp(szPath, _T(".\\"), 2))
    {
        _tcsncpy(szTempPath, szExePath, nStrLength);    // "C:\OpenMPT\"
        if(_tcslen(szTempPath) < nStrLength)
        {
            _tcsncat(szTempPath, &szPath[2], nStrLength - _tcslen(szTempPath));    //        "Somepath"
        }
        _tcscpy(szPath, szTempPath);
    }
    SetNullTerminator(szPath);
}

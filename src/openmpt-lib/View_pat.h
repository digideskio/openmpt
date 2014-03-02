#ifndef _VIEW_PATTERNS_H_
#define _VIEW_PATTERNS_H_

class CModDoc;
class CEditCommand;
class CEffectVis;    //rewbs.fxvis
class CPatternGotoDialog;
class CPatternRandomizer;

// Drag & Drop info
#define DRAGITEM_VALUEMASK           0x00FFFF
#define DRAGITEM_MASK                0xFF0000
#define DRAGITEM_CHNHEADER           0x010000
#define DRAGITEM_PATTERNHEADER       0x020000
#define DRAGITEM_PLUGNAME            0x040000 //rewbs.patPlugName

#define PATSTATUS_MOUSEDRAGSEL       0x01     // Creating a selection using the mouse
#define PATSTATUS_KEYDRAGSEL         0x02     // Creating a selection using shortcuts
#define PATSTATUS_FOCUS              0x04     // Is the pattern editor focussed
#define PATSTATUS_FOLLOWSONG         0x08     // Does the cursor follow playback
#define PATSTATUS_RECORD             0x10     // Recording enabled
#define PATSTATUS_DRAGHSCROLL        0x20     // Some weird dragging stuff (?)
#define PATSTATUS_DRAGVSCROLL        0x40     // Some weird dragging stuff (?)
#define PATSTATUS_VUMETERS           0x80     // Display channel VU meters?
#define PATSTATUS_CHORDPLAYING       0x100    // Is a chord playing? (pretty much unused)
#define PATSTATUS_DRAGNDROPEDIT      0x200    // Drag & Drop editing (?)
#define PATSTATUS_DRAGNDROPPING      0x400    // Dragging a selection around
#define PATSTATUS_MIDISPACINGPENDING 0x800    // Unused (?)
#define PATSTATUS_CTRLDRAGSEL        0x1000   // Creating a selection using Ctrl
#define PATSTATUS_PLUGNAMESINHEADERS 0x2000   // Show plugin names in channel headers //rewbs.patPlugName
#define PATSTATUS_SELECTROW          0x4000   // Selecting a whole pattern row by clicking the row numbers

// Row Spacing
#define MAX_SPACING                  64       // MAX_PATTERN_ROWS


// Selection - bit masks
// ---------------------
// A selection point (m_dwStartSel and the like) is stored in a 32-Bit variable. The structure is as follows (MSB to LSB):
// | 16 bits - row | 13 bits - channel | 3 bits - channel component |
// As you can see, the highest 16 bits contain a row index.
// It is followed by a channel index, which is 13 bits wide.
// The lowest 3 bits are used for addressing the components of a channel. They are *not* used as a bit set, but treated as one of the following integer numbers:

enum PatternColumns {
    NOTE_COLUMN = 0,
    INST_COLUMN,
    VOL_COLUMN,
    EFFECT_COLUMN,
    PARAM_COLUMN,
    LAST_COLUMN = PARAM_COLUMN
};

static_assert(MAX_BASECHANNELS <= 0x1FFF, "Check: Channel index in pattern editor is only 13 bits wide!");


//Struct for controlling selection clearing. This is used to define which data fields
//should be cleared.
struct RowMask {
    bool note;
    bool instrument;
    bool volume;
    bool command;
    bool parameter;
};
const RowMask DefaultRowMask = {
    true,
    true,
    true,
    true,
    true
};

struct ModCommandPos {
    modplug::tracker::patternindex_t nPat;
    modplug::tracker::rowindex_t nRow;
    modplug::tracker::chnindex_t nChn;
};

// Find/Replace data
struct FindReplaceStruct {
    modplug::tracker::modevent_t cmdFind;    // Find notes/instruments/effects
    modplug::tracker::modevent_t cmdReplace;
    uint32_t dwFindFlags;                    // PATSEARCH_XXX flags (=> PatternEditorDialogs.h)
    uint32_t dwReplaceFlags;
    modplug::tracker::chnindex_t nFindMinChn;                // Find in these channels (if PATSEARCH_CHANNEL is set)
    modplug::tracker::chnindex_t nFindMaxChn;
    signed char cInstrRelChange;             // relative instrument change (quick'n'dirty fix, this should be implemented in a less cryptic way)
    uint32_t dwBeginSel;                     // Find in this selection (if PATSEARCH_PATSELECTION is set)
    uint32_t dwEndSel;
};


//////////////////////////////////////////////////////////////////
// Pattern editing class


class QWinWidget;
//=======================================
class CViewPattern: public CModScrollView
//=======================================
{
protected:
    CFastBitmap m_Dib;
    CEditCommand *m_pEditWnd;
    CPatternGotoDialog *m_pGotoWnd;
    SIZE m_szHeader, m_szCell;
    UINT m_nPattern, m_nRow, m_nMidRow, m_nPlayPat, m_nPlayRow, m_nSpacing, m_nAccelChar, m_nLastPlayedRow, m_nLastPlayedOrder;

    int m_nXScroll, m_nYScroll;
    uint32_t m_nMenuParam, m_nDetailLevel;

    uint32_t m_nDragItem;    // Currently dragged item
    uint32_t m_nDropItem;    // Currently hovered item during dragondrop
    bool m_bDragging, m_bInItemRect, m_bShiftDragging;
    RECT m_rcDragItem, m_rcDropItem;

    bool m_bContinueSearch, m_bWholePatternFitsOnScreen;
    uint32_t m_dwStatus, m_dwCursor;
    uint32_t m_dwBeginSel, m_dwEndSel;            // Upper-left / Lower-right corners of selection
    uint32_t m_dwStartSel, m_dwDragPos;    // Point where selection was started
    uint16_t ChnVUMeters[MAX_BASECHANNELS];
    uint16_t OldVUMeters[MAX_BASECHANNELS];
    CListBox *ChnEffectList[MAX_BASECHANNELS]; //rewbs.patPlugName
    UINT m_nFoundInstrument;
    UINT m_nMenuOnChan;
    uint32_t m_dwLastNoteEntryTime; //rewbs.customkeys
    UINT m_nLastPlayedChannel; //rewbs.customkeys
    bool m_bLastNoteEntryBlocked;

    static modplug::tracker::modevent_t m_cmdOld;                            // Quick cursor copy/paste data
    static FindReplaceStruct m_findReplace;    // Find/replace data

// -> CODE#0012
// -> DESC="midi keyboard split"
    uint8_t activeNoteChannel[modplug::tracker::NoteCount + 1];
    uint8_t splitActiveNoteChannel[modplug::tracker::NoteCount + 1];
    int oldrow,oldchn,oldsplitchn;
// -! NEW_FEATURE#0012

// -> CODE#0018
// -> DESC="route PC keyboard inputs to midi in mechanism"
    int ignorekey;
// -! BEHAVIOUR_CHANGE#0018
public:
    CEffectVis    *m_pEffectVis;    //rewbs.fxVis


    CViewPattern();
    DECLARE_SERIAL(CViewPattern)

public:

    BOOL UpdateSizes();
    void UpdateScrollSize();
    void UpdateScrollPos();
    void UpdateIndicator();
    void UpdateXInfoText(); //rewbs.xinfo
    void UpdateColors();

    int GetXScrollPos() const { return m_nXScroll; }
    int GetYScrollPos() const { return m_nYScroll; }
    int GetColumnWidth() const { return m_szCell.cx; }
    int GetColumnHeight() const { return m_szCell.cy; }
    UINT GetCurrentPattern() const { return m_nPattern; }
    UINT GetCurrentRow() const { return m_nRow; }
    UINT GetCurrentColumn() const { return m_dwCursor; }
    UINT GetCurrentChannel() const { return (m_dwCursor >> 3); }
    UINT GetColumnOffset(uint32_t dwPos) const;
    POINT GetPointFromPosition(uint32_t dwPos);
    uint32_t GetPositionFromPoint(POINT pt);
    uint32_t GetDragItem(CPoint point, LPRECT lpRect);
    modplug::tracker::rowindex_t GetRowsPerBeat() const;
    modplug::tracker::rowindex_t GetRowsPerMeasure() const;

    void InvalidatePattern(BOOL bHdr=FALSE);
    void InvalidateRow(int n=-1);
    void InvalidateArea(uint32_t dwBegin, uint32_t dwEnd);
    void InvalidateSelection() { InvalidateArea(m_dwBeginSel, m_dwEndSel); }
    void InvalidateChannelsHeaders();
    void SetCurSel(uint32_t dwBegin, uint32_t dwEnd);
    BOOL SetCurrentPattern(UINT npat, int nrow=-1);
    BOOL SetCurrentRow(UINT nrow, BOOL bWrap=FALSE, BOOL bUpdateHorizontalScrollbar=TRUE );
    BOOL SetCurrentColumn(UINT ncol);
    // This should be used instead of consecutive calls to SetCurrentRow() then SetCurrentColumn()
    BOOL SetCursorPosition(UINT nrow, UINT ncol, BOOL bWrap=FALSE );
    BOOL DragToSel(uint32_t dwPos, BOOL bScroll, BOOL bNoMove=FALSE);
    BOOL SetPlayCursor(UINT nPat, UINT nRow);
    BOOL UpdateScrollbarPositions( BOOL bUpdateHorizontalScrollbar=TRUE );
// -> CODE#0014
// -> DESC="vst wet/dry slider"
//    BOOL EnterNote(UINT nNote, UINT nIns=0, BOOL bCheck=FALSE, int vol=-1, BOOL bMultiCh=FALSE);
    uint8_t EnterNote(UINT nNote, UINT nIns=0, BOOL bCheck=FALSE, int vol=-1, BOOL bMultiCh=FALSE);
// -! NEW_FEATURE#0014// -> CODE#0012
    BOOL ShowEditWindow();
    UINT GetCurrentInstrument() const;
    void SelectBeatOrMeasure(bool selectBeat);

    BOOL TransposeSelection(int transp);
    BOOL PrepareUndo(uint32_t dwBegin, uint32_t dwEnd);
    void DeleteRows(UINT colmin, UINT colmax, UINT nrows);
    void OnDropSelection();
    void ProcessChar(UINT nChar, UINT nFlags);

public:
    void DrawPatternData(HDC, module_renderer *, UINT, BOOL, BOOL, UINT, UINT, UINT, CRect&, int *);
    void DrawLetter(int x, int y, char letter, int sizex=10, int ofsx=0);
    void DrawNote(int x, int y, UINT note, CTuning* pTuning = NULL);
    void DrawInstrument(int x, int y, UINT instr);
    void DrawVolumeCommand(int x, int y, const modplug::tracker::modevent_t mc);
    void DrawChannelVUMeter(HDC hdc, int x, int y, UINT nChn);
    void DrawDragSel(HDC hdc);
    void OnDrawDragSel();

    //rewbs.customKeys
    void CursorJump(uint32_t distance, bool direction, bool snap);
    void TempEnterNote(int n, bool oldStyle = false, int vol = -1);
    void TempStopNote(int note, bool fromMidi=false, const bool bChordMode=false);
    void TempEnterChord(int n);
    void TempStopChord(int note) {TempStopNote(note, false, true);}
    void TempEnterIns(int val);
    void TempEnterOctave(int val);
    void TempEnterVol(int v);
    void TempEnterFX(int c, int v = -1);
    void TempEnterFXparam(int v);
    void SetSpacing(int n);
    void OnClearField(int, bool, bool=false);
    void InsertRows(UINT colmin, UINT colmax);
    void SetSelectionInstrument(const modplug::tracker::instrumentindex_t nIns);
    //end rewbs.customKeys

    void TogglePluginEditor(int chan); //rewbs.patPlugName

    void ExecutePaste(enmPatternPasteModes pasteMode);

public:
    //{{AFX_VIRTUAL(CViewPattern)
    virtual void OnDraw(CDC *);
    virtual void OnInitialUpdate();
    virtual BOOL OnScrollBy(CSize sizeScroll, BOOL bDoScroll = TRUE);
    virtual void UpdateView(uint32_t dwHintMask=0, CObject *pObj=NULL);
    virtual LRESULT OnModViewMsg(WPARAM, LPARAM);
    virtual LRESULT OnPlayerNotify(MPTNOTIFICATION *);
    //}}AFX_VIRTUAL

protected:
    //{{AFX_MSG(CViewPattern)
    afx_msg BOOL OnEraseBkgnd(CDC *) { return TRUE; }
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg void OnMouseMove(UINT, CPoint);
    afx_msg void OnLButtonUp(UINT, CPoint);
    afx_msg void OnLButtonDown(UINT, CPoint);
    afx_msg void OnLButtonDblClk(UINT, CPoint);
    afx_msg void OnRButtonDown(UINT, CPoint);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnSetFocus(CWnd *pOldWnd);
    afx_msg void OnKillFocus(CWnd *pNewWnd);
    afx_msg void OnEditCut();
    afx_msg void OnEditCopy();

    afx_msg void OnEditPaste() {ExecutePaste(pm_overwrite);};
    afx_msg void OnEditMixPaste() {ExecutePaste(pm_mixpaste);};
    afx_msg void OnEditMixPasteITStyle() {ExecutePaste(pm_mixpaste_it);};
    afx_msg void OnEditPasteFlood() {ExecutePaste(pm_pasteflood);};
    afx_msg void OnEditPushForwardPaste() {ExecutePaste(pm_pushforwardpaste);};

    afx_msg void OnClearSelection(bool ITStyle=false, RowMask sb = DefaultRowMask); //rewbs.customKeys
    afx_msg void OnGrowSelection();   //rewbs.customKeys
    afx_msg void OnShrinkSelection(); //rewbs.customKeys
    afx_msg void OnEditSelectAll();
    afx_msg void OnEditSelectColumn();
    afx_msg void OnSelectCurrentColumn();
    afx_msg void OnEditFind();
    afx_msg void OnEditGoto();
    afx_msg void OnEditFindNext();
    afx_msg void OnEditUndo();
    afx_msg void OnChannelReset();
    afx_msg void OnMuteFromClick(); //rewbs.customKeys
    afx_msg void OnSoloFromClick(); //rewbs.customKeys
    afx_msg void OnTogglePendingMuteFromClick(); //rewbs.customKeys
    afx_msg void OnPendingSoloChnFromClick();
    afx_msg void OnPendingUnmuteAllChnFromClick();
    afx_msg void OnSoloChannel(bool current); //rewbs.customKeys
    afx_msg void OnMuteChannel(bool current); //rewbs.customKeys
    afx_msg void OnUnmuteAll();
    afx_msg void OnRecordSelect();
// -> CODE#0012
// -> DESC="midi keyboard split"
    afx_msg void OnSplitRecordSelect();
// -! NEW_FEATURE#0012
    afx_msg void OnDeleteRows();
    afx_msg void OnDeleteRowsEx();
    afx_msg void OnInsertRows();
    afx_msg void OnPatternStep();
    afx_msg void OnSwitchToOrderList();
    afx_msg void OnPrevOrder();
    afx_msg void OnNextOrder();
    afx_msg void OnPrevInstrument() { PostCtrlMessage(CTRLMSG_PAT_PREVINSTRUMENT); }
    afx_msg void OnNextInstrument() { PostCtrlMessage(CTRLMSG_PAT_NEXTINSTRUMENT); }
//rewbs.customKeys - now implemented at ModDoc level
/*    afx_msg void OnPatternRestart() {}
    afx_msg void OnPatternPlay()    {}
    afx_msg void OnPatternPlayNoLoop()    {} */
//end rewbs.customKeys
    afx_msg void OnPatternRecord()    { PostCtrlMessage(CTRLMSG_SETRECORD, -1); }
    afx_msg void OnInterpolateVolume();
    afx_msg void OnInterpolateEffect();
    afx_msg void OnInterpolateNote();
    afx_msg void OnVisualizeEffect();            //rewbs.fxvis
    afx_msg void OnTransposeUp();
    afx_msg void OnTransposeDown();
    afx_msg void OnTransposeOctUp();
    afx_msg void OnTransposeOctDown();
    afx_msg void OnSetSelInstrument();
    afx_msg void OnAddChannelFront() { AddChannelBefore(GetChanFromCursor(m_nMenuParam)); }
    afx_msg void OnAddChannelAfter() { AddChannelBefore(GetChanFromCursor(m_nMenuParam) + 1); };
    afx_msg void OnDuplicateChannel();
    afx_msg void OnRemoveChannel();
    afx_msg void OnRemoveChannelDialog();
    afx_msg void OnPatternProperties();
    afx_msg void OnCursorCopy();
    afx_msg void OnCursorPaste();
    afx_msg void OnPatternAmplify();
    afx_msg void OnUpdateUndo(CCmdUI *pCmdUI);
    afx_msg void OnSelectPlugin(UINT nID);  //rewbs.patPlugName
    afx_msg LRESULT OnUpdatePosition(WPARAM nOrd, LPARAM nRow);
    afx_msg LRESULT OnMidiMsg(WPARAM, LPARAM);
    afx_msg LRESULT OnRecordPlugParamChange(WPARAM, LPARAM);
    afx_msg void OnClearSelectionFromMenu();
    afx_msg void OnSelectInstrument(UINT nid);
    afx_msg void OnSelectPCNoteParam(UINT nid);
    afx_msg void OnRunScript();
    afx_msg void OnShowTimeAtRow();
    afx_msg void OnRenameChannel();
    afx_msg void OnTogglePCNotePluginEditor();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()


public:
    afx_msg void OnInitMenu(CMenu* pMenu);
private:

    void SetSplitKeyboardSettings();
    bool HandleSplit(modplug::tracker::modevent_t* p, int note);
    bool BuildChannelControlCtxMenu(HMENU hMenu);
    bool BuildPluginCtxMenu(HMENU hMenu, UINT nChn, module_renderer* pSndFile);
    bool BuildRecordCtxMenu(HMENU hMenu, UINT nChn, CModDoc* pModDoc);
    bool BuildSoloMuteCtxMenu(HMENU hMenu, UINT nChn, module_renderer* pSndFile);
    bool BuildRowInsDelCtxMenu(HMENU hMenu);
    bool BuildMiscCtxMenu(HMENU hMenu);
    bool BuildSelectionCtxMenu(HMENU hMenu);
    bool BuildGrowShrinkCtxMenu(HMENU hMenu);
    bool BuildNoteInterpolationCtxMenu(HMENU hMenu, module_renderer* pSndFile);
    bool BuildVolColInterpolationCtxMenu(HMENU hMenu, module_renderer* pSndFile);
    bool BuildEffectInterpolationCtxMenu(HMENU hMenu, module_renderer* pSndFile);
    bool BuildEditCtxMenu(HMENU hMenu, CModDoc* pModDoc);
    bool BuildVisFXCtxMenu(HMENU hMenu);
    bool BuildRandomCtxMenu(HMENU hMenu);
    bool BuildTransposeCtxMenu(HMENU hMenu);
    bool BuildSetInstCtxMenu(HMENU hMenu, module_renderer* pSndFile);
    bool BuildAmplifyCtxMenu(HMENU hMenu);
    bool BuildChannelMiscCtxMenu(HMENU hMenu, module_renderer* pSndFile);
    bool BuildPCNoteCtxMenu(HMENU hMenu, module_renderer* pSndFile);

    modplug::tracker::rowindex_t GetSelectionStartRow();
    modplug::tracker::rowindex_t GetSelectionEndRow();
    modplug::tracker::chnindex_t GetSelectionStartChan();
    modplug::tracker::chnindex_t GetSelectionEndChan();
    UINT ListChansWhereColSelected(PatternColumns colType, CArray<UINT,UINT> &chans);

    static modplug::tracker::rowindex_t GetRowFromCursor(uint32_t cursor) { return (cursor >> 16); };
    static modplug::tracker::chnindex_t GetChanFromCursor(uint32_t cursor) { return static_cast<modplug::tracker::chnindex_t>((cursor & 0xFFFF) >> 3); };
    static UINT GetColTypeFromCursor(uint32_t cursor) { return (cursor & 0x07); };
    static uint32_t CreateCursor(modplug::tracker::rowindex_t row, modplug::tracker::chnindex_t channel = 0, UINT column = 0) { return (row << 16) | ((channel << 3) & 0x1FFF) | (column & 0x07); };

    bool IsInterpolationPossible(modplug::tracker::rowindex_t startRow, modplug::tracker::rowindex_t endRow, modplug::tracker::chnindex_t chan, PatternColumns colType, module_renderer* pSndFile);
    void Interpolate(PatternColumns type);

    // Return true if recording live (i.e. editing while following playback).
    // rSndFile must be the CSoundFile object of given rModDoc.
    bool IsLiveRecord(const CModDoc& rModDoc, const module_renderer& rSndFile) const;
    bool IsLiveRecord(const CMainFrame& rMainFrm, const CModDoc& rModDoc, const module_renderer& rSndFile) const;

    // If given edit positions are valid, sets them to iRow and iPat.
    // If not valid, set edit cursor position.
    void SetEditPos(const module_renderer& rSndFile,
                    modplug::tracker::rowindex_t& iRow, modplug::tracker::patternindex_t& iPat,
                    const modplug::tracker::rowindex_t iRowCandidate, const modplug::tracker::patternindex_t iPatCandidate) const;

    // Returns edit position.
    ModCommandPos GetEditPos(module_renderer& rSf, const bool bLiveRecord) const;

    // Returns pointer to modcommand at given position. If the position is not valid, returns pointer
    // to a dummy command.
    modplug::tracker::modevent_t* GetModCommand(module_renderer& rSf, const ModCommandPos& pos);

    bool IsEditingEnabled() const {return ((m_dwStatus&PATSTATUS_RECORD) != 0);}

    //Like IsEditingEnabled(), but shows some notification when editing is not enabled.
    bool IsEditingEnabled_bmsg();

    // Play one pattern row and stop ("step mode")
    void PatternStep(bool autoStep);

    // Add a channel.
    void AddChannelBefore(modplug::tracker::chnindex_t nBefore);

public:
    afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
private:

    void TogglePendingMute(UINT nChn);
    void PendingSoloChn(const modplug::tracker::chnindex_t nChn);
    void PendingUnmuteAllChn();

public:
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
};


inline bool CViewPattern::IsLiveRecord(const CMainFrame& rMainFrm, const CModDoc& rModDoc, const module_renderer& rSndFile) const
//----------------------------------------------------------------------------
{   //       (following song) && (following in correct document(?))  && (playback is on)
    return ((m_dwStatus & PATSTATUS_FOLLOWSONG) &&    (rMainFrm.GetFollowSong(&rModDoc) == m_hWnd) && !(rSndFile.IsPaused()));
}


inline bool CViewPattern::IsLiveRecord(const CModDoc& rModDoc, const module_renderer& rSndFile) const
//----------------------------------------------------------------------------
{
    return IsLiveRecord(*CMainFrame::GetMainFrame(), rModDoc, rSndFile);
}

#endif

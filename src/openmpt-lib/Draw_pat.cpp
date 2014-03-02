#include "stdafx.h"
#include "mainfrm.h"
#include "moddoc.h"
#include "dlg_misc.h"
#include "globals.h"
#include "view_pat.h"
#include "ChannelManagerDlg.h"
#include "legacy_soundlib/tuningbase.h"
#include <string>

#include "gui/qt5/pattern_bitmap_fonts.hpp"

using std::string;

using namespace modplug::pervasives;
using namespace modplug::tracker;
using namespace modplug::gui::qt5;

// Headers
#define ROWHDR_WIDTH            32        // Row header
#define COLHDR_HEIGHT            16        // Column header
#define COLUMN_HEIGHT            13
#define    VUMETERS_HEIGHT                13        // Height of vu-meters
#define    PLUGNAME_HEIGHT                16        // Height of vu-meters
#define VUMETERS_BMPWIDTH            32
#define VUMETERS_BMPHEIGHT            10
#define VUMETERS_MEDWIDTH            24
#define VUMETERS_LOWIDTH            16





// NOTE: See also CViewPattern::DrawNote() when changing stuff here
// or adding new fonts - The custom tuning note names might require
// some additions there.



/////////////////////////////////////////////////////////////////////////////
// CViewPattern Drawing Implementation

inline const pattern_font_metrics_t * GetCurrentPatternFont()
//------------------------------------------
{
    return (CMainFrame::m_dwPatternSetup & PATTERN_SMALLFONT) ? &small_pattern_font : &medium_pattern_font;
}


static uint8_t hilightcolor(int c0, int c1)
//--------------------------------------
{
    int cf0, cf1;

    cf0 = 0xC0 - (c1>>2) - (c0>>3);
    if (cf0 < 0x40) cf0 = 0x40;
    if (cf0 > 0xC0) cf0 = 0xC0;
    cf1 = 0x100 - cf0;
    return (uint8_t)((c0*cf0+c1*cf1)>>8);
}


void CViewPattern::UpdateColors()
//-------------------------------
{
    uint8_t r,g,b;

    m_Dib.SetAllColors(0, MAX_MODCOLORS, CMainFrame::rgbCustomColors);
    r = hilightcolor(GetRValue(CMainFrame::rgbCustomColors[MODCOLOR_BACKHILIGHT]),
                    GetRValue(CMainFrame::rgbCustomColors[MODCOLOR_BACKNORMAL]));
    g = hilightcolor(GetGValue(CMainFrame::rgbCustomColors[MODCOLOR_BACKHILIGHT]),
                    GetGValue(CMainFrame::rgbCustomColors[MODCOLOR_BACKNORMAL]));
    b = hilightcolor(GetBValue(CMainFrame::rgbCustomColors[MODCOLOR_BACKHILIGHT]),
                    GetBValue(CMainFrame::rgbCustomColors[MODCOLOR_BACKNORMAL]));
    m_Dib.SetColor(MODCOLOR_2NDHIGHLIGHT, RGB(r,g,b));
    m_Dib.SetBlendColor(CMainFrame::rgbCustomColors[MODCOLOR_BLENDCOLOR]);
}


BOOL CViewPattern::UpdateSizes()
//------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();
    int oldx = m_szCell.cx;
    m_szHeader.cx = ROWHDR_WIDTH;
    m_szHeader.cy = COLHDR_HEIGHT;
    if (m_dwStatus & PATSTATUS_VUMETERS) m_szHeader.cy += VUMETERS_HEIGHT;
    if (m_dwStatus & PATSTATUS_PLUGNAMESINHEADERS) m_szHeader.cy += PLUGNAME_HEIGHT;
    m_szCell.cx = 4 + pfnt->element_widths[0];
    if (m_nDetailLevel > 0) m_szCell.cx += pfnt->element_widths[1];
    if (m_nDetailLevel > 1) m_szCell.cx += pfnt->element_widths[2];
    if (m_nDetailLevel > 2) m_szCell.cx += pfnt->element_widths[3] + pfnt->element_widths[4];
    m_szCell.cy = pfnt->height;
    return (oldx != m_szCell.cx);
}


UINT CViewPattern::GetColumnOffset(uint32_t dwPos) const
//---------------------------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();
    UINT n = 0;
    dwPos &= 7;
    if (dwPos > 4) dwPos = 4;
    for (UINT i=0; i<dwPos; i++) n += pfnt->element_widths[i];
    return n;
}


void CViewPattern::UpdateView(uint32_t dwHintMask, CObject *)
//--------------------------------------------------------
{
    if (dwHintMask & HINT_MPTOPTIONS)
    {
        UpdateColors();
        UpdateSizes();
        UpdateScrollSize();
        InvalidatePattern(TRUE);
        return;
    }
    if (dwHintMask & HINT_MODCHANNELS)
    {
        InvalidateChannelsHeaders();
    }
    //if (((dwHintMask & 0xFFFFFF) == HINT_PATTERNDATA) & (m_nPattern != (dwHintMask >> HINT_SHIFT_PAT))) return;
    if ( (HintFlagPart(dwHintMask) == HINT_PATTERNDATA) && (m_nPattern != (dwHintMask >> HINT_SHIFT_PAT)) )
            return;

    if (dwHintMask & (HINT_MODTYPE|HINT_PATTERNDATA))
    {
        InvalidatePattern(FALSE);
    } else
    if (dwHintMask & HINT_PATTERNROW)
    {
// -> CODE#0008
// -> DESC"#define to set pattern bad_max size (number of rows) limit (now set to 1024 instead of 256)"
//            InvalidateRow(dwHintMask >> 24);
        InvalidateRow(dwHintMask >> HINT_SHIFT_ROW);
// -! BEHAVIOUR_CHANGE#0008
    }

}


POINT CViewPattern::GetPointFromPosition(uint32_t dwPos)
//---------------------------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();
    POINT pt;
    int xofs = GetXScrollPos();
    int yofs = GetYScrollPos();
    pt.x = (GetChanFromCursor(dwPos) - xofs) * GetColumnWidth();
    UINT imax = GetColTypeFromCursor(dwPos);
    if (imax > LAST_COLUMN + 1) imax = LAST_COLUMN + 1;
    if (imax > m_nDetailLevel + 1) imax = m_nDetailLevel + 1;
    for (UINT i=0; i<imax; i++)
    {
        pt.x += pfnt->element_widths[i];
    }
    if (pt.x < 0) pt.x = 0;
    pt.x += ROWHDR_WIDTH;
    pt.y = ((dwPos >> 16) - yofs + m_nMidRow) * m_szCell.cy;
    if (pt.y < 0) pt.y = 0;
    pt.y += m_szHeader.cy;
    return pt;
}


uint32_t CViewPattern::GetPositionFromPoint(POINT pt)
//------------------------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();
    int xofs = GetXScrollPos();
    int yofs = GetYScrollPos();
    int x = xofs + (pt.x - m_szHeader.cx) / GetColumnWidth();
    if (pt.x < m_szHeader.cx) x = (xofs) ? xofs - 1 : 0;
    int y = yofs - m_nMidRow + (pt.y - m_szHeader.cy) / m_szCell.cy;
    if (y < 0) y = 0;
    int xx = (pt.x - m_szHeader.cx) % GetColumnWidth(), dx = 0;
    int imax = 4;
    if (imax > (int)m_nDetailLevel+1) imax = m_nDetailLevel+1;
    int i = 0;
    for (i=0; i<imax; i++)
    {
        dx += pfnt->element_widths[i];
        if (xx < dx) break;
    }
    return (y << 16) | (x << 3) | i;
}


void CViewPattern::DrawLetter(int x, int y, char letter, int sizex, int ofsx)
//---------------------------------------------------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();
    int srcx = pfnt->space_x, srcy = pfnt->space_y;

    if ((letter >= '0') && (letter <= '9'))
    {
        srcx = pfnt->num_x;
        srcy = pfnt->num_y + (letter - '0') * COLUMN_HEIGHT;
    } else
    if ((letter >= 'A') && (letter < 'N'))
    {
        srcx = pfnt->alpha_am_x;
        srcy = pfnt->alpha_am_y + (letter - 'A') * COLUMN_HEIGHT;
    } else
    if ((letter >= 'N') && (letter <= 'Z'))
    {
        srcx = pfnt->alpha_nz_x;
        srcy = pfnt->alpha_nz_y + (letter - 'N') * COLUMN_HEIGHT;
    } else
    switch(letter)
    {
    case '?':
        srcx = pfnt->alpha_nz_x;
        srcy = pfnt->alpha_nz_y + 13 * COLUMN_HEIGHT;
        break;
    case '#':
        srcx = pfnt->alpha_am_x;
        srcy = pfnt->alpha_am_y + 13 * COLUMN_HEIGHT;
        break;
    //rewbs.smoothVST
    case '\\':
        srcx = pfnt->alpha_nz_x;
        srcy = pfnt->alpha_nz_y + 14 * COLUMN_HEIGHT;
        break;
    //end rewbs.smoothVST
    case ':':
        srcx = pfnt->alpha_nz_x;
        srcy = pfnt->alpha_nz_y + 15 * COLUMN_HEIGHT;
        break;
    case ' ':
        srcx = pfnt->space_x;
        srcy = pfnt->space_y;
        break;
    case '-':
        srcx = pfnt->alpha_am_x;
        srcy = pfnt->alpha_am_y + 15 * COLUMN_HEIGHT;
        break;
    case 'b':
        srcx = pfnt->alpha_am_x;
        srcy = pfnt->alpha_am_y + 14 * COLUMN_HEIGHT;
        break;

    }
    m_Dib.TextBlt(x, y, sizex, COLUMN_HEIGHT, srcx+ofsx, srcy);
}


void CViewPattern::DrawNote(int x, int y, UINT note, CTuning* pTuning)
//---------------------------------------------------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();

    UINT xsrc = pfnt->note_x, ysrc = pfnt->note_y, dx = pfnt->element_widths[0];
    if (!note)
    {
        m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, xsrc, ysrc);
    } else
    if (note == NoteNoteCut)
    {
        m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, xsrc, ysrc + 13*COLUMN_HEIGHT);
    } else
    if (note == NoteKeyOff)
    {
        m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, xsrc, ysrc + 14*COLUMN_HEIGHT);
    } else
    if(note == NoteFade)
    {
        m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, xsrc, ysrc + 17*COLUMN_HEIGHT);
    } else
    if(note == NotePc)
    {
        m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, xsrc, ysrc + 15*COLUMN_HEIGHT);
    } else
    if(note == NotePcSmooth)
    {
        m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, xsrc, ysrc + 16*COLUMN_HEIGHT);
    } else
    {
        if(pTuning)
        {   // Drawing custom note names
            string noteStr = pTuning->GetNoteName(note-NoteMiddleC);
            if(noteStr.size() < 3)
                noteStr.resize(3, ' ');

            // Hack: Manual tweaking for default/small font displays.
            if(pfnt == &medium_pattern_font)
            {
                DrawLetter(x, y, noteStr[0], 7, 0);
                DrawLetter(x + 7, y, noteStr[1], 6, 0);
                DrawLetter(x + 13, y, noteStr[2], 7, 1);
            }
            else
            {
                DrawLetter(x, y, noteStr[0], 5, 0);
                DrawLetter(x + 5, y, noteStr[1], 5, 0);
                DrawLetter(x + 10, y, noteStr[2], 6, 0);
            }
        }
        else //Original
        {
            UINT o = (note-1) / 12; //Octave
            UINT n = (note-1) % 12; //Note
            m_Dib.TextBlt(x, y, pfnt->note_width, COLUMN_HEIGHT, xsrc, ysrc+(n+1)*COLUMN_HEIGHT);
            if(o <= 9)
                m_Dib.TextBlt(x+pfnt->note_width, y, pfnt->octave_width, COLUMN_HEIGHT,
                                pfnt->num_x, pfnt->num_y+o*COLUMN_HEIGHT);
            else
                DrawLetter(x+pfnt->note_width, y, '?', pfnt->octave_width);
        }
    }
}


void CViewPattern::DrawInstrument(int x, int y, UINT instr)
//---------------------------------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();
    if (instr)
    {
        UINT dx = pfnt->instr_firstchar_width;
        if (instr < 100)
        {
            m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, pfnt->num_x+pfnt->instr_offset, pfnt->num_y+(instr / 10)*COLUMN_HEIGHT);
        } else
        {
            m_Dib.TextBlt(x, y, dx, COLUMN_HEIGHT, pfnt->num_10_x+pfnt->instr_10_offset, pfnt->num_10_y+((instr-100) / 10)*COLUMN_HEIGHT);
        }
        m_Dib.TextBlt(x+dx, y, pfnt->element_widths[1]-dx, COLUMN_HEIGHT, pfnt->num_x+1, pfnt->num_y+(instr % 10)*COLUMN_HEIGHT);
    } else
    {
        m_Dib.TextBlt(x, y, pfnt->element_widths[1], COLUMN_HEIGHT, pfnt->clear_x+pfnt->element_widths[0], pfnt->clear_y);
    }
}


void CViewPattern::DrawVolumeCommand(int x, int y, const modplug::tracker::modevent_t mc)
//---------------------------------------------------------------------
{
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();

    if(mc.IsPcNote())
    {    //If note is parameter control note, drawing volume command differently.
        const int val = bad_min(modplug::tracker::modevent_t::MaxColumnValue, mc.GetValueVolCol());

        m_Dib.TextBlt(x, y, 1, COLUMN_HEIGHT, pfnt->clear_x, pfnt->clear_y);
        m_Dib.TextBlt(x + 1, y, pfnt->vol_width, COLUMN_HEIGHT,
                            pfnt->num_x, pfnt->num_y+(val / 100)*COLUMN_HEIGHT);
        m_Dib.TextBlt(x+pfnt->vol_width, y, pfnt->vol_firstchar_width, COLUMN_HEIGHT,
                            pfnt->num_x, pfnt->num_y+((val / 10)%10)*COLUMN_HEIGHT);
        m_Dib.TextBlt(x+pfnt->vol_width+pfnt->vol_firstchar_width, y, pfnt->element_widths[2]-(pfnt->vol_width+pfnt->vol_firstchar_width), COLUMN_HEIGHT,
                            pfnt->num_x, pfnt->num_y+(val % 10)*COLUMN_HEIGHT);
    }
    else
    {
        if (mc.volcmd)
        {
            const int volcmd = (mc.volcmd & 0x0F);
            const int vol  = (mc.vol & 0x7F);
            m_Dib.TextBlt(x, y, pfnt->vol_width, COLUMN_HEIGHT,
                            pfnt->volcmd_x, pfnt->volcmd_y+volcmd*COLUMN_HEIGHT);
            m_Dib.TextBlt(x+pfnt->vol_width, y, pfnt->vol_firstchar_width, COLUMN_HEIGHT,
                            pfnt->num_x, pfnt->num_y+(vol / 10)*COLUMN_HEIGHT);
            m_Dib.TextBlt(x+pfnt->vol_width+pfnt->vol_firstchar_width, y, pfnt->element_widths[2]-(pfnt->vol_width+pfnt->vol_firstchar_width), COLUMN_HEIGHT,
                            pfnt->num_x, pfnt->num_y+(vol % 10)*COLUMN_HEIGHT);
        } else
        {
            int srcx = pfnt->element_widths[0] + pfnt->element_widths[1];
            m_Dib.TextBlt(x, y, pfnt->element_widths[2], COLUMN_HEIGHT, pfnt->clear_x+srcx, pfnt->clear_y);
        }
    }
}


void CViewPattern::OnDraw(CDC *pDC)
//---------------------------------
{
    ghettotimer homie(__FUNCTION__);

    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CHAR s[256];
    HGDIOBJ oldpen;
    CRect rcClient, rect, rc;
    CModDoc *pModDoc;
    module_renderer *pSndFile;
    HDC hdc;
    UINT xofs, yofs, nColumnWidth, ncols, nrows, ncolhdr;
    int xpaint, ypaint, mixPlug;

    ASSERT(pDC);
    UpdateSizes();
    if ((pModDoc = GetDocument()) == NULL) return;
    GetClientRect(&rcClient);
    hdc = pDC->m_hDC;
    oldpen = ::SelectObject(hdc, CMainFrame::penDarkGray);
    xofs = GetXScrollPos();
    yofs = GetYScrollPos();
    pSndFile = pModDoc->GetSoundFile();
    nColumnWidth = m_szCell.cx;
    nrows = (pSndFile->Patterns[m_nPattern]) ? pSndFile->Patterns[m_nPattern].GetNumRows() : 0;
    ncols = pSndFile->GetNumChannels();
    xpaint = m_szHeader.cx;
    ypaint = rcClient.top;
    ncolhdr = xofs;
    rect.SetRect(0, rcClient.top, rcClient.right, rcClient.top + m_szHeader.cy);
    if (::RectVisible(hdc, &rect))
    {
        wsprintf(s, "#%d", m_nPattern);
        rect.right = m_szHeader.cx;
        DrawButtonRect(hdc, &rect, s, FALSE,
            ((m_bInItemRect) && ((m_nDragItem & DRAGITEM_MASK) == DRAGITEM_PATTERNHEADER)) ? TRUE : FALSE);

        // Drawing Channel Headers
        while (xpaint < rcClient.right)
        {
            rect.SetRect(xpaint, ypaint, xpaint+nColumnWidth, ypaint + m_szHeader.cy);
            if (ncolhdr < ncols)
            {
// -> CODE#0012
// -> DESC="midi keyboard split"
                const char *pszfmt = pSndFile->m_bChannelMuteTogglePending[ncolhdr]? "[Channel %d]" : "Channel %d";
//                            const char *pszfmt = pModDoc->IsChannelRecord(ncolhdr) ? "Channel %d " : "Channel %d";
// -! NEW_FEATURE#0012
                if ((pSndFile->m_nType & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)) && ((uint8_t)pSndFile->ChnSettings[ncolhdr].szName[0] >= ' '))
                    pszfmt = pSndFile->m_bChannelMuteTogglePending[ncolhdr]?"%d: [%s]":"%d: %s";
                else if (m_nDetailLevel < 2) pszfmt = pSndFile->m_bChannelMuteTogglePending[ncolhdr]?"[Ch%d]":"Ch%d";
                else if (m_nDetailLevel < 3) pszfmt = pSndFile->m_bChannelMuteTogglePending[ncolhdr]?"[Chn %d]":"Chn %d";
                wsprintf(s, pszfmt, ncolhdr+1, pSndFile->ChnSettings[ncolhdr].szName);
// -> CODE#0012
// -> DESC="midi keyboard split"
//                            DrawButtonRect(hdc, &rect, s,
//                                    (pSndFile->ChnSettings[ncolhdr].dwFlags & CHN_MUTE) ? TRUE : FALSE,
//                                    ((m_bInItemRect) && ((m_nDragItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER) && ((m_nDragItem & DRAGITEM_VALUEMASK) == ncolhdr)) ? TRUE : FALSE, DT_CENTER);
//                            rect.bottom = rect.top + COLHDR_HEIGHT;
                DrawButtonRect(hdc, &rect, s,
                    (pSndFile->ChnSettings[ncolhdr].dwFlags & CHN_MUTE) ? TRUE : FALSE,
                    ((m_bInItemRect) && ((m_nDragItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER) && ((m_nDragItem & DRAGITEM_VALUEMASK) == ncolhdr)) ? TRUE : FALSE,
                    pModDoc->IsChannelRecord(ncolhdr) ? DT_RIGHT : DT_CENTER);

                // When dragging around channel headers, mark insertion position
                if(m_bDragging && !m_bInItemRect
                    && (m_nDragItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER
                    && (m_nDropItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER
                    && (m_nDropItem & DRAGITEM_VALUEMASK) == ncolhdr)
                {
                    RECT r;
                    r.top = rect.top;
                    r.bottom = rect.bottom;
                    // Drop position depends on whether hovered channel is left or right of dragged item.
                    r.left = ((m_nDropItem & DRAGITEM_VALUEMASK) < (m_nDragItem & DRAGITEM_VALUEMASK) || m_bShiftDragging) ? rect.left : rect.right - 2;
                    r.right = r.left + 2;
                    ::FillRect(hdc, &r, CMainFrame::brushText);
                }

                rect.bottom = rect.top + COLHDR_HEIGHT;

                CRect insRect;
                insRect.SetRect(xpaint, ypaint, xpaint+nColumnWidth / 8 + 3, ypaint + 16);
//                            if (MultiRecordMask[ncolhdr>>3] & (1 << (ncolhdr&7)))
                if (pModDoc->IsChannelRecord1(ncolhdr))
                {
//                                    rect.DeflateRect(1, 1);
//                                    InvertRect(hdc, &rect);
//                                    rect.InflateRect(1, 1);
                    FrameRect(hdc,&rect,CMainFrame::brushGray);
                    InvertRect(hdc, &rect);
                    s[0] = '1';
                    s[1] = '\0';
                    DrawButtonRect(hdc, &insRect, s, FALSE, FALSE, DT_CENTER);
                    FrameRect(hdc,&insRect,CMainFrame::brushBlack);
                }
                else if (pModDoc->IsChannelRecord2(ncolhdr))
                {
                    FrameRect(hdc,&rect,CMainFrame::brushGray);
                    InvertRect(hdc, &rect);
                    s[0] = '2';
                    s[1] = '\0';
                    DrawButtonRect(hdc, &insRect, s, FALSE, FALSE, DT_CENTER);
                    FrameRect(hdc,&insRect,CMainFrame::brushBlack);
                }
// -! NEW_FEATURE#0012

                if (m_dwStatus & PATSTATUS_VUMETERS)
                {
                    OldVUMeters[ncolhdr] = 0;
                    DrawChannelVUMeter(hdc, rect.left + 1, rect.bottom, ncolhdr);
                    rect.top+=VUMETERS_HEIGHT;
                    rect.bottom+=VUMETERS_HEIGHT;
                }
                if (m_dwStatus & PATSTATUS_PLUGNAMESINHEADERS)
                {
                    rect.top+=PLUGNAME_HEIGHT;
                    rect.bottom+=PLUGNAME_HEIGHT;
                    mixPlug=pSndFile->ChnSettings[ncolhdr].nMixPlugin;
                    if (mixPlug) {
                        wsprintf(s, "%d: %s", mixPlug, (pSndFile->m_MixPlugins[mixPlug-1]).pMixPlugin?(pSndFile->m_MixPlugins[mixPlug-1]).Info.szName:"[empty]");
                    } else {
                        wsprintf(s, "---");
                    }
                    DrawButtonRect(hdc, &rect, s, FALSE,
                        ((m_bInItemRect) && ((m_nDragItem & DRAGITEM_MASK) == DRAGITEM_PLUGNAME) && ((m_nDragItem & DRAGITEM_VALUEMASK) == ncolhdr)) ? TRUE : FALSE, DT_CENTER);
                }

            } else break;
            ncolhdr++;
            xpaint += nColumnWidth;
        }
    }
    ypaint += m_szHeader.cy;
    if (m_nMidRow)
    {
        if (yofs >= m_nMidRow)
        {
            yofs -= m_nMidRow;
        } else
        {
            UINT nSkip = m_nMidRow - yofs;
            UINT nPrevPat = m_nPattern;
            BOOL bPrevPatFound = FALSE;

            // Display previous pattern
            if (CMainFrame::m_dwPatternSetup & PATTERN_SHOWPREVIOUS)
            {
                const modplug::tracker::orderindex_t startOrder = static_cast<modplug::tracker::orderindex_t>(SendCtrlMessage(CTRLMSG_GETCURRENTORDER));
                if(startOrder > 0)
                {
                    modplug::tracker::orderindex_t prevOrder;
                    prevOrder = pSndFile->Order.GetPreviousOrderIgnoringSkips(startOrder);
                    //Skip +++ items

                    if(startOrder < pSndFile->Order.size() && pSndFile->Order[startOrder] == m_nPattern)
                    {
                        nPrevPat = pSndFile->Order[prevOrder];
                        bPrevPatFound = TRUE;
                    }
                }
            }
            if ((bPrevPatFound) && (nPrevPat < pSndFile->Patterns.Size()) && (pSndFile->Patterns[nPrevPat]))
            {
                UINT nPrevRows = pSndFile->Patterns[nPrevPat].GetNumRows();
                UINT n = (nSkip < nPrevRows) ? nSkip : nPrevRows;

                ypaint += (nSkip-n)*m_szCell.cy;
                rect.SetRect(0, m_szHeader.cy, nColumnWidth * ncols + m_szHeader.cx, ypaint - 1);
                m_Dib.SetBlendMode(0x80);
                DrawPatternData(hdc, pSndFile, nPrevPat, FALSE, FALSE,
                        nPrevRows-n, nPrevRows, xofs, rcClient, &ypaint);
                m_Dib.SetBlendMode(0);
            } else
            {
                ypaint += nSkip * m_szCell.cy;
                rect.SetRect(0, m_szHeader.cy, nColumnWidth * ncols + m_szHeader.cx, ypaint - 1);
            }
            if ((rect.bottom > rect.top) && (rect.right > rect.left))
            {
                ::FillRect(hdc, &rect, CMainFrame::brushGray);
                ::MoveToEx(hdc, 0, rect.bottom, NULL);
                ::LineTo(hdc, rect.right, rect.bottom);
            }
            yofs = 0;
        }
    }
    int ypatternend = ypaint + (nrows-yofs)*m_szCell.cy;
    DrawPatternData(hdc, pSndFile, m_nPattern, TRUE, (pMainFrm->GetModPlaying() == pModDoc) ? TRUE : FALSE,
                    yofs, nrows, xofs, rcClient, &ypaint);
    // Display next pattern
    if ((CMainFrame::m_dwPatternSetup & PATTERN_SHOWPREVIOUS) && (ypaint < rcClient.bottom) && (ypaint == ypatternend))
    {
        int nVisRows = (rcClient.bottom - ypaint + m_szCell.cy - 1) / m_szCell.cy;
        if ((nVisRows > 0) && (m_nMidRow))
        {
            UINT nNextPat = m_nPattern;
            BOOL bNextPatFound = FALSE;
            const modplug::tracker::orderindex_t startOrder= static_cast<modplug::tracker::orderindex_t>(SendCtrlMessage(CTRLMSG_GETCURRENTORDER));
            modplug::tracker::orderindex_t nNextOrder;
            nNextOrder = pSndFile->Order.GetNextOrderIgnoringSkips(startOrder);
            if(nNextOrder == startOrder) nNextOrder = modplug::tracker::OrderIndexInvalid;
            //Ignore skip items(+++) from sequence.
            const modplug::tracker::orderindex_t ordCount = pSndFile->Order.GetLength();

            if ((nNextOrder < ordCount) && (pSndFile->Order[startOrder] == m_nPattern))
            {
                nNextPat = pSndFile->Order[nNextOrder];
                bNextPatFound = TRUE;
            }
            if ((bNextPatFound) && (nNextPat < pSndFile->Patterns.Size()) && (pSndFile->Patterns[nNextPat]))
            {
                UINT nNextRows = pSndFile->Patterns[nNextPat].GetNumRows();
                UINT n = ((UINT)nVisRows < nNextRows) ? nVisRows : nNextRows;

                m_Dib.SetBlendMode(0x80);
                DrawPatternData(hdc, pSndFile, nNextPat, FALSE, FALSE,
                        0, n, xofs, rcClient, &ypaint);
                m_Dib.SetBlendMode(0);
            }
        }
    }
    // Drawing outside pattern area
    xpaint = m_szHeader.cx + (ncols - xofs) * nColumnWidth;
    if ((xpaint < rcClient.right) && (ypaint > rcClient.top))
    {
        rc.SetRect(xpaint, rcClient.top, rcClient.right, ypaint);
        ::FillRect(hdc, &rc, CMainFrame::brushGray);
    }
    if (ypaint < rcClient.bottom)
    {
        rc.SetRect(0, ypaint, rcClient.right+1, rcClient.bottom+1);
        DrawButtonRect(hdc, &rc, "");
    }
    // Drawing pattern selection
    if (m_dwStatus & PATSTATUS_DRAGNDROPPING)
    {
        DrawDragSel(hdc);
    }
    if (oldpen) ::SelectObject(hdc, oldpen);

// -> CODE#0015
// -> DESC="channels management dlg"
    bool activeDoc = pMainFrm ? (pMainFrm->GetActiveDoc() == GetDocument()) : false;

    if(activeDoc && CChannelManagerDlg::sharedInstance(FALSE) && CChannelManagerDlg::sharedInstance()->IsDisplayed())
        CChannelManagerDlg::sharedInstance()->SetDocument((void*)this);
// -! NEW_FEATURE#0015
}


void CViewPattern::DrawPatternData(HDC hdc,    module_renderer *pSndFile, UINT nPattern, BOOL bSelEnable,
                        BOOL bPlaying, UINT yofs, UINT nrows, UINT xofs, CRect &rcClient, int *pypaint)
//-----------------------------------------------------------------------------------------------------
{
    uint8_t bColSel[MAX_BASECHANNELS];
    const pattern_font_metrics_t * pfnt = GetCurrentPatternFont();
    modplug::tracker::modevent_t m0, *pPattern = pSndFile->Patterns[nPattern];
    CHAR s[256];
    CRect rect;
    int xpaint, ypaint = *pypaint;
    int row_col, row_bkcol;
    UINT bRowSel, bSpeedUp, nColumnWidth, ncols, maxcol;

    ncols = pSndFile->GetNumChannels();

    m0.note = m0.instr = m0.vol = m0.param = 0;
    m0.command = CmdNone;
    m0.volcmd = VolCmdNone;

    nColumnWidth = m_szCell.cx;
    rect.SetRect(m_szHeader.cx, rcClient.top, m_szHeader.cx+nColumnWidth, rcClient.bottom);
    for (UINT cmk=xofs; cmk<ncols; cmk++)
    {
        UINT c = cmk << 3;
        bColSel[cmk] = 0;
        if (bSelEnable)
        {
            for (UINT n=0; n<5; n++)
            {
                if ((c+n >= (m_dwBeginSel & 0xFFFF)) && (c+n <= (m_dwEndSel & 0xFFFF))) bColSel[cmk] |= 1 << n;
            }
        }
        if (!::RectVisible(hdc, &rect)) bColSel[cmk] |= 0x80;
        rect.left += nColumnWidth;
        rect.right += nColumnWidth;
    }
    // Max Visible Column
    maxcol = ncols;
    while ((maxcol > xofs) && (bColSel[maxcol-1] & 0x80)) maxcol--;
    // Init bitmap border
    {
        UINT maxndx = pSndFile->GetNumChannels() * m_szCell.cx;
        UINT ibmp = 0;
        if (maxndx > FASTBMP_MAXWIDTH) maxndx = FASTBMP_MAXWIDTH;
        do
        {
            ibmp += nColumnWidth;
            m_Dib.TextBlt(ibmp-4, 0, 4, m_szCell.cy, pfnt->clear_x+pfnt->width-4, pfnt->clear_y);
        } while (ibmp + nColumnWidth <= maxndx);
    }
    bRowSel = FALSE;
    row_col = row_bkcol = -1;
    for (UINT row=yofs; row<nrows; row++)
    {
        UINT col, xbmp, nbmp, oldrowcolor;

        wsprintf(s, (CMainFrame::m_dwPatternSetup & PATTERN_HEXDISPLAY) ? "%02X" : "%d", row);
        rect.left = 0;
        rect.top = ypaint;
        rect.right = rcClient.right;
        rect.bottom = rect.top + m_szCell.cy;
        if (!::RectVisible(hdc, &rect))
        {
            // No speedup for these columns next time
            for (UINT iup=xofs; iup<maxcol; iup++) bColSel[iup] &= ~0x40;
            goto SkipRow;
        }
        rect.right = rect.left + m_szHeader.cx;
        DrawButtonRect(hdc, &rect, s, !bSelEnable);
        oldrowcolor = (row_bkcol << 16) | (row_col << 8) | (bRowSel);
        bRowSel = ((row >= (m_dwBeginSel >> 16)) && (row <= (m_dwEndSel >> 16))) ? TRUE : FALSE;
        row_col = MODCOLOR_TEXTNORMAL;
        row_bkcol = MODCOLOR_BACKNORMAL;

        // time signature highlighting
        modplug::tracker::rowindex_t nBeat = pSndFile->m_nDefaultRowsPerBeat, nMeasure = pSndFile->m_nDefaultRowsPerMeasure;
        if(pSndFile->Patterns[nPattern].GetOverrideSignature())
        {
            nBeat = pSndFile->Patterns[nPattern].GetRowsPerBeat();
            nMeasure = pSndFile->Patterns[nPattern].GetRowsPerMeasure();
        }
        // secondary highlight (beats)
        if ((CMainFrame::m_dwPatternSetup & PATTERN_2NDHIGHLIGHT)
         && (nBeat) && (nBeat < nrows))
        {
            if (!(row % nBeat))
            {
                row_bkcol = MODCOLOR_2NDHIGHLIGHT;
            }
        }
        // primary highlight (measures)
        if ((CMainFrame::m_dwPatternSetup & PATTERN_STDHIGHLIGHT)
         && (nMeasure) && (nMeasure < nrows))
        {
            if (!(row % nMeasure))
            {
                row_bkcol = MODCOLOR_BACKHILIGHT;
            }
        }
        if (bSelEnable)
        {
            if ((row == m_nPlayRow) && (nPattern == m_nPlayPat))
            {
                row_col = MODCOLOR_TEXTPLAYCURSOR;
                row_bkcol = MODCOLOR_BACKPLAYCURSOR;
            }
            if (row == m_nRow)
            {
                if (m_dwStatus & PATSTATUS_FOCUS)
                {
                    row_col = MODCOLOR_TEXTCURROW;
                    row_bkcol = MODCOLOR_BACKCURROW;
                } else
                if ((m_dwStatus & PATSTATUS_FOLLOWSONG) && (bPlaying))
                {
                    row_col = MODCOLOR_TEXTPLAYCURSOR;
                    row_bkcol = MODCOLOR_BACKPLAYCURSOR;
                }
            }
        }
        // Eliminate non-visible column
        xpaint = m_szHeader.cx;
        col = xofs;
        while ((bColSel[col] & 0x80) && (col < maxcol))
        {
            bColSel[col] &= ~0x40;
            col++;
            xpaint += nColumnWidth;
        }
        // Optimization: same row color ?
        bSpeedUp = (oldrowcolor == (UINT)((row_bkcol << 16) | (row_col << 8) | bRowSel)) ? TRUE : FALSE;
        xbmp = nbmp = 0;
        do
        {
            uint32_t dwSpeedUpMask;
            modplug::tracker::modevent_t *m;
            int x, bk_col, tx_col, col_sel, fx_col;

            m = (pPattern) ? &pPattern[row*ncols+col] : &m0;
            dwSpeedUpMask = 0;
            if ((bSpeedUp) && (bColSel[col] & 0x40) && (pPattern) && (row))
            {
                modplug::tracker::modevent_t *mold = m - ncols;
                if (m->note == mold->note) dwSpeedUpMask |= 0x01;
                if ((m->instr == mold->instr) || (m_nDetailLevel < 1)) dwSpeedUpMask |= 0x02;
                if ( m->IsPcNote() || mold->IsPcNote() )
                {   // Handle speedup mask for PC notes.
                    if(m->note == mold->note)
                    {
                        if(m->GetValueVolCol() == mold->GetValueVolCol() || (m_nDetailLevel < 2)) dwSpeedUpMask |= 0x04;
                        if(m->GetValueEffectCol() == mold->GetValueEffectCol() || (m_nDetailLevel < 3)) dwSpeedUpMask |= 0x18;
                    }
                }
                else
                {
                    if (((m->volcmd == mold->volcmd) && ((!m->volcmd) || (m->vol == mold->vol))) || (m_nDetailLevel < 2)) dwSpeedUpMask |= 0x04;
                    if ((m->command == mold->command) || (m_nDetailLevel < 3)) dwSpeedUpMask |= (m->command) ? 0x08 : 0x18;
                }
                if (dwSpeedUpMask == 0x1F) goto DoBlit;
            }
            bColSel[col] |= 0x40;
            col_sel = 0;
            if (bRowSel) col_sel = bColSel[col] & 0x3F;
            tx_col = row_col;
            bk_col = row_bkcol;
            if (col_sel)
            {
                tx_col = MODCOLOR_TEXTSELECTED;
                bk_col = MODCOLOR_BACKSELECTED;
            }
            // Speedup: Empty command which is either not or fully selected
            //if ((!*((LPDWORD)m)) && (!*(((LPWORD)m)+2)) && ((!col_sel) || (col_sel == 0x1F)))
            if (m->IsEmpty() && ((!col_sel) || (col_sel == 0x1F)))
            {
                m_Dib.SetTextColor(tx_col, bk_col);
                m_Dib.TextBlt(xbmp, 0, nColumnWidth-4, m_szCell.cy, pfnt->clear_x, pfnt->clear_y);
                goto DoBlit;
            }
            x = 0;
            // Note
            if (!(dwSpeedUpMask & 0x01))
            {
                tx_col = row_col;
                bk_col = row_bkcol;
                if ((CMainFrame::m_dwPatternSetup & PATTERN_EFFECTHILIGHT) && (m->note) && (m->note <= NoteMax))
                {
                    tx_col = MODCOLOR_NOTE;
                    // Highlight notes that are not supported by the Amiga (for S3M this is not always correct)
                    if((pSndFile->m_dwSongFlags & (SONG_PT1XMODE|SONG_AMIGALIMITS)) && (m->note < NoteMiddleC - 12 || m->note >= NoteMiddleC + 2 * 12))
                        tx_col = MODCOLOR_DODGY_COMMANDS;
                }
                if (col_sel & 0x01)
                {
                    tx_col = MODCOLOR_TEXTSELECTED;
                    bk_col = MODCOLOR_BACKSELECTED;
                }
                // Drawing note
                m_Dib.SetTextColor(tx_col, bk_col);
                if(pSndFile->m_nType == MOD_TYPE_MPT && m->instr < MAX_INSTRUMENTS && pSndFile->Instruments[m->instr])
                    DrawNote(xbmp+x, 0, m->note, pSndFile->Instruments[m->instr]->pTuning);
                else //Original
                    DrawNote(xbmp+x, 0, m->note);
            }
            x += pfnt->element_widths[0];
            // Instrument
            if (m_nDetailLevel > 0)
            {
                if (!(dwSpeedUpMask & 0x02))
                {
                    tx_col = row_col;
                    bk_col = row_bkcol;
                    if ((CMainFrame::m_dwPatternSetup & PATTERN_EFFECTHILIGHT) && (m->instr))
                    {
                        tx_col = MODCOLOR_INSTRUMENT;
                    }
                    if (col_sel & 0x02 /*|| col_sel & 0x01*/) //LP Style select
                    {
                        tx_col = MODCOLOR_TEXTSELECTED;
                        bk_col = MODCOLOR_BACKSELECTED;
                    }
                    // Drawing instrument
                    m_Dib.SetTextColor(tx_col, bk_col);
                    DrawInstrument(xbmp+x, 0, m->instr);
                }
                x += pfnt->element_widths[1];
            }
            // Volume
            if (m_nDetailLevel > 1)
            {
                if (!(dwSpeedUpMask & 0x04))
                {
                    tx_col = row_col;
                    bk_col = row_bkcol;
                    if (col_sel & 0x04)
                    {
                        tx_col = MODCOLOR_TEXTSELECTED;
                        bk_col = MODCOLOR_BACKSELECTED;
                    } else
                    if ((!m->IsPcNote()) && (m->volcmd) && (m->volcmd < VolCmdMax) && (CMainFrame::m_dwPatternSetup & PATTERN_EFFECTHILIGHT))
                    {
                        if(gVolEffectColors[m->volcmd] != 0)
                            tx_col = gVolEffectColors[m->volcmd];
                    }
                    // Drawing Volume
                    m_Dib.SetTextColor(tx_col, bk_col);
                    DrawVolumeCommand(xbmp+x, 0, *m);
                }
                x += pfnt->element_widths[2];
            }
            // Command & param
            if (m_nDetailLevel > 2)
            {
                const bool isPCnote = m->IsPcNote();
                uint16_t val = m->GetValueEffectCol();
                if(val > modplug::tracker::modevent_t::MaxColumnValue) val = modplug::tracker::modevent_t::MaxColumnValue;
                fx_col = row_col;
                if (!isPCnote && (m->command) && (m->command < CmdMax) && (CMainFrame::m_dwPatternSetup & PATTERN_EFFECTHILIGHT))
                {
                    if(gEffectColors[m->command] != 0)
                        fx_col = gEffectColors[m->command];
                }
                if (!(dwSpeedUpMask & 0x08))
                {
                    tx_col = fx_col;
                    bk_col = row_bkcol;
                    if (col_sel & 0x08)
                    {
                        tx_col = MODCOLOR_TEXTSELECTED;
                        bk_col = MODCOLOR_BACKSELECTED;
                    }

                    // Drawing Command
                    m_Dib.SetTextColor(tx_col, bk_col);
                    if(isPCnote)
                    {
                        m_Dib.TextBlt(xbmp + x, 0, 2, COLUMN_HEIGHT, pfnt->clear_x+x, pfnt->clear_y);
                        m_Dib.TextBlt(xbmp + x + 2, 0, pfnt->element_widths[3], m_szCell.cy, pfnt->num_x, pfnt->num_y+(val / 100)*COLUMN_HEIGHT);
                    }
                    else
                    {
                        if (m->command)
                        {
                            UINT command = m->command & 0x3F;
                            int n =    (pSndFile->m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM)) ? mod_command_glyphs[command] : s3m_command_glyphs[command];
                            ASSERT(n > ' ');
                            //if (n <= ' ') n = '?';
                            DrawLetter(xbmp+x, 0, (char)n, pfnt->element_widths[3], pfnt->cmd_offset);
                        } else
                        {
                            m_Dib.TextBlt(xbmp+x, 0, pfnt->element_widths[3], COLUMN_HEIGHT, pfnt->clear_x+x, pfnt->clear_y);
                        }
                    }
                }
                x += pfnt->element_widths[3];
                // Param
                if (!(dwSpeedUpMask & 0x10))
                {
                    tx_col = fx_col;
                    bk_col = row_bkcol;
                    if (col_sel & 0x10)
                    {
                        tx_col = MODCOLOR_TEXTSELECTED;
                        bk_col = MODCOLOR_BACKSELECTED;
                    }

                    // Drawing param
                    m_Dib.SetTextColor(tx_col, bk_col);
                    if(isPCnote)
                    {
                        m_Dib.TextBlt(xbmp + x, 0, pfnt->cmd_firstchar_width, m_szCell.cy, pfnt->num_x, pfnt->num_y+((val / 10) % 10)*COLUMN_HEIGHT);
                        m_Dib.TextBlt(xbmp + x + pfnt->cmd_firstchar_width, 0, pfnt->element_widths[4]-pfnt->cmd_firstchar_width, m_szCell.cy, pfnt->num_x+1, pfnt->num_y+(val % 10)*COLUMN_HEIGHT);
                    }
                    else
                    {
                        if (m->command)
                        {
                            m_Dib.TextBlt(xbmp+x, 0, pfnt->cmd_firstchar_width, m_szCell.cy, pfnt->num_x, pfnt->num_y+(m->param >> 4)*COLUMN_HEIGHT);
                            m_Dib.TextBlt(xbmp+x+pfnt->cmd_firstchar_width, 0, pfnt->element_widths[4]-pfnt->cmd_firstchar_width, m_szCell.cy, pfnt->num_x+1, pfnt->num_y+(m->param & 0x0F)*COLUMN_HEIGHT);
                        } else
                        {
                            m_Dib.TextBlt(xbmp+x, 0, pfnt->element_widths[4], m_szCell.cy, pfnt->clear_x+x, pfnt->clear_y);
                        }
                    }
                }
            }
        DoBlit:
            nbmp++;
            xbmp += nColumnWidth;
            xpaint += nColumnWidth;
            if ((xbmp + nColumnWidth >= FASTBMP_MAXWIDTH) || (xpaint >= rcClient.right)) break;
        } while (++col < maxcol);
        m_Dib.Blit(hdc, xpaint-xbmp, ypaint, xbmp, m_szCell.cy);
    SkipRow:
        ypaint += m_szCell.cy;
        if (ypaint >= rcClient.bottom) break;
    }
    *pypaint = ypaint;

}


void CViewPattern::DrawChannelVUMeter(HDC hdc, int x, int y, UINT nChn)
//---------------------------------------------------------------------
{
    if (ChnVUMeters[nChn] != OldVUMeters[nChn])
    {
        UINT vul, vur;
        vul = ChnVUMeters[nChn] & 0xFF;
        vur = (ChnVUMeters[nChn] & 0xFF00) >> 8;
        vul /= 15;
        vur /= 15;
        if (vul > 8) vul = 8;
        if (vur > 8) vur = 8;
        x += (m_szCell.cx / 2);
        if (m_nDetailLevel <= 1)
        {
            DibBlt(hdc, x-VUMETERS_LOWIDTH-1, y, VUMETERS_LOWIDTH, VUMETERS_BMPHEIGHT,
                VUMETERS_BMPWIDTH*2+VUMETERS_MEDWIDTH*2, vul * VUMETERS_BMPHEIGHT, CMainFrame::bmpVUMeters);
            DibBlt(hdc, x-1, y, VUMETERS_LOWIDTH, VUMETERS_BMPHEIGHT,
                VUMETERS_BMPWIDTH*2+VUMETERS_MEDWIDTH*2+VUMETERS_LOWIDTH, vur * VUMETERS_BMPHEIGHT, CMainFrame::bmpVUMeters);
        } else
        if (m_nDetailLevel <= 2)
        {
            DibBlt(hdc, x - VUMETERS_MEDWIDTH-1, y, VUMETERS_MEDWIDTH, VUMETERS_BMPHEIGHT,
                VUMETERS_BMPWIDTH*2, vul * VUMETERS_BMPHEIGHT, CMainFrame::bmpVUMeters);
            DibBlt(hdc, x, y, VUMETERS_MEDWIDTH, VUMETERS_BMPHEIGHT,
                VUMETERS_BMPWIDTH*2+VUMETERS_MEDWIDTH, vur * VUMETERS_BMPHEIGHT, CMainFrame::bmpVUMeters);
        } else
        {
            DibBlt(hdc, x - VUMETERS_BMPWIDTH - 1, y, VUMETERS_BMPWIDTH, VUMETERS_BMPHEIGHT,
                0, vul * VUMETERS_BMPHEIGHT, CMainFrame::bmpVUMeters);
            DibBlt(hdc, x + 1, y, VUMETERS_BMPWIDTH, VUMETERS_BMPHEIGHT,
                VUMETERS_BMPWIDTH, vur * VUMETERS_BMPHEIGHT, CMainFrame::bmpVUMeters);
        }
        OldVUMeters[nChn] = ChnVUMeters[nChn];
    }
}


void CViewPattern::DrawDragSel(HDC hdc)
//-------------------------------------
{
    CModDoc *pModDoc;
    module_renderer *pSndFile;
    CRect rect;
    POINT ptTopLeft, ptBottomRight;
    uint32_t dwTopLeft, dwBottomRight;
    bool bLeft, bTop, bRight, bBottom;
    int x1, y1, x2, y2, dx, dy, c1, c2;
    int nChannels, nRows;

    if ((pModDoc = GetDocument()) == nullptr) return;
    pSndFile = pModDoc->GetSoundFile();
    bLeft = bTop = bRight = bBottom = true;
    x1 = (m_dwBeginSel & 0xFFF8) >> 3;
    y1 = (m_dwBeginSel) >> 16;
    x2 = (m_dwEndSel & 0xFFF8) >> 3;
    y2 = (m_dwEndSel) >> 16;
    c1 = (m_dwBeginSel&7);
    c2 = (m_dwEndSel&7);
    dx = (int)((m_dwDragPos & 0xFFF8) >> 3) - (int)((m_dwStartSel & 0xFFF8) >> 3);
    dy = (int)(m_dwDragPos >> 16) - (int)(m_dwStartSel >> 16);
    x1 += dx;
    x2 += dx;
    y1 += dy;
    y2 += dy;
    nChannels = pSndFile->m_nChannels;
    nRows = pSndFile->Patterns[m_nPattern].GetNumRows();
    if (x1 < GetXScrollPos()) bLeft = false;
    if (x1 >= nChannels) x1 = nChannels - 1;
    if (x1 < 0) { x1 = 0; c1 = 0; bLeft = false; }
    if (x2 >= nChannels) { x2 = nChannels-1; c2 = 4; bRight = false; }
    if (x2 < 0) x2 = 0;
    if (y1 < GetYScrollPos() - (int)m_nMidRow) bTop = false;
    if (y1 >= nRows) y1 = nRows-1;
    if (y1 < 0) { y1 = 0; bTop = false; }
    if (y2 >= nRows) { y2 = nRows-1; bBottom = false; }
    if (y2 < 0) y2 = 0;
    dwTopLeft = (y1<<16)|(x1<<3)|c1;
    dwBottomRight = ((y2+1)<<16)|(x2<<3)|(c2+1);
    ptTopLeft = GetPointFromPosition(dwTopLeft);
    ptBottomRight = GetPointFromPosition(dwBottomRight);
    if ((ptTopLeft.x >= ptBottomRight.x) || (ptTopLeft.y >= ptBottomRight.y)) return;
    // invert the brush pattern (looks just like frame window sizing)
    CBrush* pBrush = CDC::GetHalftoneBrush();
    if (pBrush != NULL)
    {
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, pBrush->m_hObject);
        // Top
        if (bTop)
        {
            rect.SetRect(ptTopLeft.x+4, ptTopLeft.y, ptBottomRight.x, ptTopLeft.y+4);
            if (!bLeft) rect.left -= 4;
            PatBlt(hdc, rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);
        }
        // Bottom
        if (bBottom)
        {
            rect.SetRect(ptTopLeft.x, ptBottomRight.y-4, ptBottomRight.x-4, ptBottomRight.y);
            if (!bRight) rect.right += 4;
            PatBlt(hdc, rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);
        }
        // Left
        if (bLeft)
        {
            rect.SetRect(ptTopLeft.x, ptTopLeft.y, ptTopLeft.x+4, ptBottomRight.y-4);
            if (!bBottom) rect.bottom += 4;
            PatBlt(hdc, rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);
        }
        // Right
        if (bRight)
        {
            rect.SetRect(ptBottomRight.x-4, ptTopLeft.y+4, ptBottomRight.x, ptBottomRight.y);
            if (!bTop) rect.top -= 4;
            PatBlt(hdc, rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);
        }
        if (hOldBrush != NULL) SelectObject(hdc, hOldBrush);
    }

}


void CViewPattern::OnDrawDragSel()
//--------------------------------
{
    HDC hdc = ::GetDC(m_hWnd);
    if (hdc != NULL)
    {
        DrawDragSel(hdc);
        ::ReleaseDC(m_hWnd, hdc);
    }
}


///////////////////////////////////////////////////////////////////////////////
// CViewPattern Scrolling Functions


void CViewPattern::UpdateScrollSize()
//-----------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        CRect rect;
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        SIZE sizeTotal, sizePage, sizeLine;
        sizeTotal.cx = m_szHeader.cx + pSndFile->m_nChannels * m_szCell.cx;
        sizeTotal.cy = m_szHeader.cy + pSndFile->Patterns[m_nPattern].GetNumRows() * m_szCell.cy;
        sizeLine.cx = m_szCell.cx;
        sizeLine.cy = m_szCell.cy;
        sizePage.cx = sizeLine.cx * 2;
        sizePage.cy = sizeLine.cy * 8;
        GetClientRect(&rect);
        m_nMidRow = 0;
        if (CMainFrame::m_dwPatternSetup & PATTERN_CENTERROW) m_nMidRow = (rect.Height() - m_szHeader.cy) / (m_szCell.cy << 1);
        if (m_nMidRow) sizeTotal.cy += m_nMidRow * m_szCell.cy * 2;
        SetScrollSizes(MM_TEXT, sizeTotal, sizePage, sizeLine);
        //UpdateScrollPos(); //rewbs.FixLPsOddScrollingIssue
        if (rect.Height() >= sizeTotal.cy)
        {
            m_bWholePatternFitsOnScreen = true;
            m_nYScroll = 0;  //rewbs.fix2977
        } else
        {
            m_bWholePatternFitsOnScreen = false;
        }
    }
}


void CViewPattern::UpdateScrollPos()
//----------------------------------
{
    CRect rect;
    GetClientRect(&rect);

    int x = GetScrollPos(SB_HORZ);
    if (x < 0) x = 0;
    m_nXScroll = (x + m_szCell.cx - 1) / m_szCell.cx;
    int y = GetScrollPos(SB_VERT);
    if (y < 0) y = 0;
    m_nYScroll = (y + m_szCell.cy - 1) / m_szCell.cy;

}


BOOL CViewPattern::OnScrollBy(CSize sizeScroll, BOOL bDoScroll)
//-------------------------------------------------------------
{
    int xOrig, xNew, x;
    int yOrig, yNew, y;

    // don't scroll if there is no valid scroll range (ie. no scroll bar)
    CScrollBar* pBar;
    uint32_t dwStyle = GetStyle();
    pBar = GetScrollBarCtrl(SB_VERT);
    if ((pBar != NULL && !pBar->IsWindowEnabled()) ||
        (pBar == NULL && !(dwStyle & WS_VSCROLL)))
    {
        // vertical scroll bar not enabled
        sizeScroll.cy = 0;
    }
    pBar = GetScrollBarCtrl(SB_HORZ);
    if ((pBar != NULL && !pBar->IsWindowEnabled()) ||
        (pBar == NULL && !(dwStyle & WS_HSCROLL)))
    {
        // horizontal scroll bar not enabled
        sizeScroll.cx = 0;
    }

    // adjust current x position
    xOrig = x = GetScrollPos(SB_HORZ);
    int xMax = GetScrollLimit(SB_HORZ);
    x += sizeScroll.cx;
    if (x < 0) x = 0; else if (x > xMax) x = xMax;

    // adjust current y position
    yOrig = y = GetScrollPos(SB_VERT);
    int yMax = GetScrollLimit(SB_VERT);
    y += sizeScroll.cy;
    if (y < 0) y = 0; else if (y > yMax) y = yMax;

    // did anything change?
    if (x == xOrig && y == yOrig) return FALSE;

    if (!bDoScroll) return TRUE;
    xNew = x;
    yNew = y;

    if (x > 0) x = (x + m_szCell.cx - 1) / m_szCell.cx; else x = 0;
    if (y > 0) y = (y + m_szCell.cy - 1) / m_szCell.cy; else y = 0;
    if ((x != m_nXScroll) || (y != m_nYScroll))
    {
        CRect rect;
        GetClientRect(&rect);
        if (x != m_nXScroll)
        {
            rect.left = m_szHeader.cx;
            rect.top = 0;
            ScrollWindow((m_nXScroll-x)*GetColumnWidth(), 0, &rect, &rect);
            m_nXScroll = x;
        }
        if (y != m_nYScroll)
        {
            rect.left = 0;
            rect.top = m_szHeader.cy;
            ScrollWindow(0, (m_nYScroll-y)*GetColumnHeight(), &rect, &rect);
            m_nYScroll = y;
        }
    }
    if (xNew != xOrig) SetScrollPos(SB_HORZ, xNew);
    if (yNew != yOrig) SetScrollPos(SB_VERT, yNew);
    return TRUE;
}


void CViewPattern::OnSize(UINT nType, int cx, int cy)
//---------------------------------------------------
{
    CScrollView::OnSize(nType, cx, cy);
    if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0))
    {
        UpdateSizes();
        UpdateScrollSize();
        OnScroll(0,0,TRUE);
    }
}


void CViewPattern::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
//---------------------------------------------------------------------------
{
    if (nSBCode == (SB_THUMBTRACK|SB_THUMBPOSITION)) m_dwStatus |= PATSTATUS_DRAGVSCROLL;
    CModScrollView::OnVScroll(nSBCode, nPos, pScrollBar);
    if (nSBCode == SB_ENDSCROLL) m_dwStatus &= ~PATSTATUS_DRAGVSCROLL;
}


void CViewPattern::SetCurSel(uint32_t dwBegin, uint32_t dwEnd)
//------------------------------------------------------
{
    RECT rect1, rect2, rect, rcInt, rcUni;
    POINT pt;
    int x1, y1, x2, y2;

    x1 = dwBegin & 0xFFFF;
    y1 = dwBegin >> 16;
    x2 = dwEnd & 0xFFFF;
    y2 = dwEnd >> 16;
    if (x1 > x2)
    {
        int x = x2;
        x2 = x1;
        x1 = x;
    }
    if (y1 > y2)
    {
        int y = y2;
        y2 = y1;
        y1 = y;
    }
    // rewbs.fix3417: adding error checking
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        if (pSndFile)
        {
            y1 = bad_max(y1, 0);
            y2 = bad_min(y2, (int)pSndFile->Patterns[m_nPattern].GetNumRows() - 1);
            x1 = bad_max(x1, 0);
            x2 = bad_min(x2, pSndFile->GetNumChannels() * 8 - (8 - LAST_COLUMN));
        }
    }
    // end rewbs.fix3417


    // Get current selection area
    pt = GetPointFromPosition(m_dwBeginSel);
    rect1.left = pt.x;
    rect1.top = pt.y;
    pt = GetPointFromPosition(m_dwEndSel + 0x10001);
    rect1.right = pt.x;
    rect1.bottom = pt.y;
    if (rect1.left < m_szHeader.cx) rect1.left = m_szHeader.cx;
    if (rect1.top < m_szHeader.cy) rect1.top = m_szHeader.cy;
    // Get new selection area
    m_dwBeginSel = (y1 << 16) | (x1);
    m_dwEndSel = (y2 << 16) | (x2);
    pt = GetPointFromPosition(m_dwBeginSel);
    rect2.left = pt.x;
    rect2.top = pt.y;
    pt = GetPointFromPosition(m_dwEndSel + 0x10001);
    rect2.right = pt.x;
    rect2.bottom = pt.y;
    if (rect2.left < m_szHeader.cx) rect2.left = m_szHeader.cx;
    if (rect2.top < m_szHeader.cy) rect2.top = m_szHeader.cy;
    IntersectRect(&rcInt, &rect1, &rect2);
    UnionRect(&rcUni, &rect1, &rect2);
    SubtractRect(&rect, &rcUni, &rcInt);
    if ((rect.left == rcUni.left) && (rect.top == rcUni.top)
     && (rect.right == rcUni.right) && (rect.bottom == rcUni.bottom))
    {
        InvalidateRect(&rect1, FALSE);
        InvalidateRect(&rect2, FALSE);
    } else
    {
        InvalidateRect(&rect, FALSE);
    }
}


void CViewPattern::InvalidatePattern(BOOL bHdr)
//---------------------------------------------
{
    CRect rect;
    GetClientRect(&rect);
    /*
    if (!bHdr)
    {
        rect.left += m_szHeader.cx;
        rect.top += m_szHeader.cy;
    }
    */
    //XXXih: turkish
    InvalidateRect(&rect, TRUE);
}


void CViewPattern::InvalidateRow(int n)
//-------------------------------------
{
    /*
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        int yofs = GetYScrollPos() - m_nMidRow;
        if (n == -1) n = m_nRow;
        if ((n < yofs) || (n >= (int)pSndFile->Patterns[m_nPattern].GetNumRows())) return;
        CRect rect;
        GetClientRect(&rect);
        rect.left = m_szHeader.cx;
        rect.top = m_szHeader.cy;
        rect.top += (n - yofs) * m_szCell.cy;
        rect.bottom = rect.top + m_szCell.cy;
        InvalidateRect(&rect, FALSE);
    }
    */

    CRect rect;
    GetClientRect(&rect);
    //XXXih: turkish
    InvalidateRect(&rect, TRUE);

}


void CViewPattern::InvalidateArea(uint32_t dwBegin, uint32_t dwEnd)
//-----------------------------------------------------------
{
    /*
    RECT rect;
    POINT pt;
    pt = GetPointFromPosition(dwBegin);
    rect.left = pt.x;
    rect.top = pt.y;
    pt = GetPointFromPosition(dwEnd + 0x10001);
    rect.right = pt.x;
    rect.bottom = pt.y;
    InvalidateRect(&rect, FALSE);
    */

    CRect rect;
    GetClientRect(&rect);
    //XXXih: turkish
    InvalidateRect(&rect, TRUE);
}


void CViewPattern::InvalidateChannelsHeaders()
//--------------------------------------------
{
    /*
    CRect rect;
    GetClientRect(&rect);
    rect.bottom = rect.top + m_szHeader.cy;
    InvalidateRect(&rect, FALSE);
    */

    CRect rect;
    GetClientRect(&rect);
    //XXXih: turkish
    InvalidateRect(&rect, TRUE);
}


void CViewPattern::UpdateIndicator()
//----------------------------------
{
    CModDoc *pModDoc = GetDocument();
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    if ((pMainFrm) && (pModDoc))
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        CHAR s[512];
        UINT nChn;
        wsprintf(s, "Row %d, Col %d", GetCurrentRow(), GetCurrentChannel() + 1);
        pMainFrm->SetUserText(s);
        if (::GetFocus() == m_hWnd)
        {
            nChn = GetChanFromCursor(m_dwCursor);
            s[0] = 0;
            if ((!(m_dwStatus & (PATSTATUS_KEYDRAGSEL/*|PATSTATUS_MOUSEDRAGSEL*/))) //rewbs.xinfo: update indicator even when dragging
             && (m_dwBeginSel == m_dwEndSel) && (pSndFile->Patterns[m_nPattern])
             && (m_nRow < pSndFile->Patterns[m_nPattern].GetNumRows()) && (nChn < pSndFile->m_nChannels))
            {
                modplug::tracker::modevent_t *m = &pSndFile->Patterns[m_nPattern][m_nRow*pSndFile->m_nChannels+nChn];

                switch (GetColTypeFromCursor(m_dwCursor))
                {
                case NOTE_COLUMN:
                    // display note
                    if(m->note >= NoteMinSpecial)
                        strcpy(s, szSpecialNoteShortDesc[m->note - NoteMinSpecial]);
                    break;
                case INST_COLUMN:
                    // display instrument
                    if (m->instr)
                    {
                        CHAR sztmp[128] = "";
                        if(m->IsPcNote())
                        {
                            // display plugin name.
                            if(m->instr <= MAX_MIXPLUGINS)
                            {
                                strncpy(sztmp, pSndFile->m_MixPlugins[m->instr - 1].GetName(), sizeof(sztmp));
                                SetNullTerminator(sztmp);
                            }
                        } else
                        {
                            // "normal" instrument
                            if (pSndFile->GetNumInstruments())
                            {
                                if ((m->instr <= pSndFile->GetNumInstruments()) && (pSndFile->Instruments[m->instr]))
                                {
                                    modplug::tracker::modinstrument_t *pIns = pSndFile->Instruments[m->instr];
                                    memcpy(sztmp, pIns->name, 32);
                                    sztmp[32] = 0;
                                    if ((m->note) && (m->note <= NoteMax))
                                    {
                                        UINT nsmp = pIns->Keyboard[m->note-1];
                                        if ((nsmp) && (nsmp <= pSndFile->GetNumSamples()))
                                        {
                                            CHAR sztmp2[64] = "";
                                            memcpy(sztmp2, pSndFile->m_szNames[nsmp], MAX_SAMPLENAME);
                                            sztmp2[32] = 0;
                                            if (sztmp2[0])
                                            {
                                                wsprintf(sztmp+strlen(sztmp), " (%d: %s)", nsmp, sztmp2);
                                            }
                                        }
                                    }
                                }
                            } else
                            {
                                if (m->instr <= pSndFile->GetNumSamples())
                                {
                                    memcpy(sztmp, pSndFile->m_szNames[m->instr], MAX_SAMPLENAME);
                                    sztmp[32] = 0;
                                }
                            }

                        }
                        if (sztmp[0]) wsprintf(s, "%d: %s", m->instr, sztmp);
                    }
                    break;
                case VOL_COLUMN:
                    // display volume command
                    if(m->IsPcNote())
                    {
                        // display plugin param name.
                        if(m->instr > 0 && m->instr <= MAX_MIXPLUGINS)
                        {
                            CHAR sztmp[128] = "";
                            strncpy(sztmp, pSndFile->m_MixPlugins[m->instr - 1].GetParamName(m->GetValueVolCol()), sizeof(sztmp));
                            SetNullTerminator(sztmp);
                            if (sztmp[0]) wsprintf(s, "%d: %s", m->GetValueVolCol(), sztmp);
                        }
                    } else
                    {
                        // "normal" volume command
                        if (!pModDoc->GetVolCmdInfo(pModDoc->GetIndexFromVolCmd(m->volcmd), s)) s[0] = 0;
                    }
                    break;
                case EFFECT_COLUMN:
                case PARAM_COLUMN:
                    // display effect command
                    if(!m->IsPcNote())
                    {
                        if (!pModDoc->GetEffectName(s, m->command, m->param, false, nChn)) s[0] = 0;
                    }
                    break;
                }
            }
            pMainFrm->SetInfoText(s);
            UpdateXInfoText();            //rewbs.xinfo
        }
    }

}

//rewbs.xinfo
void CViewPattern::UpdateXInfoText()
//----------------------------------
{
    UINT nChn = GetCurrentChannel();
    CString xtraInfo;

    CModDoc *pModDoc = GetDocument();
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    if ((pMainFrm) && (pModDoc))
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        if (!pSndFile) return;

        //xtraInfo.Format("Chan: %d; macro: %X; cutoff: %X; reso: %X; pan: %X",
        xtraInfo.Format("Chn:%d; Vol:%X; Mac:%X; Cut:%X%s; Res:%X; Pan:%X%s",
                        nChn+1,
                        pSndFile->Chn[nChn].nGlobalVol,
                        pSndFile->Chn[nChn].nActiveMacro,
                        pSndFile->Chn[nChn].nCutOff,
                        (pSndFile->Chn[nChn].nFilterMode == FLTMODE_HIGHPASS) ? "-Hi" : "",
                        pSndFile->Chn[nChn].nResonance,
                        pSndFile->Chn[nChn].nPan,
                        bitset_is_set(pSndFile->Chn[nChn].flags, vflag_ty::Surround) ? "-S" : "");

        pMainFrm->SetXInfoText(xtraInfo);
    }

    return;
}
//end rewbs.xinfo

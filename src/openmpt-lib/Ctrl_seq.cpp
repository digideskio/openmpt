#include "stdafx.h"
#include "mainfrm.h"
#include "moddoc.h"
#include "globals.h"
#include "ctrl_pat.h"
#include "view_pat.h"

//////////////////////////////////////////////////////////////
// CPatEdit

BOOL CPatEdit::PreTranslateMessage(MSG *pMsg)
//-------------------------------------------
{
    if (((pMsg->message == WM_KEYDOWN) || (pMsg->message == WM_KEYUP)) && (pMsg->wParam == VK_TAB))
    {
            if ((pMsg->message == WM_KEYUP) && (m_pParent))
            {
                    m_pParent->SwitchToView();
            }
            return TRUE;
    }
    return CEdit::PreTranslateMessage(pMsg);
}


//////////////////////////////////////////////////////////////
// COrderList

BEGIN_MESSAGE_MAP(COrderList, CWnd)
    //{{AFX_MSG_MAP(COrderList)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_MBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_HSCROLL()
    ON_WM_SIZE()
    ON_COMMAND(ID_CONTROLTAB,                        OnSwitchToView)

    ON_COMMAND(ID_ORDERLIST_INSERT,                OnInsertOrder)
    ON_COMMAND(ID_ORDERLIST_DELETE,                OnDeleteOrder)
    ON_COMMAND(ID_ORDERLIST_RENDER,                OnRenderOrder)
    ON_COMMAND(ID_ORDERLIST_EDIT_COPY,        OnEditCopy)
    ON_COMMAND(ID_ORDERLIST_EDIT_CUT,        OnEditCut)
    ON_COMMAND(ID_ORDERLIST_EDIT_PASTE,        OnEditPaste)

    ON_COMMAND(ID_PATTERN_PROPERTIES,        OnPatternProperties)
    ON_COMMAND(ID_PLAYER_PLAY,                        OnPlayerPlay)
    ON_COMMAND(ID_PLAYER_PAUSE,                        OnPlayerPause)
    ON_COMMAND(ID_PLAYER_PLAYFROMSTART,        OnPlayerPlayFromStart)
    ON_COMMAND(IDC_PATTERN_PLAYFROMSTART,OnPatternPlayFromStart)
    //ON_COMMAND(ID_PATTERN_RESTART,                OnPatternPlayFromStart)
    ON_COMMAND(ID_ORDERLIST_NEW,                OnCreateNewPattern)
    ON_COMMAND(ID_ORDERLIST_COPY,                OnDuplicatePattern)
    ON_COMMAND(ID_PATTERNCOPY,                        OnPatternCopy)
    ON_COMMAND(ID_PATTERNPASTE,                        OnPatternPaste)
    ON_COMMAND_RANGE(ID_SEQUENCE_ITEM, ID_SEQUENCE_ITEM + MAX_SEQUENCES + 2, OnSelectSequence)
    ON_MESSAGE(WM_MOD_DRAGONDROPPING,        OnDragonDropping)
    ON_MESSAGE(WM_HELPHITTEST,                        OnHelpHitTest)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


uint8_t COrderList::s_nDefaultMargins = 0;

bool COrderList::IsOrderInMargins(int order, int startOrder)
//----------------------------------------------------------
{
    const uint8_t nMargins = GetMargins();
    return ((startOrder != 0 && order - startOrder < nMargins) ||
                order - startOrder >= GetLength() - nMargins);
}


void COrderList::EnsureVisible(modplug::tracker::orderindex_t order)
//----------------------------------------------
{
    // nothing needs to be done
    if(!IsOrderInMargins(order, m_nXScroll) || order == modplug::tracker::OrderIndexInvalid) return;

    if(order < m_nXScroll)
    {
            if(order < GetMargins())
                    m_nXScroll = 0;
            else
                    m_nXScroll = order - GetMargins();
    } else
    {
            m_nXScroll = order + 2 * GetMargins() - 1;
            if(m_nXScroll < GetLength())
                    m_nXScroll = 0;
            else
                    m_nXScroll -= GetLength();
    }
}


COrderList::COrderList()
//----------------------
{
    m_hFont = NULL;
    m_pParent = nullptr;
    m_cxFont = m_cyFont = 0;
    m_pModDoc = nullptr;
    m_nScrollPos = m_nXScroll = 0;
    m_nScrollPos2nd = modplug::tracker::OrderIndexInvalid;
    m_nOrderlistMargins = s_nDefaultMargins;
    m_bScrolling = false;
    m_bDragging = false;
}


modplug::tracker::orderindex_t COrderList::GetOrderFromPoint(const CRect& rect, const CPoint& pt) const
//---------------------------------------------------------------------------------
{
    return static_cast<modplug::tracker::orderindex_t>(m_nXScroll + (pt.x - rect.left) / m_cxFont);
}


BOOL COrderList::Init(const CRect &rect, CCtrlPatterns *pParent, CModDoc *pModDoc, HFONT hFont)
//---------------------------------------------------------------------------------------------
{
    CreateEx(WS_EX_STATICEDGE, NULL, "", WS_CHILD|WS_VISIBLE, rect, pParent, IDC_ORDERLIST);
    m_pParent = pParent;
    m_pModDoc = pModDoc;
    m_hFont = hFont;
    colorText = GetSysColor(COLOR_WINDOWTEXT);
    colorTextSel = GetSysColor(COLOR_HIGHLIGHTTEXT);
    SendMessage(WM_SETFONT, (WPARAM)m_hFont);
    SetScrollPos(SB_HORZ, 0);
    EnableScrollBarCtrl(SB_HORZ, TRUE);
    return TRUE;
}


BOOL COrderList::UpdateScrollInfo()
//---------------------------------
{
    CRect rcClient;

    GetClientRect(&rcClient);
    if ((m_pModDoc) && (m_cxFont > 0) && (rcClient.right > 0))
    {
            CRect rect;
            module_renderer *pSndFile = m_pModDoc->GetSoundFile();
            SCROLLINFO info;
            UINT nPage;

            int nMax = 0;
            if(pSndFile->GetType() == MOD_TYPE_MOD)
            {   // With MOD, cut shown sequence to first '---' item...
                    nMax = pSndFile->Order.GetLengthFirstEmpty();
            }
            else
            {   // ...for S3M/IT/MPT/XM, show sequence until the last used item.
                    nMax = pSndFile->Order.GetLengthTailTrimmed();
            }

            GetScrollInfo(SB_HORZ, &info, SIF_PAGE|SIF_RANGE);
            info.fMask = SIF_PAGE|SIF_RANGE;
            info.nMin = 0;
            nPage = rcClient.right / m_cxFont;
            if (nMax <= (int)nPage) nMax = nPage + 1;
            if ((nMax != info.nMax) || (nPage != info.nPage))
            {
                    info.nPage = nPage;
                    info.nMax = nMax;
                    SetScrollInfo(SB_HORZ, &info, TRUE);
            }
    }
    return FALSE;
}


int COrderList::GetFontWidth()
//----------------------------
{
    if ((m_cxFont <= 0) && (m_hWnd) && (m_hFont))
    {
            CClientDC dc(this);
            HGDIOBJ oldfont = ::SelectObject(dc.m_hDC, m_hFont);
            CSize sz = dc.GetTextExtent("000+", 4);
            if (oldfont) ::SelectObject(dc.m_hDC, oldfont);
            return sz.cx;
    }
    return m_cxFont;
}


void COrderList::InvalidateSelection() const
//------------------------------------------
{
    modplug::tracker::orderindex_t nOrdLo = m_nScrollPos, nCount = 1;
    static modplug::tracker::orderindex_t m_nScrollPos2Old = m_nScrollPos2nd;
    if(m_nScrollPos2Old != modplug::tracker::OrderIndexInvalid)
    {
            // there were multiple orders selected - remove them all
            modplug::tracker::orderindex_t nOrdHi = m_nScrollPos;
            if(m_nScrollPos2Old < m_nScrollPos)
            {
                    nOrdLo = m_nScrollPos2Old;
            } else
            {
                    nOrdHi = m_nScrollPos2Old;
            }
            nCount = nOrdHi - nOrdLo + 1;
    }
    m_nScrollPos2Old = m_nScrollPos2nd;
    CRect rcClient, rect;
    GetClientRect(&rcClient);
    rect.left = rcClient.left + (nOrdLo - m_nXScroll) * m_cxFont;
    rect.top = rcClient.top;
    rect.right = rect.left + m_cxFont * nCount;
    rect.bottom = rcClient.bottom;
    if (rect.right > rcClient.right) rect.right = rcClient.right;
    if (rect.left < rcClient.left) rect.left = rcClient.left;
    if (rect.right > rect.left) ::InvalidateRect(m_hWnd, &rect, FALSE);
}


uint8_t COrderList::GetLength()
//--------------------------
{
    CRect rcClient;
    GetClientRect(&rcClient);
    if(m_cxFont > 0)
            return static_cast<uint8_t>(rcClient.right / m_cxFont);
    else
    {
            const int nFontWidth = GetFontWidth();
            return (nFontWidth > 0) ? static_cast<uint8_t>(rcClient.right / nFontWidth) : 0;
    }
}


ORD_SELECTION COrderList::GetCurSel(bool bIgnoreSelection) const
//--------------------------------------------------------------
{
    // returns the currently selected order(s)
    ORD_SELECTION result;
    result.nOrdLo = result.nOrdHi = m_nScrollPos;
    // bIgnoreSelection: true if only first selection marker is important.
    if(!bIgnoreSelection && m_nScrollPos2nd != modplug::tracker::OrderIndexInvalid) {
            if(m_nScrollPos2nd < m_nScrollPos) // ord2 < ord1
                    result.nOrdLo = m_nScrollPos2nd;
            else
                    result.nOrdHi = m_nScrollPos2nd;
    }
    LimitMax(result.nOrdLo, m_pModDoc->GetSoundFile()->Order.GetLastIndex());
    LimitMax(result.nOrdHi, m_pModDoc->GetSoundFile()->Order.GetLastIndex());
    return result;
}


bool COrderList::SetCurSel(modplug::tracker::orderindex_t sel, bool bEdit, bool bShiftClick, bool bIgnoreCurSel)
//------------------------------------------------------------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
    modplug::tracker::orderindex_t *nOrder = (bShiftClick) ? &m_nScrollPos2nd : &m_nScrollPos;

    if ((sel < 0) || (sel >= pSndFile->Order.GetLength()) || (!m_pParent) || (!pMainFrm)) return false;
    if (!bIgnoreCurSel && sel == *nOrder) return true;
    const uint8_t nShownLength = GetLength();
    InvalidateSelection();
    *nOrder = sel;
    if (!m_bScrolling)
    {
            const uint8_t nMargins = GetMargins(GetMarginsMax(nShownLength));
            if ((*nOrder < m_nXScroll + nMargins) || (!m_cxFont) || (!m_cyFont))
            {   // Must move first shown sequence item to left in order to show
                    // the new active order.
                    m_nXScroll = bad_max(0, *nOrder - nMargins);
                    SetScrollPos(SB_HORZ, m_nXScroll);
                    InvalidateRect(NULL, FALSE);
            } else
            {
                    modplug::tracker::orderindex_t maxsel = nShownLength;
                    if (maxsel) maxsel--;
                    if (*nOrder - m_nXScroll >= maxsel - nMargins)
                    {   // Must move first shown sequence item to right in order to show
                            // the new active order.
                            m_nXScroll = *nOrder - (maxsel - nMargins);
                            SetScrollPos(SB_HORZ, m_nXScroll);
                            InvalidateRect(NULL, FALSE);
                    }
            }
    }
    InvalidateSelection();
    if ((m_pParent) && (m_pModDoc) && (bEdit))
    {
            modplug::tracker::patternindex_t n = pSndFile->Order[m_nScrollPos];
            if ((n < pSndFile->Patterns.Size()) && (pSndFile->Patterns[n]) && !bShiftClick)
            {
                    bool bIsPlaying = (pMainFrm->GetModPlaying() == m_pModDoc);
                    if ((bIsPlaying) && (pSndFile->m_dwSongFlags & SONG_PATTERNLOOP))
                    {
                            BEGIN_CRITICAL();
                            // update channel parameters and play time
                            m_pModDoc->SetElapsedTime(m_nScrollPos, 0);

                            pSndFile->m_nPattern = n;
                            pSndFile->m_nCurrentPattern = pSndFile->m_nNextPattern = m_nScrollPos;
                            pMainFrm->ResetNotificationBuffer(); //rewbs.toCheck
                            pSndFile->m_nNextRow = 0;
                            END_CRITICAL();
                    } else if (m_pParent->GetFollowSong())
                    {
                            BEGIN_CRITICAL();
                            uint32_t dwPaused = pSndFile->m_dwSongFlags & (SONG_PAUSED|SONG_STEP|SONG_PATTERNLOOP);

                            //if (!(dwPaused & SONG_PATTERNLOOP))        // why?
                            // update channel parameters and play time
                            m_pModDoc->SetElapsedTime(m_nScrollPos, 0);

                            pSndFile->m_nCurrentPattern = m_nScrollPos;
                            pSndFile->SetCurrentOrder(m_nScrollPos);
                            pSndFile->m_dwSongFlags |= dwPaused;
                            if (bIsPlaying) pMainFrm->ResetNotificationBuffer();
                            END_CRITICAL();
                    }
                    m_pParent->SetCurrentPattern(n);
            }
    }
    UpdateInfoText();
    if(m_nScrollPos == m_nScrollPos2nd) m_nScrollPos2nd = modplug::tracker::OrderIndexInvalid;
    return true;
}


UINT COrderList::GetCurrentPattern() const
//----------------------------------------
{
    module_renderer* pSndFile = m_pModDoc ? m_pModDoc->GetSoundFile() : NULL;
    if ((pSndFile) && (m_nScrollPos < pSndFile->Order.GetLength()))
    {
            return pSndFile->Order[m_nScrollPos];
    }
    return 0;
}


// Helper function to enter pattern index into the orderlist.
// Call with param 0...9 (enter digit), 10 (decrease) or 11 (increase).
void COrderList::EnterPatternNum(int enterNum)
//--------------------------------------------
{
    if (m_pModDoc == nullptr) return;
    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
    if(pSndFile == nullptr) return;

    modplug::tracker::patternindex_t nCurNdx = (m_nScrollPos < pSndFile->Order.GetLength()) ? pSndFile->Order[m_nScrollPos] : pSndFile->Order.GetInvalidPatIndex();
    modplug::tracker::patternindex_t nMaxNdx = 0;
    for(modplug::tracker::patternindex_t nPat = 0; nPat < pSndFile->Patterns.Size(); nPat++)
            if (pSndFile->Patterns.IsValidPat(nPat)) nMaxNdx = nPat;

    if (enterNum >= 0 && enterNum <= 9) // enter 0...9
    {
            if (nCurNdx >= pSndFile->Patterns.Size()) nCurNdx = 0;

            nCurNdx = nCurNdx * 10 + static_cast<modplug::tracker::patternindex_t>(enterNum);
            static_assert(MAX_PATTERNS < 10000, "HAUAUAUAUAU");
            if ((nCurNdx >= 1000) && (nCurNdx > nMaxNdx)) nCurNdx %= 1000;
            if ((nCurNdx >= 100) && (nCurNdx > nMaxNdx)) nCurNdx %= 100;
            if ((nCurNdx >= 10) && (nCurNdx > nMaxNdx)) nCurNdx %= 10;
    } else if (enterNum == 10) // decrease pattern index
    {
            const modplug::tracker::patternindex_t nFirstInvalid = pSndFile->GetModSpecifications().hasIgnoreIndex ? pSndFile->Order.GetIgnoreIndex() : pSndFile->Order.GetInvalidPatIndex();
            if (nCurNdx == 0)
                    nCurNdx = pSndFile->Order.GetInvalidPatIndex();
            else
            {
                    nCurNdx--;
                    if ((nCurNdx > nMaxNdx) && (nCurNdx < nFirstInvalid)) nCurNdx = nMaxNdx;
            }
    } else if (enterNum == 11) // increase pattern index
    {
            if(nCurNdx >= pSndFile->Order.GetInvalidPatIndex())
            {
                    nCurNdx = 0;
            }
            else
            {
                    nCurNdx++;
                    const modplug::tracker::patternindex_t nFirstInvalid = pSndFile->GetModSpecifications().hasIgnoreIndex ? pSndFile->Order.GetIgnoreIndex() : pSndFile->Order.GetInvalidPatIndex();
                    if(nCurNdx > nMaxNdx && nCurNdx < nFirstInvalid)
                            nCurNdx = nFirstInvalid;
            }
    } else if (enterNum == 12) // ignore index (+++)
    {
            if (pSndFile->GetModSpecifications().hasIgnoreIndex)
            {
                    nCurNdx = pSndFile->Order.GetIgnoreIndex();
            }
    } else if (enterNum == 13) // invalid index (---)
    {
            nCurNdx = pSndFile->Order.GetInvalidPatIndex();
    }
    // apply
    if (nCurNdx != pSndFile->Order[m_nScrollPos])
    {
            pSndFile->Order[m_nScrollPos] = nCurNdx;
            m_pModDoc->SetModified();
            m_pModDoc->UpdateAllViews(NULL, HINT_MODSEQUENCE, this);
            InvalidateSelection();
    }
}


static const char szClipboardOrdersHdr[] = "OpenMPT %3s\r\n";
static const char szClipboardOrdCountFieldHdr[]    = "OrdNum: %u\r\n";
static const char szClipboardOrdersFieldHdr[]    = "OrdLst: ";


void COrderList::OnEditCut()
//--------------------------
{
    OnEditCopy();
    OnDeleteOrder();
}


void COrderList::OnEditPaste()
//----------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    module_renderer* pSf = m_pModDoc->GetSoundFile();
    if (!pMainFrm)
            return;
    BeginWaitCursor();
    if (pMainFrm->OpenClipboard())
    {
            HGLOBAL hCpy = ::GetClipboardData(CF_TEXT);
            LPCSTR p;

            if ((hCpy) && ((p = (LPCSTR)GlobalLock(hCpy)) != NULL))
            {
                    const uint32_t dwMemSize = GlobalSize(hCpy);

                    if (dwMemSize > sizeof(szClipboardOrdersHdr) &&
                            memcmp(p, "OpenMPT ", 8) == 0 &&
                            memcmp(p + 11, "\r\n", 2) == 0)
                    {
                            char buf[8];
                            p += sizeof(szClipboardOrdersHdr) - 1;
                            std::istrstream iStrm(p, dwMemSize - sizeof(szClipboardOrdersHdr) + 1);
                            modplug::tracker::orderindex_t nCount = 0;
                            std::vector<modplug::tracker::patternindex_t> vecPat;
                            while (iStrm.get(buf, sizeof(buf), '\n'))
                            {
                                    if (memcmp(buf, "OrdNum:", 8) == 0) // Read expected order count.
                                            iStrm >> nCount;
                                    else if (memcmp(buf, "OrdLst:", 8) != 0)
                                    {        // Unrecognized data -> skip line.
                                            iStrm.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                                            continue;
                                    }
                                    else // Read orders.
                                    {
                                            LimitMax(nCount, pSf->GetModSpecifications().ordersMax);
                                            vecPat.reserve(nCount);
                                            char bufItem[16];
                                            while (iStrm.peek() >= 32 && iStrm.getline(bufItem, sizeof(bufItem), ' '))
                                            {
                                                    if (vecPat.size() >= pSf->GetModSpecifications().ordersMax)
                                                            break;
                                                    if (!(isdigit(bufItem[0]) || bufItem[0] == '+' || bufItem[0] == '-'))
                                                            continue;
                                                    modplug::tracker::patternindex_t nPat = pSf->Order.GetInvalidPatIndex();
                                                    if (bufItem[0] == '+')
                                                    {
                                                            nPat = pSf->Order.GetIgnoreIndex();
                                                            if(!pSf->GetModSpecifications().hasIgnoreIndex) continue;
                                                    }
                                                    else if (isdigit(bufItem[0]))
                                                    {
                                                            nPat = ConvertStrTo<modplug::tracker::patternindex_t>(bufItem);
                                                            if (nPat >= pSf->GetModSpecifications().patternsMax)
                                                                    nPat = pSf->Order.GetInvalidPatIndex();
                                                    }
                                                    vecPat.push_back(nPat);
                                            }
                                            nCount = pSf->Order.Insert(m_nScrollPos, (modplug::tracker::orderindex_t)vecPat.size());
                                            for (modplug::tracker::orderindex_t nOrd = 0; nOrd < nCount; nOrd++)
                                                    pSf->Order[m_nScrollPos + nOrd] = vecPat[nOrd];
                                    }
                                    m_pModDoc->SetModified();
                                    m_pModDoc->UpdateAllViews(NULL, HINT_MODSEQUENCE, NULL);
                            }
                    }
                    GlobalUnlock(hCpy);
            }
            CloseClipboard();
    }
    EndWaitCursor();
}


void COrderList::OnEditCopy()
//---------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    if ((!pMainFrm)) return;

    const ORD_SELECTION ordsel = GetCurSel(false);

    uint32_t dwMemSize;
    HGLOBAL hCpy;

    BeginWaitCursor();
    dwMemSize = sizeof(szClipboardOrdersHdr) + sizeof(szClipboardOrdersFieldHdr) + sizeof(szClipboardOrdCountFieldHdr);
    dwMemSize += ordsel.GetSelCount() * 6 + 8;
    if ((pMainFrm->OpenClipboard()) && ((hCpy = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, dwMemSize))!=NULL))
    {
            LPCSTR pszFormatName;
            EmptyClipboard();
            switch(m_pModDoc->GetSoundFile()->GetType())
            {
                    case MOD_TYPE_S3M:        pszFormatName = "S3M"; break;
                    case MOD_TYPE_XM:        pszFormatName = "XM"; break;
                    case MOD_TYPE_IT:        pszFormatName = "IT"; break;
                    case MOD_TYPE_MPT:        pszFormatName = "MPT"; break;
                    default:                        pszFormatName = "MOD"; break;
            }
            LPSTR p = (LPSTR)GlobalLock(hCpy);
            if (p)
            {
                    const modplug::tracker::modsequence_t& seq = m_pModDoc->GetSoundFile()->Order;
                    wsprintf(p, szClipboardOrdersHdr, pszFormatName);
                    p += strlen(p);
                    wsprintf(p, szClipboardOrdCountFieldHdr, ordsel.GetSelCount());
                    strcat(p, szClipboardOrdersFieldHdr);
                    p += strlen(p);
                    for(modplug::tracker::orderindex_t i = ordsel.nOrdLo; i <= ordsel.nOrdHi; i++)
                    {
                            std::string str;
                            if (seq[i] == seq.GetInvalidPatIndex())
                                    str = "-";
                            else if (seq[i] == seq.GetIgnoreIndex())
                                    str = "+";
                            else
                                    str = Stringify(seq[i]);
                            memcpy(p, str.c_str(), str.size());
                            p += str.size();
                            *p++ = ' ';
                    }
                    *p++ = '\r';
                    *p++ = '\n';
                    *p = 0;
            }
            GlobalUnlock(hCpy);
            SetClipboardData(CF_TEXT, (HANDLE) hCpy);
            CloseClipboard();
    }
    EndWaitCursor();
}


void COrderList::UpdateView(uint32_t dwHintMask, CObject *pObj)
//----------------------------------------------------------
{
    if ((pObj != this) && (dwHintMask & HINT_MODSEQUENCE))
    {
            InvalidateRect(NULL, FALSE);
            UpdateInfoText();
    }
}


void COrderList::OnSwitchToView()
//-------------------------------
{
    if (m_pParent) m_pParent->PostViewMessage(VIEWMSG_SETFOCUS);
}


void COrderList::UpdateInfoText()
//-------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    module_renderer *pSndFile;
    if ((pMainFrm != nullptr) && (m_pModDoc != nullptr) && (pSndFile = m_pModDoc->GetSoundFile()) != nullptr && (::GetFocus() == m_hWnd))
    {
            CHAR s[128];
            strcpy(s, "");

            // MOD orderlist always ends after first empty pattern
            const modplug::tracker::orderindex_t nLength = (pSndFile->GetType() & MOD_TYPE_MOD) ? pSndFile->Order.GetLengthFirstEmpty() : pSndFile->Order.GetLengthTailTrimmed();

            if(CMainFrame::m_dwPatternSetup & PATTERN_HEXDISPLAY)
            {
                    wsprintf(s, "Position %02Xh of %02Xh", m_nScrollPos, nLength);
            }
            else
            {
                    wsprintf(s, "Position %d of %d (%02Xh of %02Xh)", m_nScrollPos, nLength, m_nScrollPos, nLength);
            }

            if (m_nScrollPos < pSndFile->Order.GetLength())
            {
                    modplug::tracker::patternindex_t nPat = pSndFile->Order[m_nScrollPos];
                    if (nPat < pSndFile->Patterns.Size())
                    {
                            CHAR szpat[MAX_PATTERNNAME] = "";
                            if (pSndFile->Patterns[nPat].GetName(szpat, MAX_PATTERNNAME) && strlen(szpat))
                            {
                                    wsprintf(s+strlen(s), ": %s", szpat);
                            }
                    }
            }
            pMainFrm->SetInfoText(s);
    }
}


/////////////////////////////////////////////////////////////////
// COrderList messages

void COrderList::OnPaint()
//------------------------
{
    CHAR s[64];
    CPaintDC dc(this);
    HGDIOBJ oldfont = ::SelectObject(dc.m_hDC, m_hFont);
    HGDIOBJ oldpen = ::SelectObject(dc.m_hDC, CMainFrame::penSeparator);
    // First time ?
    if ((m_cxFont <= 0) || (m_cyFont <= 0))
    {
            CSize sz = dc.GetTextExtent("000+", 4);
            m_cxFont = sz.cx;
            m_cyFont = sz.cy;
    }
    if ((m_cxFont > 0) && (m_cyFont > 0) && (m_pModDoc))
    {
            CRect rcClient, rect;

            UpdateScrollInfo();
            BOOL bFocus = (::GetFocus() == m_hWnd);
            dc.SetBkMode(TRANSPARENT);
            module_renderer *pSndFile = m_pModDoc->GetSoundFile();
            GetClientRect(&rcClient);
            rect = rcClient;
            modplug::tracker::orderindex_t nIndex = m_nXScroll;
            ORD_SELECTION selection = GetCurSel(false);

            //Scrolling the shown orders(the showns rectangles)?
            while (rect.left < rcClient.right)
            {
                    bool bHighLight = ((bFocus) && (nIndex >= selection.nOrdLo && nIndex <= selection.nOrdHi)) ? true : false;
                    const modplug::tracker::patternindex_t nPat = (nIndex < pSndFile->Order.GetLength()) ? pSndFile->Order[nIndex] : modplug::tracker::PatternIndexInvalid;
                    if ((rect.right = rect.left + m_cxFont) > rcClient.right) rect.right = rcClient.right;
                    rect.right--;
                    if (bHighLight) {
                            FillRect(dc.m_hDC, &rect, CMainFrame::brushHighLight);
                    } else {
                            FillRect(dc.m_hDC, &rect, CMainFrame::brushWindow);
                    }


                    //Drawing the shown pattern-indicator or drag position.
                    if (nIndex == ((m_bDragging) ? m_nDropPos : m_nScrollPos))
                    {
                            rect.InflateRect(-1, -1);
                            dc.DrawFocusRect(&rect);
                            rect.InflateRect(1, 1);
                    }
                    MoveToEx(dc.m_hDC, rect.right, rect.top, NULL);
                    LineTo(dc.m_hDC, rect.right, rect.bottom);
                    //Drawing the 'ctrl-transition' indicator
                    if (nIndex == pSndFile->m_nSeqOverride-1)
                    {
                            MoveToEx(dc.m_hDC, rect.left+4, rect.bottom-4, NULL);
                            LineTo(dc.m_hDC, rect.right-4, rect.bottom-4);
                    }

            CMainFrame *pMainFrm = CMainFrame::GetMainFrame();

            //Drawing 'playing'-indicator.
                    if(nIndex == pSndFile->GetCurrentOrder() && pMainFrm->IsPlaying() )
                    {
                            MoveToEx(dc.m_hDC, rect.left+4, rect.top+2, NULL);
                            LineTo(dc.m_hDC, rect.right-4, rect.top+2);
                    }
                    s[0] = 0;
                    if ((nIndex < pSndFile->Order.GetLength()) && (rect.left + m_cxFont - 4 <= rcClient.right))
                    {
                            if (nPat == pSndFile->Order.GetInvalidPatIndex()) strcpy(s, "---");
                            else if (nPat == pSndFile->Order.GetIgnoreIndex()) strcpy(s, "+++");
                            else if (nPat < pSndFile->Patterns.Size()) wsprintf(s, "%u", nPat);
                            else strcpy(s, "???");
                    }
                    dc.SetTextColor((bHighLight) ? colorTextSel : colorText);
                    dc.DrawText(s, -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                    rect.left += m_cxFont;
                    nIndex++;
            }
    }
    if (oldpen) ::SelectObject(dc.m_hDC, oldpen);
    if (oldfont) ::SelectObject(dc.m_hDC, oldfont);
}


void COrderList::OnSetFocus(CWnd *pWnd)
//-------------------------------------
{
    CWnd::OnSetFocus(pWnd);
    InvalidateSelection();
    UpdateInfoText();
    CMainFrame::GetMainFrame()->m_pOrderlistHasFocus = this;
}


void COrderList::OnKillFocus(CWnd *pWnd)
//--------------------------------------
{
    CWnd::OnKillFocus(pWnd);
    InvalidateSelection();
    CMainFrame::GetMainFrame()->m_pOrderlistHasFocus = nullptr;
}


void COrderList::OnLButtonDown(UINT nFlags, CPoint pt)
//----------------------------------------------------
{
    CRect rect;
    GetClientRect(&rect);
    if (pt.y < rect.bottom) {
        SetFocus();
        // mark pattern (+skip to)
        const int oldXScroll = m_nXScroll;

        modplug::tracker::orderindex_t nOrder = GetOrderFromPoint(rect, pt);
        ORD_SELECTION selection = GetCurSel(false);

        // check if cursor is in selection - if it is, only react on MouseUp as the user might want to drag those orders
        if(m_nScrollPos2nd == modplug::tracker::OrderIndexInvalid || nOrder < selection.nOrdLo || nOrder > selection.nOrdHi)
        {
                m_nScrollPos2nd = modplug::tracker::OrderIndexInvalid;
        }
        m_bDragging = IsOrderInMargins(m_nScrollPos, oldXScroll) ? false : true;

        if(m_bDragging == true)
        {
                m_nDragOrder = GetCurSel(true).nOrdLo;
                m_nDropPos = m_nDragOrder;
                SetCapture();
        }
    } else {
        CWnd::OnLButtonDown(nFlags, pt);
    }
}


void COrderList::OnLButtonUp(UINT nFlags, CPoint pt)
//--------------------------------------------------
{
    CRect rect;
    GetClientRect(&rect);
    bool bSelection = false;

    if (m_bDragging)
    {
            m_bDragging = false;
            ReleaseCapture();
            if (rect.PtInRect(pt))
            {
                    modplug::tracker::orderindex_t n = GetOrderFromPoint(rect, pt);
                    if ((n != modplug::tracker::OrderIndexInvalid) && (n == m_nDropPos) && (m_pModDoc))
                    {
                            // drag multiple orders (not quite as easy...)
                            ORD_SELECTION selection = GetCurSel(false);
                            // move how many orders from where?
                            modplug::tracker::orderindex_t nMoveCount = (selection.nOrdHi - selection.nOrdLo), nMovePos = selection.nOrdLo;
                            // drop before or after the selection
                            bool bMoveBack = !(m_nDragOrder < (UINT)m_nDropPos);
                            // don't do anything if drop position is inside the selection
                            if((m_nDropPos >= selection.nOrdLo && m_nDropPos <= selection.nOrdHi) || m_nDragOrder == m_nDropPos) return;
                            // drag one order or multiple orders?
                            bool bMultiSelection = (selection.nOrdLo != selection.nOrdHi);

                            for(int i = 0; i <= nMoveCount; i++)
                            {
                                    if(!m_pModDoc->MoveOrder(nMovePos, m_nDropPos, true, bSelection)) return;
                                    if((bMoveBack ^ bSelection) == true && bMultiSelection)
                                    {
                                            nMovePos++;
                                            m_nDropPos++;
                                    }
                                    if(bMoveBack && bSelection && bMultiSelection) {
                                            nMovePos += 2;
                                            m_nDropPos++;
                                    }
                            }
                            if(bMultiSelection)
                            {
                                    // adjust selection
                                    m_nScrollPos2nd = m_nDropPos - 1;
                                    m_nDropPos -= nMoveCount + (bMoveBack ? 0 : 1);
                                    SetCurSel((bMoveBack && (!bSelection)) ? m_nDropPos - 1 : m_nDropPos);
                            } else
                            {
                                    SetCurSel(((m_nDragOrder < m_nDropPos) && (!bSelection)) ? m_nDropPos - 1 : m_nDropPos);
                            }
                            m_pModDoc->SetModified();
                    }
                    else
                    {
                            modplug::tracker::orderindex_t nOrder = GetOrderFromPoint(rect, pt);
                            ORD_SELECTION selection = GetCurSel(false);

                            // this should actually have equal signs but that breaks multiselect: nOrder >= selection.nOrdLo && nOrder <= section.nOrdHi
                            if (pt.y < rect.bottom && m_nScrollPos2nd != modplug::tracker::OrderIndexInvalid && nOrder > selection.nOrdLo && nOrder < selection.nOrdHi)
                            {
                                    // Remove selection if we didn't drag anything but multiselect was active
                                    m_nScrollPos2nd = modplug::tracker::OrderIndexInvalid;
                                    SetFocus();
                                    SetCurSel(GetOrderFromPoint(rect, pt));
                            }
                    }
            }
            InvalidateRect(NULL, FALSE);
    } else
    {
            CWnd::OnLButtonUp(nFlags, pt);
    }
}


void COrderList::OnMouseMove(UINT nFlags, CPoint pt)
//--------------------------------------------------
{
    if ((m_bDragging) && (m_cxFont))
    {
            CRect rect;

            GetClientRect(&rect);
            modplug::tracker::orderindex_t n = modplug::tracker::OrderIndexInvalid;
            if (rect.PtInRect(pt))
            {
                    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
                    n = GetOrderFromPoint(rect, pt);
                    if (n >= pSndFile->Order.GetLength() || n >= pSndFile->GetModSpecifications().ordersMax) n = modplug::tracker::OrderIndexInvalid;
            }
            if (n != m_nDropPos)
            {
                    if (n != modplug::tracker::OrderIndexInvalid)
                    {
                            m_nDropPos = n;
                            InvalidateRect(NULL, FALSE);
                            SetCursor(CMainFrame::curDragging);
                    } else
                    {
                            m_nDropPos = modplug::tracker::OrderIndexInvalid;
                            SetCursor(CMainFrame::curNoDrop);
                    }
            }
    } else
    {
            CWnd::OnMouseMove(nFlags, pt);
    }
}


void COrderList::OnSelectSequence(UINT nid)
//-----------------------------------------
{
    SelectSequence(static_cast<modplug::tracker::sequenceindex_t>(nid - ID_SEQUENCE_ITEM));
}


void COrderList::OnRButtonDown(UINT nFlags, CPoint pt)
//----------------------------------------------------
{
    CRect rect;
    GetClientRect(&rect);
    if (m_bDragging)
    {
            m_nDropPos = modplug::tracker::OrderIndexInvalid;
            OnLButtonUp(nFlags, pt);
    }
    if (pt.y >= rect.bottom) return;

    module_renderer *pSndFile = m_pModDoc->GetSoundFile();

    bool bMultiSelection = (m_nScrollPos2nd != modplug::tracker::OrderIndexInvalid);

    if(!bMultiSelection) SetCurSel(GetOrderFromPoint(rect, pt));
    SetFocus();
    HMENU hMenu = ::CreatePopupMenu();
    if(!hMenu) return;

    // check if at least one pattern in the current selection exists
    bool bPatternExists = false;
    ORD_SELECTION selection = GetCurSel(false);
    for(modplug::tracker::orderindex_t nOrd = selection.nOrdLo; nOrd <= selection.nOrdHi; nOrd++)
    {
            bPatternExists = ((pSndFile->Order[nOrd] < pSndFile->Patterns.Size())
                    && (pSndFile->Patterns[pSndFile->Order[nOrd]] != nullptr));
            if(bPatternExists) break;
    }

    const uint32_t greyed = bPatternExists ? 0 : MF_GRAYED;

    if(bMultiSelection)
    {
            // several patterns are selected.
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_INSERT, "&Insert Patterns");
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_DELETE, "&Remove Patterns");
            AppendMenu(hMenu, MF_SEPARATOR, NULL, "");
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_EDIT_COPY, "&Copy Orders");
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_EDIT_CUT, "&C&ut Orders");
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_EDIT_PASTE, "&Paste Orders");
            AppendMenu(hMenu, MF_SEPARATOR, NULL, "");
            AppendMenu(hMenu, MF_STRING | greyed, ID_ORDERLIST_COPY, "&Duplicate Patterns");
    }
    else
    {
            // only one pattern is selected
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_INSERT, "&Insert Pattern");
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_DELETE, "&Remove Pattern");
            AppendMenu(hMenu, MF_SEPARATOR, NULL, "");
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_NEW, "Create &New Pattern");
            AppendMenu(hMenu, MF_STRING | greyed, ID_ORDERLIST_COPY, "&Duplicate Pattern");
            AppendMenu(hMenu, MF_STRING | greyed, ID_PATTERNCOPY, "&Copy Pattern");
            AppendMenu(hMenu, MF_STRING | greyed, ID_PATTERNPASTE, "P&aste Pattern");
            AppendMenu(hMenu, MF_STRING, ID_ORDERLIST_EDIT_PASTE, "&Paste Orders");
            if (pSndFile->TypeIsIT_MPT_XM())
            {
                    AppendMenu(hMenu, MF_SEPARATOR, NULL, "");
                    AppendMenu(hMenu, MF_STRING | greyed, ID_PATTERN_PROPERTIES, "&Pattern properties...");
            }
            if (pSndFile->GetType() == MOD_TYPE_MPT)
            {
                    AppendMenu(hMenu, MF_SEPARATOR, NULL, "");

                    HMENU menuSequence = ::CreatePopupMenu();
                    AppendMenu(hMenu, MF_POPUP, (uintptr_t)menuSequence, TEXT("Sequences"));

                    const modplug::tracker::sequenceindex_t numSequences = pSndFile->Order.GetNumSequences();
                    for(modplug::tracker::sequenceindex_t i = 0; i < numSequences; i++)
                    {
                            CString str;
                            if(pSndFile->Order.GetSequence(i).m_sName.empty())
                                    str.Format(TEXT("Sequence %u"), i);
                            else
                                    str.Format(TEXT("%u: %s"), i, (LPCTSTR)pSndFile->Order.GetSequence(i).m_sName.c_str());
                            const UINT flags = (pSndFile->Order.GetCurrentSequenceIndex() == i) ? MF_STRING|MF_CHECKED : MF_STRING;
                            AppendMenu(menuSequence, flags, ID_SEQUENCE_ITEM + i, str);
                    }
                    if (pSndFile->Order.GetNumSequences() < MAX_SEQUENCES)
                    {
                            AppendMenu(menuSequence, MF_STRING, ID_SEQUENCE_ITEM + MAX_SEQUENCES, TEXT("Duplicate current sequence"));
                            AppendMenu(menuSequence, MF_STRING, ID_SEQUENCE_ITEM + MAX_SEQUENCES + 1, TEXT("Create empty sequence"));
                    }
                    if (pSndFile->Order.GetNumSequences() > 1)
                            AppendMenu(menuSequence, MF_STRING, ID_SEQUENCE_ITEM + MAX_SEQUENCES + 2, TEXT("Delete current sequence"));
            }
    }
    AppendMenu(hMenu, MF_SEPARATOR, NULL, "");
    AppendMenu(hMenu, MF_STRING | greyed, ID_ORDERLIST_RENDER, "Render to &Wave");

    ClientToScreen(&pt);
    ::TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);
    ::DestroyMenu(hMenu);
}


void COrderList::OnLButtonDblClk(UINT, CPoint)
//--------------------------------------------
{
    if ((m_pModDoc) && (m_pParent))
    {
            m_nScrollPos2nd = modplug::tracker::OrderIndexInvalid;
            SetFocus();
            module_renderer *pSndFile = m_pModDoc->GetSoundFile();
            m_pParent->SetCurrentPattern(pSndFile->Order[m_nScrollPos]);
    }
}


void COrderList::OnMButtonDown(UINT nFlags, CPoint pt)
//----------------------------------------------------
{
    UNREFERENCED_PARAMETER(nFlags);
    QueuePattern(pt);
}


void COrderList::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar*)
//--------------------------------------------------------------
{
    UINT nNewPos = m_nXScroll;
    UINT smin, smax;

    GetScrollRange(SB_HORZ, (LPINT)&smin, (LPINT)&smax);
    m_bScrolling = true;
    switch(nSBCode)
    {
    case SB_LEFT:                        nNewPos = 0; break;
    case SB_LINELEFT:                if (nNewPos) nNewPos--; break;
    case SB_LINERIGHT:                if (nNewPos < smax) nNewPos++; break;
    case SB_PAGELEFT:                if (nNewPos > 4) nNewPos -= 4; else nNewPos = 0; break;
    case SB_PAGERIGHT:                if (nNewPos+4 < smax) nNewPos += 4; else nNewPos = smax; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:        nNewPos = nPos; if (nNewPos & 0xFFFF8000) nNewPos = smin; break;
    case SB_ENDSCROLL:                m_bScrolling = false; break;
    }
    if (nNewPos > smax) nNewPos = smax;
    if (nNewPos != m_nXScroll)
    {
            m_nXScroll = static_cast<modplug::tracker::orderindex_t>(nNewPos);
            SetScrollPos(SB_HORZ, m_nXScroll);
            InvalidateRect(NULL, FALSE);
    }
}


void COrderList::OnSize(UINT nType, int cx, int cy)
//-------------------------------------------------
{
    int nPos;
    int smin, smax;

    CWnd::OnSize(nType, cx, cy);
    UpdateScrollInfo();
    GetScrollRange(SB_HORZ, &smin, &smax);
    nPos = GetScrollPos(SB_HORZ);
    if (nPos > smax) nPos = smax;
    if (m_nXScroll != nPos)
    {
            m_nXScroll = static_cast<modplug::tracker::orderindex_t>(nPos);
            SetScrollPos(SB_HORZ, m_nXScroll);
            InvalidateRect(NULL, FALSE);
    }
}


void COrderList::OnInsertOrder()
//------------------------------
{
    // insert the same order(s) after the currently selected order(s)
    if (m_pModDoc)
    {
            module_renderer *pSndFile = m_pModDoc->GetSoundFile();

            ORD_SELECTION selection = GetCurSel(false);
            modplug::tracker::orderindex_t nInsertCount = selection.nOrdHi - selection.nOrdLo, nInsertEnd = selection.nOrdHi;

            for(modplug::tracker::orderindex_t i = 0; i <= nInsertCount; i++)
            {
                    //Checking whether there is some pattern at the end of orderlist.
                    if (pSndFile->Order.GetLength() < 1 || pSndFile->Order.Last() < pSndFile->Patterns.Size())
                    {
                            if(pSndFile->Order.GetLength() < pSndFile->GetModSpecifications().ordersMax)
                                    pSndFile->Order.Append();
                    }
                    for(int j = pSndFile->Order.GetLastIndex(); j > nInsertEnd; j--)
                            pSndFile->Order[j] = pSndFile->Order[j - 1];
            }
            // now that there is enough space in the order list, overwrite the orders
            for(modplug::tracker::orderindex_t i = 0; i <= nInsertCount; i++)
            {
                    if(nInsertEnd + i + 1 < pSndFile->GetModSpecifications().ordersMax
                       &&
                       nInsertEnd + i + 1 < pSndFile->Order.GetLength())
                            pSndFile->Order[nInsertEnd + i + 1] = pSndFile->Order[nInsertEnd - nInsertCount + i];
            }
            m_nScrollPos = bad_min(nInsertEnd + 1, pSndFile->Order.GetLastIndex());
            if(nInsertCount > 0)
                    m_nScrollPos2nd = bad_min(m_nScrollPos + nInsertCount, pSndFile->Order.GetLastIndex());
            else
                    m_nScrollPos2nd = modplug::tracker::OrderIndexInvalid;

            InvalidateSelection();
            EnsureVisible(m_nScrollPos2nd);
            // first inserted order has higher priority than the last one
            EnsureVisible(m_nScrollPos);

            InvalidateRect(NULL, FALSE);
            m_pModDoc->SetModified();
            m_pModDoc->UpdateAllViews(NULL, HINT_MODSEQUENCE, this);
    }
}

void COrderList::OnRenderOrder()
//------------------------------
{
    ORD_SELECTION selection = GetCurSel(false);
    m_pModDoc->OnFileWaveConvert(selection.nOrdLo, selection.nOrdHi);
}


void COrderList::OnDeleteOrder()
//------------------------------
{
    if (m_pModDoc)
    {
            module_renderer *pSndFile = m_pModDoc->GetSoundFile();

            ORD_SELECTION selection = GetCurSel(false);
            // remove selection
            m_nScrollPos2nd = modplug::tracker::OrderIndexInvalid;

            pSndFile->Order.Remove(selection.nOrdLo, selection.nOrdHi);

            m_pModDoc->SetModified();
            InvalidateRect(NULL, FALSE);
            m_pModDoc->UpdateAllViews(NULL, HINT_MODSEQUENCE, this);

            SetCurSel(selection.nOrdLo);
            modplug::tracker::patternindex_t nNewPat = pSndFile->Order[selection.nOrdLo];
            if ((nNewPat < pSndFile->Patterns.Size()) && (pSndFile->Patterns[nNewPat] != nullptr) && (m_pParent))
            {
                    m_pParent->SetCurrentPattern(nNewPat);
            }
    }
}


void COrderList::OnPatternProperties()
//------------------------------------
{
    if (m_pParent) m_pParent->PostViewMessage(VIEWMSG_PATTERNPROPERTIES);
}


void COrderList::OnPlayerPlay()
//-----------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, ID_PLAYER_PLAY);
}


void COrderList::OnPlayerPause()
//------------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, ID_PLAYER_PAUSE);
}


void COrderList::OnPlayerPlayFromStart()
//--------------------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, ID_PLAYER_PLAYFROMSTART);
}


void COrderList::OnPatternPlayFromStart()
//---------------------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, IDC_PATTERN_PLAYFROMSTART);
}


void COrderList::OnCreateNewPattern()
//-----------------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, ID_ORDERLIST_NEW);
}


void COrderList::OnDuplicatePattern()
//-----------------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, ID_ORDERLIST_COPY);
}


void COrderList::OnPatternCopy()
//------------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, ID_PATTERNCOPY);
}


void COrderList::OnPatternPaste()
//-------------------------------
{
    if (m_pParent) m_pParent->PostMessage(WM_COMMAND, ID_PATTERNPASTE);
}


LRESULT COrderList::OnHelpHitTest(WPARAM, LPARAM)
//-----------------------------------------------
{
    return HID_BASE_COMMAND + IDC_ORDERLIST;
}


LRESULT COrderList::OnDragonDropping(WPARAM bDoDrop, LPARAM lParam)
//-----------------------------------------------------------------
{
    LPDRAGONDROP pDropInfo = (LPDRAGONDROP)lParam;
    modplug::tracker::orderindex_t posdest;
    BOOL bCanDrop;
    module_renderer *pSndFile;
    CPoint pt;

    if ((!pDropInfo) || (!m_pModDoc) || (m_pModDoc != pDropInfo->pModDoc) || (!m_cxFont)) return FALSE;
    pSndFile = m_pModDoc->GetSoundFile();
    bCanDrop = FALSE;
    switch(pDropInfo->dwDropType)
    {
    case DRAGONDROP_ORDER:
            if (pDropInfo->dwDropItem >= pSndFile->Order.size()) break;
    case DRAGONDROP_PATTERN:
            bCanDrop = TRUE;
            break;
    }
    if ((!bCanDrop) || (!bDoDrop)) return bCanDrop;
    GetCursorPos(&pt);
    ScreenToClient(&pt);
    if (pt.x < 0) pt.x = 0;
    posdest = static_cast<modplug::tracker::orderindex_t>(m_nXScroll + (pt.x / m_cxFont));
    if (posdest >= pSndFile->Order.GetLength()) return FALSE;
    switch(pDropInfo->dwDropType)
    {
    case DRAGONDROP_PATTERN:
            pSndFile->Order[posdest] = static_cast<modplug::tracker::patternindex_t>(pDropInfo->dwDropItem);
            break;

    case DRAGONDROP_ORDER:
            pSndFile->Order[posdest] = pSndFile->Order[pDropInfo->dwDropItem];
            break;
    }
    if (bCanDrop)
    {
            InvalidateRect(NULL, FALSE);
            m_pModDoc->SetModified();
            m_pModDoc->UpdateAllViews(NULL, HINT_MODSEQUENCE, this);
            SetCurSel(posdest, true);
    }
    return bCanDrop;
}


uint8_t COrderList::SetMargins(int i)
//--------------------------------
{
    m_nOrderlistMargins = static_cast<uint8_t>(i);
    return GetMargins();
}

void COrderList::SelectSequence(const modplug::tracker::sequenceindex_t nSeq)
//-------------------------------------------------------
{
    BEGIN_CRITICAL();
    CMainFrame::GetMainFrame()->ResetNotificationBuffer();
    module_renderer& rSf = *m_pModDoc->GetSoundFile();
    if (nSeq == MAX_SEQUENCES + 2)
    {
            CString strParam; strParam.Format(TEXT("%u: %s"), rSf.Order.GetCurrentSequenceIndex(), (LPCTSTR)rSf.Order.m_sName.c_str());
            CString str;
            AfxFormatString1(str, IDS_CONFIRM_SEQUENCE_DELETE, strParam);
            if (AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDYES)
                    rSf.Order.RemoveSequence();
            else
            {
                    END_CRITICAL();
                    return;
            }
    }
    else if (nSeq == MAX_SEQUENCES || nSeq == MAX_SEQUENCES + 1)
            rSf.Order.AddSequence((nSeq == MAX_SEQUENCES));
    else if (nSeq < rSf.Order.GetNumSequences())
            rSf.Order.SetSequence(nSeq);
    modplug::tracker::orderindex_t nPosCandidate = rSf.Order.GetLengthTailTrimmed() - 1;
    SetCurSel(bad_min(m_nScrollPos, nPosCandidate), true, false, true);
    if (m_pParent)
            m_pParent->SetCurrentPattern(rSf.Order[m_nScrollPos]);

    UpdateScrollInfo();
    END_CRITICAL();
    UpdateView(HINT_MODSEQUENCE);
    m_pModDoc->SetModified();
    m_pModDoc->UpdateAllViews(NULL, HINT_MODSEQUENCE, this);
}


void COrderList::QueuePattern(CPoint pt)
//--------------------------------------
{
    CRect rect;
    GetClientRect(&rect);

    if(!rect.PtInRect(pt)) return;
    if (m_pModDoc == nullptr) return;
    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
    if(pSndFile == nullptr) return;

    const modplug::tracker::patternindex_t nIgnore = pSndFile->Order.GetIgnoreIndex();
    const modplug::tracker::patternindex_t nInvalid = pSndFile->Order.GetInvalidPatIndex();
    const modplug::tracker::orderindex_t nLength = pSndFile->Order.GetLength();
    modplug::tracker::orderindex_t nOrder = GetOrderFromPoint(rect, pt);

    // If this is not a playable order item, find the next valid item.
    while(nOrder < nLength && (pSndFile->Order[nOrder] == nIgnore || pSndFile->Order[nOrder] == nInvalid))
    {
            nOrder++;
    }

    if (nOrder < nLength)
    {
            if (pSndFile->m_nSeqOverride == static_cast<UINT>(nOrder) + 1)
            {
                    pSndFile->m_nSeqOverride = 0;
            } else
            {
                    pSndFile->m_nSeqOverride = nOrder + 1;
            }
            InvalidateRect(NULL, FALSE);
    }
}

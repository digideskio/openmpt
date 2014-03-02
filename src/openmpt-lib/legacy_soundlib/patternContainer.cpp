#include "stdafx.h"
#include "patternContainer.h"
#include "sndfile.h"
#include "../mainfrm.h"
#include "../serialization_utils.h"
#include "../version.h"


void CPatternContainer::ClearPatterns()
//-------------------------------------
{
    DestroyPatterns();
    m_Patterns.assign(m_Patterns.size(), CPattern(*this));
}


void CPatternContainer::DestroyPatterns()
//---------------------------------------
{
    for(modplug::tracker::patternindex_t i = 0; i < m_Patterns.size(); i++)
    {
            Remove(i);
    }
}


modplug::tracker::patternindex_t CPatternContainer::Insert(const modplug::tracker::rowindex_t rows)
//---------------------------------------------------------
{
    modplug::tracker::patternindex_t i = 0;
    for(i = 0; i < m_Patterns.size(); i++)
            if(!m_Patterns[i]) break;
    if(Insert(i, rows))
            return modplug::tracker::PatternIndexInvalid;
    else return i;

}


bool CPatternContainer::Insert(const modplug::tracker::patternindex_t index, const modplug::tracker::rowindex_t rows)
//---------------------------------------------------------------------------
{
    const CModSpecifications& specs = m_rSndFile.GetModSpecifications();
    if(index >= specs.patternsMax || index > m_Patterns.size() || rows > specs.patternRowsMax)
            return true;
    if(index < m_Patterns.size() && m_Patterns[index])
            return true;

    if(index == m_Patterns.size())
    {
            if(index < specs.patternsMax)
                    m_Patterns.push_back(CPattern(*this));
            else
            {
                    ErrorBox(IDS_ERR_TOOMANYPAT, CMainFrame::GetMainFrame());
                    return true;
            }
    }

    if(m_Patterns[index].m_ModCommands != nullptr)
    {
            CPattern::FreePattern(m_Patterns[index].m_ModCommands);
    }
    m_Patterns[index].m_ModCommands = CPattern::AllocatePattern(rows, m_rSndFile.GetNumChannels());
    m_Patterns[index].m_Rows = rows;
    m_Patterns[index].RemoveSignature();
    m_Patterns[index].SetName("");

    if(!m_Patterns[index]) return true;

    return false;
}


bool CPatternContainer::Remove(const modplug::tracker::patternindex_t ipat)
//-----------------------------------------------------
{
    if(ipat >= m_Patterns.size())
            return true;
    m_Patterns[ipat].Deallocate();
    return false;
}


bool CPatternContainer::IsPatternEmpty(const modplug::tracker::patternindex_t nPat) const
//-------------------------------------------------------------------
{
    if(!IsValidPat(nPat))
            return false;

    const modplug::tracker::modevent_t *m = m_Patterns[nPat].m_ModCommands;
    for(size_t i = m_Patterns[nPat].GetNumChannels() * m_Patterns[nPat].GetNumRows(); i > 0; i--, m++)
    {
            if(!m->IsEmpty(true))
                    return false;
    }
    return true;
}


modplug::tracker::patternindex_t CPatternContainer::GetIndex(const CPattern* const pPat) const
//--------------------------------------------------------------------------
{
    const modplug::tracker::patternindex_t endI = static_cast<modplug::tracker::patternindex_t>(m_Patterns.size());
    for(modplug::tracker::patternindex_t i = 0; i<endI; i++)
            if(&m_Patterns[i] == pPat) return i;

    return endI;
}


void CPatternContainer::ResizeArray(const modplug::tracker::patternindex_t newSize)
//-------------------------------------------------------------
{
    if(Size() <= newSize)
            m_Patterns.resize(newSize, CPattern(*this));
    else
    {
            for(modplug::tracker::patternindex_t i = Size(); i > newSize; i--) Remove(i-1);
            m_Patterns.resize(newSize, CPattern(*this));
    }
}


void CPatternContainer::OnModTypeChanged(const MODTYPE /*oldtype*/)
//-----------------------------------------------------------------
{
    const CModSpecifications specs = m_rSndFile.GetModSpecifications();
    if(specs.patternsMax < Size())
            ResizeArray(bad_max(MAX_PATTERNS, specs.patternsMax));
    else if(Size() < MAX_PATTERNS)
            ResizeArray(MAX_PATTERNS);

    // remove pattern time signatures
    if(!specs.hasPatternSignatures)
    {
            for(modplug::tracker::patternindex_t nPat = 0; nPat < m_Patterns.size(); nPat++)
            {
                    m_Patterns[nPat].RemoveSignature();
            }
    }
}


void CPatternContainer::Init()
//----------------------------
{
    for(modplug::tracker::patternindex_t i = 0; i < Size(); i++)
    {
            Remove(i);
    }

    ResizeArray(MAX_PATTERNS);
}


modplug::tracker::patternindex_t CPatternContainer::GetNumNamedPatterns() const
//---------------------------------------------------------
{
    if(Size() == 0)
    {
            return 0;
    }
    for(modplug::tracker::patternindex_t nPat = Size(); nPat > 0; nPat--)
    {
            if(m_Patterns[nPat - 1].GetName() != "")
            {
                    return nPat;
            }
    }
    return 0;
}



void WriteModPatterns(std::ostream& oStrm, const CPatternContainer& patc)
//----------------------------------------------------------------------
{
    srlztn::Ssb ssb(oStrm);
    ssb.BeginWrite(FileIdPatterns, MptVersion::num);
    const modplug::tracker::patternindex_t nPatterns = patc.Size();
    uint16_t nCount = 0;
    for(uint16_t i = 0; i < nPatterns; i++) if (patc[i])
    {
            ssb.WriteItem(patc[i], &i, sizeof(i), &WriteModPattern);
            nCount = i + 1;
    }
    ssb.WriteItem<uint16_t>(nCount, "num"); // Index of last pattern + 1.
    ssb.FinishWrite();
}


void ReadModPatterns(std::istream& iStrm, CPatternContainer& patc, const size_t)
//--------------------------------------------------------------------------------
{
    srlztn::Ssb ssb(iStrm);
    ssb.BeginRead(FileIdPatterns, MptVersion::num);
    if ((ssb.m_Status & srlztn::SNT_FAILURE) != 0)
            return;
    modplug::tracker::patternindex_t nPatterns = patc.Size();
    uint16_t nCount = UINT16_MAX;
    if (ssb.ReadItem(nCount, "num") != srlztn::Ssb::EntryNotFound)
            nPatterns = nCount;
    LimitMax(nPatterns, ModSpecs::mptm.patternsMax);
    if (nPatterns > patc.Size())
            patc.ResizeArray(nPatterns);
    for(uint16_t i = 0; i < nPatterns; i++)
    {
            ssb.ReadItem(patc[i], &i, sizeof(i), &ReadModPattern);
    }
}
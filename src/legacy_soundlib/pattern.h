#ifndef PATTERN_H
#define PATTERN_H

#include <vector>
#include "../tracker/tracker.h"
#include "Snd_defs.h"

using std::vector;

class CPatternContainer;
class CSoundFile;

typedef modplug::tracker::modcommand_t* PatternRow;

//============
class CPattern
//============
{
    friend class CPatternContainer;
    
public:
//BEGIN: OPERATORS
    //To mimic modplug::tracker::modcommand_t*
    operator modplug::tracker::modcommand_t*() { return m_ModCommands; }
    operator const modplug::tracker::modcommand_t*() const { return m_ModCommands; }
    CPattern& operator=(modplug::tracker::modcommand_t* const p) { m_ModCommands = p; return *this; }
    CPattern& operator=(const CPattern& pat)
    {
    	m_ModCommands = pat.m_ModCommands;
    	m_Rows = pat.m_Rows;
    	m_RowsPerBeat = pat.m_RowsPerBeat;
    	m_RowsPerMeasure = pat.m_RowsPerMeasure;
    	m_PatternName = pat.m_PatternName;
    	return *this;
    }
//END: OPERATORS

//BEGIN: INTERFACE METHODS
public:
    modplug::tracker::modcommand_t* GetpModCommand(const ROWINDEX r, const CHANNELINDEX c) { return &m_ModCommands[r * GetNumChannels() + c]; }
    const modplug::tracker::modcommand_t* GetpModCommand(const ROWINDEX r, const CHANNELINDEX c) const { return &m_ModCommands[r * GetNumChannels() + c]; }
    
    ROWINDEX GetNumRows() const { return m_Rows; }
    ROWINDEX GetRowsPerBeat() const { return m_RowsPerBeat; }			// pattern-specific rows per beat
    ROWINDEX GetRowsPerMeasure() const { return m_RowsPerMeasure; }		// pattern-specific rows per measure
    bool GetOverrideSignature() const { return (m_RowsPerBeat + m_RowsPerMeasure > 0); }	// override song time signature?

    // Return true if modcommand can be accessed from given row, false otherwise.
    bool IsValidRow(const ROWINDEX iRow) const { return (iRow < GetNumRows()); }

    // Return PatternRow object which has operator[] defined so that modplug::tracker::modcommand_t
    // at (iRow, iChn) can be accessed with GetRow(iRow)[iChn].
    PatternRow GetRow(const ROWINDEX iRow) { return GetpModCommand(iRow, 0); }

    CHANNELINDEX GetNumChannels() const;

    bool Resize(const ROWINDEX newRowCount, const bool showDataLossWarning = true);

    // Deallocates pattern data. 
    void Deallocate();

    // Removes all modcommands from the pattern.
    void ClearCommands();

    // Returns associated soundfile.
    CSoundFile& GetSoundFile();
    const CSoundFile& GetSoundFile() const;

    bool SetData(modplug::tracker::modcommand_t* p, const ROWINDEX rows) { m_ModCommands = p; m_Rows = rows; return false; }

    // Set pattern signature (rows per beat, rows per measure). Returns true on success.
    bool SetSignature(const ROWINDEX rowsPerBeat, const ROWINDEX rowsPerMeasure);
    void RemoveSignature() { m_RowsPerBeat = m_RowsPerMeasure = 0; }

    // Patter name functions (for both CString and char[] arrays) - bool functions return true on success.
    bool SetName(char *newName, size_t maxChars = MAX_PATTERNNAME);
    bool SetName(CString newName) { m_PatternName = newName; return true; };
    bool GetName(char *buffer, size_t maxChars = MAX_PATTERNNAME) const;
    CString GetName() const { return m_PatternName; };

    // Double number of rows
    bool Expand();

    // Halve number of rows
    bool Shrink();

    bool WriteITPdata(FILE* f) const;
    bool ReadITPdata(const uint8_t* const lpStream, uint32_t& streamPos, const uint32_t datasize, const uint32_t dwMemLength);
    //Parameters:
    //1. Pointer to the beginning of the stream
    //2. Tells where to start(lpStream+streamPos) and number of bytes read is added to it.
    //3. How many bytes to read
    //4. Length of the stream.
    //Returns true on error.

    // Static allocation / deallocation helpers
    static modplug::tracker::modcommand_t* AllocatePattern(ROWINDEX rows, CHANNELINDEX nchns);
    static void FreePattern(modplug::tracker::modcommand_t *pat);

//END: INTERFACE METHODS

    typedef modplug::tracker::modcommand_t* iterator;
    typedef const modplug::tracker::modcommand_t *const_iterator;

    iterator Begin() { return m_ModCommands; }
    const_iterator Begin() const { return m_ModCommands; }

    iterator End() { return (m_ModCommands != nullptr) ? m_ModCommands + m_Rows * GetNumChannels() : nullptr; }
    const_iterator End() const { return (m_ModCommands != nullptr) ? m_ModCommands + m_Rows * GetNumChannels() : nullptr; }

    CPattern(CPatternContainer& patCont) : m_ModCommands(0), m_Rows(64), m_rPatternContainer(patCont), m_RowsPerBeat(0), m_RowsPerMeasure(0) {};

protected:
    modplug::tracker::modcommand_t& GetModCommand(size_t i) { return m_ModCommands[i]; }
    //Returns modcommand from (floor[i/channelCount], i%channelCount) 

    modplug::tracker::modcommand_t& GetModCommand(ROWINDEX r, CHANNELINDEX c) { return m_ModCommands[r*GetNumChannels()+c]; }
    const modplug::tracker::modcommand_t& GetModCommand(ROWINDEX r, CHANNELINDEX c) const { return m_ModCommands[r*GetNumChannels()+c]; }


//BEGIN: DATA
protected:
    modplug::tracker::modcommand_t* m_ModCommands;
    ROWINDEX m_Rows;
    ROWINDEX m_RowsPerBeat;		// patterns-specific time signature. if != 0, this is implicitely set.
    ROWINDEX m_RowsPerMeasure;	// dito
    CString m_PatternName;
    CPatternContainer& m_rPatternContainer;
//END: DATA
};


const char FileIdPattern[] = "mptP";

void ReadModPattern(std::istream& iStrm, CPattern& patc, const size_t nSize = 0);
void WriteModPattern(std::ostream& oStrm, const CPattern& patc);


#endif
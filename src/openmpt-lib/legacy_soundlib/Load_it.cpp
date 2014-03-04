/*
 * OpenMPT
 *
 * Load_it.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "Loaders.h"
#include "IT_DEFS.H"
#include "tuningcollection.h"
#include "../moddoc.h"
#include "../serialization_utils.h"
#include <fstream>
#include <strstream>
#include <list>
#include "../version.h"

#include "../modformat/it/it.hpp"
#include "../pervasives/binaryparse.hpp"

#define str_tooMuchPatternData    (GetStrI18N((_TEXT("Warning: File format limit was reached. Some pattern data may not get written to file."))))
#define str_pattern                            (GetStrI18N((_TEXT("pattern"))))
#define str_PatternSetTruncationNote (GetStrI18N((_TEXT("The module contains %u patterns but only %u patterns can be loaded in this OpenMPT version.\n"))))
#define str_SequenceTruncationNote (GetStrI18N((_TEXT("Module has sequence of length %u; it will be truncated to maximum supported length, %u.\n"))))
#define str_LoadingIncompatibleVersion    TEXT("The file informed that it is incompatible with this version of OpenMPT. Loading was terminated.\n")
#define str_LoadingMoreRecentVersion    TEXT("The loaded file was made with a more recent OpenMPT version and this version may not be able to load all the features or play the file correctly.\n")

const uint16_t verMptFileVer = 0x891;
const uint16_t verMptFileVerLoadLimit = 0x1000; // If cwtv-field is greater or equal to this value,
                                              // the MPTM file will not be loaded.

/*
MPTM version history for cwtv-field in "IT" header (only for MPTM files!):
0x890(1.18.02.00) -> 0x891(1.19.00.00): Pattern-specific time signatures
                                        Fixed behaviour of Pattern Loop command for rows > 255 (r617)
0x88F(1.18.01.00) -> 0x890(1.18.02.00): Removed volume command velocity :xy, added delay-cut command :xy.
0x88E(1.17.02.50) -> 0x88F(1.18.01.00): Numerous changes
0x88D(1.17.02.49) -> 0x88E(1.17.02.50): Changed ID to that of IT and undone the orderlist change done in
                       0x88A->0x88B. Now extended orderlist is saved as extension.
0x88C(1.17.02.48) -> 0x88D(1.17.02.49): Some tuning related changes - that part fails to read on older versions.
0x88B -> 0x88C: Changed type in which tuning number is printed to file: size_t -> uint16_t.
0x88A -> 0x88B: Changed order-to-pattern-index table type from uint8_t-array to vector<UINT>.
*/


static bool AreNonDefaultTuningsUsed(module_renderer& sf)
//--------------------------------------------------
{
    const modplug::tracker::instrumentindex_t iCount = sf.GetNumInstruments();
    for(modplug::tracker::instrumentindex_t i = 1; i <= iCount; i++)
    {
        if(sf.Instruments[i] != nullptr && sf.Instruments[i]->pTuning != 0)
            return true;
    }
    return false;
}

void ReadTuningCollection(istream& iStrm, CTuningCollection& tc, const size_t) {tc.Deserialize(iStrm);}
void WriteTuningCollection(ostream& oStrm, const CTuningCollection& tc) {tc.Serialize(oStrm);}

void WriteTuningMap(ostream& oStrm, const module_renderer& sf)
//-------------------------------------------------------
{
    if(sf.GetNumInstruments() > 0)
    {
        //Writing instrument tuning data: first creating
        //tuning name <-> tuning id number map,
        //and then writing the tuning id for every instrument.
        //For example if there are 6 instruments and
        //first half use tuning 'T1', and the other half
        //tuning 'T2', the output would be something like
        //T1 1 T2 2 1 1 1 2 2 2

        //Creating the tuning address <-> tuning id number map.
        typedef map<CTuning*, uint16_t> TNTS_MAP;
        typedef TNTS_MAP::iterator TNTS_MAP_ITER;
        TNTS_MAP tNameToShort_Map;

        unsigned short figMap = 0;
        for(UINT i = 1; i <= sf.GetNumInstruments(); i++) if (sf.Instruments[i] != nullptr)
        {
            TNTS_MAP_ITER iter = tNameToShort_Map.find(sf.Instruments[i]->pTuning);
            if(iter != tNameToShort_Map.end())
                continue; //Tuning already mapped.

            tNameToShort_Map[sf.Instruments[i]->pTuning] = figMap;
            figMap++;
        }

        //...and write the map with tuning names replacing
        //the addresses.
        const uint16_t tuningMapSize = static_cast<uint16_t>(tNameToShort_Map.size());
        oStrm.write(reinterpret_cast<const char*>(&tuningMapSize), sizeof(tuningMapSize));
        for(TNTS_MAP_ITER iter = tNameToShort_Map.begin(); iter != tNameToShort_Map.end(); iter++)
        {
            if(iter->first)
                StringToBinaryStream<uint8_t>(oStrm, iter->first->GetName());
            else //Case: Using original IT tuning.
                StringToBinaryStream<uint8_t>(oStrm, "->MPT_ORIGINAL_IT<-");

            srlztn::Binarywrite<uint16_t>(oStrm, iter->second);
        }

        //Writing tuning data for instruments.
        for(UINT i = 1; i <= sf.GetNumInstruments(); i++)
        {
            TNTS_MAP_ITER iter = tNameToShort_Map.find(sf.Instruments[i]->pTuning);
            if(iter == tNameToShort_Map.end()) //Should never happen
            {
#ifdef MODPLUG_TRACKER
                if(sf.GetpModDoc())
                    sf.GetpModDoc()->AddToLog(_T("Error: 210807_1\n"));
#endif // MODPLUG_TRACKER
                return;
            }
            srlztn::Binarywrite(oStrm, iter->second);
        }
    }
}

template<class TUNNUMTYPE, class STRSIZETYPE>
static bool ReadTuningMap(istream& iStrm, map<uint16_t, string>& shortToTNameMap, const size_t maxNum = 500)
//--------------------------------------------------------------------------------------------------------
{
    typedef map<uint16_t, string> MAP;
    typedef MAP::iterator MAP_ITER;
    TUNNUMTYPE numTuning = 0;
    iStrm.read(reinterpret_cast<char*>(&numTuning), sizeof(numTuning));
    if(numTuning > maxNum)
        return true;

    for(size_t i = 0; i<numTuning; i++)
    {
        string temp;
        uint16_t ui;
        if(StringFromBinaryStream<STRSIZETYPE>(iStrm, temp, 255))
            return true;

        iStrm.read(reinterpret_cast<char*>(&ui), sizeof(ui));
        shortToTNameMap[ui] = temp;
    }
    if(iStrm.good())
        return false;
    else
        return true;
}

void ReadTuningMap(istream& iStrm, module_renderer& csf, const size_t = 0)
//-------------------------------------------------------------------
{
    typedef map<uint16_t, string> MAP;
    typedef MAP::iterator MAP_ITER;
    MAP shortToTNameMap;
    ReadTuningMap<uint16_t, uint8_t>(iStrm, shortToTNameMap);

    //Read & set tunings for instruments
    std::list<string> notFoundTunings;
    for(UINT i = 1; i<=csf.GetNumInstruments(); i++)
    {
        uint16_t ui;
        iStrm.read(reinterpret_cast<char*>(&ui), sizeof(ui));
        MAP_ITER iter = shortToTNameMap.find(ui);
        if(csf.Instruments[i] && iter != shortToTNameMap.end())
        {
            const string str = iter->second;

            if(str == string("->MPT_ORIGINAL_IT<-"))
            {
                csf.Instruments[i]->pTuning = nullptr;
                continue;
            }

            csf.Instruments[i]->pTuning = csf.GetTuneSpecificTunings().GetTuning(str);
            if(csf.Instruments[i]->pTuning)
                continue;

            csf.Instruments[i]->pTuning = csf.GetLocalTunings().GetTuning(str);
            if(csf.Instruments[i]->pTuning)
                continue;

            csf.Instruments[i]->pTuning = csf.GetBuiltInTunings().GetTuning(str);
            if(csf.Instruments[i]->pTuning)
                continue;

            if(str == "TET12" && csf.GetBuiltInTunings().GetNumTunings() > 0)
                csf.Instruments[i]->pTuning = &csf.GetBuiltInTunings().GetTuning(0);

            if(csf.Instruments[i]->pTuning)
                continue;

            //Checking if not found tuning already noticed.
            std::list<std::string>::iterator iter;
            iter = find(notFoundTunings.begin(), notFoundTunings.end(), str);
            if(iter == notFoundTunings.end())
            {
                notFoundTunings.push_back(str);
#ifdef MODPLUG_TRACKER
                if(csf.GetpModDoc() != nullptr)
                {
                    string erm = string("Tuning ") + str + string(" used by the module was not found.\n");
                    csf.GetpModDoc()->AddToLog(erm.c_str());
                    csf.GetpModDoc()->SetModified(); //The tuning is changed so the modified flag is set.
                }
#endif // MODPLUG_TRACKER

            }
            csf.Instruments[i]->pTuning = csf.Instruments[i]->s_DefaultTuning;

        }
        else //This 'else' happens probably only in case of corrupted file.
        {
            if(csf.Instruments[i])
                csf.Instruments[i]->pTuning = modinstrument_t::s_DefaultTuning;
        }

    }
    //End read&set instrument tunings
}



#pragma warning(disable:4244) //conversion from 'type1' to 'type2', possible loss of data

uint8_t autovibit2xm[8] =
{ 0, 3, 1, 4, 2, 0, 0, 0 };

uint8_t autovibxm2it[8] =
{ 0, 2, 4, 1, 3, 0, 0, 0 };

//////////////////////////////////////////////////////////
// Impulse Tracker IT file support


static inline UINT ConvertVolParam(UINT value)
//--------------------------------------------
{
    return (value > 9) ? 9 : value;
}


// Convert MPT's internal envelope format into an IT/MPTM envelope.
void MPTEnvToIT(const modplug::tracker::modenvelope_t *mptEnv, ITENVELOPE *itEnv, const uint8_t envOffset, const uint8_t envDefault)
//---------------------------------------------------------------------------------------------------------------
{
    if(mptEnv->flags & ENV_ENABLED)    itEnv->flags |= 1;
    if(mptEnv->flags & ENV_LOOP)            itEnv->flags |= 2;
    if(mptEnv->flags & ENV_SUSTAIN)    itEnv->flags |= 4;
    if(mptEnv->flags & ENV_CARRY)            itEnv->flags |= 8;
    itEnv->num = (uint8_t)bad_min(mptEnv->num_nodes, 25);
    itEnv->lpb = (uint8_t)mptEnv->loop_start;
    itEnv->lpe = (uint8_t)mptEnv->loop_end;
    itEnv->slb = (uint8_t)mptEnv->sustain_start;
    itEnv->sle = (uint8_t)mptEnv->sustain_end;

    if(mptEnv->num_nodes > 0)
    {
        // Attention: Full MPTM envelope is stored in extended instrument properties
        for(size_t ev = 0; ev < 25; ev++)
        {
            itEnv->data[ev * 3] = mptEnv->Values[ev] - envOffset;
            itEnv->data[ev * 3 + 1] = mptEnv->Ticks[ev] & 0xFF;
            itEnv->data[ev * 3 + 2] = mptEnv->Ticks[ev] >> 8;
        }
    } else
    {
        // Fix non-existing envelopes so that they can still be edited in Impulse Tracker.
        itEnv->num = 2;
        MemsetZero(itEnv->data);
        itEnv->data[0] = itEnv->data[3] = envDefault - envOffset;
        itEnv->data[4] = 10;
    }
}


// Convert IT/MPTM envelope data into MPT's internal envelope format - To be used by ITInstrToMPT()
void ITEnvToMPT(const ITENVELOPE *itEnv, modplug::tracker::modenvelope_t *mptEnv, const uint8_t envOffset, const int iEnvMax)
//-----------------------------------------------------------------------------------------------------------
{
    if(itEnv->flags & 1) mptEnv->flags |= ENV_ENABLED;
    if(itEnv->flags & 2) mptEnv->flags |= ENV_LOOP;
    if(itEnv->flags & 4) mptEnv->flags |= ENV_SUSTAIN;
    if(itEnv->flags & 8) mptEnv->flags |= ENV_CARRY;
    mptEnv->num_nodes = bad_min(itEnv->num, iEnvMax);
    mptEnv->loop_start = itEnv->lpb;
    mptEnv->loop_end = itEnv->lpe;
    mptEnv->sustain_start = itEnv->slb;
    mptEnv->sustain_end = itEnv->sle;

    // Attention: Full MPTM envelope is stored in extended instrument properties
    for (UINT ev = 0; ev < 25; ev++)
    {
        mptEnv->Values[ev] = itEnv->data[ev * 3] + envOffset;
        mptEnv->Ticks[ev] = (itEnv->data[ev * 3 + 2] << 8) | (itEnv->data[ev * 3 + 1]);
    }
}


//BOOL CSoundFile::ITInstrToMPT(const void *p, modinstrument_t *pIns, UINT trkvers)
long module_renderer::ITInstrToMPT(const void *p, modinstrument_t *pIns, UINT trkvers) //rewbs.modularInstData
//-----------------------------------------------------------------------------
{
    // Envelope point count. Limited to 25 in IT format.
    const int iEnvMax = (m_nType & MOD_TYPE_MPT) ? MAX_ENVPOINTS : 25;

    long returnVal=0;
    pIns->pTuning = m_defaultInstrument.pTuning;
    pIns->nPluginVelocityHandling = PLUGIN_VELOCITYHANDLING_CHANNEL;
    pIns->nPluginVolumeHandling = PLUGIN_VOLUMEHANDLING_IGNORE;
    if (trkvers < 0x0200)
    {
        const ITOLDINSTRUMENT *pis = (const ITOLDINSTRUMENT *)p;
        memcpy(pIns->name, pis->name, 26);
        memcpy(pIns->legacy_filename, pis->filename, 12);
        SpaceToNullStringFixed<26>(pIns->name);
        SpaceToNullStringFixed<12>(pIns->legacy_filename);
        pIns->fadeout = pis->fadeout << 6;
        pIns->global_volume = 64;
        for (UINT j = 0; j < 120; j++)
        {
            UINT note = pis->keyboard[j*2];
            UINT ins = pis->keyboard[j*2+1];
            if (ins < MAX_SAMPLES) pIns->Keyboard[j] = ins;
            if (note < 120) pIns->NoteMap[j] = note + 1;
            else pIns->NoteMap[j] = j + 1;
        }
        if (pis->flags & 0x01) pIns->volume_envelope.flags |= ENV_ENABLED;
        if (pis->flags & 0x02) pIns->volume_envelope.flags |= ENV_LOOP;
        if (pis->flags & 0x04) pIns->volume_envelope.flags |= ENV_SUSTAIN;
        pIns->volume_envelope.loop_start = pis->vls;
        pIns->volume_envelope.loop_end = pis->vle;
        pIns->volume_envelope.sustain_start = pis->sls;
        pIns->volume_envelope.sustain_end = pis->sle;
        pIns->volume_envelope.num_nodes = 25;
        for (UINT ev=0; ev<25; ev++)
        {
            if ((pIns->volume_envelope.Ticks[ev] = pis->nodes[ev*2]) == 0xFF)
            {
                pIns->volume_envelope.num_nodes = ev;
                break;
            }
            pIns->volume_envelope.Values[ev] = pis->nodes[ev*2+1];
        }

        pIns->new_note_action = pis->nna;
        pIns->duplicate_check_type = pis->dnc;
        pIns->default_pan = 0x80;

        pIns->volume_envelope.release_node = ENV_RELEASE_NODE_UNSET;
        pIns->panning_envelope.release_node = ENV_RELEASE_NODE_UNSET;
        pIns->pitch_envelope.release_node = ENV_RELEASE_NODE_UNSET;
    } else
    {
        const ITINSTRUMENT *pis = (const ITINSTRUMENT *)p;
        memcpy(pIns->name, pis->name, 26);
        memcpy(pIns->legacy_filename, pis->filename, 12);
        SpaceToNullStringFixed<26>(pIns->name);
        SpaceToNullStringFixed<12>(pIns->legacy_filename);
        if (pis->mpr<=128)
            pIns->midi_program = pis->mpr;
        pIns->midi_channel = pis->mch;
        if (pIns->midi_channel > 16)    //rewbs.instroVSTi
        {                                                            //(handle old format where midichan
                                        // and mixplug are 1 value)
            pIns->nMixPlug = pIns->midi_channel-128;
            pIns->midi_channel = 0;
        }
        if (pis->mbank<=128)
            pIns->midi_bank = pis->mbank;
        pIns->fadeout = pis->fadeout << 5;
        pIns->global_volume = pis->gbv >> 1;
        if (pIns->global_volume > 64) pIns->global_volume = 64;
        for (UINT j = 0; j < 120; j++)
        {
            UINT note = pis->keyboard[j*2];
            UINT ins = pis->keyboard[j*2+1];
            if (ins < MAX_SAMPLES) pIns->Keyboard[j] = ins;
            if (note < 120) pIns->NoteMap[j] = note + 1;
            else pIns->NoteMap[j] = j + 1;
        }
        // Olivier's MPT Instrument Extension
        if (*((int *)pis->dummy) == 'MPTX')
        {
            const ITINSTRUMENTEX *pisex = (const ITINSTRUMENTEX *)pis;
            for (UINT k = 0; k < 120; k++)
            {
                pIns->Keyboard[k] |= ((UINT)pisex->keyboardhi[k] << 8);
            }
        }
        //rewbs.modularInstData
        //find end of standard header
        uint8_t* pEndInstHeader;
        if (*((int *)pis->dummy) == 'MPTX')
            pEndInstHeader=(uint8_t*)pis+sizeof(ITINSTRUMENTEX);
        else
            pEndInstHeader=(uint8_t*)pis+sizeof(ITINSTRUMENT);

        //If the next piece of data is 'INSM' we have modular extensions to our instrument...
        if ( *( (UINT*)pEndInstHeader ) == 'INSM' )
        {
            //...the next piece of data must be the total size of the modular data
            long modularInstSize = *((long *)(pEndInstHeader+4));

            //handle chunks
            uint8_t* pModularInst = (uint8_t*)(pEndInstHeader+4+sizeof(modularInstSize)); //4 is for 'INSM'
            pEndInstHeader+=4+sizeof(modularInstSize)+modularInstSize;
            while  (pModularInst<pEndInstHeader) //4 is for 'INSM'
            {
                UINT chunkID = *((int *)pModularInst);
                pModularInst+=4;
                switch (chunkID)
                {
                    /*case 'DMMY':
                        MessageBox(NULL, "Dummy chunk identified", NULL, MB_OK|MB_ICONEXCLAMATION);
                        pModularInst+=1024;
                        break;*/
                    case 'PLUG':
                        pIns->nMixPlug = *(pModularInst);
                        pModularInst+=sizeof(pIns->nMixPlug);
                        break;
                    /*How to load more chunks?  -- see also how to save chunks
                    case: 'MYID':
                        // handle chunk data, as pointed to by pModularInst
                        // move pModularInst as appropriate
                        break;
                    */

                    default: pModularInst++; //move forward one byte and try to recognize again.

                }
            }
            returnVal = 4+sizeof(modularInstSize)+modularInstSize;
        }
        //end rewbs.modularInstData


        // Volume Envelope
        ITEnvToMPT(&pis->volenv, &pIns->volume_envelope, 0, iEnvMax);
        // Panning Envelope
        ITEnvToMPT(&pis->panenv, &pIns->panning_envelope, 32, iEnvMax);
        // Pitch Envelope
        ITEnvToMPT(&pis->pitchenv, &pIns->pitch_envelope, 32, iEnvMax);
        if (pis->pitchenv.flags & 0x80) pIns->pitch_envelope.flags |= ENV_FILTER;

        pIns->new_note_action = pis->nna;
        pIns->duplicate_check_type = pis->dct;
        pIns->duplicate_note_action = pis->dca;
        pIns->pitch_pan_separation = pis->pps;
        pIns->pitch_pan_center = pis->ppc;
        pIns->default_filter_cutoff = pis->ifc;
        pIns->default_filter_resonance = pis->ifr;
        pIns->random_volume_weight = pis->rv;
        pIns->random_pan_weight = pis->rp;
        pIns->default_pan = (pis->dfp & 0x7F) << 2;
        SetDefaultInstrumentValues(pIns);
        pIns->nPluginVelocityHandling = PLUGIN_VELOCITYHANDLING_CHANNEL;
        pIns->nPluginVolumeHandling = PLUGIN_VOLUMEHANDLING_IGNORE;
        if (pIns->default_pan > 256) pIns->default_pan = 128;
        if (pis->dfp < 0x80) pIns->flags |= INS_SETPANNING;
    }

    if ((pIns->volume_envelope.loop_start >= iEnvMax) || (pIns->volume_envelope.loop_end >= iEnvMax)) pIns->volume_envelope.flags &= ~ENV_LOOP;
    if ((pIns->volume_envelope.sustain_start >= iEnvMax) || (pIns->volume_envelope.sustain_end >= iEnvMax)) pIns->volume_envelope.flags &= ~ENV_SUSTAIN;

    return returnVal; //return offset
}


void CopyPatternName(CPattern &pattern, char **patNames, UINT &patNamesLen)
//-------------------------------------------------------------------------
{
    if(*patNames != nullptr && patNamesLen > 0)
    {
        pattern.SetName(*patNames, bad_min(MAX_PATTERNNAME, patNamesLen));
        *patNames += MAX_PATTERNNAME;
        patNamesLen -= bad_min(MAX_PATTERNNAME, patNamesLen);
    }
}


bool module_renderer::ReadIT(const uint8_t * const lpStream, const uint32_t dwMemLength)
//----------------------------------------------------------------------
{
    std::shared_ptr<const uint8_t> dummyptr(lpStream, [] (const uint8_t *ptr) { });
    auto horf = modplug::pervasives::binaryparse::mkcontext(dummyptr, dwMemLength);
    auto ret = modplug::modformat::it::read(horf);
    ITFILEHEADER *pifh = (ITFILEHEADER *)lpStream;

    uint32_t dwMemPos = sizeof(ITFILEHEADER);
    vector<uint32_t> inspos;
    vector<uint32_t> smppos;
    vector<uint32_t> patpos;
// Using eric's code here to take care of NNAs etc..
// -> CODE#0006
// -> DESC="misc quantity changes"
//    uint8_t chnmask[64], channels_used[64];
//    modplug::tracker::modcommand_t lastvalue[64];
    uint8_t chnmask[MAX_BASECHANNELS], channels_used[MAX_BASECHANNELS];
    modplug::tracker::modevent_t lastvalue[MAX_BASECHANNELS];
// -! BEHAVIOUR_CHANGE#0006

    bool interpretModPlugMade = false;
    bool hasModPlugExtensions = false;

    if ((!lpStream) || (dwMemLength < 0xC0)) return false;
    if ((pifh->id != LittleEndian(IT_IMPM) && pifh->id != LittleEndian(IT_MPTM)) || (pifh->insnum > 0xFF)
     || (pifh->smpnum >= MAX_SAMPLES) || (!pifh->ordnum)) return false;
    if (dwMemPos + pifh->ordnum + pifh->insnum*4
     + pifh->smpnum*4 + pifh->patnum*4 > dwMemLength) return false;


    uint32_t mptStartPos = dwMemLength;
    memcpy(&mptStartPos, lpStream + (dwMemLength - sizeof(uint32_t)), sizeof(uint32_t));
    if(mptStartPos >= dwMemLength || mptStartPos < 0x100)
        mptStartPos = dwMemLength;

    if(pifh->id == LittleEndian(IT_MPTM))
    {
        ChangeModTypeTo(MOD_TYPE_MPT);
    }
    else
    {
        if(mptStartPos <= dwMemLength - 3 && pifh->cwtv > 0x888)
        {
            char temp[3];
            const char ID[3] = {'2','2','8'};
            memcpy(temp, lpStream + mptStartPos, 3);
            if(!memcmp(temp, ID, 3)) ChangeModTypeTo(MOD_TYPE_MPT);
            else ChangeModTypeTo(MOD_TYPE_IT);
        }
        else ChangeModTypeTo(MOD_TYPE_IT);

        if(GetType() == MOD_TYPE_IT)
        {
            // Which tracker was used to made this?
            if((pifh->cwtv & 0xF000) == 0x5000)
            {
                // OpenMPT Version number (Major.Minor)
                // This will only be interpreted as "made with ModPlug" (i.e. disable compatible playback etc) if the "reserved" field is set to "OMPT" - else, compatibility was used.
                m_dwLastSavedWithVersion = (pifh->cwtv & 0x0FFF) << 16;
                if(pifh->reserved == LittleEndian(IT_OMPT))
                    interpretModPlugMade = true;
            } else if(pifh->cmwt == 0x888 || pifh->cwtv == 0x888)
            {
                // OpenMPT 1.17 and 1.18 (raped IT format)
                // Exact version number will be determined later.
                interpretModPlugMade = true;
                m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 17, 00, 00);
            } else if(pifh->cwtv == 0x0217 && pifh->cmwt == 0x0200 && pifh->reserved == 0)
            {
                // ModPlug Tracker 1.16 (semi-raped IT format)
                m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 16, 00, 00);
                interpretModPlugMade = true;
            } else if(pifh->cwtv == 0x0214 && pifh->cmwt == 0x0202 && pifh->reserved == 0)
            {
                // ModPlug Tracker b3.3 - 1.09, instruments 557 bytes apart
                m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 09, 00, 00);
                interpretModPlugMade = true;
            }
            else if(pifh->cwtv == 0x0214 && pifh->cmwt == 0x0200 && pifh->reserved == 0)
            {
                // ModPlug Tracker 1.00a5, instruments 560 bytes apart
                m_dwLastSavedWithVersion = MAKE_VERSION_NUMERIC(1, 00, 00, 00);
                interpretModPlugMade = true;
            }
        }
        else // case: type == MOD_TYPE_MPT
        {
            if (pifh->cwtv >= verMptFileVerLoadLimit)
            {
                if (GetpModDoc())
                    GetpModDoc()->AddToLog(str_LoadingIncompatibleVersion);
                return false;
            }
            else if (pifh->cwtv > verMptFileVer)
            {
                if (GetpModDoc())
                    GetpModDoc()->AddToLog(str_LoadingMoreRecentVersion);
            }
        }
    }

    if(GetType() == MOD_TYPE_IT) mptStartPos = dwMemLength;

    // Read row highlights
    if((pifh->special & 0x04))
    {
        // MPT 1.09, 1.07 and most likely other old MPT versions leave this blank (0/0), but have the "special" flag set.
        // Newer versions of MPT and OpenMPT 1.17 *always* write 4/16 here.
        // Thus, we will just ignore those old versions.
        if(m_dwLastSavedWithVersion == 0 || m_dwLastSavedWithVersion >= MAKE_VERSION_NUMERIC(1, 17, 03, 02))
        {
            m_nDefaultRowsPerBeat = pifh->highlight_minor;
            m_nDefaultRowsPerMeasure = pifh->highlight_major;
        }
#ifdef DEBUG
        if((pifh->highlight_minor | pifh->highlight_major) == 0)
        {
            Log("IT Header: Row highlight is 0");
        }
#endif
    }

    if (pifh->flags & 0x08) m_dwSongFlags |= SONG_LINEARSLIDES;
    if (pifh->flags & 0x10) m_dwSongFlags |= SONG_ITOLDEFFECTS;
    if (pifh->flags & 0x20) m_dwSongFlags |= SONG_ITCOMPATGXX;
    if ((pifh->flags & 0x80) || (pifh->special & 0x08)) m_dwSongFlags |= SONG_EMBEDMIDICFG;
    if (pifh->flags & 0x1000) m_dwSongFlags |= SONG_EXFILTERRANGE;

    assign_without_padding(song_name, pifh->songname, 26);

    // Global Volume
    m_nDefaultGlobalVolume = pifh->globalvol << 1;
    if (m_nDefaultGlobalVolume > MAX_GLOBAL_VOLUME) m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;
    if (pifh->speed) m_nDefaultSpeed = pifh->speed;
    m_nDefaultTempo = bad_max(32, pifh->tempo); // tempo 31 is possible. due to conflicts with the rest of the engine, let's just clamp it to 32.
    m_nSamplePreAmp = bad_min(pifh->mv, 128);

    // Reading Channels Pan Positions
    for (int ipan=0; ipan</*MAX_BASECHANNELS*/64; ipan++) if (pifh->chnpan[ipan] != 0xFF) //Header only has room for settings for 64 chans...
    {
        ChnSettings[ipan].nVolume = pifh->chnvol[ipan];
        ChnSettings[ipan].nPan = 128;
        if (pifh->chnpan[ipan] & 0x80) ChnSettings[ipan].dwFlags |= CHN_MUTE;
        UINT n = pifh->chnpan[ipan] & 0x7F;
        if (n <= 64) ChnSettings[ipan].nPan = n << 2;
        if (n == 100) ChnSettings[ipan].dwFlags |= CHN_SURROUND;
    }
    if (m_nChannels < GetModSpecifications().channelsMin) m_nChannels = GetModSpecifications().channelsMin;

    // Reading Song Message
    if ((pifh->special & 0x01) && (pifh->msglength) && (pifh->msglength <= dwMemLength) && (pifh->msgoffset < dwMemLength - pifh->msglength))
    {
        // Generally, IT files should use CR for line endings. However, ChibiTracker uses LF. One could do...
        // if(pifh->cwtv == 0x0214 && pifh->cmwt == 0x0214 && pifh->reserved == LittleEndian(IT_CHBI)) --> Chibi detected.
        // But we'll just use autodetection here:
        //ReadMessage(lpStream + pifh->msgoffset, pifh->msglength, leAutodetect);
    }
    // Reading orders
    UINT nordsize = pifh->ordnum;
    if(GetType() == MOD_TYPE_IT)
    {
        if(nordsize > MAX_ORDERS) nordsize = MAX_ORDERS;
        Order.ReadAsByte(lpStream + dwMemPos, nordsize, dwMemLength-dwMemPos);
        dwMemPos += pifh->ordnum;
    }
    else
    {
        if(nordsize > GetModSpecifications().ordersMax)
        {
#ifdef MODPLUG_TRACKER
            CString str;
            str.Format(str_SequenceTruncationNote, nordsize, GetModSpecifications().ordersMax);
            if(GetpModDoc() != nullptr) GetpModDoc()->AddToLog(str);
#endif // MODPLUG_TRACKER
            nordsize = GetModSpecifications().ordersMax;
        }

        if(pifh->cwtv > 0x88A && pifh->cwtv <= 0x88D)
            dwMemPos += Order.Deserialize(lpStream+dwMemPos, dwMemLength-dwMemPos);
        else
        {
            Order.ReadAsByte(lpStream + dwMemPos, nordsize, dwMemLength - dwMemPos);
            dwMemPos += pifh->ordnum;
            //Replacing 0xFF and 0xFE with new corresponding indexes
            Order.Replace(0xFE, Order.GetIgnoreIndex());
            Order.Replace(0xFF, Order.GetInvalidPatIndex());
        }
    }

    // Find the first parapointer.
    // This is used for finding out whether the edit history is actually stored in the file or not,
    // as some early versions of Schism Tracker set the history flag, but didn't save anything.
    // We will consider the history invalid if it ends after the first parapointer.
    uint32_t minptr = dwMemLength;

    // Reading Instrument Offsets
    inspos.resize(pifh->insnum);
    for(size_t n = 0; n < pifh->insnum; n++)
    {
        if(4 > dwMemLength - dwMemPos)
            return false;
        uint32_t insptr = LittleEndian(*(uint32_t *)(lpStream + dwMemPos));
        inspos[n] = insptr;
        if(insptr > 0)
        {
            minptr = bad_min(minptr, insptr);
        }
        dwMemPos += 4;
    }

    // Reading Sample Offsets
    smppos.resize(pifh->smpnum);
    for(size_t n = 0; n < pifh->smpnum; n++)
    {
        if(4 > dwMemLength - dwMemPos)
            return false;
        uint32_t smpptr = LittleEndian(*(uint32_t *)(lpStream + dwMemPos));
        smppos[n] = smpptr;
        if(smpptr > 0)
        {
            minptr = bad_min(minptr, smpptr);
        }
        dwMemPos += 4;
    }

    // Reading Pattern Offsets
    patpos.resize(pifh->patnum);
    for(size_t n = 0; n < pifh->patnum; n++)
    {
        if(4 > dwMemLength - dwMemPos)
            return false;
        uint32_t patptr = LittleEndian(*(uint32_t *)(lpStream + dwMemPos));
        patpos[n] = patptr;
        if(patptr > 0)
        {
            minptr = bad_min(minptr, patptr);
        }
        dwMemPos += 4;
    }

    // Reading Patterns Offsets
    if(patpos.size() > GetModSpecifications().patternsMax)
    {
        // Hack: Note user here if file contains more patterns than what can be read.
#ifdef MODPLUG_TRACKER
        if(GetpModDoc() != nullptr)
        {
            CString str;
            str.Format(str_PatternSetTruncationNote, patpos.size(), GetModSpecifications().patternsMax);
            GetpModDoc()->AddToLog(str);
        }
#endif // MODPLUG_TRACKER
    }

    if(pifh->special & 0x01)
    {
        minptr = bad_min(minptr, pifh->msgoffset);
    }

    // Reading IT Edit History Info
    // This is only supposed to be present if bit 1 of the special flags is set.
    // However, old versions of Schism and probably other trackers always set this bit
    // even if they don't write the edit history count. So we have to filter this out...
    // This is done by looking at the parapointers. If the history data end after
    // the first parapointer, we assume that it's actually no history data.
    if (dwMemPos + 2 < dwMemLength && (pifh->special & 0x02))
    {
        const size_t nflt = LittleEndianW(*((uint16_t*)(lpStream + dwMemPos)));
        dwMemPos += 2;

        if (nflt * 8 <= dwMemLength - dwMemPos && dwMemPos + nflt * 8 <= minptr)
        {
#ifdef MODPLUG_TRACKER
            if(GetpModDoc() != nullptr)
            {
                GetpModDoc()->GetFileHistory()->clear();
                for(size_t n = 0; n < nflt; n++)
                {
                    ITHISTORYSTRUCT itHistory = *((ITHISTORYSTRUCT *)(lpStream + dwMemPos));
                    itHistory.fatdate = LittleEndianW(itHistory.fatdate);
                    itHistory.fattime = LittleEndianW(itHistory.fattime);
                    itHistory.runtime = LittleEndian(itHistory.runtime);

                    FileHistory mptHistory;
                    MemsetZero(mptHistory);
                    // Decode FAT date and time
                    mptHistory.loadDate.tm_year = ((itHistory.fatdate >> 9) & 0x7F) + 80;
                    mptHistory.loadDate.tm_mon = CLAMP((itHistory.fatdate >> 5) & 0x0F, 1, 12) - 1;
                    mptHistory.loadDate.tm_mday = CLAMP(itHistory.fatdate & 0x1F, 1, 31);
                    mptHistory.loadDate.tm_hour = CLAMP((itHistory.fattime >> 11) & 0x1F, 0, 23);
                    mptHistory.loadDate.tm_min = CLAMP((itHistory.fattime >> 5) & 0x3F, 0, 59);
                    mptHistory.loadDate.tm_sec = CLAMP((itHistory.fattime & 0x1F) * 2, 0, 59);
                    mptHistory.openTime = itHistory.runtime * (HISTORY_TIMER_PRECISION / 18.2f);
                    GetpModDoc()->GetFileHistory()->push_back(mptHistory);

#ifdef DEBUG
                    const uint32_t seconds = (uint32_t)(((double)itHistory.runtime) / 18.2f);
                    CHAR stime[128];
                    wsprintf(stime, "IT Edit History: Loaded %04u-%02u-%02u %02u:%02u:%02u, open for %u:%02u:%02u (%u ticks)\n", ((itHistory.fatdate >> 9) & 0x7F) + 1980, (itHistory.fatdate >> 5) & 0x0F, itHistory.fatdate & 0x1F, (itHistory.fattime >> 11) & 0x1F, (itHistory.fattime >> 5) & 0x3F, (itHistory.fattime & 0x1F) * 2, seconds / 3600, (seconds / 60) % 60, seconds % 60, itHistory.runtime);
                    Log(stime);
#endif // DEBUG

                    dwMemPos += 8;
                }
            } else
#endif // MODPLUG_TRACKER
            {
                dwMemPos += nflt * 8;
            }
        } else
        {
            // Oops, we were not supposed to read this.
            dwMemPos -= 2;
        }
    }
    // Another non-conforming application is unmo3 < v2.4.0.1, which doesn't set the special bit
    // at all, but still writes the two edit history length bytes (zeroes)...
    else if(dwMemPos + 2 < dwMemLength && pifh->highlight_major == 0 && pifh->highlight_minor == 0 && pifh->cmwt == 0x0214 && pifh->cwtv == 0x0214 && pifh->reserved == 0 && (pifh->special & (0x02|0x04)) == 0)
    {
        const size_t nflt = LittleEndianW(*((uint16_t*)(lpStream + dwMemPos)));
        if(nflt == 0)
        {
            dwMemPos += 2;
        }
    }

    // Reading MIDI Output & Macros
    if (m_dwSongFlags & SONG_EMBEDMIDICFG)
    {
        if (dwMemPos + sizeof(MODMIDICFG) < dwMemLength)
        {
            memcpy(&m_MidiCfg, lpStream + dwMemPos, sizeof(MODMIDICFG));
            SanitizeMacros();
            dwMemPos += sizeof(MODMIDICFG);
        }
    }
    // Ignore MIDI data. Fixes some files like denonde.it that were made with old versions of Impulse Tracker (which didn't support Zxx filters) and have Zxx effects in the patterns.
    if (pifh->cwtv < 0x0214)
    {
        MemsetZero(m_MidiCfg.szMidiSFXExt);
        MemsetZero(m_MidiCfg.szMidiZXXExt);
        m_dwSongFlags |= SONG_EMBEDMIDICFG;
    }

    // Read pattern names: "PNAM"
    char *patNames = nullptr;
    UINT patNamesLen = 0;
    if ((dwMemPos + 8 < dwMemLength) && (*((uint32_t *)(lpStream+dwMemPos)) == 0x4d414e50))
    {
        patNamesLen = *((uint32_t *)(lpStream + dwMemPos + 4));
        dwMemPos += 8;
        if ((dwMemPos + patNamesLen <= dwMemLength) && (patNamesLen > 0))
        {
            patNames = (char *)(lpStream + dwMemPos);
            dwMemPos += patNamesLen;
        }
    }

    m_nChannels = GetModSpecifications().channelsMin;
    // Read channel names: "CNAM"
    if ((dwMemPos + 8 < dwMemLength) && (*((uint32_t *)(lpStream+dwMemPos)) == 0x4d414e43))
    {
        UINT len = *((uint32_t *)(lpStream+dwMemPos+4));
        dwMemPos += 8;
        if ((dwMemPos + len <= dwMemLength) && (len <= MAX_BASECHANNELS*MAX_CHANNELNAME))
        {
            UINT n = len / MAX_CHANNELNAME;
            if (n > m_nChannels) m_nChannels = n;
            for (UINT i=0; i<n; i++)
            {
                memcpy(ChnSettings[i].szName, (lpStream+dwMemPos+i*MAX_CHANNELNAME), MAX_CHANNELNAME);
                ChnSettings[i].szName[MAX_CHANNELNAME-1] = 0;
            }
            dwMemPos += len;
        }
    }
    // Read mix plugins information
    if (dwMemPos + 8 < dwMemLength)
    {
        dwMemPos += LoadMixPlugins(lpStream+dwMemPos, dwMemLength-dwMemPos);
    }

    //UINT npatterns = pifh->patnum;
    UINT npatterns = patpos.size();

    if (npatterns > GetModSpecifications().patternsMax)
        npatterns = GetModSpecifications().patternsMax;

    // Checking for unused channels
    for (UINT patchk=0; patchk<npatterns; patchk++)
    {
        memset(chnmask, 0, sizeof(chnmask));

        if ((!patpos[patchk]) || ((uint32_t)patpos[patchk] >= dwMemLength - 4))
            continue;

        UINT len = *((uint16_t *)(lpStream+patpos[patchk]));
        UINT rows = *((uint16_t *)(lpStream+patpos[patchk]+2));

        if(rows <= ModSpecs::itEx.patternRowsMax && rows > ModSpecs::it.patternRowsMax)
        {
            //interpretModPlugMade = true;    // Chibi also does this.
            hasModPlugExtensions = true;
        }

        if ((rows < GetModSpecifications().patternRowsMin) || (rows > GetModSpecifications().patternRowsMax))
            continue;

        if (patpos[patchk]+8+len > dwMemLength)
            continue;

        UINT i = 0;
        const uint8_t *p = lpStream+patpos[patchk]+8;
        UINT nrow = 0;

        while (nrow<rows)
        {
            if (i >= len) break;
            uint8_t b = p[i++]; // p is the bytestream offset at current pattern_position
            if (!b)
            {
                nrow++;
                continue;
            }

            UINT ch = b & IT_bitmask_patternChanField_c;   // 0x7f We have some data grab a byte keeping only 7 bits
            if (ch)
                ch = (ch - 1);// & IT_bitmask_patternChanMask_c;   // 0x3f mask of the byte again, keeping only 6 bits

            if (b & IT_bitmask_patternChanEnabled_c)            // 0x80 check if the upper bit is enabled.
            {
                if (i >= len)
                    break;
                chnmask[ch] = p[i++];       // set the channel mask for this channel.
            }
            // Channel used
            if (chnmask[ch] & 0x0F)         // if this channel is used set m_nChannels
            {
// -> CODE#0006
// -> DESC="misc quantity changes"
//                            if ((ch >= m_nChannels) && (ch < 64)) m_nChannels = ch+1;
                if ((ch >= m_nChannels) && (ch < MAX_BASECHANNELS)) m_nChannels = ch+1;
// -! BEHAVIOUR_CHANGE#0006
            }
            // Now we actually update the pattern-row entry the note,instrument etc.
            // Note
            if (chnmask[ch] & 1) i++;
            // Instrument
            if (chnmask[ch] & 2) i++;
            // Volume
            if (chnmask[ch] & 4) i++;
            // Effect
            if (chnmask[ch] & 8) i += 2;
            if (i >= len) break;
        }
    }


    // Reading Instruments
    m_nInstruments = 0;
    if (pifh->flags & 0x04) m_nInstruments = pifh->insnum;
    if (m_nInstruments >= MAX_INSTRUMENTS) m_nInstruments = MAX_INSTRUMENTS-1;
    for (UINT nins=0; nins<m_nInstruments; nins++)
    {
        if ((inspos[nins] > 0) && (inspos[nins] < dwMemLength - (pifh->cmwt < 0x200 ? sizeof(ITOLDINSTRUMENT) : sizeof(ITINSTRUMENT))))
        {
            modinstrument_t *pIns = new modinstrument_t;
            if (!pIns) continue;
            Instruments[nins+1] = pIns;
            memset(pIns, 0, sizeof(modinstrument_t));
            ITInstrToMPT(lpStream + inspos[nins], pIns, pifh->cmwt);
        }
    }

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
    // In order to properly compute the position, in file, of eventual extended settings
    // such as custom ramping we need to keep the "real" size of the last sample as those extra
    // setting will follow this sample in the file
    UINT lastSampleOffset = 0;
    if (pifh->smpnum > 0) {
        lastSampleOffset = smppos[pifh->smpnum - 1] + sizeof(ITSAMPLESTRUCT);
    }
// -! NEW_FEATURE#0027

    // Reading Samples
    m_nSamples = bad_min(pifh->smpnum, MAX_SAMPLES - 1);
    for (UINT nsmp = 0; nsmp < m_nSamples; nsmp++) if ((smppos[nsmp]) && (smppos[nsmp] <= dwMemLength - sizeof(ITSAMPLESTRUCT)))
    {
        ITSAMPLESTRUCT *pis = (ITSAMPLESTRUCT *)(lpStream+smppos[nsmp]);
        if (pis->id == LittleEndian(IT_IMPS))
        {
            modsample_t *pSmp = &Samples[nsmp+1];
            memcpy(pSmp->legacy_filename, pis->filename, 12);
            SpaceToNullStringFixed<12>(pSmp->legacy_filename);
            pSmp->flags = sflags_ty();
            pSmp->length = 0;
            pSmp->loop_start = pis->loopbegin;
            pSmp->loop_end = pis->loopend;
            pSmp->sustain_start = pis->susloopbegin;
            pSmp->sustain_end = pis->susloopend;
            pSmp->c5_samplerate = pis->C5Speed;
            if (!pSmp->c5_samplerate) pSmp->c5_samplerate = 8363;
            if (pis->C5Speed < 256) pSmp->c5_samplerate = 256;
            pSmp->default_volume = pis->vol << 2;
            if (pSmp->default_volume > 256) pSmp->default_volume = 256;
            pSmp->global_volume = pis->gvl;
            if (pSmp->global_volume > 64) pSmp->global_volume = 64;
            if (pis->flags & 0x10) bitset_add(pSmp->flags, sflag_ty::Loop);
            if (pis->flags & 0x20) bitset_add(pSmp->flags, sflag_ty::SustainLoop);
            if (pis->flags & 0x40) bitset_add(pSmp->flags, sflag_ty::BidiLoop);
            if (pis->flags & 0x80) bitset_add(pSmp->flags, sflag_ty::BidiSustainLoop);
            pSmp->default_pan = (pis->dfp & 0x7F) << 2;
            if (pSmp->default_pan > 256) pSmp->default_pan = 256;
            if (pis->dfp & 0x80) bitset_add(pSmp->flags, sflag_ty::ForcedPanning);
            pSmp->vibrato_type = autovibit2xm[pis->vit & 7];
            pSmp->vibrato_rate = pis->vis;
            pSmp->vibrato_depth = pis->vid & 0x7F;
            pSmp->vibrato_sweep = pis->vir; //(pis->vir + 3) / 4;
            if(pSmp->vibrato_sweep == 0 && (pSmp->vibrato_depth || pSmp->vibrato_rate) && m_dwLastSavedWithVersion && m_dwLastSavedWithVersion < MAKE_VERSION_NUMERIC(1, 17, 03, 02))
            {
                pSmp->vibrato_sweep = 255;    // Let's correct this little stupid mistake in history.
            }

            if(pis->samplepointer) lastSampleOffset = pis->samplepointer; // MPTX hack

            if ((pis->samplepointer) && (pis->samplepointer < dwMemLength) && (pis->length))
            {
                pSmp->length = pis->length;
                if (pSmp->length > MAX_SAMPLE_LENGTH) pSmp->length = MAX_SAMPLE_LENGTH;
                UINT flags = (pis->cvt & 1) ? RS_PCM8S : RS_PCM8U;
                if (pis->flags & 2)    // 16-bit
                {
                    flags += 5;
                    if (pis->flags & 4 && pifh->cwtv >= 0x214) flags |= RSF_STEREO;    // some old version of IT didn't clear the stereo flag when importing samples. Luckily, all other trackers are identifying as IT 2.14+, so let's check for old IT versions.
                    pSmp->sample_tag = stag_ty::Int16;
                    // IT 2.14 16-bit packed sample?
                    if (pis->flags & 8) flags = ((pifh->cmwt >= 0x215) && (pis->cvt & 4)) ? RS_IT21516 : RS_IT21416;
                } else    // 8-bit
                {
                    pSmp->sample_tag = stag_ty::Int8;
                    if (pis->flags & 4 && pifh->cwtv >= 0x214) flags |= RSF_STEREO;    // some old version of IT didn't clear the stereo flag when importing samples. Luckily, all other trackers are identifying as IT 2.14+, so let's check for old IT versions.
                    if (pis->cvt == 0xFF) flags = RS_ADPCM4; else
                    // IT 2.14 8-bit packed sample?
                    if (pis->flags & 8)    flags =        ((pifh->cmwt >= 0x215) && (pis->cvt & 4)) ? RS_IT2158 : RS_IT2148;
                }
// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"
//                            ReadSample(&Ins[nsmp+1], flags, (LPSTR)(lpStream+pis->samplepointer), dwMemLength - pis->samplepointer);
                lastSampleOffset = pis->samplepointer + ReadSample(&Samples[nsmp+1], flags, (LPSTR)(lpStream+pis->samplepointer), dwMemLength - pis->samplepointer);
// -! NEW_FEATURE#0027
            }
        }
        memcpy(m_szNames[nsmp + 1], pis->name, 26);
        SpaceToNullStringFixed<26>(m_szNames[nsmp + 1]);
    }
    m_nSamples = bad_max(1, m_nSamples);

    m_nMinPeriod = 8;
    m_nMaxPeriod = 0xF000;

// -> CODE#0027
// -> DESC="per-instrument volume ramping setup"

    // Compute extra instruments settings position
    if(lastSampleOffset > 0) dwMemPos = lastSampleOffset;

    // Load instrument and song extensions.
    if(mptStartPos >= dwMemPos)
    {
        if(interpretModPlugMade)
        {
            m_nMixLevels = mixLevels_original;
        }
        const uint8_t * ptr = LoadExtendedInstrumentProperties(lpStream + dwMemPos, lpStream + mptStartPos, &interpretModPlugMade);
        LoadExtendedSongProperties(GetType(), ptr, lpStream, mptStartPos, &interpretModPlugMade);
    }

// -! NEW_FEATURE#0027


    // Reading Patterns
    Patterns.ResizeArray(bad_max(MAX_PATTERNS, npatterns));
    for (UINT npat=0; npat<npatterns; npat++)
    {
        if ((!patpos[npat]) || ((uint32_t)patpos[npat] >= dwMemLength - 4))
        {
            if(Patterns.Insert(npat, 64))
            {
#ifdef MODPLUG_TRACKER
                CString s;
                s.Format(TEXT("Allocating patterns failed starting from pattern %u\n"), npat);
                if(GetpModDoc() != nullptr) GetpModDoc()->AddToLog(s);
#endif // MODPLUG_TRACKER
                break;
            }
            // Now (after the Insert() call), we can read the pattern name.
            CopyPatternName(Patterns[npat], &patNames, patNamesLen);
            continue;
        }
        UINT len = *((uint16_t *)(lpStream+patpos[npat]));
        UINT rows = *((uint16_t *)(lpStream+patpos[npat]+2));
        if ((rows < GetModSpecifications().patternRowsMin) || (rows > GetModSpecifications().patternRowsMax)) continue;
        if (patpos[npat]+8+len > dwMemLength) continue;

        if(Patterns.Insert(npat, rows)) continue;

        // Now (after the Insert() call), we can read the pattern name.
        CopyPatternName(Patterns[npat], &patNames, patNamesLen);

        memset(lastvalue, 0, sizeof(lastvalue));
        memset(chnmask, 0, sizeof(chnmask));
        modplug::tracker::modevent_t *m = Patterns[npat];
        UINT i = 0;
        const uint8_t *p = lpStream+patpos[npat]+8;
        UINT nrow = 0;
        while (nrow<rows)
        {
            if (i >= len) break;
            uint8_t b = p[i++];
            if (!b)
            {
                nrow++;
                m+=m_nChannels;
                continue;
            }

            UINT ch = b & IT_bitmask_patternChanField_c; // 0x7f

            if (ch)
                ch = (ch - 1); //& IT_bitmask_patternChanMask_c; // 0x3f

            if (b & IT_bitmask_patternChanEnabled_c)  // 0x80
            {
                if (i >= len)
                    break;
                chnmask[ch] = p[i++];
            }

            // Now we grab the data for this particular row/channel.

            if ((chnmask[ch] & 0x10) && (ch < m_nChannels))
            {
                m[ch].note = lastvalue[ch].note;
            }
            if ((chnmask[ch] & 0x20) && (ch < m_nChannels))
            {
                m[ch].instr = lastvalue[ch].instr;
            }
            if ((chnmask[ch] & 0x40) && (ch < m_nChannels))
            {
                m[ch].volcmd = lastvalue[ch].volcmd;
                m[ch].vol = lastvalue[ch].vol;
            }
            if ((chnmask[ch] & 0x80) && (ch < m_nChannels))
            {
                m[ch].command = lastvalue[ch].command;
                m[ch].param = lastvalue[ch].param;
            }
            if (chnmask[ch] & 1)    // Note
            {
                if (i >= len) break;
                UINT note = p[i++];
                if (ch < m_nChannels)
                {
                    if (note < 0x80) note++;
                    if(!(m_nType & MOD_TYPE_MPT))
                    {
                        if(note > NoteMax && note < 0xFD) note = NoteFade;
                        else if(note == 0xFD) note = NoteNone;
                    }
                    m[ch].note = note;
                    lastvalue[ch].note = note;
                    channels_used[ch] = TRUE;
                }
            }
            if (chnmask[ch] & 2)
            {
                if (i >= len) break;
                UINT instr = p[i++];
                if (ch < m_nChannels)
                {
                    m[ch].instr = instr;
                    lastvalue[ch].instr = instr;
                }
            }
            if (chnmask[ch] & 4)
            {
                if (i >= len) break;
                UINT vol = p[i++];
                if (ch < m_nChannels)
                {
                    // 0-64: Set Volume
                    if (vol <= 64) { m[ch].volcmd = VolCmdVol; m[ch].vol = vol; } else
                    // 128-192: Set Panning
                    if ((vol >= 128) && (vol <= 192)) { m[ch].volcmd = VolCmdPan; m[ch].vol = vol - 128; } else
                    // 65-74: Fine Volume Up
                    if (vol < 75) { m[ch].volcmd = VolCmdFineUp; m[ch].vol = vol - 65; } else
                    // 75-84: Fine Volume Down
                    if (vol < 85) { m[ch].volcmd = VolCmdFineDown; m[ch].vol = vol - 75; } else
                    // 85-94: Volume Slide Up
                    if (vol < 95) { m[ch].volcmd = VolCmdSlideUp; m[ch].vol = vol - 85; } else
                    // 95-104: Volume Slide Down
                    if (vol < 105) { m[ch].volcmd = VolCmdSlideDown; m[ch].vol = vol - 95; } else
                    // 105-114: Pitch Slide Up
                    if (vol < 115) { m[ch].volcmd = VolCmdPortamentoDown; m[ch].vol = vol - 105; } else
                    // 115-124: Pitch Slide Down
                    if (vol < 125) { m[ch].volcmd = VolCmdPortamentoUp; m[ch].vol = vol - 115; } else
                    // 193-202: Portamento To
                    if ((vol >= 193) && (vol <= 202)) { m[ch].volcmd = VolCmdPortamento; m[ch].vol = vol - 193; } else
                    // 203-212: Vibrato depth
                    if ((vol >= 203) && (vol <= 212))
                    {
                        m[ch].volcmd = VolCmdVibratoDepth; m[ch].vol = vol - 203;
                        // Old versions of ModPlug saved this as vibrato speed instead, so let's fix that
                        if(m_dwLastSavedWithVersion && m_dwLastSavedWithVersion <= MAKE_VERSION_NUMERIC(1, 17, 02, 54))
                            m[ch].volcmd = VolCmdVibratoSpeed;
                    } else
                    // 213-222: Unused (was velocity)
                    // 223-232: Offset
                    if ((vol >= 223) && (vol <= 232)) { m[ch].volcmd = VolCmdOffset; m[ch].vol = vol - 223; } //rewbs.volOff
                    lastvalue[ch].volcmd = m[ch].volcmd;
                    lastvalue[ch].vol = m[ch].vol;
                }
            }
            // Reading command/param
            if (chnmask[ch] & 8)
            {
                if (i > len - 2) break;
                UINT cmd = p[i++];
                UINT param = p[i++];
                if (ch < m_nChannels)
                {
                    if (cmd)
                    {
                        //XXXih: gross
                        m[ch].command = (modplug::tracker::cmd_t) cmd;
                        m[ch].param = param;
                        S3MConvert(&m[ch], true);
                        lastvalue[ch].command = m[ch].command;
                        lastvalue[ch].param = m[ch].param;
                    }
                }
            }
        }
    }

    if(m_dwLastSavedWithVersion < MAKE_VERSION_NUMERIC(1, 17, 2, 50))
    {
        SetModFlag(MSF_COMPATIBLE_PLAY, false);
        SetModFlag(MSF_MIDICC_BUGEMULATION, true);
        SetModFlag(MSF_OLDVOLSWING, true);
    }

    if(GetType() == MOD_TYPE_IT)
    {
        // Set appropriate mod flags if the file was not made with MPT.
        if(!interpretModPlugMade)
        {
            SetModFlag(MSF_MIDICC_BUGEMULATION, false);
            SetModFlag(MSF_OLDVOLSWING, false);
            SetModFlag(MSF_COMPATIBLE_PLAY, true);
        }
    }
    else
    {
        //START - mpt specific:
        //Using member cwtv on pifh as the version number.
        const uint16_t version = pifh->cwtv;
        if(version > 0x889)
        {
            const char* const cpcMPTStart = reinterpret_cast<const char*>(lpStream + mptStartPos);
            std::istrstream iStrm(cpcMPTStart, dwMemLength-mptStartPos);

            if(version >= 0x88D)
            {
                srlztn::Ssb ssb(iStrm);
                ssb.BeginRead("mptm", MptVersion::num);
                ssb.ReadItem(GetTuneSpecificTunings(), "0", 1, &ReadTuningCollection);
                ssb.ReadItem(*this, "1", 1, &ReadTuningMap);
                ssb.ReadItem(Order, "2", 1, &ReadModSequenceOld);
                ssb.ReadItem(Patterns, FileIdPatterns, strlen(FileIdPatterns), &ReadModPatterns);
                ssb.ReadItem(Order, FileIdSequences, strlen(FileIdSequences), &ReadModSequences);

                if (ssb.m_Status & srlztn::SNT_FAILURE)
                {
#ifdef MODPLUG_TRACKER
                    if(GetpModDoc() != nullptr) GetpModDoc()->AddToLog(_T("Unknown error occured while deserializing file.\n"));
#endif // MODPLUG_TRACKER
                }
            }
            else //Loading for older files.
            {
                if(GetTuneSpecificTunings().Deserialize(iStrm))
                {
#ifdef MODPLUG_TRACKER
                    if(GetpModDoc() != nullptr) GetpModDoc()->AddToLog(_T("Error occured - loading failed while trying to load tune specific tunings.\n"));
#endif // MODPLUG_TRACKER
                }
                else
                {
                    ReadTuningMap(iStrm, *this);
                }
            }
        } //version condition(MPT)
    }

    return true;
}
//end plastiq: code readability improvements

#ifndef MODPLUG_NO_FILESAVE

// Save edit history. Pass a null pointer for *f to retrieve the number of bytes that would be written.
uint32_t SaveITEditHistory(const module_renderer *pSndFile, FILE *f)
//----------------------------------------------------------
{
#ifdef MODPLUG_TRACKER
    CModDoc *pModDoc = pSndFile->GetpModDoc();
    const size_t num = (pModDoc != nullptr) ? pModDoc->GetFileHistory()->size() + 1 : 0;    // + 1 for this session
#else
    const size_t num = 0;
#endif // MODPLUG_TRACKER

    uint16_t fnum = bad_min(num, UINT16_MAX);    // Number of entries that are actually going to be written
    const size_t bytes_written = 2 + fnum * 8;    // Number of bytes that are actually going to be written

    if(f == nullptr)
        return bytes_written;

    // Write number of history entries
    fnum = LittleEndianW(fnum);
    fwrite(&fnum, 2, 1, f);

#ifdef MODPLUG_TRACKER
    // Write history data
    const size_t start = (num > UINT16_MAX) ? num - UINT16_MAX : 0;
    for(size_t n = start; n < num; n++)
    {
        tm loadDate;
        MemsetZero(loadDate);
        uint32_t openTime;

        if(n < num - 1)
        {
            // Previous timestamps
            const FileHistory *mptHistory = &(pModDoc->GetFileHistory()->at(n));
            loadDate = mptHistory->loadDate;
            openTime = mptHistory->openTime * (18.2f / HISTORY_TIMER_PRECISION);
        } else
        {
            // Current ("new") timestamp
            const time_t creationTime = pModDoc->GetCreationTime();
            //localtime_s(&loadDate, &creationTime);
            const tm* const p = localtime(&creationTime);
            if (p != nullptr)
                loadDate = *p;
            else if (pSndFile->GetModDocPtr() != nullptr)
                pSndFile->GetModDocPtr()->AddLogEvent(LogEventUnexpectedError,
                                                      __FUNCTION__,
                                                      _T("localtime() returned nullptr."));
            openTime = (uint32_t)((double)difftime(time(nullptr), creationTime) * 18.2f);
        }

        ITHISTORYSTRUCT itHistory;
        // Create FAT file dates
        itHistory.fatdate = loadDate.tm_mday | ((loadDate.tm_mon + 1) << 5) | ((loadDate.tm_year - 80) << 9);
        itHistory.fattime = (loadDate.tm_sec / 2) | (loadDate.tm_min << 5) | (loadDate.tm_hour << 11);
        itHistory.runtime = openTime;

        itHistory.fatdate = LittleEndianW(itHistory.fatdate);
        itHistory.fattime = LittleEndianW(itHistory.fattime);
        itHistory.runtime = LittleEndian(itHistory.runtime);

        fwrite(&itHistory, 1, sizeof(itHistory), f);
    }
#endif // MODPLUG_TRACKER

    return bytes_written;
}

#pragma warning(disable:4100)


bool module_renderer::SaveIT(LPCSTR lpszFileName, UINT nPacking)
//---------------------------------------------------------
{
    uint32_t dwPatNamLen, dwChnNamLen;
    ITFILEHEADER header;
    ITINSTRUMENT iti;
    ITSAMPLESTRUCT itss;
    vector<bool>smpcount(GetNumSamples(), false);
    uint32_t inspos[MAX_INSTRUMENTS];
    vector<uint32_t> patpos;
    uint32_t smppos[MAX_SAMPLES];
    uint32_t dwPos = 0, dwHdrPos = 0, dwExtra = 0;
    uint16_t patinfo[4];
// -> CODE#0006
// -> DESC="misc quantity changes"
//    uint8_t chnmask[64];
//    modplug::tracker::modcommand_t lastvalue[64];
    uint8_t chnmask[MAX_BASECHANNELS];
    modplug::tracker::modevent_t lastvalue[MAX_BASECHANNELS];
// -! BEHAVIOUR_CHANGE#0006
    uint8_t buf[8 * MAX_BASECHANNELS];
    FILE *f;


    if ((!lpszFileName) || ((f = fopen(lpszFileName, "wb")) == NULL)) return false;


    memset(inspos, 0, sizeof(inspos));
    memset(smppos, 0, sizeof(smppos));
    // Writing Header
    memset(&header, 0, sizeof(header));
    dwPatNamLen = 0;
    dwChnNamLen = 0;
    header.id = LittleEndian(IT_IMPM);
    copy_with_padding(header.songname, 26, this->song_name);

    header.highlight_minor = (uint8_t)bad_min(m_nDefaultRowsPerBeat, 0xFF);
    header.highlight_major = (uint8_t)bad_min(m_nDefaultRowsPerMeasure, 0xFF);

    if(GetType() == MOD_TYPE_MPT)
    {
        if(!Order.NeedsExtraDatafield()) header.ordnum = Order.size();
        else header.ordnum = bad_min(Order.size(), MAX_ORDERS); //Writing MAX_ORDERS at bad_max here, and if there's more, writing them elsewhere.

        //Crop unused orders from the end.
        while(header.ordnum > 1 && Order[header.ordnum - 1] == Order.GetInvalidPatIndex()) header.ordnum--;
    } else
    {
        // An additional "---" pattern is appended so Impulse Tracker won't ignore the last order item.
        // Interestingly, this can exceed IT's 256 order limit. Also, IT will always save at least two orders.
        header.ordnum = bad_min(Order.GetLengthTailTrimmed(), ModSpecs::itEx.ordersMax) + 1;
        if(header.ordnum < 2) header.ordnum = 2;
    }

    header.insnum = m_nInstruments;
    header.smpnum = m_nSamples;
    header.patnum = (GetType() == MOD_TYPE_MPT) ? Patterns.Size() : MAX_PATTERNS;
    if(Patterns.Size() < header.patnum) Patterns.ResizeArray(header.patnum);
    while ((header.patnum > 0) && (!Patterns[header.patnum - 1])) header.patnum--;

    patpos.resize(header.patnum, 0);

    //VERSION
    if(GetType() == MOD_TYPE_MPT)
    {
        header.cwtv = verMptFileVer;    // Used in OMPT-hack versioning.
        header.cmwt = 0x888;
    }
    else //IT
    {
        MptVersion::VersionNum vVersion = MptVersion::num;
        header.cwtv = LittleEndianW(0x5000 | (uint16_t)((vVersion >> 16) & 0x0FFF)); // format: txyy (t = tracker ID, x = version major, yy = version minor), e.g. 0x5117 (OpenMPT = 5, 117 = v1.17)
        header.cmwt = LittleEndianW(0x0214);    // Common compatible tracker :)
        // hack from schism tracker:
        for(modplug::tracker::instrumentindex_t nIns = 1; nIns <= GetNumInstruments(); nIns++)
        {
            if(Instruments[nIns] && Instruments[nIns]->pitch_envelope.flags & ENV_FILTER)
            {
                header.cmwt = LittleEndianW(0x0217);
                break;
            }
        }
        // This way, we indicate that the file will most likely contain OpenMPT hacks. Compatibility export puts 0 here.
        header.reserved = LittleEndian(IT_OMPT);
    }

    header.flags = 0x0001;
    header.special = 0x02 | 0x04 ;    // 0x02: embed file edit history, 0x04: store row highlight in the header
    if (m_nInstruments) header.flags |= 0x04;
    if (m_dwSongFlags & SONG_LINEARSLIDES) header.flags |= 0x08;
    if (m_dwSongFlags & SONG_ITOLDEFFECTS) header.flags |= 0x10;
    if (m_dwSongFlags & SONG_ITCOMPATGXX) header.flags |= 0x20;
    if (m_dwSongFlags & SONG_EXFILTERRANGE) header.flags |= 0x1000;
    header.globalvol = m_nDefaultGlobalVolume >> 1;
    header.mv = CLAMP(m_nSamplePreAmp, 0, 128);
    header.speed = m_nDefaultSpeed;
    header.tempo = bad_min(m_nDefaultTempo, 255);  //Limit this one to 255, we save the real one as an extension below.
    header.sep = 128; // pan separation
    dwHdrPos = sizeof(header) + header.ordnum;
    // Channel Pan and Volume
    memset(header.chnpan, 0xA0, 64);
    memset(header.chnvol, 64, 64);
    for (UINT ich=0; ich</*m_nChannels*/64; ich++) //Header only has room for settings for 64 chans...
    {
        header.chnpan[ich] = ChnSettings[ich].nPan >> 2;
        if (ChnSettings[ich].dwFlags & CHN_SURROUND) header.chnpan[ich] = 100;
        header.chnvol[ich] = ChnSettings[ich].nVolume;
        if (ChnSettings[ich].dwFlags & CHN_MUTE) header.chnpan[ich] |= 0x80;
    }

    for (UINT ich=0; ich<m_nChannels; ich++)
    {
        if (ChnSettings[ich].szName[0])
        {
            dwChnNamLen = (ich+1) * MAX_CHANNELNAME;
        }
    }
    if (dwChnNamLen) dwExtra += dwChnNamLen + 8;

    if (m_dwSongFlags & SONG_EMBEDMIDICFG)
    {
        header.flags |= 0x80;
        header.special |= 0x08;
        dwExtra += sizeof(MODMIDICFG);
    }
    // Pattern Names
    const modplug::tracker::patternindex_t numNamedPats = Patterns.GetNumNamedPatterns();
    if (numNamedPats > 0)
    {
        dwExtra += (numNamedPats * MAX_PATTERNNAME) + 8;
    }
    // Mix Plugins
    dwExtra += SaveMixPlugins(NULL, TRUE);
    dwExtra += SaveITEditHistory(this, nullptr);    // Just calculate the size of this extra block for now.
    // Comments
    if (m_lpszSongComments)
    {
        header.special |= 1;
        header.msglength = strlen(m_lpszSongComments)+1;
        header.msgoffset = dwHdrPos + dwExtra + header.insnum*4 + header.patnum*4 + header.smpnum*4;
    }
    // Write file header
    fwrite(&header, 1, sizeof(header), f);
    Order.WriteAsByte(f, header.ordnum);
    if (header.insnum) fwrite(inspos, 4, header.insnum, f);
    if (header.smpnum) fwrite(smppos, 4, header.smpnum, f);
    if (header.patnum) fwrite(&patpos[0], 4, header.patnum, f);
    // Writing edit history information
    SaveITEditHistory(this, f);
    // Writing midi cfg
    if (header.flags & 0x80)
    {
        fwrite(&m_MidiCfg, 1, sizeof(MODMIDICFG), f);
    }
    // Writing pattern names
    if (numNamedPats)
    {
        uint32_t d = 0x4d414e50;
        fwrite(&d, 1, 4, f);
        d = numNamedPats * MAX_PATTERNNAME;
        fwrite(&d, 1, 4, f);

        for(modplug::tracker::patternindex_t nPat = 0; nPat < numNamedPats; nPat++)
        {
            char name[MAX_PATTERNNAME];
            MemsetZero(name);
            Patterns[nPat].GetName(name, MAX_PATTERNNAME);
            fwrite(name, 1, MAX_PATTERNNAME, f);
        }
    }
    // Writing channel names
    if (dwChnNamLen)
    {
        uint32_t d = 0x4d414e43;
        fwrite(&d, 1, 4, f);
        fwrite(&dwChnNamLen, 1, 4, f);
        UINT nChnNames = dwChnNamLen / MAX_CHANNELNAME;
        for (UINT inam=0; inam<nChnNames; inam++)
        {
            fwrite(ChnSettings[inam].szName, 1, MAX_CHANNELNAME, f);
        }
    }
    // Writing mix plugins info
    SaveMixPlugins(f, FALSE);
    // Writing song message
    dwPos = dwHdrPos + dwExtra + (header.insnum + header.smpnum + header.patnum) * 4;
    if (header.special & 1)
    {
        dwPos += strlen(m_lpszSongComments) + 1;
        fwrite(m_lpszSongComments, 1, strlen(m_lpszSongComments)+1, f);
    }
    // Writing instruments
    for (UINT nins=1; nins<=header.insnum; nins++)
    {
        bool bKbdEx = false;    // extended sample map (for samples > 255)
        uint8_t keyboardex[NoteMax];

        memset(&iti, 0, sizeof(iti));
        iti.id = LittleEndian(IT_IMPI);    // "IMPI"
        //iti.trkvers = 0x211;
        iti.trkvers = 0x220;    //rewbs.itVersion
        if (Instruments[nins])
        {
            modinstrument_t *pIns = Instruments[nins];
            memcpy(iti.filename, pIns->legacy_filename, 12);
            memcpy(iti.name, pIns->name, 26);
            iti.mbank = pIns->midi_bank;
            iti.mpr = pIns->midi_program;
            if(pIns->midi_channel || pIns->nMixPlug == 0)
            {
                // default. prefer midi channel over mixplug to keep the semantics intact.
                iti.mch = pIns->midi_channel;
            } else
            {
                // keep compatibility with MPT 1.16's instrument format if possible, as XMPlay/BASS also uses this.
                iti.mch = pIns->nMixPlug + 128;
            }
            iti.nna = pIns->new_note_action;
            //if (pIns->duplicate_check_type<DCT_PLUGIN) iti.dct = pIns->duplicate_check_type; else iti.dct =0;
            iti.dct = pIns->duplicate_check_type; //rewbs.instroVSTi: will other apps barf if they get an unknown DCT?
            iti.dca = pIns->duplicate_note_action;
            iti.fadeout = bad_min(pIns->fadeout >> 5, 256);
            iti.pps = pIns->pitch_pan_separation;
            iti.ppc = pIns->pitch_pan_center;
            iti.gbv = (uint8_t)(pIns->global_volume << 1);
            iti.dfp = (uint8_t)(pIns->default_pan >> 2);
            if (!(pIns->flags & INS_SETPANNING)) iti.dfp |= 0x80;
            iti.rv = pIns->random_volume_weight;
            iti.rp = pIns->random_pan_weight;
            iti.ifc = pIns->default_filter_cutoff;
            iti.ifr = pIns->default_filter_resonance;
            iti.nos = 0;
            for (UINT i=0; i<NoteMax; i++) if (pIns->Keyboard[i] < MAX_SAMPLES)
            {
                const UINT smp = pIns->Keyboard[i];
                if (smp && smp <= GetNumSamples() && !smpcount[smp - 1])
                {
                    smpcount[smp - 1] = true;
                    iti.nos++;
                }
                iti.keyboard[i*2] = (pIns->NoteMap[i] >= NoteMin && pIns->NoteMap[i] <= NoteMax) ? (pIns->NoteMap[i] - 1) : i;
                iti.keyboard[i*2+1] = smp;
                if (smp > 0xff) bKbdEx = true;
                keyboardex[i] = (smp>>8);
            } else keyboardex[i] = 0;
            // Writing Volume envelope
            MPTEnvToIT(&pIns->volume_envelope, &iti.volenv, 0, 64);
            // Writing Panning envelope
            MPTEnvToIT(&pIns->panning_envelope, &iti.panenv, 32, 32);
            // Writing Pitch Envelope
            MPTEnvToIT(&pIns->pitch_envelope, &iti.pitchenv, 32, 32);
            if (pIns->pitch_envelope.flags & ENV_FILTER) iti.pitchenv.flags |= 0x80;
        } else
        // Save Empty Instrument
        {
            for (UINT i=0; i<NoteMax; i++) iti.keyboard[i*2] = i;
            iti.ppc = 5*12;
            iti.gbv = 128;
            iti.dfp = 0x20;
            iti.ifc = 0xFF;
        }
        if (!iti.nos) iti.trkvers = 0;
        // Writing instrument
        if (bKbdEx) *((int *)iti.dummy) = 'MPTX';
        inspos[nins-1] = dwPos;
        dwPos += sizeof(ITINSTRUMENT);
        fwrite(&iti, 1, sizeof(ITINSTRUMENT), f);
        if (bKbdEx)
        {
            dwPos += NoteMax;
            fwrite(keyboardex, 1, NoteMax, f);
        }

        //------------ rewbs.modularInstData
        if (Instruments[nins])
        {
            long modularInstSize = 0;
            UINT ModInstID = 'INSM';
            fwrite(&ModInstID, 1, sizeof(ModInstID), f);    // mark this as an instrument with modular extensions
            long sizePos = ftell(f);                            // we will want to write the modular data's total size here
            fwrite(&modularInstSize, 1, sizeof(modularInstSize), f);    // write a DUMMY size, just to move file pointer by a long

            //Write chunks
            UINT ID;
            {    //VST Slot chunk:
                ID='PLUG';
                fwrite(&ID, 1, sizeof(int), f);
                modinstrument_t *pIns = Instruments[nins];
                fwrite(&(pIns->nMixPlug), 1, sizeof(uint8_t), f);
                modularInstSize += sizeof(int)+sizeof(uint8_t);
            }
            //How to save your own modular instrument chunk:
    /*            {
                ID='MYID';
                fwrite(&ID, 1, sizeof(int), f);
                instModularDataSize+=sizeof(int);

                //You can save your chunk size somwhere here if you need variable chunk size.
                fwrite(myData, 1, myDataSize, f);
                instModularDataSize+=myDataSize;
            }
    */
            //write modular data's total size
            long curPos = ftell(f);                    // remember current pos
            fseek(f, sizePos, SEEK_SET);    // go back to  sizePos
            fwrite(&modularInstSize, 1, sizeof(modularInstSize), f);    // write data
            fseek(f, curPos, SEEK_SET);            // go back to where we were.

            //move forward
            dwPos+=sizeof(ModInstID)+sizeof(modularInstSize)+modularInstSize;
        }
        //------------ end rewbs.modularInstData
    }
    // Writing sample headers
    memset(&itss, 0, sizeof(itss));
    for (UINT hsmp=0; hsmp<header.smpnum; hsmp++)
    {
        smppos[hsmp] = dwPos;
        dwPos += sizeof(ITSAMPLESTRUCT);
        fwrite(&itss, 1, sizeof(ITSAMPLESTRUCT), f);
    }
    // Writing Patterns
    bool bNeedsMptPatSave = false;
    for (UINT npat=0; npat<header.patnum; npat++)
    {
        uint32_t dwPatPos = dwPos;
        if (!Patterns[npat]) continue;
        patpos[npat] = dwPos;
        patinfo[0] = 0;
        patinfo[1] = Patterns[npat].GetNumRows();
        patinfo[2] = 0;
        patinfo[3] = 0;

        if(Patterns[npat].GetOverrideSignature())
            bNeedsMptPatSave = true;

        // Check for empty pattern
        if (Patterns[npat].GetNumRows() == 64 && Patterns.IsPatternEmpty(npat))
        {
            patpos[npat] = 0;
            continue;
        }

        fwrite(patinfo, 8, 1, f);
        dwPos += 8;
        memset(chnmask, 0xFF, sizeof(chnmask));
        memset(lastvalue, 0, sizeof(lastvalue));
        modplug::tracker::modevent_t *m = Patterns[npat];
        for (UINT row=0; row<Patterns[npat].GetNumRows(); row++)
        {
            UINT len = 0;
            for (UINT ch=0; ch<m_nChannels; ch++, m++)
            {
                // Skip mptm-specific notes.
                if(GetType() == MOD_TYPE_MPT && (m->IsPcNote()))
                    {bNeedsMptPatSave = true; continue;}

                uint8_t b = 0;
                UINT command = m->command;
                UINT param = m->param;
                UINT vol = 0xFF;
                UINT note = m->note;
                if (note) b |= 1;
                if ((note) && (note < NoteMinSpecial)) note--;
                if (note == NoteFade && GetType() != MOD_TYPE_MPT) note = 0xF6;
                if (m->instr) b |= 2;
                if (m->volcmd)
                {
                    UINT volcmd = m->volcmd;
                    switch(volcmd)
                    {
                    case VolCmdVol:                    vol = m->vol; if (vol > 64) vol = 64; break;
                    case VolCmdPan:            vol = m->vol + 128; if (vol > 192) vol = 192; break;
                    case VolCmdSlideUp:            vol = 85 + ConvertVolParam(m->vol); break;
                    case VolCmdSlideDown:    vol = 95 + ConvertVolParam(m->vol); break;
                    case VolCmdFineUp:            vol = 65 + ConvertVolParam(m->vol); break;
                    case VolCmdFineDown:    vol = 75 + ConvertVolParam(m->vol); break;
                    case VolCmdVibratoDepth:    vol = 203 + ConvertVolParam(m->vol); break;
                    case VolCmdVibratoSpeed:    vol = 0xFF /*203 + ConvertVolParam(m->vol)*/; break; // not supported!
                    case VolCmdPortamento:    vol = 193 + ConvertVolParam(m->vol); break;
                    case VolCmdPortamentoDown:            vol = 105 + ConvertVolParam(m->vol); break;
                    case VolCmdPortamentoUp:            vol = 115 + ConvertVolParam(m->vol); break;
                    case VolCmdOffset:                    vol = 223 + ConvertVolParam(m->vol); break; //rewbs.volOff
                    default:                                    vol = 0xFF;
                    }
                }
                if (vol != 0xFF) b |= 4;
                if (command)
                {
                    S3MSaveConvert(&command, &param, true);
                    if (command) b |= 8;
                }
                // Packing information
                if (b)
                {
                    // Same note ?
                    if (b & 1)
                    {
                        if ((note == lastvalue[ch].note) && (lastvalue[ch].volcmd & 1))
                        {
                            b &= ~1;
                            b |= 0x10;
                        } else
                        {
                            lastvalue[ch].note = note;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (volcmd_t) (lastvalue[ch].volcmd | 1);
                        }
                    }
                    // Same instrument ?
                    if (b & 2)
                    {
                        if ((m->instr == lastvalue[ch].instr) && (lastvalue[ch].volcmd & 2))
                        {
                            b &= ~2;
                            b |= 0x20;
                        } else
                        {
                            lastvalue[ch].instr = m->instr;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (volcmd_t) (lastvalue[ch].volcmd | 2);
                        }
                    }
                    // Same volume column byte ?
                    if (b & 4)
                    {
                        if ((vol == lastvalue[ch].vol) && (lastvalue[ch].volcmd & 4))
                        {
                            b &= ~4;
                            b |= 0x40;
                        } else
                        {
                            lastvalue[ch].vol = vol;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (volcmd_t) (lastvalue[ch].volcmd | 4);
                        }
                    }
                    // Same command / param ?
                    if (b & 8)
                    {
                        if ((command == lastvalue[ch].command) && (param == lastvalue[ch].param) && (lastvalue[ch].volcmd & 8))
                        {
                            b &= ~8;
                            b |= 0x80;
                        } else
                        {
                            //XXXih: gross!
                            lastvalue[ch].command = (modplug::tracker::cmd_t) command;
                            lastvalue[ch].param = param;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (modplug::tracker::volcmd_t) (lastvalue[ch].volcmd | 8);
                        }
                    }
                    if (b != chnmask[ch])
                    {
                        chnmask[ch] = b;
                        buf[len++] = (ch+1) | 0x80;
                        buf[len++] = b;
                    } else
                    {
                        buf[len++] = ch+1;
                    }
                    if (b & 1) buf[len++] = note;
                    if (b & 2) buf[len++] = m->instr;
                    if (b & 4) buf[len++] = vol;
                    if (b & 8)
                    {
                        buf[len++] = command;
                        buf[len++] = param;
                    }
                }
            }
            buf[len++] = 0;
            if(patinfo[0] > UINT16_MAX - len)
            {
#ifdef MODPLUG_TRACKER
                if(GetpModDoc())
                {
                    CString str;
                    str.Format("%s (%s %u)\n", str_tooMuchPatternData, str_pattern, npat);
                    GetpModDoc()->AddToLog(str);
                }
#endif // MODPLUG_TRACKER
                break;
            }
            else
            {
                dwPos += len;
                patinfo[0] += len;
                fwrite(buf, 1, len, f);
            }
        }
        fseek(f, dwPatPos, SEEK_SET);
        fwrite(patinfo, 8, 1, f);
        fseek(f, dwPos, SEEK_SET);
    }
    // Writing Sample Data
    for (UINT nsmp=1; nsmp<=header.smpnum; nsmp++)
    {
        modsample_t *psmp = &Samples[nsmp];
        memset(&itss, 0, sizeof(itss));
        memcpy(itss.filename, psmp->legacy_filename, 12);
        memcpy(itss.name, m_szNames[nsmp], 26);
        itss.id = LittleEndian(IT_IMPS);
        itss.gvl = (uint8_t)psmp->global_volume;

        UINT flags = RS_PCM8S;
        if(psmp->length && psmp->sample_data.generic)
        {
            itss.flags = 0x01;
            if (bitset_is_set(psmp->flags, sflag_ty::Loop)) itss.flags |= 0x10;
            if (bitset_is_set(psmp->flags, sflag_ty::SustainLoop)) itss.flags |= 0x20;
            if (bitset_is_set(psmp->flags, sflag_ty::BidiLoop)) itss.flags |= 0x40;
            if (bitset_is_set(psmp->flags, sflag_ty::BidiSustainLoop)) itss.flags |= 0x80;
#ifndef NO_PACKING
            if (nPacking)
            {
                if ((!(psmp->flags & (CHN_16BIT|CHN_STEREO)))
                    && (CanPackSample(psmp->sample_data, psmp->length, nPacking)))
                {
                    flags = RS_ADPCM4;
                    itss.cvt = 0xFF;
                }
            } else
#endif // NO_PACKING
            {
                if (bitset_is_set(psmp->flags, sflag_ty::Stereo))
                {
                    flags = RS_STPCM8S;
                    itss.flags |= 0x04;
                }
                if (psmp->sample_tag == stag_ty::Int16)
                {
                    itss.flags |= 0x02;
                    flags = bitset_is_set(psmp->flags, sflag_ty::Stereo) ? RS_STPCM16S : RS_PCM16S;
                }
            }
            itss.cvt = 0x01;
        }
        else
        {
            itss.flags = 0x00;
        }

        itss.C5Speed = psmp->c5_samplerate;
        if (!itss.C5Speed) itss.C5Speed = 8363;
        itss.length = psmp->length;
        itss.loopbegin = psmp->loop_start;
        itss.loopend = psmp->loop_end;
        itss.susloopbegin = psmp->sustain_start;
        itss.susloopend = psmp->sustain_end;
        itss.vol = psmp->default_volume >> 2;
        itss.dfp = psmp->default_pan >> 2;
        itss.vit = autovibxm2it[psmp->vibrato_type & 7];
        itss.vis = bad_min(psmp->vibrato_rate, 64);
        itss.vid = bad_min(psmp->vibrato_depth, 32);
        itss.vir = bad_min(psmp->vibrato_sweep, 255); //(psmp->vibrato_sweep < 64) ? psmp->vibrato_sweep * 4 : 255;
        if (bitset_is_set(psmp->flags, sflag_ty::ForcedPanning)) itss.dfp |= 0x80;

        itss.samplepointer = dwPos;
        fseek(f, smppos[nsmp-1], SEEK_SET);
        fwrite(&itss, 1, sizeof(ITSAMPLESTRUCT), f);
        fseek(f, dwPos, SEEK_SET);
        if ((psmp->sample_data.generic) && (psmp->length))
        {
            dwPos += WriteSample(f, psmp, flags);
        }
    }

    //Save hacked-on extra info
    SaveExtendedInstrumentProperties(Instruments, header.insnum, f);
    SaveExtendedSongProperties(f);

    // Updating offsets
    fseek(f, dwHdrPos, SEEK_SET);
    if (header.insnum) fwrite(inspos, 4, header.insnum, f);
    if (header.smpnum) fwrite(smppos, 4, header.smpnum, f);
    if (header.patnum) fwrite(&patpos[0], 4, header.patnum, f);

    if(GetType() == MOD_TYPE_IT)
    {
        fclose(f);
        return true;
    }

    //hack
    //BEGIN: MPT SPECIFIC:
    //--------------------

    fseek(f, 0, SEEK_END);
    std::ofstream fout(f);
    const uint32_t MPTStartPos = fout.tellp();

    srlztn::Ssb ssb(fout);
    ssb.BeginWrite("mptm", MptVersion::num);

    if (GetTuneSpecificTunings().GetNumTunings() > 0)
        ssb.WriteItem(GetTuneSpecificTunings(), "0", 1, &WriteTuningCollection);
    if (AreNonDefaultTuningsUsed(*this))
        ssb.WriteItem(*this, "1", 1, &WriteTuningMap);
    if (Order.NeedsExtraDatafield())
        ssb.WriteItem(Order, "2", 1, &WriteModSequenceOld);
    if (bNeedsMptPatSave)
        ssb.WriteItem(Patterns, FileIdPatterns, strlen(FileIdPatterns), &WriteModPatterns);
    ssb.WriteItem(Order, FileIdSequences, strlen(FileIdSequences), &WriteModSequences);

    ssb.FinishWrite();

    if (ssb.m_Status & srlztn::SNT_FAILURE)
    {
#ifdef MODPLUG_TRACKER
        if(GetpModDoc())
            GetpModDoc()->AddToLog("Error occured in writing MPTM extensions.\n");
#endif // MODPLUG_TRACKER
    }

    //Last 4 bytes should tell where the hack mpt things begin.
    if(!fout.good())
    {
        fout.clear();
        fout.write(reinterpret_cast<const char*>(&MPTStartPos), sizeof(MPTStartPos));
        return false;
    }
    fout.write(reinterpret_cast<const char*>(&MPTStartPos), sizeof(MPTStartPos));
    fout.close();
    //END  : MPT SPECIFIC
    //-------------------

    //NO WRITING HERE ANYMORE.

    return true;
}


#pragma warning(default:4100)
#endif // MODPLUG_NO_FILESAVE

//HACK: This is a quick fix. Needs to be better integrated into player and GUI.
//And need to split into subroutines and eliminate code duplication with SaveIT.
bool module_renderer::SaveCompatIT(LPCSTR lpszFileName)
//------------------------------------------------
{
    const int IT_MAX_CHANNELS=64;
    uint32_t dwPatNamLen, dwChnNamLen;
    ITFILEHEADER header;
    ITINSTRUMENT iti;
    ITSAMPLESTRUCT itss;
    vector<bool>smpcount(GetNumSamples(), false);
    uint32_t inspos[MAX_INSTRUMENTS];
    uint32_t patpos[MAX_PATTERNS];
    uint32_t smppos[MAX_SAMPLES];
    uint32_t dwPos = 0, dwHdrPos = 0, dwExtra = 0;
    uint16_t patinfo[4];
// -> CODE#0006
// -> DESC="misc quantity changes"
    uint8_t chnmask[IT_MAX_CHANNELS];
    modplug::tracker::modevent_t lastvalue[IT_MAX_CHANNELS];
    UINT nChannels = bad_min(m_nChannels, IT_MAX_CHANNELS);
// -! BEHAVIOUR_CHANGE#0006
    uint8_t buf[512];
    FILE *f;


    if ((!lpszFileName) || ((f = fopen(lpszFileName, "wb")) == NULL)) return false;
    memset(inspos, 0, sizeof(inspos));
    memset(patpos, 0, sizeof(patpos));
    memset(smppos, 0, sizeof(smppos));
    // Writing Header
    memset(&header, 0, sizeof(header));
    dwPatNamLen = 0;
    dwChnNamLen = 0;
    header.id = LittleEndian(IT_IMPM);
    copy_with_padding(header.songname, 26, this->song_name);

    header.highlight_minor = (uint8_t)bad_min(m_nDefaultRowsPerBeat, 0xFF);
    header.highlight_major = (uint8_t)bad_min(m_nDefaultRowsPerMeasure, 0xFF);

    // An additional "---" pattern is appended so Impulse Tracker won't ignore the last order item.
    // Interestingly, this can exceed IT's 256 order limit. Also, IT will always save at least two orders.
    header.ordnum = bad_min(Order.GetLengthTailTrimmed(), ModSpecs::it.ordersMax) + 1;
    if(header.ordnum < 2) header.ordnum = 2;

    header.patnum = MAX_PATTERNS;
    while ((header.patnum > 0) && (!Patterns[header.patnum-1])) {
        header.patnum--;
    }

    header.insnum = m_nInstruments;
    header.smpnum = m_nSamples;

    MptVersion::VersionNum vVersion = MptVersion::num;
    header.cwtv = LittleEndianW(0x5000 | (uint16_t)((vVersion >> 16) & 0x0FFF)); // format: txyy (t = tracker ID, x = version major, yy = version minor), e.g. 0x5117 (OpenMPT = 5, 117 = v1.17)
    header.cmwt = LittleEndianW(0x0214);    // Common compatible tracker :)
    // hack from schism tracker:
    for(modplug::tracker::instrumentindex_t nIns = 1; nIns <= GetNumInstruments(); nIns++)
    {
        if(Instruments[nIns] && Instruments[nIns]->pitch_envelope.flags & ENV_FILTER)
        {
            header.cmwt = LittleEndianW(0x0217);
            break;
        }
    }

    header.flags = 0x0001;
    header.special = 0x02 | 0x04 ;    // 0x02: embed file edit history, 0x04: store row highlight in the header
    if (m_nInstruments) header.flags |= 0x04;
    if (m_dwSongFlags & SONG_LINEARSLIDES) header.flags |= 0x08;
    if (m_dwSongFlags & SONG_ITOLDEFFECTS) header.flags |= 0x10;
    if (m_dwSongFlags & SONG_ITCOMPATGXX) header.flags |= 0x20;
    //if (m_dwSongFlags & SONG_EXFILTERRANGE) header.flags |= 0x1000;
    header.globalvol = m_nDefaultGlobalVolume >> 1;
    header.mv = CLAMP(m_nSamplePreAmp, 0, 128);
    header.speed = m_nDefaultSpeed;
    header.tempo = bad_min(m_nDefaultTempo, 255);  //Limit this one to 255, we save the real one as an extension below.
    header.sep = 128; // pan separation
    dwHdrPos = sizeof(header) + header.ordnum;
    // Channel Pan and Volume
    memset(header.chnpan, 0xA0, 64);
    memset(header.chnvol, 64, 64);
    for (UINT ich=0; ich</*m_nChannels*/64; ich++) //Header only has room for settings for 64 chans...
    {
        header.chnpan[ich] = ChnSettings[ich].nPan >> 2;
        if (ChnSettings[ich].dwFlags & CHN_SURROUND) header.chnpan[ich] = 100;
        header.chnvol[ich] = ChnSettings[ich].nVolume;
        if (ChnSettings[ich].dwFlags & CHN_MUTE) header.chnpan[ich] |= 0x80;
/*            if (ChnSettings[ich].szName[0])
        {
            dwChnNamLen = (ich+1) * MAX_CHANNELNAME;
        }
*/
    }
//    if (dwChnNamLen) dwExtra += dwChnNamLen + 8;

    if (m_dwSongFlags & SONG_EMBEDMIDICFG)
    {
        header.flags |= 0x80;
        header.special |= 0x08;
        dwExtra += sizeof(MODMIDICFG);
    }
    // Pattern Names
/*    if ((m_nPatternNames) && (m_lpszPatternNames))
    {
        dwPatNamLen = m_nPatternNames * MAX_PATTERNNAME;
        while ((dwPatNamLen >= MAX_PATTERNNAME) && (!m_lpszPatternNames[dwPatNamLen-MAX_PATTERNNAME])) dwPatNamLen -= MAX_PATTERNNAME;
        if (dwPatNamLen < MAX_PATTERNNAME) dwPatNamLen = 0;
        if (dwPatNamLen) dwExtra += dwPatNamLen + 8;
    }
*/    // Mix Plugins
    //dwExtra += SaveMixPlugins(NULL, TRUE);
    dwExtra += SaveITEditHistory(this, nullptr);    // Just calculate the size of this extra block for now.
    // Comments
    if (m_lpszSongComments)
    {
        header.special |= 1;
        header.msglength = strlen(m_lpszSongComments)+1;
        header.msgoffset = dwHdrPos + dwExtra + header.insnum*4 + header.patnum*4 + header.smpnum*4;
    }
    // Write file header
    fwrite(&header, 1, sizeof(header), f);
    Order.WriteAsByte(f, header.ordnum);
    if (header.insnum) fwrite(inspos, 4, header.insnum, f);
    if (header.smpnum) fwrite(smppos, 4, header.smpnum, f);
    if (header.patnum) fwrite(patpos, 4, header.patnum, f);
    // Writing edit history information
    SaveITEditHistory(this, f);
    // Writing midi cfg
    if (header.flags & 0x80)
    {
        fwrite(&m_MidiCfg, 1, sizeof(MODMIDICFG), f);
    }
    // Writing pattern names
/*    if (dwPatNamLen)
    {
        uint32_t d = 0x4d414e50;
        fwrite(&d, 1, 4, f);
        fwrite(&dwPatNamLen, 1, 4, f);
        fwrite(m_lpszPatternNames, 1, dwPatNamLen, f);
    }
*/    // Writing channel Names
/*    if (dwChnNamLen)
    {
        uint32_t d = 0x4d414e43;
        fwrite(&d, 1, 4, f);
        fwrite(&dwChnNamLen, 1, 4, f);
        UINT nChnNames = dwChnNamLen / MAX_CHANNELNAME;
        for (UINT inam=0; inam<nChnNames; inam++)
        {
            fwrite(ChnSettings[inam].szName, 1, MAX_CHANNELNAME, f);
        }
    }
*/    // Writing mix plugins info
/*    SaveMixPlugins(f, FALSE);
*/    // Writing song message
    dwPos = dwHdrPos + dwExtra + (header.insnum + header.smpnum + header.patnum) * 4;
    if (header.special & 1)
    {
        dwPos += strlen(m_lpszSongComments) + 1;
        fwrite(m_lpszSongComments, 1, strlen(m_lpszSongComments)+1, f);
    }
    // Writing instruments
    for (UINT nins=1; nins<=header.insnum; nins++)
    {
        bool bKbdEx = false;    // extended sample map (for samples > 255)
        uint8_t keyboardex[NoteMax];

        memset(&iti, 0, sizeof(iti));
        iti.id = LittleEndian(IT_IMPI);    // "IMPI"
        iti.trkvers = 0x0214;
        if (Instruments[nins])
        {
            modinstrument_t *pIns = Instruments[nins];
            memcpy(iti.filename, pIns->legacy_filename, 12);
            memcpy(iti.name, pIns->name, 26);
            SetNullTerminator(iti.name);
            iti.mbank = pIns->midi_bank;
            iti.mpr = pIns->midi_program;
            iti.mch = pIns->midi_channel;
            iti.nna = pIns->new_note_action;
            if (pIns->duplicate_check_type<DCT_PLUGIN) iti.dct = pIns->duplicate_check_type; else iti.dct =0;
            iti.dca = pIns->duplicate_note_action;
            iti.fadeout = bad_min(pIns->fadeout >> 5 , 256);
            iti.pps = pIns->pitch_pan_separation;
            iti.ppc = pIns->pitch_pan_center;
            iti.gbv = (uint8_t)(pIns->global_volume << 1);
            iti.dfp = (uint8_t)(pIns->default_pan >> 2);
            if (!(pIns->flags & INS_SETPANNING)) iti.dfp |= 0x80;
            iti.rv = pIns->random_volume_weight;
            iti.rp = pIns->random_pan_weight;
            iti.ifc = pIns->default_filter_cutoff;
            iti.ifr = pIns->default_filter_resonance;
            iti.nos = 0;
            for (UINT i=0; i<NoteMax; i++) if (pIns->Keyboard[i] < MAX_SAMPLES)
            {
                const UINT smp = pIns->Keyboard[i];
                if (smp && smp <= GetNumSamples() && !smpcount[smp - 1])
                {
                    smpcount[smp - 1] = true;
                    iti.nos++;
                }
                iti.keyboard[i*2] = (pIns->NoteMap[i] >= NoteMin && pIns->NoteMap[i] <= NoteMax) ? (pIns->NoteMap[i] - 1) : i;
                iti.keyboard[i*2+1] = smp;
                //if (smp > 0xFF) bKbdEx = true;    // no extended sample map in compat mode
                keyboardex[i] = (smp>>8);
            } else keyboardex[i] = 0;
            // Writing Volume envelope
            MPTEnvToIT(&pIns->volume_envelope, &iti.volenv, 0, 64);
            // Writing Panning envelope
            MPTEnvToIT(&pIns->panning_envelope, &iti.panenv, 32, 32);
            // Writing Pitch Envelope
            MPTEnvToIT(&pIns->pitch_envelope, &iti.pitchenv, 32, 32);
            if (pIns->pitch_envelope.flags & ENV_FILTER) iti.pitchenv.flags |= 0x80;
        } else
        // Save Empty Instrument
        {
            for (UINT i=0; i<NoteMax; i++) iti.keyboard[i*2] = i;
            iti.ppc = 5*12;
            iti.gbv = 128;
            iti.dfp = 0x20;
            iti.ifc = 0xFF;
        }
        if (!iti.nos) iti.trkvers = 0;
        // Writing instrument
        if (bKbdEx) *((int *)iti.dummy) = 'MPTX';
        inspos[nins-1] = dwPos;
        dwPos += sizeof(ITINSTRUMENT);
        fwrite(&iti, 1, sizeof(ITINSTRUMENT), f);
        if (bKbdEx)
        {
            dwPos += NoteMax;
            fwrite(keyboardex, 1, NoteMax, f);
        }

        //------------ rewbs.modularInstData
/*            if (Instruments[nins])
        {
            long modularInstSize = 0;
            UINT ModInstID = 'INSM';
            fwrite(&ModInstID, 1, sizeof(ModInstID), f);    // mark this as an instrument with modular extensions
            long sizePos = ftell(f);                            // we will want to write the modular data's total size here
            fwrite(&modularInstSize, 1, sizeof(modularInstSize), f);    // write a DUMMY size, just to move file pointer by a long

            //Write chunks
            UINT ID;
            {    //VST Slot chunk:
                ID='PLUG';
                fwrite(&ID, 1, sizeof(int), f);
                modinstrument_t *pIns = Instruments[nins];
                fwrite(&(pIns->nMixPlug), 1, sizeof(uint8_t), f);
                modularInstSize += sizeof(int)+sizeof(uint8_t);
            }
*/                    //How to save your own modular instrument chunk:
    /*            {
                ID='MYID';
                fwrite(&ID, 1, sizeof(int), f);
                instModularDataSize+=sizeof(int);

                //You can save your chunk size somwhere here if you need variable chunk size.
                fwrite(myData, 1, myDataSize, f);
                instModularDataSize+=myDataSize;
            }
    */
/*                    //write modular data's total size
            long curPos = ftell(f);                    // remember current pos
            fseek(f, sizePos, SEEK_SET);    // go back to  sizePos
            fwrite(&modularInstSize, 1, sizeof(modularInstSize), f);    // write data
            fseek(f, curPos, SEEK_SET);            // go back to where we were.

            //move forward
            dwPos+=sizeof(ModInstID)+sizeof(modularInstSize)+modularInstSize;
        }
*/            //------------ end rewbs.modularInstData
    }
    // Writing sample headers
    memset(&itss, 0, sizeof(itss));
    for (UINT hsmp=0; hsmp<header.smpnum; hsmp++)
    {
        smppos[hsmp] = dwPos;
        dwPos += sizeof(ITSAMPLESTRUCT);
        fwrite(&itss, 1, sizeof(ITSAMPLESTRUCT), f);
    }
    // Writing Patterns
    for (UINT npat=0; npat<header.patnum; npat++)
    {
        uint32_t dwPatPos = dwPos;
        UINT len;
        if (!Patterns[npat]) continue;
        patpos[npat] = dwPos;
        patinfo[0] = 0;
        patinfo[1] = Patterns[npat].GetNumRows();
        patinfo[2] = 0;
        patinfo[3] = 0;

        // Check for empty pattern
        if (Patterns[npat].GetNumRows() == 64 && Patterns.IsPatternEmpty(npat))
        {
            patpos[npat] = 0;
            continue;
        }

        fwrite(patinfo, 8, 1, f);
        dwPos += 8;
        memset(chnmask, 0xFF, sizeof(chnmask));
        memset(lastvalue, 0, sizeof(lastvalue));
        modplug::tracker::modevent_t *m = Patterns[npat];
        for (UINT row=0; row<Patterns[npat].GetNumRows(); row++)
        {
            len = 0;
            for (UINT ch = 0; ch < m_nChannels; ch++, m++)
            {
                if(ch >= nChannels) continue;
                uint8_t b = 0;
                UINT command = m->command;
                UINT param = m->param;
                UINT vol = 0xFF;
                UINT note = m->note;
                if(note == NotePc || note == NotePcSmooth) note = NoteNone;
                if (note) b |= 1;
                if ((note) && (note < NoteMinSpecial)) note--;
                if (note == NoteFade) note = 0xF6;
                if (m->instr) b |= 2;
                if (m->volcmd)
                {
                    UINT volcmd = m->volcmd;
                    switch(volcmd)
                    {
                    case VolCmdVol:                    vol = m->vol; if (vol > 64) vol = 64; break;
                    case VolCmdPan:            vol = m->vol + 128; if (vol > 192) vol = 192; break;
                    case VolCmdSlideUp:            vol = 85 + ConvertVolParam(m->vol); break;
                    case VolCmdSlideDown:    vol = 95 + ConvertVolParam(m->vol); break;
                    case VolCmdFineUp:            vol = 65 + ConvertVolParam(m->vol); break;
                    case VolCmdFineDown:    vol = 75 + ConvertVolParam(m->vol); break;
                    case VolCmdVibratoDepth:    vol = 203 + ConvertVolParam(m->vol); break;
                    case VolCmdVibratoSpeed:    if(command == CmdNone) { // illegal command -> move if possible
                                                    command = CmdVibrato; param = ConvertVolParam(m->vol) << 4; vol = 0xFF;
                                                } else { vol = 203;}
                                                break;
                    case VolCmdPortamento:    vol = 193 + ConvertVolParam(m->vol); break;
                    case VolCmdPortamentoDown:            vol = 105 + ConvertVolParam(m->vol); break;
                    case VolCmdPortamentoUp:            vol = 115 + ConvertVolParam(m->vol); break;
                    default:                                    vol = 0xFF;
                    }
                }
                if (vol != 0xFF) b |= 4;
                if (command)
                {
                    S3MSaveConvert(&command, &param, true, true);
                    if (command) b |= 8;
                }
                // Packing information
                if (b)
                {
                    // Same note ?
                    if (b & 1)
                    {
                        if ((note == lastvalue[ch].note) && (lastvalue[ch].volcmd & 1))
                        {
                            b &= ~1;
                            b |= 0x10;
                        } else
                        {
                            lastvalue[ch].note = note;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (volcmd_t) (lastvalue[ch].volcmd | 1);
                        }
                    }
                    // Same instrument ?
                    if (b & 2)
                    {
                        if ((m->instr == lastvalue[ch].instr) && (lastvalue[ch].volcmd & 2))
                        {
                            b &= ~2;
                            b |= 0x20;
                        } else
                        {
                            lastvalue[ch].instr = m->instr;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (volcmd_t) (lastvalue[ch].volcmd | 2);
                        }
                    }
                    // Same volume column byte ?
                    if (b & 4)
                    {
                        if ((vol == lastvalue[ch].vol) && (lastvalue[ch].volcmd & 4))
                        {
                            b &= ~4;
                            b |= 0x40;
                        } else
                        {
                            lastvalue[ch].vol = vol;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (volcmd_t) (lastvalue[ch].volcmd | 4);
                        }
                    }
                    // Same command / param ?
                    if (b & 8)
                    {
                        if ((command == lastvalue[ch].command) && (param == lastvalue[ch].param) && (lastvalue[ch].volcmd & 8))
                        {
                            b &= ~8;
                            b |= 0x80;
                        } else
                        {
                            //XXXih: gross!
                            lastvalue[ch].command = (modplug::tracker::cmd_t) command;
                            lastvalue[ch].param = param;
                            //XXXih: gross!!
                            lastvalue[ch].volcmd = (modplug::tracker::volcmd_t) (lastvalue[ch].volcmd | 8);
                        }
                    }
                    if (b != chnmask[ch])
                    {
                        chnmask[ch] = b;
                        buf[len++] = (ch+1) | 0x80;
                        buf[len++] = b;
                    } else
                    {
                        buf[len++] = ch+1;
                    }
                    if (b & 1) buf[len++] = note;
                    if (b & 2) buf[len++] = m->instr;
                    if (b & 4) buf[len++] = vol;
                    if (b & 8)
                    {
                        buf[len++] = command;
                        buf[len++] = param;
                    }
                }
            }
            buf[len++] = 0;
            dwPos += len;
            patinfo[0] += len;
            fwrite(buf, 1, len, f);
        }
        fseek(f, dwPatPos, SEEK_SET);
        fwrite(patinfo, 8, 1, f);
        fseek(f, dwPos, SEEK_SET);
    }
    // Writing Sample Data
    for (UINT nsmp=1; nsmp<=header.smpnum; nsmp++)
    {
        modsample_t *psmp = &Samples[nsmp];
        memset(&itss, 0, sizeof(itss));
        memcpy(itss.filename, psmp->legacy_filename, 12);
        memcpy(itss.name, m_szNames[nsmp], 26);
        SetNullTerminator(itss.name);
        itss.id = LittleEndian(IT_IMPS);
        itss.gvl = (uint8_t)psmp->global_volume;

        UINT flags = RS_PCM8S;
        if(psmp->length && psmp->sample_data.generic)
        {
            itss.flags = 0x01;
            if (bitset_is_set(psmp->flags, sflag_ty::Loop)) itss.flags |= 0x10;
            if (bitset_is_set(psmp->flags, sflag_ty::SustainLoop)) itss.flags |= 0x20;
            if (bitset_is_set(psmp->flags, sflag_ty::BidiLoop)) itss.flags |= 0x40;
            if (bitset_is_set(psmp->flags, sflag_ty::BidiSustainLoop)) itss.flags |= 0x80;

            if (bitset_is_set(psmp->flags, sflag_ty::Stereo)) {
                flags = RS_STPCM8S;
                itss.flags |= 0x04;
            }
            if (psmp->sample_tag == stag_ty::Int16) {
                itss.flags |= 0x02;
                flags = bitset_is_set(psmp->flags, sflag_ty::Stereo) ? RS_STPCM16S : RS_PCM16S;
            }

            itss.cvt = 0x01;
        }
        else
        {
            itss.flags = 0x00;
        }

        itss.C5Speed = psmp->c5_samplerate;
        if (!itss.C5Speed) itss.C5Speed = 8363;
        itss.length = psmp->length;
        itss.loopbegin = psmp->loop_start;
        itss.loopend = psmp->loop_end;
        itss.susloopbegin = psmp->sustain_start;
        itss.susloopend = psmp->sustain_end;
        itss.vol = psmp->default_volume >> 2;
        itss.dfp = psmp->default_pan >> 2;
        itss.vit = autovibxm2it[psmp->vibrato_type & 7];
        itss.vis = bad_min(psmp->vibrato_rate, 64);
        itss.vid = bad_min(psmp->vibrato_depth, 32);
        itss.vir = bad_min(psmp->vibrato_sweep, 255);
        if (bitset_is_set(psmp->flags, sflag_ty::ForcedPanning)) itss.dfp |= 0x80;

        itss.samplepointer = dwPos;
        fseek(f, smppos[nsmp-1], SEEK_SET);
        fwrite(&itss, 1, sizeof(ITSAMPLESTRUCT), f);
        fseek(f, dwPos, SEEK_SET);
        if ((psmp->sample_data.generic) && (psmp->length))
        {
            dwPos += WriteSample(f, psmp, flags);
        }
    }

    //Save hacked-on extra info
//    SaveExtendedInstrumentProperties(Instruments, header.insnum, f);
//    SaveExtendedSongProperties(f);

    // Updating offsets
    fseek(f, dwHdrPos, SEEK_SET);
    if (header.insnum) fwrite(inspos, 4, header.insnum, f);
    if (header.smpnum) fwrite(smppos, 4, header.smpnum, f);
    if (header.patnum) fwrite(patpos, 4, header.patnum, f);
    fclose(f);
    return true;

}

//////////////////////////////////////////////////////////////////////////////
// IT 2.14 compression

uint32_t ITReadBits(uint32_t &bitbuf, UINT &bitnum, LPBYTE &ibuf, CHAR n)
//-----------------------------------------------------------------
{
    uint32_t retval = 0;
    UINT i = n;

    if (n > 0)
    {
        do
        {
            if (!bitnum)
            {
                bitbuf = *ibuf++;
                bitnum = 8;
            }
            retval >>= 1;
            retval |= bitbuf << 31;
            bitbuf >>= 1;
            bitnum--;
            i--;
        } while (i);
        i = n;
    }
    return (retval >> (32-i));
}

#define IT215_SUPPORT
void ITUnpack8Bit(LPSTR pSample, uint32_t dwLen, LPBYTE lpMemFile, uint32_t dwMemLength, BOOL b215)
//-------------------------------------------------------------------------------------------
{
    LPSTR pDst = pSample;
    LPBYTE pSrc = lpMemFile;
    uint32_t wHdr = 0;
    uint32_t wCount = 0;
    uint32_t bitbuf = 0;
    UINT bitnum = 0;
    uint8_t bLeft = 0, bTemp = 0, bTemp2 = 0;

    while (dwLen)
    {
        if (!wCount)
        {
            wCount = 0x8000;
            wHdr = *((LPWORD)pSrc);
            pSrc += 2;
            bLeft = 9;
            bTemp = bTemp2 = 0;
            bitbuf = bitnum = 0;
        }
        uint32_t d = wCount;
        if (d > dwLen) d = dwLen;
        // Unpacking
        uint32_t dwPos = 0;
        do
        {
            uint16_t wBits = (uint16_t)ITReadBits(bitbuf, bitnum, pSrc, bLeft);
            if (bLeft < 7)
            {
                uint32_t i = 1 << (bLeft-1);
                uint32_t j = wBits & 0xFFFF;
                if (i != j) goto UnpackByte;
                wBits = (uint16_t)(ITReadBits(bitbuf, bitnum, pSrc, 3) + 1) & 0xFF;
                bLeft = ((uint8_t)wBits < bLeft) ? (uint8_t)wBits : (uint8_t)((wBits+1) & 0xFF);
                goto Next;
            }
            if (bLeft < 9)
            {
                uint16_t i = (0xFF >> (9 - bLeft)) + 4;
                uint16_t j = i - 8;
                if ((wBits <= j) || (wBits > i)) goto UnpackByte;
                wBits -= j;
                bLeft = ((uint8_t)(wBits & 0xFF) < bLeft) ? (uint8_t)(wBits & 0xFF) : (uint8_t)((wBits+1) & 0xFF);
                goto Next;
            }
            if (bLeft >= 10) goto SkipByte;
            if (wBits >= 256)
            {
                bLeft = (uint8_t)(wBits + 1) & 0xFF;
                goto Next;
            }
        UnpackByte:
            if (bLeft < 8)
            {
                uint8_t shift = 8 - bLeft;
                char c = (char)(wBits << shift);
                c >>= shift;
                wBits = (uint16_t)c;
            }
            wBits += bTemp;
            bTemp = (uint8_t)wBits;
            bTemp2 += bTemp;
#ifdef IT215_SUPPORT
            pDst[dwPos] = (b215) ? bTemp2 : bTemp;
#else
            pDst[dwPos] = bTemp;
#endif
        SkipByte:
            dwPos++;
        Next:
            if (pSrc >= lpMemFile+dwMemLength+1) return;
        } while (dwPos < d);
        // Move On
        wCount -= d;
        dwLen -= d;
        pDst += d;
    }
}


void ITUnpack16Bit(LPSTR pSample, uint32_t dwLen, LPBYTE lpMemFile, uint32_t dwMemLength, BOOL b215)
//--------------------------------------------------------------------------------------------
{
    signed short *pDst = (signed short *)pSample;
    LPBYTE pSrc = lpMemFile;
    uint32_t wHdr = 0;
    uint32_t wCount = 0;
    uint32_t bitbuf = 0;
    UINT bitnum = 0;
    uint8_t bLeft = 0;
    signed short wTemp = 0, wTemp2 = 0;

    while (dwLen)
    {
        if (!wCount)
        {
            wCount = 0x4000;
            wHdr = *((LPWORD)pSrc);
            pSrc += 2;
            bLeft = 17;
            wTemp = wTemp2 = 0;
            bitbuf = bitnum = 0;
        }
        uint32_t d = wCount;
        if (d > dwLen) d = dwLen;
        // Unpacking
        uint32_t dwPos = 0;
        do
        {
            uint32_t dwBits = ITReadBits(bitbuf, bitnum, pSrc, bLeft);
            if (bLeft < 7)
            {
                uint32_t i = 1 << (bLeft-1);
                uint32_t j = dwBits;
                if (i != j) goto UnpackByte;
                dwBits = ITReadBits(bitbuf, bitnum, pSrc, 4) + 1;
                bLeft = ((uint8_t)(dwBits & 0xFF) < bLeft) ? (uint8_t)(dwBits & 0xFF) : (uint8_t)((dwBits+1) & 0xFF);
                goto Next;
            }
            if (bLeft < 17)
            {
                uint32_t i = (0xFFFF >> (17 - bLeft)) + 8;
                uint32_t j = (i - 16) & 0xFFFF;
                if ((dwBits <= j) || (dwBits > (i & 0xFFFF))) goto UnpackByte;
                dwBits -= j;
                bLeft = ((uint8_t)(dwBits & 0xFF) < bLeft) ? (uint8_t)(dwBits & 0xFF) : (uint8_t)((dwBits+1) & 0xFF);
                goto Next;
            }
            if (bLeft >= 18) goto SkipByte;
            if (dwBits >= 0x10000)
            {
                bLeft = (uint8_t)(dwBits + 1) & 0xFF;
                goto Next;
            }
        UnpackByte:
            if (bLeft < 16)
            {
                uint8_t shift = 16 - bLeft;
                signed short c = (signed short)(dwBits << shift);
                c >>= shift;
                dwBits = (uint32_t)c;
            }
            dwBits += wTemp;
            wTemp = (signed short)dwBits;
            wTemp2 += wTemp;
#ifdef IT215_SUPPORT
            pDst[dwPos] = (b215) ? wTemp2 : wTemp;
#else
            pDst[dwPos] = wTemp;
#endif
        SkipByte:
            dwPos++;
        Next:
            if (pSrc >= lpMemFile+dwMemLength+1) return;
        } while (dwPos < d);
        // Move On
        wCount -= d;
        dwLen -= d;
        pDst += d;
        if (pSrc >= lpMemFile+dwMemLength) break;
    }
}


#ifndef MODPLUG_NO_FILESAVE
UINT module_renderer::SaveMixPlugins(FILE *f, BOOL bUpdate)
//----------------------------------------------------
{


// -> CODE#0006
// -> DESC="misc quantity changes"
//    uint32_t chinfo[64];
    uint32_t chinfo[MAX_BASECHANNELS];
// -! BEHAVIOUR_CHANGE#0006
    CHAR s[32];
    uint32_t nPluginSize;
    UINT nTotalSize = 0;
    UINT nChInfo = 0;

    for (UINT i=0; i<MAX_MIXPLUGINS; i++)
    {
        PSNDMIXPLUGIN p = &m_MixPlugins[i];
        if ((p->Info.dwPluginId1) || (p->Info.dwPluginId2))
        {
            nPluginSize = sizeof(SNDMIXPLUGININFO)+4; // plugininfo+4 (datalen)
            if ((p->pMixPlugin) && (bUpdate))
            {
                p->pMixPlugin->SaveAllParameters();
            }
            if (p->pPluginData)
            {
                nPluginSize += p->nPluginDataSize;
            }

            // rewbs.modularPlugData
            uint32_t MPTxPlugDataSize = 4 + (sizeof(m_MixPlugins[i].fDryRatio)) +     //4 for ID and size of dryRatio
                                     4 + (sizeof(m_MixPlugins[i].defaultProgram)); //rewbs.plugDefaultProgram
                                // for each extra entity, add 4 for ID, plus size of entity, plus optionally 4 for size of entity.

            nPluginSize += MPTxPlugDataSize+4; //+4 is for size itself: sizeof(uint32_t) is 4
            // rewbs.modularPlugData
            if (f)
            {
                // write plugin ID
                s[0] = 'F';
                s[1] = 'X';
                s[2] = '0' + (i/10);
                s[3] = '0' + (i%10);
                fwrite(s, 1, 4, f);

                // write plugin size:
                fwrite(&nPluginSize, 1, 4, f);
                fwrite(&p->Info, 1, sizeof(SNDMIXPLUGININFO), f);
                fwrite(&m_MixPlugins[i].nPluginDataSize, 1, 4, f);
                if (m_MixPlugins[i].pPluginData) {
                    fwrite(m_MixPlugins[i].pPluginData, 1, m_MixPlugins[i].nPluginDataSize, f);
                }

                //rewbs.dryRatio
                fwrite(&MPTxPlugDataSize, 1, 4, f);

                //write ID for this xPlugData chunk:
                s[0] = 'D'; s[1] = 'W';    s[2] = 'R'; s[3] = 'T';
                fwrite(s, 1, 4, f);
                //Write chunk data itself (Could include size if you want variable size. Not necessary here.)
                fwrite(&(m_MixPlugins[i].fDryRatio), 1, sizeof(float), f);
                //end rewbs.dryRatio

                //rewbs.plugDefaultProgram
                //if (nProgram>=0) {
                    s[0] = 'P'; s[1] = 'R';    s[2] = 'O'; s[3] = 'G';
                    fwrite(s, 1, 4, f);
                    //Write chunk data itself (Could include size if you want variable size. Not necessary here.)
                    fwrite(&(m_MixPlugins[i].defaultProgram), 1, sizeof(float), f);
                //}
                //end rewbs.plugDefaultProgram

            }
            nTotalSize += nPluginSize + 8;
        }
    }
    for (UINT j=0; j<m_nChannels; j++)
    {
// -> CODE#0006
// -> DESC="misc quantity changes"
//            if (j < 64)
        if (j < MAX_BASECHANNELS)
// -! BEHAVIOUR_CHANGE#0006
        {
            if ((chinfo[j] = ChnSettings[j].nMixPlugin) != 0)
            {
                nChInfo = j+1;
            }
        }
    }
    if (nChInfo)
    {
        if (f)
        {
            nPluginSize = 'XFHC';
            fwrite(&nPluginSize, 1, 4, f);
            nPluginSize = nChInfo*4;
            fwrite(&nPluginSize, 1, 4, f);
            fwrite(chinfo, 1, nPluginSize, f);
        }
        nTotalSize += nChInfo*4 + 8;
    }
    return nTotalSize;
}
#endif


UINT module_renderer::LoadMixPlugins(const void *pData, UINT nLen)
//-----------------------------------------------------------
{
    const uint8_t *p = (const uint8_t *)pData;
    UINT nPos = 0;

    while (nPos+8 < nLen)    // read 4 magic bytes + size
    {
        uint32_t nPluginSize;
        UINT nPlugin;

        nPluginSize = *(uint32_t *)(p+nPos+4);
        if (nPluginSize > nLen-nPos-8) break;;
        if ((*(uint32_t *)(p+nPos)) == 'XFHC')
        {
// -> CODE#0006
// -> DESC="misc quantity changes"
//                    for (UINT ch=0; ch<64; ch++) if (ch*4 < nPluginSize)
            for (UINT ch=0; ch<MAX_BASECHANNELS; ch++) if (ch*4 < nPluginSize)
// -! BEHAVIOUR_CHANGE#0006
            {
                ChnSettings[ch].nMixPlugin = *(uint32_t *)(p+nPos+8+ch*4);
            }
        }

        else if ((*(uint32_t *)(p+nPos)) == 'XFQE')
        {
            // XXXih: ignore EQ
        }

        //Load plugin Data
        else
        {
            if ((p[nPos] != 'F') || (p[nPos+1] != 'X') || (p[nPos+2] < '0') || (p[nPos+3] < '0'))
            {
                break;
            }
            nPlugin = (p[nPos+2]-'0')*10 + (p[nPos+3]-'0');                    //calculate plug-in number.

            if ((nPlugin < MAX_MIXPLUGINS) && (nPluginSize >= sizeof(SNDMIXPLUGININFO)+4))
            {
                //MPT's standard plugin data. Size not specified in file.. grrr..
                m_MixPlugins[nPlugin].Info = *(const SNDMIXPLUGININFO *)(p+nPos+8);

                //data for VST setchunk? size lies just after standard plugin data.
                uint32_t dwExtra = *(uint32_t *)(p+nPos+8+sizeof(SNDMIXPLUGININFO));

                if ((dwExtra) && (dwExtra <= nPluginSize-sizeof(SNDMIXPLUGININFO)-4))
                {
                    m_MixPlugins[nPlugin].nPluginDataSize = 0;
                    m_MixPlugins[nPlugin].pPluginData = new char [dwExtra];
                    if (m_MixPlugins[nPlugin].pPluginData)
                    {
                        m_MixPlugins[nPlugin].nPluginDataSize = dwExtra;
                        memcpy(m_MixPlugins[nPlugin].pPluginData, p+nPos+8+sizeof(SNDMIXPLUGININFO)+4, dwExtra);
                    }
                }

                //rewbs.modularPlugData
                uint32_t dwXPlugData = *(uint32_t *)(p+nPos+8+sizeof(SNDMIXPLUGININFO)+dwExtra+4); //read next uint32_t into dwMPTExtra

                //if dwMPTExtra is positive and there are dwMPTExtra bytes left in nPluginSize, we have some more data!
                if ((dwXPlugData) && ((int)dwXPlugData <= (int)nPluginSize-(int)(sizeof(SNDMIXPLUGININFO)+dwExtra+8)))
                {
                    uint32_t startPos = nPos+8+sizeof(SNDMIXPLUGININFO)+dwExtra+8; // start of extra data for this plug
                    uint32_t endPos = startPos + dwXPlugData;                                            // end of extra data for this plug
                    uint32_t currPos = startPos;

                    while (currPos < endPos) //cycle through all the bytes
                    {
                        // do we recognize this chunk?
                        //rewbs.dryRatio
                        //TODO: turn this into a switch statement like for modular instrument data
                        if ((p[currPos] == 'D') && (p[currPos+1] == 'W') && (p[currPos+2] == 'R') && (p[currPos+3] == 'T'))
                        {
                            currPos+=4;// move past ID
                            m_MixPlugins[nPlugin].fDryRatio = *(float*) (p+currPos);
                            currPos+= sizeof(float); //move past data
                        }
                        //end rewbs.dryRatio

                        //rewbs.plugDefaultProgram
                        else if ((p[currPos] == 'P') && (p[currPos+1] == 'R') && (p[currPos+2] == 'O') && (p[currPos+3] == 'G'))
                        {
                            currPos+=4;// move past ID
                            m_MixPlugins[nPlugin].defaultProgram = *(long*) (p+currPos);
                            currPos+= sizeof(long); //move past data
                        }
                        //end rewbs.plugDefaultProgram
                        //else if.. (add extra attempts to recognize chunks here)
                        else // otherwise move forward a byte.
                        {
                            currPos++;
                        }

                    }

                }
                //end rewbs.modularPlugData

            }
        }
        nPos += nPluginSize + 8;
    }
    return nPos;
}


void module_renderer::SaveExtendedInstrumentProperties(modinstrument_t *instruments[], UINT nInstruments, FILE* f)
//------------------------------------------------------------------------------------------------------------
// Used only when saving IT, XM and MPTM.
// ITI, ITP saves using Ericus' macros etc...
// The reason is that ITs and XMs save [code][size][ins1.Value][ins2.Value]...
// whereas ITP saves [code][size][ins1.Value][code][size][ins2.Value]...
// too late to turn back....
{
    __int32 code = 0;

/*    if(Instruments[1] == NULL) {
        return;
    }*/

    code = 'MPTX';                                                    // write extension header code
    fwrite(&code, 1, sizeof(__int32), f);

    if (nInstruments == 0) {
        return;
    }

    WriteInstrumentPropertyForAllInstruments('VR..', sizeof(m_defaultInstrument.volume_ramp_up),  f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('VRD.', sizeof(m_defaultInstrument.volume_ramp_down),f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('MiP.', sizeof(m_defaultInstrument.nMixPlug),    f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('MC..', sizeof(m_defaultInstrument.midi_channel),f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('MP..', sizeof(m_defaultInstrument.midi_program),f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('MB..', sizeof(m_defaultInstrument.midi_bank),   f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('P...', sizeof(m_defaultInstrument.default_pan),        f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('GV..', sizeof(m_defaultInstrument.global_volume),  f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('FO..', sizeof(m_defaultInstrument.fadeout),    f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('R...', sizeof(m_defaultInstrument.resampling_mode), f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('CS..', sizeof(m_defaultInstrument.random_cutoff_weight),   f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('RS..', sizeof(m_defaultInstrument.random_resonance_weight),   f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('FM..', sizeof(m_defaultInstrument.default_filter_mode), f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('PERN', sizeof(m_defaultInstrument.pitch_envelope.release_node ), f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('AERN', sizeof(m_defaultInstrument.panning_envelope.release_node), f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('VERN', sizeof(m_defaultInstrument.volume_envelope.release_node), f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('PTTL', sizeof(m_defaultInstrument.pitch_to_tempo_lock),  f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('PVEH', sizeof(m_defaultInstrument.nPluginVelocityHandling),  f, instruments, nInstruments);
    WriteInstrumentPropertyForAllInstruments('PVOH', sizeof(m_defaultInstrument.nPluginVolumeHandling),  f, instruments, nInstruments);

    if (m_nType & MOD_TYPE_MPT) {
        UINT maxNodes = 0;
        for (modplug::tracker::instrumentindex_t nIns = 1; nIns <= m_nInstruments; nIns++) if(Instruments[nIns] != nullptr) {
            maxNodes = bad_max(maxNodes, Instruments[nIns]->volume_envelope.num_nodes);
            maxNodes = bad_max(maxNodes, Instruments[nIns]->panning_envelope.num_nodes);
            maxNodes = bad_max(maxNodes, Instruments[nIns]->pitch_envelope.num_nodes);
        }
        // write full envelope information for MPTM files (more env points)
        if (maxNodes > 25) {
            WriteInstrumentPropertyForAllInstruments('VE..', sizeof(m_defaultInstrument.volume_envelope.num_nodes), f, instruments, nInstruments);
            WriteInstrumentPropertyForAllInstruments('VP[.', sizeof(m_defaultInstrument.volume_envelope.Ticks ), f, instruments, nInstruments);
            WriteInstrumentPropertyForAllInstruments('VE[.', sizeof(m_defaultInstrument.volume_envelope.Values), f, instruments, nInstruments);

            WriteInstrumentPropertyForAllInstruments('PE..', sizeof(m_defaultInstrument.panning_envelope.num_nodes), f, instruments, nInstruments);
            WriteInstrumentPropertyForAllInstruments('PP[.', sizeof(m_defaultInstrument.panning_envelope.Ticks),  f, instruments, nInstruments);
            WriteInstrumentPropertyForAllInstruments('PE[.', sizeof(m_defaultInstrument.panning_envelope.Values), f, instruments, nInstruments);

            WriteInstrumentPropertyForAllInstruments('PiE.', sizeof(m_defaultInstrument.pitch_envelope.num_nodes), f, instruments, nInstruments);
            WriteInstrumentPropertyForAllInstruments('PiP[', sizeof(m_defaultInstrument.pitch_envelope.Ticks),  f, instruments, nInstruments);
            WriteInstrumentPropertyForAllInstruments('PiE[', sizeof(m_defaultInstrument.pitch_envelope.Values), f, instruments, nInstruments);
        }
    }
}

void module_renderer::WriteInstrumentPropertyForAllInstruments(__int32 code,  __int16 size, FILE* f, modinstrument_t *instruments[], UINT nInstruments)
//-------------------------------------------------------------------------------------------------------------------------------------------------
{
    //XXXih: this is disgusting
    fwrite(&code, 1, sizeof(__int32), f);            //write code
    fwrite(&size, 1, sizeof(__int16), f);            //write size
    for (UINT nins = 1; nins <= nInstruments; nins++) {  //for all instruments...
        uint8_t* pField;
        if (instruments[nins]) {
            pField = GetInstrumentHeaderFieldPointer(instruments[nins], code, size); //get ptr to field
        } else {
            pField = GetInstrumentHeaderFieldPointer(&m_defaultInstrument, code, size); //get ptr to field
        }
        fwrite(pField, 1, size, f);                            //write field data
    }
}

void module_renderer::SaveExtendedSongProperties(FILE* f)
//--------------------------------------------------
{  //Extra song data - Yet Another Hack.
    __int16 size;
    __int32 code = 'MPTS';                                    //Extra song file data
    fwrite(&code, 1, sizeof(__int32), f);

    code = 'DT..';                                                    //write m_nDefaultTempo field code
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nDefaultTempo);                    //write m_nDefaultTempo field size
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nDefaultTempo, 1, size, f);    //write m_nDefaultTempo

    code = 'RPB.';                                                    //write m_nRowsPerBeat
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nDefaultRowsPerBeat);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nDefaultRowsPerBeat, 1, size, f);

    code = 'RPM.';                                                    //write m_nRowsPerMeasure
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nDefaultRowsPerMeasure);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nDefaultRowsPerMeasure, 1, size, f);

    code = 'C...';                                                    //write m_nChannels
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nChannels);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nChannels, 1, size, f);

    if(TypeIsIT_MPT() && m_nChannels > 64)    //IT header has room only for 64 channels. Save the
    {                                                                                    //settings that do not fit to the header here as an extension.
        code = 'ChnS';
        fwrite(&code, 1, sizeof(__int32), f);
        size = (m_nChannels - 64)*2;
        fwrite(&size, 1, sizeof(__int16), f);
        for(UINT ich = 64; ich < m_nChannels; ich++)
        {
            uint8_t panvol[2];
            panvol[0] = ChnSettings[ich].nPan >> 2;
            if (ChnSettings[ich].dwFlags & CHN_SURROUND) panvol[0] = 100;
            if (ChnSettings[ich].dwFlags & CHN_MUTE) panvol[0] |= 0x80;
            panvol[1] = ChnSettings[ich].nVolume;
            fwrite(&panvol, sizeof(panvol), 1, f);
        }
    }

    code = 'TM..';                                                    //write m_nTempoMode
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nTempoMode);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nTempoMode, 1, size, f);

    code = 'PMM.';                                                    //write m_nMixLevels
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nMixLevels);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nMixLevels, 1, size, f);

    code = 'CWV.';                                                    //write m_dwCreatedWithVersion
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_dwCreatedWithVersion);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_dwCreatedWithVersion, 1, size, f);

    code = 'LSWV';                                                    //write m_dwLastSavedWithVersion
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_dwLastSavedWithVersion);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_dwLastSavedWithVersion, 1, size, f);

    code = 'SPA.';                                                    //write m_nSamplePreAmp
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nSamplePreAmp);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nSamplePreAmp, 1, size, f);

    code = 'VSTV';                                                    //write m_nVSTiVolume
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nVSTiVolume);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nVSTiVolume, 1, size, f);

    code = 'DGV.';                                                    //write m_nDefaultGlobalVolume
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nDefaultGlobalVolume);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nDefaultGlobalVolume, 1, size, f);

    code = 'RP..';                                                    //write m_nRestartPos
    fwrite(&code, 1, sizeof(__int32), f);
    size = sizeof(m_nRestartPos);
    fwrite(&size, 1, sizeof(__int16), f);
    fwrite(&m_nRestartPos, 1, size, f);


    //Additional flags for XM/IT/MPTM
    if(m_ModFlags)
    {
        code = 'MSF.';
        fwrite(&code, 1, sizeof(__int32), f);
        size = sizeof(m_ModFlags);
        fwrite(&size, 1, sizeof(__int16), f);
        fwrite(&m_ModFlags, 1, size, f);
    }

    //MIMA, MIDI mapping directives
    if(GetMIDIMapper().GetCount() > 0)
    {
        const size_t objectsize = GetMIDIMapper().GetSerializationSize();
        if(objectsize > size_t(INT16_MAX))
        {
#ifdef MODPLUG_TRACKER
            if(GetpModDoc())
                GetpModDoc()->AddToLog("Datafield overflow with MIDI to plugparam mappings; data won't be written.\n");
#endif // MODPLUG_TRACKER
        }
        else
        {
            code = 'MIMA';
            fwrite(&code, 1, sizeof(__int32), f);
            size = static_cast<int16_t>(objectsize);
            fwrite(&size, 1, sizeof(__int16), f);
            GetMIDIMapper().Serialize(f);
        }
    }


    return;
}

const uint8_t * module_renderer::LoadExtendedInstrumentProperties(const uint8_t * const pStart,
                                                     const uint8_t * const pEnd,
                                                     bool* pInterpretMptMade)
//---------------------------------------------------------------------------
{
    if( pStart == NULL || pEnd <= pStart || uintptr_t(pEnd - pStart) < 4)
        return NULL;

    int32_t code = 0;
    int16_t size = 0;
    const uint8_t * ptr = pStart;

    memcpy(&code, ptr, sizeof(code));

    if(code != 'MPTX')
        return NULL;

    // Found MPTX, interpret the file MPT made.
    if(pInterpretMptMade != NULL)
        *pInterpretMptMade = true;

    ptr += sizeof(int32_t);                                                    // jump extension header code
    while( ptr < pEnd && uintptr_t(pEnd-ptr) >= 4) //Loop 'till beginning of end of file/mpt specific looking for inst. extensions
    {
        memcpy(&code, ptr, sizeof(code));    // read field code
        if (code == 'MPTS')                                    //Reached song extensions, break out of this loop
            return ptr;

        ptr += sizeof(code);                            // jump field code

        if((uintptr_t)(pEnd - ptr) < 2)
            return NULL;

        memcpy(&size, ptr, sizeof(size));    // read field size
        ptr += sizeof(size);                            // jump field size

        if(IsValidSizeField(ptr, pEnd, size) == false)
            return NULL;

        for(UINT nins=1; nins<=m_nInstruments; nins++)
        {
            if(Instruments[nins])
                ReadInstrumentExtensionField(Instruments[nins], ptr, code, size);
        }
    }

    return NULL;
}


void module_renderer::LoadExtendedSongProperties(const MODTYPE modtype,
                                            const uint8_t * ptr,
                                            const uint8_t * const lpStream,
                                            const size_t searchlimit,
                                            bool* pInterpretMptMade)
//-------------------------------------------------------------------
{
    if(searchlimit < 6 || ptr == NULL || ptr < lpStream || uintptr_t(ptr - lpStream) > searchlimit - 4)
        return;

    const uint8_t * const pEnd = lpStream + searchlimit;

    int32_t code = 0;
    int16_t size = 0;

    memcpy(&code, ptr, sizeof(code));

    if(code != 'MPTS')
        return;

    // Found MPTS, interpret the file MPT made.
    if(pInterpretMptMade != NULL)
        *pInterpretMptMade = true;

    // HACK: Reset mod flags to default values here, as they are not always written.
    m_ModFlags = 0;

    // Case macros.
    #define CASE(id, data)    \
        case id: fadr = reinterpret_cast<uint8_t*>(&data); nMaxReadCount = bad_min(size, sizeof(data)); break;
    #define CASE_NOTXM(id, data) \
        case id: if(modtype != MOD_TYPE_XM) {fadr = reinterpret_cast<uint8_t*>(&data); nMaxReadCount = bad_min(size, sizeof(data));} break;

    ptr += sizeof(code); // jump extension header code
    while( uintptr_t(ptr - lpStream) <= searchlimit-6 ) //Loop until given limit.
    {
        code = (*((int32_t *)ptr));                    // read field code
        ptr += sizeof(int32_t);                            // jump field code
        size = (*((int16_t *)ptr));                    // read field size
        ptr += sizeof(int16_t);                            // jump field size

        if(IsValidSizeField(ptr, pEnd, size) == false)
            break;

        size_t nMaxReadCount = 0;
        uint8_t * fadr = NULL;

        switch (code)                                    // interpret field code
        {
            CASE('DT..', m_nDefaultTempo);
            CASE('RPB.', m_nDefaultRowsPerBeat);
            CASE('RPM.', m_nDefaultRowsPerMeasure);
            CASE_NOTXM('C...', m_nChannels);
            CASE('TM..', m_nTempoMode);
            CASE('PMM.', m_nMixLevels);
            CASE('CWV.', m_dwCreatedWithVersion);
            CASE('LSWV', m_dwLastSavedWithVersion);
            CASE('SPA.', m_nSamplePreAmp);
            CASE('VSTV', m_nVSTiVolume);
            CASE('DGV.', m_nDefaultGlobalVolume);
            CASE_NOTXM('RP..', m_nRestartPos);
            CASE('MSF.', m_ModFlags);
            case 'MIMA': GetMIDIMapper().Deserialize(ptr, size); break;
            case 'ChnS':
                if( (size <= 63*2) && (size % 2 == 0) )
                {
                    const uint8_t* pData = ptr;
                    const __int16 nLoopLimit = bad_min(size/2, ChnSettings.size() - 64);
                    for(__int16 i = 0; i<nLoopLimit; i++, pData += 2) if(pData[0] != 0xFF)
                    {
                        ChnSettings[i+64].nVolume = pData[1];
                        ChnSettings[i+64].nPan = 128;
                        if (pData[0] & 0x80) ChnSettings[i+64].dwFlags |= CHN_MUTE;
                        const UINT n = pData[0] & 0x7F;
                        if (n <= 64) ChnSettings[i+64].nPan = n << 2;
                        if (n == 100) ChnSettings[i+64].dwFlags |= CHN_SURROUND;
                    }
                }

            break;
        }

        if (fadr != NULL)                                    // if field code recognized
            memcpy(fadr,ptr,nMaxReadCount);    // read field data

        ptr += size;                                            // jump field data
    }

    // Validate read values.
    Limit(m_nDefaultTempo, GetModSpecifications().tempoMin, GetModSpecifications().tempoMax);
    //m_nRowsPerBeat
    //m_nRowsPerMeasure
    LimitMax(m_nChannels, GetModSpecifications().channelsMax);
    //m_nTempoMode
    //m_nMixLevels
    //m_dwCreatedWithVersion
    //m_dwLastSavedWithVersion
    //m_nSamplePreAmp
    //m_nVSTiVolume
    //m_nDefaultGlobalVolume);
    //m_nRestartPos
    //m_ModFlags


    #undef CASE
    #undef CASE_NOTXM
}

void module_renderer::S3MConvert(modplug::tracker::modevent_t *m, bool bIT) const
//--------------------------------------------------------
{
    UINT command = m->command;
    UINT param = m->param;
    switch (command | 0x40)
    {
    case 'A':    command = CmdSpeed; break;
    case 'B':    command = CmdPositionJump; break;
    case 'C':    command = CmdPatternBreak; if (!bIT) param = (param >> 4) * 10 + (param & 0x0F); break;
    case 'D':    command = CmdVolSlide; break;
    case 'E':    command = CmdPortaDown; break;
    case 'F':    command = CmdPortaUp; break;
    case 'G':    command = CmdPorta; break;
    case 'H':    command = CmdVibrato; break;
    case 'I':    command = CmdTremor; break;
    case 'J':    command = CmdArpeggio; break;
    case 'K':    command = CmdVibratoVolSlide; break;
    case 'L':    command = CmdPortaVolSlide; break;
    case 'M':    command = CmdChannelVol; break;
    case 'N':    command = CmdChannelVolSlide; break;
    case 'O':    command = CmdOffset; break;
    case 'P':    command = CmdPanningSlide; break;
    case 'Q':    command = CmdRetrig; break;
    case 'R':    command = CmdTremolo; break;
    case 'S':    command = CmdS3mCmdEx; break;
    case 'T':    command = CmdTempo; break;
    case 'U':    command = CmdFineVibrato; break;
    case 'V':    command = CmdGlobalVol; break;
    case 'W':    command = CmdGlobalVolSlide; break;
    case 'X':    command = CmdPanning8; break;
    case 'Y':    command = CmdPanbrello; break;
    case 'Z':    command = CmdMidi; break;
    case '\\':    command = CmdSmoothMidi; break; //rewbs.smoothVST
    // Chars under 0x40 don't save properly, so map : to ] and # to [.
    case ']':    command = CmdDelayCut; break;
    case '[':    command = CmdExtendedParameter; break;
    default:    command = 0;
    }
    //XXXih: gross
    m->command = (modplug::tracker::cmd_t) command;
    m->param = param;
}


void module_renderer::S3MSaveConvert(UINT *pcmd, UINT *pprm, bool bIT, bool bCompatibilityExport) const
//------------------------------------------------------------------------------------------------
{
    UINT command = *pcmd;
    UINT param = *pprm;
    switch(command)
    {
    case CmdSpeed:                            command = 'A'; break;
    case CmdPositionJump:            command = 'B'; break;
    case CmdPatternBreak:            command = 'C'; if (!bIT) param = ((param / 10) << 4) + (param % 10); break;
    case CmdVolSlide:            command = 'D'; break;
    case CmdPortaDown:    command = 'E'; if ((param >= 0xE0) && (m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))) param = 0xDF; break;
    case CmdPortaUp:            command = 'F'; if ((param >= 0xE0) && (m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))) param = 0xDF; break;
    case CmdPorta:    command = 'G'; break;
    case CmdVibrato:                    command = 'H'; break;
    case CmdTremor:                    command = 'I'; break;
    case CmdArpeggio:                    command = 'J'; break;
    case CmdVibratoVolSlide:            command = 'K'; break;
    case CmdPortaVolSlide:            command = 'L'; break;
    case CmdChannelVol:            command = 'M'; break;
    case CmdChannelVolSlide:    command = 'N'; break;
    case CmdOffset:                    command = 'O'; break;
    case CmdPanningSlide:            command = 'P'; break;
    case CmdRetrig:                    command = 'Q'; break;
    case CmdTremolo:                    command = 'R'; break;
    case CmdS3mCmdEx:                    command = 'S'; break;
    case CmdTempo:                            command = 'T'; break;
    case CmdFineVibrato:            command = 'U'; break;
    case CmdGlobalVol:            command = 'V'; break;
    case CmdGlobalVolSlide:    command = 'W'; break;
    case CmdPanning8:
        command = 'X';
        if (bIT && !(m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)))
        {
            if (param == 0xA4) { command = 'S'; param = 0x91; }    else
            if (param <= 0x80) { param <<= 1; if (param > 255) param = 255; } else
            command = param = 0;
        } else
        if (!bIT && (m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM)))
        {
            param >>= 1;
        }
        break;
    case CmdPanbrello:                    command = 'Y'; break;
    case CmdMidi:                            command = 'Z'; break;
    case CmdSmoothMidi:  //rewbs.smoothVST
        if(bCompatibilityExport)
            command = 'Z';
        else
            command = '\\';
        break;
    case CmdExtraFinePortaUpDown:
        if (param & 0x0F) switch(param & 0xF0)
        {
        case 0x10:    command = 'F'; param = (param & 0x0F) | 0xE0; break;
        case 0x20:    command = 'E'; param = (param & 0x0F) | 0xE0; break;
        case 0x90:    command = 'S'; break;
        default:    command = param = 0;
        } else command = param = 0;
        break;
    case CmdModCmdEx:
        command = 'S';
        switch(param & 0xF0)
        {
        case 0x00:    command = param = 0; break;
        case 0x10:    command = 'F'; param |= 0xF0; break;
        case 0x20:    command = 'E'; param |= 0xF0; break;
        case 0x30:    param = (param & 0x0F) | 0x10; break;
        case 0x40:    param = (param & 0x0F) | 0x30; break;
        case 0x50:    param = (param & 0x0F) | 0x20; break;
        case 0x60:    param = (param & 0x0F) | 0xB0; break;
        case 0x70:    param = (param & 0x0F) | 0x40; break;
        case 0x90:    command = 'Q'; param &= 0x0F; break;
        case 0xA0:    if (param & 0x0F) { command = 'D'; param = (param << 4) | 0x0F; } else command=param=0; break;
        case 0xB0:    if (param & 0x0F) { command = 'D'; param |= 0xF0; } else command=param=0; break;
        }
        break;
    // Chars under 0x40 don't save properly, so map : to ] and # to [.
    case CmdDelayCut:
        if(bCompatibilityExport || !bIT)
            command = param = 0;
        else
            command = ']';
        break;
    case CmdExtendedParameter:
        if(bCompatibilityExport || !bIT)
            command = param = 0;
        else
            command = '[';
        break;
    default:    command = param = 0;
    }
    command &= ~0x40;
    *pcmd = command;
    *pprm = param;
}



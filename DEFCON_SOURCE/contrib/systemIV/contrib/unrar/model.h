#ifndef _RAR_PPMMODEL_
#define _RAR_PPMMODEL_

#include "coder.h"
#include "rartypes.h"
#include "suballoc.h"

const int MAX_O=64;                   /* maximum allowed model order */

const int INT_BITS=7, PERIOD_BITS=7, TOT_BITS=INT_BITS+PERIOD_BITS,
          INTERVAL=1 << INT_BITS, BIN_SCALE=1 << TOT_BITS, MAX_FREQ=124;

#pragma pack(1)

struct SEE2_CONTEXT
{ // SEE-contexts for PPM-contexts with masked symbols
  unsigned short Summ;
  unsigned char Shift, Count;
  void init(int InitVal)
  {
    Summ=InitVal << (Shift=PERIOD_BITS-4);
    Count=4;
  }
  unsigned int getMean()
  {
    unsigned int RetVal=SHORT16(Summ) >> Shift;
    Summ -= RetVal;
    return RetVal+(RetVal == 0);
  }
  void update()
  {
    if (Shift < PERIOD_BITS && --Count == 0)
    {
      Summ += Summ;
      Count=3 << Shift++;
    }
  }
};


class ModelPPM;
struct PPM_CONTEXT;

struct STATE
{
  unsigned char Symbol;
  unsigned char Freq;
  PPM_CONTEXT* Successor;
};

struct PPM_CONTEXT 
{
    unsigned short NumStats;
    union
    {
      struct
      {
        unsigned short SummFreq;
        STATE _PACK_ATTR * Stats;
      } U;
      STATE OneState;
    };

    PPM_CONTEXT* Suffix;
    inline void encodeBinSymbol(ModelPPM *Model,int symbol);  // MaxOrder:
    inline void encodeSymbol1(ModelPPM *Model,int symbol);    //  ABCD    context
    inline void encodeSymbol2(ModelPPM *Model,int symbol);    //   BCD    suffix
    inline void decodeBinSymbol(ModelPPM *Model);  //   BCDE   successor
    inline bool decodeSymbol1(ModelPPM *Model);    // other orders:
    inline bool decodeSymbol2(ModelPPM *Model);    //   BCD    context
    inline void update1(ModelPPM *Model,STATE* p); //    CD    suffix
    inline void update2(ModelPPM *Model,STATE* p); //   BCDE   successor
    void rescale(ModelPPM *Model);
    inline PPM_CONTEXT* createChild(ModelPPM *Model,STATE* pStats,STATE& FirstState);
    inline SEE2_CONTEXT* makeEscFreq2(ModelPPM *Model,int Diff);
};
#pragma pack()

const unsigned int UNIT_SIZE=sizeof(PPM_CONTEXT);
const unsigned int FIXED_UNIT_SIZE=12;

/*
inline PPM_CONTEXT::PPM_CONTEXT(STATE* pStats,PPM_CONTEXT* ShorterContext):
        NumStats(1), Suffix(ShorterContext) { pStats->Successor=this; }
inline PPM_CONTEXT::PPM_CONTEXT(): NumStats(0) {}
*/

template <class T>
inline void _PPMD_SWAP(T& t1,T& t2) { T tmp=t1; t1=t2; t2=tmp; }


class ModelPPM
{
  private:
    friend struct PPM_CONTEXT;
    
    _PACK_ATTR SEE2_CONTEXT SEE2Cont[25][16], DummySEE2Cont;
    
    struct PPM_CONTEXT *MinContext, *MedContext, *MaxContext;
    STATE* FoundState;      // found next state transition
    int NumMasked, InitEsc, OrderFall, MaxOrder, RunLength, InitRL;
    unsigned char CharMask[256], NS2Indx[256], NS2BSIndx[256], HB2Flag[256];
    unsigned char EscCount, PrevSuccess, HiBitsFlag;
    unsigned short BinSumm[128][64];               // binary SEE-contexts

    RangeCoder Coder;
    SubAllocator SubAlloc;

    void RestartModelRare();
    void StartModelRare(int MaxOrder);
    inline PPM_CONTEXT* CreateSuccessors(bool Skip,STATE* p1);

    inline void UpdateModel();
    inline void ClearMask();
  public:
    ModelPPM();
    bool DecodeInit(Unpack *UnpackRead,int &EscChar);
    int DecodeChar();
};

#endif

#ifndef _RAR_VM_
#define _RAR_VM_

#include <stdlib.h>
#include "array.h"
#include "getbits.h"

#define VM_STANDARDFILTERS

#ifndef SFX_MODULE
#define VM_OPTIMIZE
#endif


#define VM_MEMSIZE                  0x40000
#define VM_MEMMASK           (VM_MEMSIZE-1)
#define VM_GLOBALMEMADDR            0x3C000
#define VM_GLOBALMEMSIZE             0x2000
#define VM_FIXEDGLOBALSIZE               64

enum VM_Commands
{
  VM_MOV,  VM_CMP,  VM_ADD,  VM_SUB,  VM_JZ,   VM_JNZ,  VM_INC,  VM_DEC,
  VM_JMP,  VM_XOR,  VM_AND,  VM_OR,   VM_TEST, VM_JS,   VM_JNS,  VM_JB,
  VM_JBE,  VM_JA,   VM_JAE,  VM_PUSH, VM_POP,  VM_CALL, VM_RET,  VM_NOT,
  VM_SHL,  VM_SHR,  VM_SAR,  VM_NEG,  VM_PUSHA,VM_POPA, VM_PUSHF,VM_POPF,
  VM_MOVZX,VM_MOVSX,VM_XCHG, VM_MUL,  VM_DIV,  VM_ADC,  VM_SBB,  VM_PRINT,

#ifdef VM_OPTIMIZE
  VM_MOVB, VM_MOVD, VM_CMPB, VM_CMPD,

  VM_ADDB, VM_ADDD, VM_SUBB, VM_SUBD, VM_INCB, VM_INCD, VM_DECB, VM_DECD,
  VM_NEGB, VM_NEGD,
#endif

  VM_STANDARD
};

enum VM_StandardFilters {
  VMSF_NONE, VMSF_E8, VMSF_E8E9, VMSF_ITANIUM, VMSF_RGB, VMSF_AUDIO, 
  VMSF_DELTA, VMSF_UPCASE
};

enum VM_Flags {VM_FC=1,VM_FZ=2,VM_FS=0x80000000};

enum VM_OpType {VM_OPREG,VM_OPINT,VM_OPREGMEM,VM_OPNONE};

struct VM_PreparedOperand
{
  VM_OpType Type;
  unsigned int Data;
  unsigned int Base;
  unsigned int *Addr;
};

struct VM_PreparedCommand
{
  VM_Commands OpCode;
  bool ByteMode;
  VM_PreparedOperand Op1,Op2;
};


struct VM_PreparedProgram
{
  VM_PreparedProgram() {AltCmd=NULL;}

  Array<VM_PreparedCommand> Cmd;
  VM_PreparedCommand *AltCmd;
  int CmdCount;

  Array<unsigned char> GlobalData;
  Array<unsigned char> StaticData;
  unsigned int InitR[7];

  unsigned char *FilteredData;
  unsigned int FilteredDataSize;
};

class RarVM:BitInput
{
  private:
    inline unsigned int GetValue(bool ByteMode,unsigned int *Addr);
    inline void SetValue(bool ByteMode,unsigned int *Addr,unsigned int Value);
    inline unsigned int* GetOperand(VM_PreparedOperand *CmdOp);
    void PrintState(unsigned int IP);
    void DecodeArg(VM_PreparedOperand &Op,bool ByteMode);
#ifdef VM_OPTIMIZE
    void Optimize(VM_PreparedProgram *Prg);
#endif
    bool ExecuteCode(VM_PreparedCommand *PreparedCode,int CodeSize);
#ifdef VM_STANDARDFILTERS
    VM_StandardFilters IsStandardFilter(unsigned char *Code,int CodeSize);
    void ExecuteStandardFilter(VM_StandardFilters FilterType);
    unsigned int FilterItanium_GetBits(unsigned char *Data,int BitPos,int BitCount);
    void FilterItanium_SetBits(unsigned char *Data,unsigned int BitField,int BitPos,
      int BitCount);
#endif

    unsigned char *Mem;
    unsigned int R[8];
    unsigned int Flags;
  public:
    RarVM();
    ~RarVM();
    void Init();
    void Prepare(unsigned char *Code,int CodeSize,VM_PreparedProgram *Prg);
    void Execute(VM_PreparedProgram *Prg);
    void SetValue(unsigned int *Addr,unsigned int Value);
    void SetMemory(unsigned int Pos,unsigned char *Data,unsigned int DataSize);
    static unsigned int ReadData(BitInput &Inp);
};

#endif

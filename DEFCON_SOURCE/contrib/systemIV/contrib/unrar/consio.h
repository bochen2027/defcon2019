#ifndef _RAR_CONSIO_
#define _RAR_CONSIO_

enum {ALARM_SOUND,ERROR_SOUND,QUESTION_SOUND};

enum PASSWORD_TYPE {PASSWORD_GLOBAL,PASSWORD_FILE,PASSWORD_ARCHIVE};

void InitConsoleOptions(MESSAGE_TYPE MsgStream,bool Sound);

void mprintf(const char *fmt,...);
void eprintf(const char *fmt,...);

#ifndef SILENT
void Alarm();
void GetPasswordText(char *Str,int MaxLength);
unsigned int GetKey();
bool GetPassword(PASSWORD_TYPE Type,const char *FileName,char *Password,int MaxLength);
int Ask(const char *AskStr);
#endif

int KbdAnsi(char *Addr,int Size);
void OutComment(char *Comment,int Size);

#ifdef SILENT
inline void Alarm() {}
inline void GetPasswordText(char *Str,int MaxLength) {}
inline unsigned int GetKey() {return(0);}
inline bool GetPassword(PASSWORD_TYPE Type,const char *FileName,char *Password,int MaxLength) {return(false);}
inline int Ask(const char *AskStr) {return(0);}
#endif

#endif

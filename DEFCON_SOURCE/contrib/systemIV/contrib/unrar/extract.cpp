#include "rarbloat.h"

#pragma warning (disable:4800)

CmdExtract::CmdExtract()
{
  TotalFileCount=0;
  *Password=0;
  Unp=new Unpack(&DataIO);
  Unp->Init(NULL);
}


CmdExtract::~CmdExtract()
{
  delete Unp;
  memset(Password,0,sizeof(Password));
}


void CmdExtract::DoExtract(CommandData *Cmd, UncompressedArchive *_uncompressed)
{
	DataIO.SetCurrentCommand(*Cmd->Command);
	
	struct FindData FD;
	while (Cmd->GetArcName(ArcName, ArcNameW, sizeof(ArcName)))
	{
		if (FindFile::FastFind(ArcName, ArcNameW, &FD))
		{
			DataIO.TotalArcSize += FD.Size;
		}
		Cmd->ArcNames->Rewind();
		while (Cmd->GetArcName(ArcName, ArcNameW, sizeof(ArcName)))
		{
			while (ExtractArchive(Cmd, _uncompressed) == EXTRACT_ARC_REPEAT)
				;
			if (FindFile::FastFind(ArcName, ArcNameW, &FD))
			{
				DataIO.ProcessedArcSize += FD.Size;
			}
		}
		
		if (TotalFileCount == 0 && *Cmd->Command != 'I')
		{
			mprintf(St(MExtrNoFiles));
			ErrHandler.SetErrorCode(WARNING);
		}
		else 
		{
			if (*Cmd->Command == 'I')
				mprintf(St(MDone));
			else
			{
				if (ErrHandler.GetErrorCount() == 0)
					mprintf(St(MExtrAllOk));
				else
					mprintf(St(MExtrTotalErr), ErrHandler.GetErrorCount());
			}
		}
	}
}


void CmdExtract::ExtractArchiveInit(CommandData *Cmd,Archive &Arc)
{
  DataIO.UnpArcSize=Arc.FileLength();

  FileCount=0;
  MatchedArgs=0;
#ifndef SFX_MODULE
  FirstFile=true;
#endif

  if (*Cmd->Password!=0)
    strcpy(Password,Cmd->Password);
  PasswordAll=(*Cmd->Password!=0);

  DataIO.UnpVolume=false;

  PrevExtracted=false;
  SignatureFound=false;
  AllMatchesExact=true;
  ReconstructDone=false;
}


EXTRACT_ARC_CODE CmdExtract::ExtractArchive(CommandData *Cmd, UncompressedArchive *_uncompressed)
{
	Archive Arc(Cmd);
	if (!Arc.WOpen(ArcName, ArcNameW))
	{
		ErrHandler.SetErrorCode(OPEN_ERROR);
		return (EXTRACT_ARC_NEXT);
	}
	
	if (!Arc.IsArchive(true))
	{
		mprintf(St(MNotRAR), ArcName);
		if (CmpExt(ArcName, "rar"))
			ErrHandler.SetErrorCode(WARNING);
		return (EXTRACT_ARC_NEXT);
	}
	
	if (Arc.Volume && Arc.NotFirstVolume)
	{
		char FirstVolName[NM];
		
		VolNameToFirstName(ArcName, FirstVolName, (Arc.NewMhd.Flags & MHD_NEWNUMBERING));
		if (stricomp(ArcName, FirstVolName) != 0 && FileExist(FirstVolName) &&
			Cmd->ArcNames->Search(FirstVolName, NULL, false))
			return (EXTRACT_ARC_NEXT);
	}
	ExtractArchiveInit(Cmd, Arc);
	
	if (*Cmd->Command == 'T' || *Cmd->Command == 'I')
		Cmd->Test = true;
	
	if (*Cmd->Command == 'I')
		Cmd->DisablePercentage = true;
	else
	{
		if (Cmd->Test)
			mprintf(St(MExtrTest), ArcName);
		else
			mprintf(St(MExtracting), ArcName);
	}
		
	Arc.ViewComment();
	
	while (1)
	{
		int Size = Arc.ReadHeader();
		bool Repeat = false;
		
		int headType = Arc.GetHeaderType();
		if (headType != ENDARC_HEAD)
		{
			MemMappedFile *mmfile = _uncompressed->AllocNewFile(Arc.NewLhd.FileName, Arc.NewLhd.UnpSize);
			DataIO.SetUnpackToMemory(mmfile->m_data, Arc.NewLhd.UnpSize);
		
			if (!ExtractCurrentFile(Cmd, Arc, Size, Repeat))
			{
				if (Repeat)
				{
					return (EXTRACT_ARC_REPEAT);
				}
				else
					break;
			}
		}
		else
		{
			break;
		}
	}

	return (EXTRACT_ARC_NEXT);
}


bool CmdExtract::ExtractCurrentFile(CommandData *Cmd,Archive &Arc,int HeaderSize,bool &Repeat)
{
  char Command=*Cmd->Command;
  if (HeaderSize<=0)
    if (DataIO.UnpVolume)
    {
#ifdef NOVOLUME
      return(false);
#else
      if (!MergeArchive(Arc,NULL,false,Command))
      {
        ErrHandler.SetErrorCode(WARNING);
        return(false);
      }
      SignatureFound=false;
#endif
    }
    else
      return(false);
  int HeadType=Arc.GetHeaderType();
  if (HeadType!=FILE_HEAD)
  {
    if (HeadType==AV_HEAD || HeadType==SIGN_HEAD)
      SignatureFound=true;
    if (HeadType==SUB_HEAD && PrevExtracted)
      SetExtraInfo(Cmd,Arc,DestFileName,*DestFileNameW ? DestFileNameW:NULL);
    if (HeadType==NEWSUB_HEAD)
    {
      if (Arc.SubHead.CmpName(SUBHEAD_TYPE_AV))
        SignatureFound=true;
#if !defined(NOSUBBLOCKS) && !defined(_WIN_CE)
      if (PrevExtracted)
        SetExtraInfoNew(Cmd,Arc,DestFileName,*DestFileNameW ? DestFileNameW:NULL);
#endif
    }
    if (HeadType==ENDARC_HEAD)
      if (Arc.EndArcHead.Flags & EARC_NEXT_VOLUME)
      {
#ifndef NOVOLUME
        if (!MergeArchive(Arc,NULL,false,Command))
        {
          ErrHandler.SetErrorCode(WARNING);
          return(false);
        }
        SignatureFound=false;
#endif
        Arc.Seek(Arc.CurBlockPos,SEEK_SET);
        return(true);
      }
      else
        return(false);
    Arc.SeekToNext();
    return(true);
  }
  PrevExtracted=false;

  if (SignatureFound ||
      !Cmd->Recurse && MatchedArgs>=Cmd->FileArgs->ItemsCount() &&
      AllMatchesExact)
    return(false);

  char ArcFileName[NM];
  IntToExt(Arc.NewLhd.FileName,Arc.NewLhd.FileName);
  strcpy(ArcFileName,Arc.NewLhd.FileName);

  wchar ArcFileNameW[NM];
  *ArcFileNameW=0;

  bool EqualNames=false;
  int MatchNumber=Cmd->IsProcessFile(Arc.NewLhd,&EqualNames);
  bool ExactMatch=MatchNumber!=0;
  if (Cmd->ExclPath==EXCL_BASEPATH)
  {
    *Cmd->ArcPath=0;
    if (ExactMatch)
    {
      Cmd->FileArgs->Rewind();
      if (Cmd->FileArgs->GetString(Cmd->ArcPath,NULL,sizeof(Cmd->ArcPath),MatchNumber-1))
        *PointToName(Cmd->ArcPath)=0;
    }
  }
  if (ExactMatch && !EqualNames)
    AllMatchesExact=false;

  bool WideName=(Arc.NewLhd.Flags & LHD_UNICODE);

  wchar *DestNameW=WideName ? DestFileNameW:NULL;

  ConvertPath(ArcFileName,ArcFileName);

  if (Arc.IsArcLabel())
    return(true);

  if (Arc.NewLhd.Flags & LHD_VERSION)
  {
    if (Cmd->VersionControl!=1 && !EqualNames)
    {
      if (Cmd->VersionControl==0)
        ExactMatch=false;
      int Version=ParseVersionFileName(ArcFileName,ArcFileNameW,false);
      if (Cmd->VersionControl-1==Version)
        ParseVersionFileName(ArcFileName,ArcFileNameW,true);
      else
        ExactMatch=false;
    }
  }
  else
    if (!Arc.IsArcDir() && Cmd->VersionControl>1)
      ExactMatch=false;

  Arc.ConvertAttributes();

#ifndef SFX_MODULE
  if ((Arc.NewLhd.Flags & (LHD_SPLIT_BEFORE/*|LHD_SOLID*/)) && FirstFile)
  {
    char CurVolName[NM];
    strcpy(CurVolName,ArcName);

    VolNameToFirstName(ArcName,ArcName,(Arc.NewMhd.Flags & MHD_NEWNUMBERING));
    if (stricomp(ArcName,CurVolName)!=0 && FileExist(ArcName))
    {
      *ArcNameW=0;
      Repeat=true;
      return(false);
    }
#if !defined(RARDLL) && !defined(_WIN_CE)
    if (!ReconstructDone)
    {
      ReconstructDone=true;

      RecVolumes RecVol;
      if (RecVol.Restore(Cmd,Arc.FileName,Arc.FileNameW,true))
      {
        Repeat=true;
        return(false);
      }
    }
#endif
    strcpy(ArcName,CurVolName);
  }
#endif
  DataIO.UnpVolume=(Arc.NewLhd.Flags & LHD_SPLIT_AFTER);
  DataIO.NextVolumeMissing=false;

  Arc.Seek(Arc.NextBlockPos-Arc.NewLhd.FullPackSize,SEEK_SET);

  bool TestMode=false;
  bool ExtrFile=false;
  bool SkipSolid=false;

#ifndef SFX_MODULE
  if (FirstFile && (ExactMatch || Arc.Solid) && (Arc.NewLhd.Flags & (LHD_SPLIT_BEFORE/*|LHD_SOLID*/))!=0)
  {
    if (ExactMatch)
    {
      Log(Arc.FileName,St(MUnpCannotMerge),ArcFileName);
#ifdef RARDLL
      Cmd->DllError=ERAR_BAD_DATA;
#endif
    }
    ExactMatch=false;
  }

  FirstFile=false;
#endif

  if (ExactMatch || (SkipSolid=Arc.Solid)!=0)
  {
    if (Arc.NewLhd.Flags & LHD_PASSWORD)
#ifndef RARDLL
      if (*Password==0)
#endif
      {
#ifdef RARDLL
        if (*Cmd->Password==0)
          if (Cmd->Callback==NULL ||
              Cmd->Callback(UCM_NEEDPASSWORD,Cmd->UserData,(LONG)Cmd->Password,sizeof(Cmd->Password))==-1)
            return(false);
        strcpy(Password,Cmd->Password);

#else
        if (!GetPassword(PASSWORD_FILE,ArcFileName,Password,sizeof(Password)))
        {
          return(false);
        }
#endif
      }
#if !defined(GUI) && !defined(SILENT)
      else
        if (!PasswordAll && (!Arc.Solid || Arc.NewLhd.UnpVer>=20 && (Arc.NewLhd.Flags & LHD_SOLID)==0))
        {
          eprintf(St(MUseCurPsw),ArcFileName);
          switch(Cmd->AllYes ? 1:Ask(St(MYesNoAll)))
          {
            case -1:
              ErrHandler.Exit(USER_BREAK);
            case 2:
              if (!GetPassword(PASSWORD_FILE,ArcFileName,Password,sizeof(Password)))
              {
                return(false);
              }
              break;
            case 3:
              PasswordAll=true;
              break;
          }
        }
#endif

#ifndef SFX_MODULE
    if (*Cmd->ExtrPath==0 && *Cmd->ExtrPathW!=0)
      WideToChar(Cmd->ExtrPathW,DestFileName);
    else
#endif
      strcpy(DestFileName,Cmd->ExtrPath);


#ifndef SFX_MODULE
    if (Cmd->AppendArcNameToPath)
    {
      strcat(DestFileName,PointToName(Arc.FileName));
      SetExt(DestFileName,NULL);
      AddEndSlash(DestFileName);
    }
#endif

    char *ExtrName=ArcFileName;

#ifndef SFX_MODULE
    int Length=strlen(Cmd->ArcPath);
    if (Length>0 && strnicomp(Cmd->ArcPath,ArcFileName,Length)==0)
    {
      ExtrName+=Length;
      while (*ExtrName==CPATHDIVIDER)
        ExtrName++;
    }
#endif

    if (Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
      strcat(DestFileName,PointToName(ExtrName));
    else
      strcat(DestFileName,ExtrName);

#ifndef SFX_MODULE
    if (!WideName && *Cmd->ExtrPathW!=0)
    {
      DestNameW=DestFileNameW;
      WideName=true;
      CharToWide(ArcFileName,ArcFileNameW);
    }
#endif

    if (WideName)
    {
      if (*Cmd->ExtrPathW!=0)
        strcpyw(DestFileNameW,Cmd->ExtrPathW);
      else
        CharToWide(Cmd->ExtrPath,DestFileNameW);

#ifndef SFX_MODULE
      if (Cmd->AppendArcNameToPath)
      {
        wchar FileNameW[NM];
        if (*Arc.FileNameW!=0)
          strcpyw(FileNameW,Arc.FileNameW);
        else
          CharToWide(Arc.FileName,FileNameW);
        strcatw(DestFileNameW,PointToName(FileNameW));
        SetExt(DestFileNameW,NULL);
        AddEndSlash(DestFileNameW);
      }
#endif
      wchar *ExtrNameW=ArcFileNameW;
#ifndef SFX_MODULE
      if (Length>0)
      {
        wchar ArcPathW[NM];
        CharToWide(Cmd->ArcPath,ArcPathW);
        Length=strlenw(ArcPathW);
      }
      ExtrNameW+=Length;
      while (*ExtrNameW==CPATHDIVIDER)
        ExtrNameW++;
#endif

      if (Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
        strcatw(DestFileNameW,PointToName(ExtrNameW));
      else
        strcatw(DestFileNameW,ExtrNameW);
    }
    else
      *DestFileNameW=0;

    ExtrFile=!SkipSolid/* && *ExtrName*/;
    if ((Cmd->FreshFiles || Cmd->UpdateFiles) && (Command=='E' || Command=='X'))
    {
      struct FindData FD;
      if (FindFile::FastFind(DestFileName,DestNameW,&FD))
      {
        if (FD.mtime >= Arc.NewLhd.mtime)
          ExtrFile=false;
      }
      else
        if (Cmd->FreshFiles)
          ExtrFile=false;
    }

#ifdef RARDLL
    if (*Cmd->DllDestName)
    {
      strncpy(DestFileName,Cmd->DllDestName,sizeof(DestFileName));
      *DestFileNameW=0;
      if (Cmd->DllOpMode!=RAR_EXTRACT)
        ExtrFile=false;
    }
    if (*Cmd->DllDestNameW)
    {
      strncpyw(DestFileNameW,Cmd->DllDestNameW,sizeof(DestFileNameW)/sizeof(DestFileNameW[0]));
      DestNameW=DestFileNameW;
      if (Cmd->DllOpMode!=RAR_EXTRACT)
        ExtrFile=false;
    }
#endif

#ifdef SFX_MODULE
    if (Arc.NewLhd.UnpVer!=UNP_VER && Arc.NewLhd.Method!=0x30)
#else
    if (Arc.NewLhd.UnpVer<13 || Arc.NewLhd.UnpVer>UNP_VER)
#endif
    {
#ifndef SILENT
      Log(Arc.FileName,St(MUnknownMeth),ArcFileName);
#ifndef SFX_MODULE
      Log(Arc.FileName,St(MVerRequired),Arc.NewLhd.UnpVer/10,Arc.NewLhd.UnpVer%10);
#endif
#endif
      ExtrFile=false;
      ErrHandler.SetErrorCode(WARNING);
#ifdef RARDLL
      Cmd->DllError=ERAR_UNKNOWN_FORMAT;
#endif
    }

    File CurFile;

    if (!IsLink(Arc.NewLhd.FileAttr))
      if (Arc.IsArcDir())
      {
        if (!ExtrFile || Command=='P' || Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
          return(true);
        if (SkipSolid)
        {
#ifndef GUI
          mprintf(St(MExtrSkipFile),ArcFileName);
#endif
          return(true);
        }
        TotalFileCount++;
        if (Cmd->Test)
        {
#ifndef GUI
          mprintf(St(MExtrTestFile),ArcFileName);
          mprintf(" %s",St(MOk));
#endif
          return(true);
        }
        MKDIR_CODE MDCode=MakeDir(DestFileName,DestNameW,Arc.NewLhd.FileAttr);
        bool DirExist=false;
        if (MDCode!=MKDIR_SUCCESS)
        {
          DirExist=FileExist(DestFileName,DestNameW);
          if (DirExist && !IsDir(GetFileAttr(DestFileName,DestNameW)))
          {
            bool UserReject;
            FileCreate(Cmd,NULL,DestFileName,DestNameW,Cmd->Overwrite,&UserReject,Arc.NewLhd.UnpSize,Arc.NewLhd.FileTime);
            DirExist=false;
          }
          CreatePath(DestFileName,DestNameW,true);
          MDCode=MakeDir(DestFileName,DestNameW,Arc.NewLhd.FileAttr);
        }
        if (MDCode==MKDIR_SUCCESS)
        {
#ifndef GUI
          mprintf(St(MCreatDir),DestFileName);
          mprintf(" %s",St(MOk));
#endif
          PrevExtracted=true;
        }
        else
          if (DirExist)
          {
            SetFileAttr(DestFileName,DestNameW,Arc.NewLhd.FileAttr);
            PrevExtracted=true;
          }
          else
          {
            Log(Arc.FileName,St(MExtrErrMkDir),DestFileName);
            ErrHandler.SysErrMsg();
#ifdef RARDLL
            Cmd->DllError=ERAR_ECREATE;
#endif
            ErrHandler.SetErrorCode(CREATE_ERROR);
          }
        if (PrevExtracted)
          SetDirTime(DestFileName,
            Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.NewLhd.mtime,
            Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.NewLhd.ctime,
            Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.NewLhd.atime);
        return(true);
      }
      else
      {
        if (Cmd->Test && ExtrFile)
          TestMode=true;
#if !defined(GUI) && !defined(SFX_MODULE)
        if (Command=='P' && ExtrFile)
          CurFile.SetHandleType(FILE_HANDLESTD);
#endif
ExtrFile = false;
        if ((Command=='E' || Command=='X') && ExtrFile && !Cmd->Test)
        {
          bool UserReject;
          if (!FileCreate(Cmd,&CurFile,DestFileName,DestNameW,Cmd->Overwrite,&UserReject,Arc.NewLhd.UnpSize,Arc.NewLhd.FileTime))
          {
            ExtrFile=false;
            if (!UserReject)
            {
              ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
              ErrHandler.SetErrorCode(CREATE_ERROR);
#ifdef RARDLL
              Cmd->DllError=ERAR_ECREATE;
#endif
              if (!IsNameUsable(DestFileName))
              {
                Log(Arc.FileName,St(MCorrectingName));
                MakeNameUsable(DestFileName,true);
                CreatePath(DestFileName,NULL,true);
                if (FileCreate(Cmd,&CurFile,DestFileName,NULL,Cmd->Overwrite,&UserReject,Arc.NewLhd.FullUnpSize,Arc.NewLhd.FileTime))
                  ExtrFile=true;
                else
                  ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
              }
            }
          }
        }
      }

    if (!ExtrFile && Arc.Solid)
    {
      SkipSolid=true;
      TestMode=true;
      ExtrFile=true;
    }
    if (ExtrFile)
    {
      if (!SkipSolid)
      {
        if (!TestMode && Command!='P' && CurFile.IsDevice())
        {
          Log(Arc.FileName,St(MInvalidName),DestFileName);
          ErrHandler.WriteError(Arc.FileName,DestFileName);
        }
        TotalFileCount++;
      }
      FileCount++;
#ifndef GUI
      if (Command!='I')
        if (SkipSolid)
          mprintf(St(MExtrSkipFile),ArcFileName);
        else
          switch(Cmd->Test ? 'T':Command)
          {
            case 'T':
              mprintf(St(MExtrTestFile),ArcFileName);
              break;
#ifndef SFX_MODULE
            case 'P':
              mprintf(St(MExtrPrinting),ArcFileName);
              break;
#endif
            case 'X':
            case 'E':
              mprintf(St(MExtrFile),DestFileName);
              break;
          }
      if (!Cmd->DisablePercentage)
        mprintf("     ");
#endif
      DataIO.CurUnpRead=0;
      DataIO.CurUnpWrite=0;
      DataIO.UnpFileCRC=Arc.OldFormat ? 0 : 0xffffffff;
      DataIO.PackedCRC=0xffffffff;
      DataIO.SetEncryption(
        (Arc.NewLhd.Flags & LHD_PASSWORD) ? Arc.NewLhd.UnpVer:0,Password,
        (Arc.NewLhd.Flags & LHD_SALT) ? Arc.NewLhd.Salt:NULL,false);
      DataIO.SetPackedSizeToRead(Arc.NewLhd.FullPackSize);
      DataIO.SetFiles(&Arc,&CurFile);
      DataIO.SetTestMode(TestMode);
      DataIO.SetSkipUnpCRC(SkipSolid);
#ifndef _WIN_CE
      if (!TestMode && !Arc.BrokenFileHeader &&
          (Arc.NewLhd.FullPackSize<<11)>Arc.NewLhd.FullUnpSize &&
          (Arc.NewLhd.FullUnpSize<100000000 || Arc.FileLength()>Arc.NewLhd.FullPackSize))
        CurFile.Prealloc(Arc.NewLhd.FullUnpSize);
#endif

      CurFile.SetAllowDelete(!Cmd->KeepBroken);

      if (!ExtractLink(DataIO,Arc,DestFileName,DataIO.UnpFileCRC,Command=='X' || Command=='E') &&
          (Arc.NewLhd.Flags & LHD_SPLIT_BEFORE)==0)
        if (Arc.NewLhd.Method==0x30)
          UnstoreFile(DataIO,Arc.NewLhd.FullUnpSize);
        else
        {
          Unp->SetDestSize(Arc.NewLhd.FullUnpSize);
#ifndef SFX_MODULE
          if (Arc.NewLhd.UnpVer<=15)
            Unp->DoUnpack(15,FileCount>1 && Arc.Solid);
          else
#endif
            Unp->DoUnpack(Arc.NewLhd.UnpVer,Arc.NewLhd.Flags & LHD_SOLID);
        }

      if (Arc.IsOpened())
        Arc.SeekToNext();

      bool BrokenFile=false;
      if (!SkipSolid)
      {
        if (Arc.OldFormat && UINT32(DataIO.UnpFileCRC)==UINT32(Arc.NewLhd.FileCRC) ||
            !Arc.OldFormat && UINT32(DataIO.UnpFileCRC)==UINT32(Arc.NewLhd.FileCRC^0xffffffff))
        {
#ifndef GUI
          if (Command!='P' && Command!='I')
            mprintf("%s%s ",Cmd->DisablePercentage ? " ":"\b\b\b\b\b ",St(MOk));
#endif
        }
        else
        {
          char *BadArcName=(Arc.NewLhd.Flags & LHD_SPLIT_BEFORE) ? NULL:Arc.FileName;
          if (Arc.NewLhd.Flags & LHD_PASSWORD)
          {
            Log(BadArcName,St(MEncrBadCRC),ArcFileName);
          }
          else
          {
            Log(BadArcName,St(MCRCFailed),ArcFileName);
          }
          BrokenFile=true;
          ErrHandler.SetErrorCode(CRC_ERROR);
#ifdef RARDLL
          Cmd->DllError=ERAR_BAD_DATA;
#endif
          Alarm();
        }
      }
#ifndef GUI
      else
        mprintf("\b\b\b\b\b     ");
#endif

      if (!TestMode && (Command=='X' || Command=='E') &&
          !IsLink(Arc.NewLhd.FileAttr))
      {
#if defined(_WIN_32) || defined(_EMX)
        if (Cmd->ClearArc)
          Arc.NewLhd.FileAttr&=~FA_ARCH;
#endif
        if (!BrokenFile || Cmd->KeepBroken)
        {
          if (BrokenFile)
            CurFile.Truncate();
          CurFile.SetOpenFileStat(
            Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.NewLhd.mtime,
            Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.NewLhd.ctime,
            Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.NewLhd.atime);
          CurFile.Close();
          CurFile.SetCloseFileStat(
            Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.NewLhd.mtime,
            Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.NewLhd.atime,
            Arc.NewLhd.FileAttr);
          PrevExtracted=true;
        }
      }
    }
  }
  if (ExactMatch)
    MatchedArgs++;
  if (DataIO.NextVolumeMissing || !Arc.IsOpened())
    return(false);
  if (!ExtrFile)
    if (!Arc.Solid)
      Arc.SeekToNext();
    else
      if (!SkipSolid)
        return(false);
  return(true);
}


void CmdExtract::UnstoreFile(ComprDataIO &DataIO,Int64 DestUnpSize)
{
  Array<unsigned char> Buffer(0x10000);
  while (1)
  {
    unsigned int Code=DataIO.UnpRead(&Buffer[0],Buffer.Size());
    if (Code==0 || (int)Code==-1)
      break;
    Code=Code<DestUnpSize ? Code:int64to32(DestUnpSize);
    DataIO.UnpWrite(&Buffer[0],Code);
    if (DestUnpSize>=0)
      DestUnpSize-=Code;
  }
}

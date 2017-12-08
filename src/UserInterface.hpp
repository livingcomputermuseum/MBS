//++
// UserInterface.hpp -> UserInterface for MASSBUS server
//
//       COPYRIGHT (C) 2017 BY THE LIVING COMPUTER MUSEUM, SEATTLE WA.
//
// LICENSE:
//    This file is part of the MASSBUS SERVER project.  MBS is free software;
// you may redistribute it and/or modify it under the terms of the GNU Affero
// General Public License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
//    MBS is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
// more details.  You should have received a copy of the GNU Affero General
// Public License along with MBS.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This class implements the MASSBUS server specific user interface (as
// opposed to the classes in CommandParser.hpp, which implement a generic
// command line parser).  The attentive reader can't help but notice that
// everything here is static - there's only one UI object, ever.  The critical
// readed might ask "Well then, why bother with a class definition at all?"
// Good point, but if nothing else it helps keep things together and localizes
// the UI implementation to this class.
//
//   Think of it as a poor man's namespace ...
//
// Bob Armstrong <bob@jfcl.com>   [5-NOV-2013]
//
// REVISION HISTORY:
//  1-NOV-13  RLA   New file.
// 25-APR-14  RLA   Add TDUMP verb.
//--
#pragma once

class CUI {
  //++
  //--

  // This class can never be instanciated, so all constructors are hidden ...
private:
  CUI() {};
  ~CUI() {};
  CUI(const CUI &) = delete;
  void operator= (const CUI &) = delete;

  //   Believe it or not, the only public hook into this whole thing is this
  // table of verbs that we pass to the command parser ...
public:
  static CCmdVerb * const g_aVerbs[];

  // Keyword tables ...
private:
  static const CCmdArgKeyword::keyword_t m_keysDriveType[];
  static const CCmdArgKeyword::keyword_t m_keysControllerType[];
  static const CCmdArgKeyword::keyword_t m_keysImageFormat[];
  static const CCmdArgKeyword::keyword_t m_keysPortType[];
  static const CCmdArgKeyword::keyword_t m_keysShareMode[];

  // Argument tables ...
private:
  static CCmdArgName     m_argUnit, m_argOptUnit, m_argAlias, m_argBus;
  static CCmdArgKeyword  m_argDriveType, m_argControllerType;
  static CCmdArgKeyword  m_argFormat, m_argPort, m_argShare;
  static CCmdArgNumber   m_argSerial, m_argBits, m_argCount;
  static CCmdArgNumber   m_argTransferDelay, m_argDataClock;
  static CCmdArgFileName m_argFileName, m_argOptFileName;
  static CCmdArgPCIAddress  m_argPCI;
  static CCmdArgDiskAddress m_argBlockNumber;

  // Modifier definitions ...
private:
  static CCmdModifier m_modSerial, m_modAlias, m_modOnline, m_modWrite;
  static CCmdModifier m_modBits, m_modFormat, m_modPort, m_modConfiguration;
  static CCmdModifier m_modOctal, m_modCount, m_modClock, m_modDelay;
  static CCmdModifier m_modForce, m_modShare;

  // Verb definitions ...
private:
  // CREATE verb definition ...
  static CCmdArgument * const m_argsCreate[];
  static CCmdModifier * const m_modsCreate[];
  static CCmdVerb m_cmdCreate;

  // CONNECT and DISCONNECT verb definitions ...
  static CCmdArgument * const m_argsConnect[];
  static CCmdModifier * const m_modsConnect[];
  static CCmdArgument * const m_argsDisconnect[];
  static CCmdVerb m_cmdConnect, m_cmdDisconnect;

  // ATTACH and DETACH verb definition ...
  static CCmdArgument * const m_argsAttach[];
  static CCmdModifier * const m_modsAttach[];
  static CCmdArgument * const m_argsDetach[];
  static CCmdVerb m_cmdAttach, m_cmdDetach;

  // REWIND verb definition ...
  static CCmdArgument * const m_argsRewind[];
  static CCmdVerb m_cmdRewind;

  // SET and SHOW verb definitions ...
  static CCmdArgument * const m_argsSetUnit[];
  static CCmdArgument * const m_argsShowUnit[];
  static CCmdArgument * const m_argsSetUPE[];
  static CCmdArgument * const m_argsShowUPE[];
  static CCmdModifier * const m_modsSetUnit[];
  static CCmdModifier * const m_modsSetUPE[];
  static CCmdVerb * const g_aSetVerbs[];
  static CCmdVerb * const g_aShowVerbs[];
  static CCmdVerb m_cmdSet, m_cmdSetUnit, m_cmdSetUPE;
  static CCmdVerb m_cmdShowUnit, m_cmdShowUPE;
  static CCmdVerb m_cmdShow, m_cmdShowAll, m_cmdShowVersion;

  // DUMP DISK and DUMP TAPE verb definition ...
  static CCmdArgument * const m_argsTapeDump[];
  static CCmdArgument * const m_argsDiskDump[];
  static CCmdModifier * const m_modsTapeDump[];
  static CCmdModifier * const m_modsDiskDump[];
  static CCmdVerb m_cmdDump, m_cmdTapeDump, m_cmdDiskDump;
  static CCmdVerb * const g_aDumpVerbs[];

  // Verb action routines ....
private:
  static bool DoCreate(CCmdParser &cmd);
  static bool DoConnect(CCmdParser &cmd), DoDisconnect(CCmdParser &cmd);
  static bool DoAttach(CCmdParser &cmd), DoDetach(CCmdParser &cmd);
  static bool DoSetUnit(CCmdParser &cmd), DoShowUnit(CCmdParser &cmd);
  static bool DoSetUPE(CCmdParser &cmd), DoShowUPE(CCmdParser &cmd);
  static bool DoShowVersion(CCmdParser &cmd), DoShowAll(CCmdParser &cmd);
  static bool DoTapeDump(CCmdParser &cmd), DoDiskDump(CCmdParser &cmd);
  static bool DoRewind(CCmdParser &cmd);

  // Other "helper" routines ...
private:
  static bool ParseCU (const string &strUnit, CMBA *&pBus, uint8_t &nUnit);
  static bool FindUnit (const string &strUnit, CMBA *&pBus, uint8_t &nUnit);
  static bool FindUnit (const string &strUnit, CMBA *&pBus, CBaseDrive *&pUnit);
  static bool FindDisk (const string &strUnit, CMBA *&pBus, CDiskDrive *&pDisk, bool fCheckAttach=true);
  static bool FindTape (const string &strUnit, CMBA *&pBus, CTapeDrive *&pTape, bool fCheckAttach=true);
  static bool FindBus (const string &strBus, char &chBus, CMBA *&pBus);
  static void ShowAllUnits();
  static void ShowOneUnit (const CBaseDrive *pUnit, bool fHeading);
  static void ShowAllUPEs();
  static void ShowOneUPE (const CDECUPE *pUPE, bool fHeading);
  static bool SetUnit (CMBA *pMBA, CBaseDrive *pDrive);
  static void DumpDisk18 (CMBA *pBus, CDiskDrive *pDrive, uint32_t lLBN);
  static void DumpDisk16 (CMBA *pBus, CDiskDrive *pDrive, uint32_t lLBN, bool fHex);

};

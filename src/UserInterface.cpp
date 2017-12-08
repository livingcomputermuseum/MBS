//++
// UserInterface.cpp -> MBS Specific User Interface code
//
//       COPYRIGHT (C) 2015-2017 Vulcan Inc.
//       Developed by Living Computers: Museum+Labs
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
//   This module implements the user interface specific to the MASSBUS Server
// process.  The first half of the file are parse tables for the generic
// command line parser classes from CommandParser.cpp, and the second half
// is the action routines needed to implement these commands.
//
//   One warning - remember that the UI runs in a different thread from the
// routines that execute the MASSBUS commands.  This means that any UI command
// that messes with a MASSBUS or CBaseDrive object must first acquire exclusive
// access to that MBA using the LockUI() method.  Once we're done fiddling with
// the server data, access is released with the UnlockUI() method.
//
// CHANGES FROM THE SPECIFICATION -
//   * The verb "QUIT" is a synonym for "EXIT"
//   * The "SET <unit>" syntax is no longer allowed - use "SET UNIT <unit>"
//   * Likewise, "SHOW <unit>" is not longer allowed - use "SHOW UNIT <unit>"
//   * "SET UNIT <unit> /ALIAS=xyz" was added for Rich
//   * "SET UPE" and "SHOW UPE" are reserved but not implemented
//   * "SET LOGGING" was added with modifiers /FILE=, /NOFILE, /CONSOLE and /LEVEL
//   * "SHOW LOGGING" and "SHOW VERSION" were added
//   * "Are you sure" was added to DISCONNECT, DETACH, EXIT and QUIT
//   * /SERIAL_NUMBER only accepts arguments in the range 0..65535
//   * EXIT or QUIT in a script just exits the script
//   * HELP command added
//   * If ";" is the first non-blank character on a line, the line is ignored
//   * The drive type (e.g. RP04) is now the second argument to CONNECT
//     The /TYPE= modifier has been eliminated
//   * "SHOW <unit>" is not implemented (however "SHOW" and "SHOW ALL" are)
//   * CREATE now requires three arguments - bus name, type and PCI address
//   * Command aliases were added, including DEFINE, UNDEFINE and SHOW ALIASES.
//   * The REWIND command was added
//
// Bob Armstrong <bob@jfcl.com>   [5-NOV-2013]
//
// REVISION HISTORY:
// 30-OCT-13  RLA   New file.
// 27-NOV-13  RLA   Add "subverbs" for SET UNIT, SET LOGGING, and SET UPE.
//                  Add "SHOW UNIT", "SHOW UPE", "SHOW VERSION", "SHOW LOGGING" and "SHOW ALL"
// 24-APR-14  RLA   Add tape drive support.
//                  Add DUMP TAPE and DUMP DISK commands.
// 15-OCT-14  RLA   Give error for SET UNIT .../ONLINE if unit is not attached.
// 17-DEC-14  RLA   DoSetLog() should return TRUE, not FALSE!
// 18-DEC-14  RLA   Check bus and drive compatibility in CREATE & CONNECT.
// 12-JUN-15  RLA   Split out common commands to CStandardUI
// 18-JUN-15  RLA   Add DEFINE, UNDEFINE, SHOW ALIAS and REWIND commands
// 10-SEP-15  RLA   Add TU77 and TU45 drives
// 22-SEP-15  RLA   Add /CONFIGURATION to CREATE to load the FPGA bitstream
//                  Add the /FORCE modifier to CREATE
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // memset(), strlen(), etc ...
#include "UPELIB.hpp"           // UPE library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "UPE.hpp"              // UPE library FPGA interface methods
#include "LogFile.hpp"          // UPE library message logging facility
#include "ImageFile.hpp"        // UPE library image file methods
#include "BitStream.hpp"        // UPE library Xilinx bitstream methods
#include "CommandParser.hpp"    // UPE library command line parsing methods
#include "CommandLine.hpp"      // UPE library shell (argc/argv) parser methods
#include "MBS.hpp"              // global declarations for this project
#include "DECUPE.hpp"           // DEC specific UPE/FPGA interface methods
#include "MASSBUS.h"            // MASSBUS commands and registers
#include "DriveType.hpp"        // static drive type data
#include "LogFile.hpp"          // message logging facility
#include "BaseDrive.hpp"        // single MASSBUS drive emulation
#include "DiskDrive.hpp"        // disk specific methods
#include "TapeDrive.hpp"        // tape specific methods
#include "MBA.hpp"              // MASSBUS drive collection class
#include "StandardUI.hpp"       // UPE library standard UI commands
#include "UserInterface.hpp"    // declarations for this module


// Drive type keywords ...
//   This is simply a table of the drive type names and the associated IDT
// value.  Note that we do something slightly uncool in that we assume a
// small integer (the IDT) can be cast to a void pointer type.  That's
// almost certainly safe, but it is an assumption...
const CCmdArgKeyword::keyword_t CUI::m_keysDriveType[] = {
  {"RP04", CDriveType::RP04},
  {"RP06", CDriveType::RP06},
  {"RP07", CDriveType::RP07},
  {"RM03", CDriveType::RM03},
  {"RM05", CDriveType::RM05},
  {"RM80", CDriveType::RM80},
  {"TU78", CDriveType::TU78},
  {"TU77", CDriveType::TU77},
  {"TU45", CDriveType::TU45},
  {NULL, 0}
};

// Controller type keywords ...
const CCmdArgKeyword::keyword_t CUI::m_keysControllerType[] = {
  {"DISK", CDECUPE::TYPE_DISK},
  {"TAPE", CDECUPE::TYPE_TAPE},
  {"MEIS", CDECUPE::TYPE_MEIS},
  {NULL, 0}
};

// Image file format keywords ...
//   This doesn't really do anything now, so the value is moot...
const CCmdArgKeyword::keyword_t CUI::m_keysImageFormat[] = {
  {"SIMH", 0},  {NULL, 0}
};

// Port type keywords ...
//   These don't really do anything now, so the value is moot...
const CCmdArgKeyword::keyword_t CUI::m_keysPortType[] = {
  {"A", 0},  {"B", 0},  {"BOTH", 0},  {NULL, 0}
};

// File sharing keywords ...
const CCmdArgKeyword::keyword_t CUI::m_keysShareMode[] = {
  {"NO*NE",  CImageFile::SHARE_NONE},
  {"RE*AD",  CImageFile::SHARE_READ},
  {"WR*ITE", CImageFile::SHARE_WRITE},
  {NULL, 0}
};


// Argument definitions ...
//   These objects define the arguments for all command line parameters as
// well as the arguments for command line modifiers that take a value.  The
// CCmdArgument objects don't distinguish between these two usages.
//
//   Notice that these are shared by many commands - for example, the same
// g_UnitArg object is shared by every command that takes a unit number as an
// argument.  That's probably not the most elegant way, however it saves a lot
// of object definitions and, since only one command can ever be parsed at any
// one time, it's harmless.
//
//   One last note - none of these can be "const" even though you might want to
// make them so.  That's because the CCmdArgument objects store the results of
// the parse in the object itself.
CCmdArgName        CUI::m_argUnit("unit");
CCmdArgName        CUI::m_argOptUnit("unit", true);
CCmdArgName        CUI::m_argAlias("alias");
CCmdArgKeyword     CUI::m_argDriveType("type", m_keysDriveType);
CCmdArgKeyword     CUI::m_argControllerType("type", m_keysControllerType);
CCmdArgNumber      CUI::m_argSerial("serial number", 10, 1, 65535);
CCmdArgFileName    CUI::m_argFileName("file name");
CCmdArgFileName    CUI::m_argOptFileName("file name", true);
CCmdArgNumber      CUI::m_argBits("bits", 10, 16, 18);
CCmdArgKeyword     CUI::m_argFormat("format", m_keysImageFormat);
CCmdArgKeyword     CUI::m_argPort("port", m_keysPortType);
CCmdArgName        CUI::m_argBus("bus");
CCmdArgPCIAddress  CUI::m_argPCI("PCI address", true);
CCmdArgDiskAddress CUI::m_argBlockNumber("block number", false);
CCmdArgNumber      CUI::m_argCount("sector count", 10, 1, 65535);
CCmdArgNumber      CUI::m_argDataClock("data clock", 0, 0, 255);
CCmdArgNumber      CUI::m_argTransferDelay("transfer delay", 0, 0, 255);
CCmdArgKeyword     CUI::m_argShare("share mode", m_keysShareMode);

// Modifier definitions ...
//   Like the command arguments, modifier objects may be shared by several
// commands.
CCmdModifier     CUI::m_modSerial("SE*RIAL_NUMBER", NULL, &m_argSerial);
CCmdModifier     CUI::m_modAlias("AL*IAS", NULL, &m_argAlias);
CCmdModifier     CUI::m_modOnline("ONL*INE", "OFFL*INE");
CCmdModifier     CUI::m_modWrite("WR*ITE", "NOWR*ITE");
CCmdModifier     CUI::m_modBits("BI*TS", NULL, &m_argBits);
CCmdModifier     CUI::m_modFormat("FORM*AT", NULL, &m_argFormat);
CCmdModifier     CUI::m_modPort("PO*RT", NULL, &m_argPort);
CCmdModifier     CUI::m_modOctal("OCT*AL","HEX*ADECIMAL");
CCmdModifier     CUI::m_modCount("CO*UNT", NULL, &m_argCount);
CCmdModifier     CUI::m_modClock("CLO*CK", NULL, &m_argDataClock);
CCmdModifier     CUI::m_modDelay("DEL*AY", NULL, &m_argTransferDelay);
CCmdModifier     CUI::m_modForce("FORCE", "NOFORCE");
CCmdModifier     CUI::m_modShare("SHA*RE", NULL, &m_argShare);
CCmdModifier     CUI::m_modConfiguration("CONF*IGURATION", NULL, &m_argFileName);

// CREATE verb definition ...
CCmdArgument * const CUI::m_argsCreate[]     = {&m_argBus, &m_argControllerType, &m_argPCI, NULL};
CCmdModifier * const CUI::m_modsCreate[]     = {&m_modForce, &m_modConfiguration, NULL};
CCmdVerb CUI::m_cmdCreate("CRE*ATE", &DoCreate, m_argsCreate, m_modsCreate);

// CONNECT and DISCONNECT verb definitions ...
CCmdArgument * const CUI::m_argsConnect[]    = {&m_argUnit, &m_argDriveType, NULL};
CCmdModifier * const CUI::m_modsConnect[]    = {&m_modSerial, &m_modAlias, NULL};
CCmdArgument * const CUI::m_argsDisconnect[] = {&m_argUnit, NULL};
CCmdVerb CUI::m_cmdConnect ("CON*NECT", &DoConnect, m_argsConnect, m_modsConnect);
CCmdVerb CUI::m_cmdDisconnect ("DISC*ONNECT", &DoDisconnect, m_argsDisconnect, NULL);

// ATTACH and DETACH verb definition ...
CCmdArgument * const CUI::m_argsAttach[] = {&m_argUnit, &m_argFileName, NULL};
CCmdModifier * const CUI::m_modsAttach[] = {&m_modWrite, &m_modOnline, &m_modBits, &m_modFormat, &m_modShare, NULL};
CCmdArgument * const CUI::m_argsDetach[] = {&m_argUnit, NULL};
CCmdVerb CUI::m_cmdAttach("ATT*ACH", &DoAttach, m_argsAttach, m_modsAttach);
CCmdVerb CUI::m_cmdDetach("DET*ACH", &DoDetach, m_argsDetach, NULL);

// REWIND verb definition ...
CCmdArgument * const CUI::m_argsRewind[] = {&m_argUnit, NULL};
CCmdVerb CUI::m_cmdRewind("REW*IND", &DoRewind, m_argsRewind, NULL);

// SET verb definition ...
CCmdArgument * const CUI::m_argsSetUnit[] = {&m_argUnit, NULL};
CCmdModifier * const CUI::m_modsSetUnit[] = {&m_modWrite, &m_modOnline, &m_modPort, &m_modAlias, NULL};
CCmdArgument * const CUI::m_argsSetUPE[] = {&m_argPCI, NULL};
CCmdModifier * const CUI::m_modsSetUPE[] = {&m_modDelay, &m_modClock, NULL};
CCmdVerb CUI::m_cmdSetUnit("UN*IT", &DoSetUnit, m_argsSetUnit, m_modsSetUnit);
CCmdVerb CUI::m_cmdSetUPE("UPE", &DoSetUPE, m_argsSetUPE, m_modsSetUPE);
CCmdVerb * const CUI::g_aSetVerbs[] = {
  &m_cmdSetUnit, &CStandardUI::m_cmdSetLog,
  &CStandardUI::m_cmdSetWindow, &m_cmdSetUPE, NULL
};
CCmdVerb CUI::m_cmdSet("SE*T", NULL, NULL, NULL, g_aSetVerbs);

// SHOW verb definition ...
CCmdArgument * const CUI::m_argsShowUnit[] = {&m_argOptUnit, NULL};
CCmdArgument * const CUI::m_argsShowUPE[] = {&m_argPCI, NULL};
CCmdVerb CUI::m_cmdShowUnit("UN*IT", &DoShowUnit, m_argsShowUnit);
CCmdVerb CUI::m_cmdShowUPE("UPE", &DoShowUPE, m_argsShowUPE);
CCmdVerb CUI::m_cmdShowVersion("VER*SION", &DoShowVersion);
CCmdVerb CUI::m_cmdShowAll("ALL", &DoShowAll);
CCmdVerb * const CUI::g_aShowVerbs[] = {
  &m_cmdShowUnit, &CStandardUI::m_cmdShowLog, &m_cmdShowUPE,
  &CStandardUI::m_cmdShowAliases, &m_cmdShowVersion, &m_cmdShowAll,
  NULL
};
CCmdVerb CUI::m_cmdShow("SH*OW", NULL, NULL, NULL, g_aShowVerbs);

// DUMP DISK and DUMP TAPE verb definition ...
CCmdArgument * const CUI::m_argsTapeDump[] = {&m_argFileName, NULL};
CCmdArgument * const CUI::m_argsDiskDump[] = {&m_argUnit, &m_argBlockNumber, NULL};
CCmdModifier * const CUI::m_modsTapeDump[] = {&m_modOctal, NULL};
CCmdModifier * const CUI::m_modsDiskDump[] = {&m_modOctal, &m_modCount, NULL};
CCmdVerb CUI::m_cmdTapeDump("T*APE", &DoTapeDump, m_argsTapeDump, m_modsTapeDump);
CCmdVerb CUI::m_cmdDiskDump("D*ISK", &DoDiskDump, m_argsDiskDump, m_modsDiskDump);
CCmdVerb * const CUI::g_aDumpVerbs[] = {
  &m_cmdTapeDump, &m_cmdDiskDump, NULL
};
CCmdVerb CUI::m_cmdDump("DU*MP", NULL, NULL, NULL, g_aDumpVerbs);

// Master list of all verbs ...
CCmdVerb * const CUI::g_aVerbs[] = {
  &m_cmdCreate,
  &m_cmdConnect, &m_cmdDisconnect, &m_cmdAttach, &m_cmdDetach,
  &m_cmdSet, &m_cmdShow, &m_cmdDump, &m_cmdRewind,
  &CStandardUI::m_cmdDefine, &CStandardUI::m_cmdUndefine,
  &CStandardUI::m_cmdIndirect, &CStandardUI::m_cmdExit,
  &CStandardUI::m_cmdQuit, &CCmdParser::g_cmdHelp,
  NULL
};


bool CUI::ParseCU (const string &strUnit, CMBA *&pBus, uint8_t &nUnit)
{
  //++
  //   This routine will attempt to parse a one or two character ("cu") unit
  // specification.  The first character must be a letter, "A" thru whatever,
  // and indicates the FPGA/UPE adapter number.  The second character must be
  // a digit, 0..7, which indicates the unit on that adapter.  If only one MBA
  // is attached then the letter may be omitted and the first MASSBUS is
  // assumed (e.g. "1" is the same as "A1").  If the string given doesn't
  // conform to these specifications then false is returned and an error
  // message is printed...
  //--
  const char *psz = strUnit.c_str();  char ch = toupper(*psz);
  pBus = NULL;  nUnit = CMBA::MAXUNIT;

  // The MBA must exist, so if there are none, punt...
  if (g_pMBAs->Count() == 0) {
    CMDERRS("no MASSBUS connected");  return false;
  }

  // Look for a letter - it's optional only if there's exactly one MASSBUS.
  if (isalpha(ch)) {
    if ((pBus = g_pMBAs->FindBus(ch)) == NULL) {
      CMDERRS("illegal MASSBUS name \"" << strUnit << "\"");  return false;
    }
    ch = *++psz;
  } else if (g_pMBAs->Count() > 1) {
    CMDERRS("specify a MASSBUS name \"" << strUnit << "\"");  return false;
  } else
    pBus = &g_pMBAs->Bus(0);

  // Now look for a single digit unit number ...
  if (isdigit(ch)) {
    nUnit = ch - '0';
    if ((nUnit < CMBA::MAXUNIT) && (*++psz == '\0')) return true;
  }
  CMDERRS("illegal unit number \"" << strUnit << "\"");
  return false;
}


bool CUI::FindUnit (const string &strUnit, CMBA *&pBus, uint8_t &nUnit)
{
  //++
  //   This routine converts either a "cu" format unit name or an alias, the
  // latter being searched first, into a MASSBUS pointer and a unit number.
  // If we're unsuccessful, an error message is printed.  Note that calling
  // this routine implies that the unit MUST already exist, and therefore it
  // cannot be used to parse the argument for the CONNECT command!
  //--

  // Search the alias names first ...
  if (g_pMBAs->FindUnit(strUnit, pBus, nUnit)) return true;

  // No match - try the old fashioned way ...
  if (!ParseCU(strUnit, pBus, nUnit)) return false;
  if (pBus->UnitExists(nUnit)) return true;
  CMDERRS("unit \"" << strUnit << "\" is not connected");
  return false;
}


bool CUI::FindUnit (const string &strUnit, CMBA *&pBus, CBaseDrive *&pUnit)
{
  //++
  //    This is the same as the previous version, however this one returns an
  // actual pointer to the unit, rather than the unit number ...
  //--
  uint8_t nUnit;
  if (!FindUnit(strUnit, pBus, nUnit)) return false;
  pUnit = pBus->Unit(nUnit);
  return true;
}


bool CUI::FindDisk (const string &strUnit, CMBA *&pBus, CDiskDrive *&pDisk, bool fCheckAttach)
{
  //++
  //   This routine just calls FindUnit(), but then it verifies that the unit
  // selected is a disk drive type and casts the result to a CDiskDrive.  If
  // the fCheckAttach parameter is TRUE, then it will also verify that the drive
  // is currently attached.  Note that we don't check the ONLINE status - that's
  // only relevant to the host interface side, and most UI commands don't care.
  //--
  CBaseDrive *pDrive = NULL;
  if (!FindUnit(m_argUnit.GetValue(), pBus, pDrive)) return false;
  if (!pDrive->IsDisk()) {
    CMDERRS("Unit " << *pDrive << " is not a disk");  return false;
  }
  if (fCheckAttach && !pDrive->IsAttached()) {
    CMDERRS("Unit " << *pDrive << " is not attached");  return false;
  }
  pDisk = (CDiskDrive *) pDrive;
  return true;
}


bool CUI::FindTape (const string &strUnit, CMBA *&pBus, CTapeDrive *&pTape, bool fCheckAttach)
{
  //++
  //   This routine just calls FindUnit(), but then it verifies that the unit
  // selected is a tape drive type and casts the result to a CTapeDrive...
  //--
  CBaseDrive *pDrive = NULL;
  if (!FindUnit(m_argUnit.GetValue(), pBus, pDrive)) return false;
  if (!pDrive->IsTape()) {
    CMDERRS("Unit " << *pDrive << " is not a tape");  return false;
  }
  if (fCheckAttach && !pDrive->IsAttached()) {
    CMDERRS("Unit " << *pDrive << " is not attached");  return false;
  }
  pTape = (CTapeDrive *) pDrive;
  return true;
}


bool CUI::FindBus (const string &strBus, char &chBus, CMBA *&pBus)
{
  //++
  //   This method will parse a MASSBUS name.  That part's easy - bus names
  // are currently just a single letter, 'A' .. whatever.  It returns the
  // bus name (one character) and a pointer to the corresponding MBA object.
  // If no bus with this name exists, then the latter will be null.
  //--
  const char *psz = strBus.c_str();  char ch = toupper(*psz);
  pBus = NULL;
  if (isalpha(ch)) {
    chBus = ch;  pBus = g_pMBAs->FindBus(ch);
    if (*++psz == '\0') return true;
  }
  CMDERRS("illegal bus name \"" << strBus << "\"");
  return false;
}


bool CUI::DoCreate (CCmdParser &cmd)
{
  //++
  //   The CREATE command creates a new virtual MASSBUS and (optionally)
  // connects it to a physical FPGA/UPE board.  The <bus> argument is the
  // name of the bus to be created, and the <PCI address> is the BDF address
  // of the PLX chip on the FPGA card (e.g. "06:0A.0").  If the PCI address
  // is omitted, an "offline" UPE is created and connected to the MASSBUS.
  // Offline UPEs are handy for debugging, but not much else.
  //
  // Format:
  //    CREATE <bus> <type> [<PCI address>]
  //
  // This command has no qualifiers.
  //--
  char chBus;  CMBA *pBus;  CDECUPE *pUPE;  uint8_t nVHDLtype;  bool fForce;

  // First parse the MASSBUS name - that's easy ...
  if (!FindBus(m_argBus.GetValue(), chBus, pBus)) return false;
  if (pBus != NULL) {
    CMDERRS("MASSBUS " << chBus << " already exists");  return false;
  }

  // Open the required UPE ...
  if (!g_pUPEs->Open(m_argPCI.GetBus(), m_argPCI.GetSlot(), (CUPE *&) pUPE)) return false;

  //   If the user wants to load a configuration bitstream into the FPGA, then
  // now is the time to do it!  Note that the configuration file is just a
  // Xilinx .BIT file from WebPack.  BTW, this is pointless but a NOP for
  // offline UPEs...
  if (m_modConfiguration.IsPresent()) {
    CBitStream bs(m_argFileName.GetFullPath());
    if (!bs.Open()) goto CloseUPE;
    LOGS(DEBUG, "Loading configuration \"" << bs.GetDesignName() << "\"");
    if (!pUPE->LoadConfiguration(&bs)) goto CloseUPE;
  }

  //   If this is a real (e.g. online) UPE, then make sure that the FPGA bit
  // stream loaded is of the correct type (i.e. disk, tape, NI, etc).  If this
  // is an offline (e.g. virtual) UPE, then just set the type to what we want.
  nVHDLtype = LOBYTE(m_argControllerType.GetKeyValue());
  if (pUPE->IsOffline()) {
    pUPE->SetVHDLtype(nVHDLtype);  //pUPE->SetVHDLversion(0xFFFF);
  } else {
    if (pUPE->GetVHDLtype() != nVHDLtype) {
      CMDERRS("UPE " << *pUPE << " has the wrong bit stream loaded");
      goto CloseUPE;
    }
  }

  //   See if the UPE is in use by another process and, if it is, ask the user
  // if he wants to steal it.  After that, lock it to our process ...
  fForce = m_modForce.IsPresent() && !m_modForce.IsNegated();
  if (!pUPE->Lock(fForce)) {
    string msg = "Seize control of UPE " + pUPE->GetBDF();
    if (cmd.InScript() || !cmd.AreYouSure(msg)) goto CloseUPE;
    pUPE->Lock(true);
  }

  // Initialize the FPGA control registers and capture interrupts ...
  if (!pUPE->Initialize()) goto CloseUPE;

  // And create the MASSBUS object for it ...
  return g_pMBAs->Create(chBus, pUPE, pBus);

  // Here if something fails after the UPE is opened - close it and return ...
CloseUPE:
  pUPE->Close();  return false;
}


bool CUI::DoConnect (CCmdParser &cmd)
{
  //++
  //   The CONNECT command connects a virtual drive to a MASSBUS. This is not
  // the same as putting a pack in the drive and spinning it up – that’s done
  // by the ATTACH command. It’s the functional equivalent of plugging a
  // MASSBUS cable into a drive, and in the real world this type of change
  // didn’t happen often.
  //
  // Format:
  //    CONNECT <unit> <type> /SERIAL_NUMBER=nnnn /ALIAS=xyz
  //--
  uint8_t nUnit;  CMBA *pBus;
  if (!ParseCU(m_argUnit.GetValue(), pBus, nUnit)) return false;
  if (pBus->UnitExists(nUnit)) {
    CMDERRS("unit " << pBus->Unit(nUnit)->GetCU() << " is already connected");
    return false;
  }
  uint8_t nIDT = LOBYTE(m_argDriveType.GetKeyValue ());
  if (!pBus->IsCompatible (nIDT)) {
    CMDERRS("unit type not compatible with MASSBUS type");
    return false;
  }
  pBus->LockUI();
  CBaseDrive &drive = pBus->AddUnit(nUnit, nIDT);
  if (m_argAlias.IsPresent()) drive.SetAlias(m_argAlias.GetValue());
  if (m_argSerial.IsPresent()) drive.SetSerialNumber(m_argSerial.GetNumber());
  pBus->UnlockUI();
  return true;
}


bool CUI::DoDisconnect (CCmdParser &cmd)
{
  //++
  //   The DISCONNECT command disconnects a virtual drive from a MASSBUS. In
  // the real world this would have been difficult to do while the host system
  // was running, but with the MDE it’s trivial. If the specified virtual drive
  // is online when this command is issued, it’ll be taken offline first. If
  // the virtual drive is attached to an image file, then it will be detached.
  //
  // Format:
  //    DISCONNECT <unit>
  //
  // This command has no qualifiers.
  //--
  CMBA *pBus;  CBaseDrive *pDrive;
  if (!FindUnit(m_argUnit.GetValue(), pBus, pDrive)) return false;
  if (pDrive->IsOnline()) {
    if (!cmd.AreYouSure("Unit " + pDrive->GetName() + " is online.")) return true;
  }
  pBus->LockUI();
  pBus->RemoveUnit(pDrive);
  pBus->UnlockUI();
  return true;
}


bool CUI::DoAttach (CCmdParser &cmd)
{
  //++
  //   The ATTACH command connects a virtual drive to an image file in the PC
  // file system. If the image file specified does not exist then an empty
  // file will be created.
  //
  // Format:
  //    ATTACH <unit> <file-name> /BITS=nn /FORMAT=xyz /ONLINE /NOWRITE /SHARE=xxx
  //--
  CMBA *pBus=NULL;  CBaseDrive *pDrive=NULL;

  // The first part is device (disk vs tape) independent ...
  if (!FindUnit(m_argUnit.GetValue(), pBus, pDrive)) return false;
  if (pDrive->IsAttached()) {
    if (!cmd.AreYouSure("Unit " + pDrive->GetName() + " is already attached.")) return true;
  }
  bool fOnline = m_modOnline.IsPresent() && !m_modOnline.IsNegated();
  int nShareMode = m_modShare.IsPresent() ? MKINT32(m_argShare.GetKeyValue()) : 0;

  //  Figure out the write locked/write enabled status of this device.  Notice
  // that for tape drives write locked is the default unless /WRITE is explicitly
  // specified, but for disks and all other devices, write enabled is the default
  // unless /NOWRITE is specified.
  bool fWrite = m_modWrite.IsPresent() ? !m_modWrite.IsNegated() : !(pDrive->IsTape());

  //   Note that with the addition of tape drive support the /BITS modifier
  // is optional.  For the moment it defaults to 18 bits...
  bool f18bits = true;
  if (m_argBits.IsPresent() && (m_argBits.GetNumber() == 16))  f18bits = false;
  pBus->LockUI();
  if (!pDrive->Attach(m_argFileName.GetFullPath(), !fWrite, nShareMode))
    {pBus->UnlockUI();  return false;}

  // And the rest is device (disk vs tape) dependent ...
  if (pDrive->IsDisk()) {
    CDiskDrive *pDisk = (CDiskDrive *) pDrive;
    pDisk->Set18Bit(f18bits);
  } else {
    //CTapeDrive *pTape = (CTapeDrive *) pDrive;
  }
  if (fOnline) pDrive->GoOnline();

  // All done!
  pBus->UnlockUI();
  return true;
}


bool CUI::DoDetach (CCmdParser &cmd)
{
  //++
  //   The DETACH command disconnects a virtual drive from an image. If the
  // specified unit is online it becomes offline first.  Note that this
  // command does not disconnect the drive from the MASSBUS; from the host’s
  // perspective, the drive is still present and powered up, but it simply has
  // no pack mounted.
  //
  // Format:
  //    DETACH <unit>
  //
  // This command has no qualifiers.
  //--
  CMBA *pBus;  CBaseDrive *pDrive;
  if (!FindUnit(m_argUnit.GetValue(), pBus, pDrive)) return false;
  if (!pDrive->IsAttached()) {
    CMDERRS("Unit " << *pDrive << " is not attached");
    return false;
  }
  pBus->LockUI();
  pDrive->Detach();
  pBus->UnlockUI();
  return true;
}


bool CUI::DoRewind (CCmdParser &cmd)
{
  //++
  //   The REWIND command rewinds a virtual tape drive.  It's equivalent to the
  // operator taking the drive offline, pressing the REWIND button, and then
  // putting it back online again.
  //
  // Format:
  //    REWIND <unit>
  //--

  // Find the selected device and unit, and make sure it's a tape drive ...
  CMBA *pBus;  CBaseDrive *pDrive;
  if (!FindUnit(m_argUnit.GetValue(), pBus, pDrive)) return false;
  if (!pDrive->IsAttached()) {
    CMDERRS("Unit " << *pDrive << " is not attached");
    return false;
  }
  if (!pDrive->IsTape()) {
    CMDERRS(m_argUnit.GetValue() << " is not a tape drive");  return false;
  }

  // Rewind the tape drive and we're done ...
  pBus->LockUI();
  ((CTapeDrive *) pDrive)->ManualRewind();
  pBus->UnlockUI();
  return true;
}


bool DoLoadUPE (CCmdParser &cmd)
{
  //++
  //
  // Format:
  //    LOAD UPE <PCI address> <bitstream file>
  //--
  return true;
}


bool CUI::DoSetUnit(CCmdParser &cmd)
{
  //++
  //   This method executes the "SET UNIT" subverb of the SET command.  This
  // allows the read only, online/offline, port and alias name of the unit to
  // be modified.
  //
  // Format:
  //    SET UNIT <unit> /[NO]WRITE /[ON|OFF]LINE /PORT=x /ALIAS=xyz
  //--
  CMBA *pBus = NULL;  CBaseDrive *pDrive = NULL;
  if (!FindUnit(m_argUnit.GetValue(), pBus, pDrive)) return false;
  pBus->LockUI();

  // "SET <unit> /NOWRITE" ...
  if (m_modWrite.IsPresent()) {
    if (m_modWrite.IsNegated() != pDrive->IsReadOnly())
      pDrive->SetReadOnly(m_modWrite.IsNegated());
  }

  // "SET <unit> /ONLINE" ...
  if (m_modOnline.IsPresent()) {
    if ( m_modOnline.IsNegated() &&  pDrive->IsOnline()) pDrive->GoOffline();
    if (!m_modOnline.IsNegated() && !pDrive->IsOnline()) {
      // Can only go online if the drive is attached!!
      if (!pDrive->IsAttached()) {
        CMDERRS("Unit " << *pDrive << " is not attached");
      } else
        pDrive->GoOnline();
    }
  }

  // "SET <unit> /PORT" ...
  if (m_modPort.IsPresent())
    CMDERRS("SET <unit> /PORT is not yet implemented");

  // "SET <unit> /ALIAS" ...
  if (m_modAlias.IsPresent()) pDrive->SetAlias(m_argAlias.GetValue());

  pBus->UnlockUI();
  return true;
}


bool CUI::DoSetUPE(CCmdParser &cmd)
{
  //++
  //   The "SET UPE" command allows certain UPE parameters to be modified, such
  // as the data clock speed and transfer delay.
  //
  // Format:
  //    SET UPE <PCI address> [/CLOCK=nn] [/DELAY=nn]
  //
  // Note that the clock and delay values are limited to 8 bits and default to
  // the DECIMAL radix.  Hexadecimal numbers may be specified by prefixing them
  // with the usual "0x" sequence.
  //--
  CDECUPE *pUPE = (CDECUPE *) g_pUPEs->Find(m_argPCI.GetBus(), m_argPCI.GetSlot());
  if (pUPE == NULL) {
    CMDERRS("no UPE card found with address " << CUPE::GetBDF(m_argPCI.GetBus(), m_argPCI.GetSlot(), 0));
    return false;
  }
  if (m_modDelay.IsPresent()) pUPE->SetTransferDelay(m_argTransferDelay.GetNumber());
  if (m_modClock.IsPresent()) pUPE->SetDataClock(m_argDataClock.GetNumber());
  return true;
}


bool CUI::DoShowVersion (CCmdParser &cmd)
{
  //++
  // Show just the MBS version number ...
  //--
  CMDOUTF("\nMASSBUS Disk and Tape Emulator v%d UPE Library v%d\n", MBSVER, UPEVER);
  return true;
}


void CUI::ShowOneUnit (const CBaseDrive *pUnit, bool fHeading)
{
  //++
  // Show detailed information for just one disk drive unit ...
  //--
  char szBuffer[CLog::MAXMSG], szBits[5];
  if (fHeading) {
    CMDOUTF("\nUnit Alias       Type    S/N   Status      Port  Image File");
    CMDOUTF("---- ----------  ----  ------  ----------  ----  ----------------------------");
  }
  // A bit of funny stuff for the 16/18 bit flag (which is disk only!) ...
  if (pUnit->IsDisk())
    sprintf_s(szBits, sizeof(szBits), "%02db", ((CDiskDrive *) pUnit)->Is18Bit() ? 18 : 16);
  else
    memset(szBits, 0, sizeof(szBits));
  // Now print the status message ...
  string sFileName = CStandardUI::Abbreviate(pUnit->GetFileName().c_str(), 28);
  sprintf_s(szBuffer, sizeof(szBuffer),
    " %2.2s  %-10.10s  %-4.4s  %6d  %3.3s %2.2s %3.3s  A/B   %-28s",
    pUnit->GetCU().c_str(), pUnit->GetAlias().c_str(), pUnit->GetType()->GetName(),
    pUnit->GetSerial(), pUnit->IsOnline() ? "ONL" : "OFL",
    pUnit->IsReadOnly() ? "RO" : "RW", szBits, sFileName.c_str());
  CMDOUTS(szBuffer);
}


void CUI::ShowAllUnits()
{
  //++
  // Show a table of all disk drives and their status ...
  //--
  uint8_t nDrives = 0;
  for (CMBAs::const_iterator itBus = g_pMBAs->begin();  itBus != g_pMBAs->end();  ++itBus) {
    for (uint8_t i = 0;  i < CMBA::MAXUNIT;  ++i) {
      if (!(*itBus)->UnitExists(i)) continue;
      ShowOneUnit((*itBus)->Unit(i), (nDrives == 0));  ++nDrives;
    }
  }
  if (nDrives == 0)
    CMDOUTF("No drives connected\n");
  else
    CMDOUTF("\n%d drives connected\n", nDrives);
}


bool CUI::DoShowUnit (CCmdParser &cmd)
{
  //++
  //   Show the current status of the specified unit.  If no unit name is
  // specified, then show the status of all drives ...
  //--
  if (!m_argOptUnit.IsPresent()) {
    ShowAllUnits();
  } else {
    CMBA *pBus;  CBaseDrive *pDrive;
    if (!FindUnit(m_argOptUnit.GetValue(), pBus, pDrive)) return false;
    ShowOneUnit(pDrive, true);  CMDOUTS("");
  }
  return true;
}


void CUI::ShowAllUPEs()
{
  //++
  // Show a table of all UPE/FPGA boards and their status ...
  //--
  uint8_t nUPEs = 0;
  for (CUPEs::const_iterator it = g_pUPEs->begin();  it != g_pUPEs->end();  ++it) {
    CDECUPE *pUPE = (CDECUPE *) *it;
    ShowOneUPE(pUPE, (nUPEs == 0));  ++nUPEs;
  }
  if (nUPEs == 0)
    CMDOUTF("No UPE/FPGA boards connected\n");
  else
    CMDOUTF("\n%d UPE/FPGA boards connected\n", nUPEs);
}


void CUI::ShowOneUPE (const CDECUPE *pUPE, bool fHeading)
{
  //++
  // Show detailed information for just one UPE ...
  //--
  char szBuffer[CLog::MAXMSG];
  if (fHeading) {
    CMDOUTF("\nUPE     Bus Slot   PLX     VHDL TDLY DCLK Type  Status    MBA  Units");
    CMDOUTF(  "------- --- ---- --------  ---- ---- ---- ----  --------  ---  -----");
  }
  sprintf_s(szBuffer, sizeof(szBuffer), "%s %3d  %3d  %04X %02X",
    pUPE->GetBDF().c_str(), pUPE->GetPCIBus(), pUPE->GetPCISlot(),
    pUPE->GetPLXChip(), pUPE->GetPLXRevision());
  if (pUPE->IsOpen()) {
    CMBA *pMBA = g_pMBAs->FindUPE(pUPE);
    assert(pMBA != NULL);
    char sz[CLog::MAXMSG];
    sprintf_s(sz, sizeof(sz), "  %4d 0x%02X 0x%02X %s  %s   %c    %d/%d",
      pUPE->GetRevision(), pUPE->GetTransferDelay(), pUPE->GetDataClock(),
      (pUPE->IsDisk() ? "DISK" : pUPE->IsTape() ? "TAPE" : pUPE->IsNI() ? "MEIS" : "????"),
      (pUPE->IsOffline() ? "OFFLINE " : pUPE->IsCableConnected() ? "ONLINE  " : "NO CABLE"),
      pMBA->GetName(), pMBA->UnitsOnline(), pMBA->UnitsConnected());
    strcat_s(szBuffer, sizeof(szBuffer), sz);
  }
  CMDOUTS(szBuffer);
}
//UPE     Bus Slot   PLX     VHDL TDLY DCLK Type  Status    Bus  Units
//------- --- ---- --------  ---- ---- ---- ----  --------  ---  -----
//00:00.0   0   0  0000 00      0 0x00 0x00 DISK  NO CABLE   A    0/0
//                                                OFFLINE
//                                                ONLINE

bool CUI::DoShowUPE (CCmdParser &cmd)
{
  //++
  //   Show the current status of the specified UPE.  If no UPE is specified,
  // then show the status of all known UPEs ...
  //--
  if (!m_argPCI.IsPresent()) {
    ShowAllUPEs();
  } else {
    CDECUPE *pUPE = (CDECUPE *) g_pUPEs->Find(m_argPCI.GetBus(), m_argPCI.GetSlot());
    if (pUPE == NULL) {
      CMDERRS("no UPE card found with address " << CUPE::GetBDF(m_argPCI.GetBus(), m_argPCI.GetSlot(), 0));
      return false;
    } else {
      ShowOneUPE(pUPE, true);  CMDOUTS("");
    }
  }
  return true;
}


bool CUI::DoShowAll (CCmdParser &cmd)
{
  //++
  // Show everything!
  //--
  DoShowVersion(cmd);  CStandardUI::DoShowLog(cmd);
  ShowAllUPEs();  ShowAllUnits();  CStandardUI::DoShowAllAliases(cmd);
  return true;
}


void CUI::DumpDisk18 (CMBA *pBus, CDiskDrive *pDisk, uint32_t lLBN)
{
  //++
  //   This routine will dump a single disk block in 18/36 bit format.  ASCII
  // text is disassembled as standard 7 bit, 5 characters per word, format.
  // Sorry, there's no SIXBIT output (at least not yet) nor is there any hex
  // option here...
  //--

  // Read the sector data ...
  uint32_t alData[SECTOR_SIZE];
  if (!pDisk->ReadSector18(lLBN, alData)) {
    CMDERRS("Error reading from unit " << *pDisk);  return;
  }

  // Print the header ...
  uint16_t nCylinder;  uint8_t nHead, nSector;
  pDisk->GetType()->LBAtoCHS(lLBN, nCylinder, nHead, nSector, true);
  printf("\n>>>>>>>>>>> Dump of Unit %s Type %s LBN %d (%d,%d,%d) <<<<<<<<<<<<\n",
    pDisk->GetName().c_str(), pDisk->GetType()->GetName(), lLBN, nCylinder, nHead, nSector);

  // And dump the data ...
  for (uint32_t i = 0;  i < SECTOR_SIZE;  i += 8) {
    printf("%03o/ ", i);
    // Dump in octal ...
    for (uint32_t j = 0;  j < 8;  j += 2)
      printf("%06o%06o ", MASK18(alData[i+j]), MASK18(alData[i+j+1]));
    printf(" ");
    // Dump in ASCII ...
    for (uint32_t j = 0;  j < 8;  j += 2) {
      uint64_t w = MK36(alData[i+j], alData[i+j+1]);
      for (uint32_t k = 0;  k < 5;  k++) {
        char ch = (w >> 29) & 0177;  w <<= 7;
        printf("%c", isprint(ch) ? ch : '.');
      }
    }
    printf("\n");
  }
  printf("\n");
}


void CUI::DumpDisk16 (CMBA *pBus, CDiskDrive *pDisk, uint32_t lLBN, bool fHex)
{
  //++
  //   And this routine will dump a single disk block in 16/32 bit PDP11 or VAX
  // format.  ASCII text is disassembled as one byte, 8 bits, per character just
  // as you'd expect.  Note that this version has the option to dump in octal
  // (usually PDP11) or hexadecimal (usually VAX) format.
  //--

  // Read the sector data and print the header ...
  uint32_t alData[SECTOR_SIZE];
  if (!pDisk->ReadSector16(lLBN, alData)) {
    CMDERRS("Error reading from unit " << *pDisk);  return;
  }
  uint16_t nCylinder;  uint8_t nHead, nSector;
  pDisk->GetType()->LBAtoCHS(lLBN, nCylinder, nHead, nSector, false);
  printf("\n>>>>>>>>>>> Dump of Unit %s Type %s LBN %d (%d,%d,%d) <<<<<<<<<<<<\n",
    pDisk->GetName().c_str(), pDisk->GetType()->GetName(), lLBN, nCylinder, nHead, nSector);

  // And dump the data ...
  for (uint32_t i = 0;  i < SECTOR_SIZE;  i += 8) {
    printf((fHex ? "%02X/ " : "%03o/ "), i);
    // Dump in octal ...
    for (uint32_t j = 0;  j < 8;  j++)
      printf((fHex ? "%04X " : "%06o "), LOWORD(alData[i+j]));
    printf(" ");
    // Dump in ASCII ...
    for (uint32_t j = 0;  j < 8;  j++) {
      char c1 = HIBYTE(alData[i+j]) & 0x7F;
      char c2 = LOBYTE(alData[i+j]) & 0x7F;
      printf("%c%c", (isprint(c2) ? c2 : '.'), (isprint(c1) ? c1 : '.'));
    }
    printf("\n");
  }
  printf("\n");
}


bool CUI::DoDiskDump (CCmdParser &cmd)
{
  //++
  //   The DUMP DISK command dumps one or more disk sectors in octal, hex or
  // ASCII (no, sorry - it doesn't do SIXBIT or RAD50, at least not yet!).
  // The disk unit specified must already be attached to a file, however it
  // does not need to be online to the host.
  //
  // Format:
  //    DUMP DISK <unit> <block> [/COUNT=nnnn] [/HEX] [/OCTAL]
  //
  //   Note that the <block> argument may be specified as either a single integer
  // LBN or a cylinder/head/sector address specified as "(c,h,s)"...
  //--

  // Find the unit specified and make sure it's really a disk.
  CMBA *pBus;  CDiskDrive *pDisk;
  if (!FindDisk(m_argUnit.GetValue(), pBus, pDisk)) return false;

  // if CHS addressing was specified, convert it to a single LBN ...
  uint32_t lLBN;
  if (m_argBlockNumber.IsCHS())
    lLBN = pDisk->GetType()->CHStoLBA(
      m_argBlockNumber.GetCylinder(),
      m_argBlockNumber.GetHead(),
      m_argBlockNumber.GetSector(),
      pDisk->Is18Bit());
  else
    lLBN = m_argBlockNumber.GetBlock();

  // Set the block count to 1 if /COUNT was not specified ...
  if (!m_argCount.IsPresent()) m_argCount.SetNumber(1);

  // Dump the block(s) and we're done!
  for (uint32_t i = 0;  i < m_argCount.GetNumber();  ++i) {
    if (pDisk->Is18Bit())
      DumpDisk18(pBus, pDisk, lLBN+i);
    else
      DumpDisk16(pBus, pDisk, lLBN+i, m_modOctal.IsNegated());
  }
  return true;
}


bool CUI::DoTapeDump (CCmdParser &cmd)
{
  //++
  //
  // Format:
  //    TDUMP <tape-file>
  //
  // This command has no qualifiers.
  //--
  CTapeImageFile tape;  uint8_t data[CTapeImageFile::MAXRECLEN];  CTapeImageFile::METADATA meta;
  if (!tape.Open(m_argFileName.GetFullPath(), true)) return false;

  for (;;) {
    meta = tape.ReadForwardRecord(data, sizeof(data));
    if (meta > 0) {
      CMDOUTF("<data record, length=%d>", meta);
    } else if (meta == CTapeImageFile::TAPEMARK) {
      CMDOUTF("<TAPE MARK>");
    } else if (meta == CTapeImageFile::EOTBOT) {
      CMDOUTF("<END OF TAPE>");  break;
    } else {
      CMDOUTF("*** TAPE ERROR (%d) ***", meta);  break;
    }
  }

  CMDOUTF("\n\n >>> NOW REVERSE!!! <<< \n");

  for (;;) {
    meta = tape.ReadReverseRecord(data, sizeof(data));
    if (meta > 0) {
      CMDOUTF("<data record, length=%d>", meta);
    } else if (meta == CTapeImageFile::TAPEMARK) {
      CMDOUTF("<TAPE MARK>");
    } else if (meta == CTapeImageFile::EOTBOT) {
      CMDOUTF("<BEGINNING OF TAPE>");  break;
    } else {
      CMDOUTF("*** TAPE ERROR (%d) ***", meta);  break;
    }
  }
  tape.Close();
  return true;
}

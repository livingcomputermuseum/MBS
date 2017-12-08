//++
// mbs.cpp - MASSBUS Server Daemon
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
//   This file is the main program for the MASSBUS server task.  It responds
// to commands from the FPGA board (running Bruce's UPE configuration) and
// transfers data to and from PC container files.  It's pretty easy when you
// think about it like that ...
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "UPELIB.hpp"           // UPE library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "UPE.hpp"              // UPE library FPGA interface methods
#include "ConsoleWindow.hpp"    // UPE console window methods
#include "LogFile.hpp"          // UPE library message logging facility
#include "ImageFile.hpp"        // UPE library image file methods
#include "CommandParser.hpp"    // UPE library command line parsing methods
#include "CommandLine.hpp"      // UPE library shell (argc/argv) parser methods
#include "StandardUI.hpp"       // UPE library standard UI commands
#include "MBS.hpp"              // global declarations for this project
#include "DECUPE.hpp"           // DEC specific FPGA interface definitions
#include "MASSBUS.h"            // MASSBUS commands and registers
#include "DriveType.hpp"        // static drive type data
#include "BaseDrive.hpp"        // basic MASSBUS drive emulation
#include "DiskDrive.hpp"        // disk specific methods
#include "TapeDrive.hpp"        // tape specific methods
#include "MBA.hpp"              // MASSBUS drive collection class
#include "UserInterface.hpp"    // MBS user interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in MBS.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CConsoleWindow *m_pConsole = NULL;  // console window object
CLog           *m_pLog     = NULL;  // message logging object (including console!)
CCmdParser     *m_pParser  = NULL;  // command line parser
CUPEs          *g_pUPEs    = NULL;  // collection of all known UPEs on this PC
CMBAs          *g_pMBAs    = NULL;  // collection of all MASSBUS adapters created


static bool ConfirmExit (CCmdParser &cmd)
{
  //++
  //   This routine is called whenever this application has been requested
  // to exit.  It checks to see if any drives are actually online and, if
  // any are, asks the operator for confirmation first.  It returns true if
  // we really should exit and false if we shouldn't.
  //--
  uint32_t nDrives = g_pMBAs->UnitsOnline();
  if (nDrives == 0) return true;
  ostringstream osDrives;
  osDrives << nDrives << " units are online";
  return cmd.AreYouSure(osDrives.str());
}


int main(int argc, char *argv[])
{
  //++
  //   Here's the main program for the MASSBUS server.  It doesn't look all
  // that hard after all!
  //--

  //   The very first thing is to create and initialize the console window
  // object, and after that we create and initialize the log object.  We
  // can't issue any error messages until we done these two things!
  m_pConsole = DBGNEW CConsoleWindow();
  m_pLog = DBGNEW CLog(PROGRAM, m_pConsole);

  //   Parse the command options.  Note that we want to do this BEFORE we
  // setup the console window, since the command line may tell us to detach
  // and create a new window...
  if (!CStandardUI::ParseOptions(PROGRAM, argc, argv)) goto shutdown;

  //   Set the console window defaults - foreground and background color,
  // scrolling buffer size, title, and icon ...
  m_pConsole->SetTitle("MASSBUS Disk and Tape Emulator v%d", MBSVER);
  //m_pConsole->SetIcon(IDI_MBS);
  //m_pConsole->SetBufferSize(132, 2000);
  m_pConsole->SetWindowSize(80, 40);
  m_pConsole->SetColors(CConsoleWindow::YELLOW, CConsoleWindow::BLACK);

  // We're finally ready to say hello ...
#ifdef _DEBUG
  CMDOUTF("MASSBUS Disk and Tape Emulator v%d DEBUG BUILD on %s %s", MBSVER, __DATE__, __TIME__);
#else
  CMDOUTF("MASSBUS Disk and Tape Emulator v%d RELEASE BUILD on %s %s", MBSVER, __DATE__, __TIME__);
#endif
  CMDOUTS("UPE Library v" << UPEVER << " PLX SDK library v" << CUPE::GetSDKVersion());

  // Create the UPE collection and populate it with all known FPGA/UPE boards.
  g_pUPEs = new CUPEs(NewDECUPE);
  g_pUPEs->Enumerate();
  if (g_pUPEs->Count() == 0) {
    LOGS(WARNING, "no UPEs detected");
  } else {
    LOGS(DEBUG, g_pUPEs->Count() << " UPEs detected");
  }

  //   Create an empty MASSBUS collection.  It'll be populated gradually as
  // the operator issues CREATE commands ...
  g_pMBAs = new CMBAs();

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  m_pParser = new CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, m_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    m_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the MBS program.  Note that any MASSBUS adapters that are created get
  // their own threads for executing disk functions.
  m_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

shutdown:
  // Delete all our global objects.  Once again, the order here is important!
  delete m_pParser;   // the command line parser can go away first
  delete g_pMBAs;     // spin down disks, and delete all MBAs
  delete g_pUPEs;     // disconnect all UPEs
  delete m_pLog;      // close the log file
  delete m_pConsole;  // lastly (always lastly!) close the console window
#ifdef _DEBUG
//system("pause");
//_CrtDumpMemoryLeaks();
#endif
  return 0;
}

//++
// MBA.cpp -> CMBA (collection of MASSBUS drives) methods 
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
//   This module implements the CMBA class - this class is really nothing more
// than a simple collection of CDiskDrive or CTapeDrive objects that represen
// the individual units on the bus.  The methods in this class deal with things
// that are global to the bus, such as dispatching commands.
//
//   It's worth pointing out that the MBS user interface runs in the background
// and a separate thread is created for each MBA object.  This thread runs
// the CommandLoop() method which endlessly reads and executes MASSBUS commands
// from the FPGA/UPE. It's not thread safe for the background code or the UI to
// directly call any method that modifies this object - instead, the UI needs
// to use the LockUI() and UnlockUI() methods to guarantee exclusive access.
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // strlen(), strncpy(), etc ...
#include <vector>               // C++ std::vector template
#include <unordered_set>        // C++ std::unordered_set (a simple list) template
#include <unordered_map>        // C++ std::unordered_map (aka hash table) template
#include <iostream>             // C++ style output for ERRORS() and DEBUGS() ...
#include <sstream>              // ostringstream, et al, for LOGS() ...
#include "Mutex.hpp"            // CMutex critical section lock
#include "Thread.hpp"           // CThread portable thread library
#include "UPELIB.hpp"           // UPE library definitions
#include "UPE.hpp"              // UPE library FPGA interface methods
#include "LogFile.hpp"          // UPE library message logging facility
#include "ImageFile.hpp"        // UPE library image file methods
#include "MBS.hpp"              // global declarations for this project
#include "MASSBUS.h"            // MASSBUS commands and registers
#include "DECUPE.hpp"           // DEC specific UPE/FPGA interface methods
#include "DriveType.hpp"        // static drive type data
#include "BaseDrive.hpp"        // single disk drive emulation
#include "DiskDrive.hpp"        // disk specific emulation
#include "TapeDrive.hpp"        // tape specific emulation
#include "MBA.hpp"              // declarations for this module



CMBA::CMBA (char chBus, CDECUPE &upe)
  : m_chBus(chBus), m_UPE(upe), m_ChannelThread(&CMBA::CommandLoop)
{
  //++
  //   The constructor simply initializes an empty collection of drives.
  // The only thing we really need to know is a pointer to the FPGA/UPE
  // associated with this MASSBUS - since each FPGA is connected to exactly
  // one bus, all drives in this collection share the same UPE.
  //--
  for (uint8_t i = 0;  i < MAXUNIT;  ++i)  m_apUnits[i] = NULL;
  string sName = string("MASSBUS ") + string(1, GetName());
  m_ChannelThread.SetName(sName.c_str());
  m_ChannelThread.SetParameter(this);
}


CMBA::~CMBA()
{
  //++
  // The destructor deletes all the attached units, if any ...
  //--
  m_ChannelThread.WaitExit();
  for (uint8_t i = 0;  i < MAXUNIT;  ++i)
    if (UnitExists(i)) delete m_apUnits[i];
}


CBaseDrive &CMBA::AddUnit (uint8_t nUnit, CBaseDrive &Unit)
{
  //++
  //   This method adds an existing CBaseDrive object to this MASSBUS.  Note
  // that once added the drive becomes the "property" of this collection - 
  // it'll be deleted by the RemoveDrive() method.  The caller isn't expected
  // to explicitly delete it.
  //--
  assert(!UnitExists(nUnit) && IsCompatible(Unit));
  m_apUnits[nUnit] = &Unit;  SetDriveMap();
  LOGS(DEBUG, Unit.GetType() << " unit " << nUnit << " connected to MASSBUS " << GetName());
  return Unit;
}


CBaseDrive &CMBA::AddUnit (uint8_t nUnit, uint8_t nIDT)
{
  //++
  // Create a new disk or tape object and add it to the bus ...
  //--
  assert(!UnitExists(nUnit) && IsCompatible(nIDT));
  if (CDriveType::GetDriveType(nIDT)->IsTape())
    return AddUnit(nUnit, *new CTapeDrive(*this, nUnit, nIDT));
  else
    return AddUnit(nUnit, *new CDiskDrive(*this, nUnit, nIDT));
}


void CMBA::RemoveUnit (uint8_t nUnit)
{
  //++
  // Remove a drive from this collection and delete the CBaseDrive object!
  //--
  assert(UnitExists(nUnit));
  delete m_apUnits[nUnit];  m_apUnits[nUnit] = NULL;  SetDriveMap();
  LOGS(DEBUG, "unit " << nUnit << " disconnected from MASSBUS " << GetName());
}


void CMBA::RemoveUnit (CBaseDrive *pUnit)
{
  //++
  // Remove a drive given it's handle rather than the unit number.
  //--
  assert(pUnit != NULL);
  uint8_t nUnit = pUnit->GetUnit();
  assert(m_apUnits[nUnit] == pUnit);
  RemoveUnit(nUnit);
}


uint8_t CMBA::FindUnit (const string &strAlias) const
{
  //++
  //  Search all units on this MBA for one with the specified alias ...
  //--
  for (uint8_t i = 0;  i < MAXUNIT;  ++i) {
    if (!UnitExists(i)) continue;
    if (strAlias == m_apUnits[i]->GetAlias()) return i;
  }
  return MAXUNIT;
}


void CMBA::SetDriveMap () const
{
  //++
  //   This routine sets the map of connected drives for the FPGA.  The drive
  // map is eight bits, bit 0 corresponds to unit 0, bit 1 to unit 1, etc.
  // A 1 bit means the drive exists, and zero means there's no drive with that
  // unit select.
  //--
  uint32_t nMap = 0;
  for (uint8_t i = 0;  i < MAXUNIT;  ++i) {
    if (UnitExists(i))  nMap |= 1 << i;
  }
  m_UPE.SetDrivesAttached(nMap);
}


uint32_t CMBA::UnitsConnected() const
{
  //++
  // Count the number of units connected to this MBA ...
  //--
  uint32_t nCount = 0;
  for (uint8_t i = 0;  i < MAXUNIT;  ++i)
    if (UnitExists(i))  ++nCount;
  return nCount;
}


uint32_t CMBA::UnitsOnline() const
{
  //++
  // Count the number of units online on this MBA ...
  //--
  uint32_t nCount = 0;
  for (uint8_t i = 0;  i < MAXUNIT;  ++i)
    if (UnitExists(i)  &&  m_apUnits[i]->IsOnline())  ++nCount;
  return nCount;
}


bool CMBA::IsCompatible (const CBaseDrive &Unit) const
{
  //++
  //   There are different "flavors" of the VHDL code that runs in the UPE -
  // a disk emulation version, a tape emulation version, and an MEIS version.
  // This means that a MASSBUS connected to, say, a UPE running the tape code
  // can only connect to tape drive units.  This routine checks the base unit
  // specified to determine if it's compatible.
  //--
  if (IsDisk() && Unit.IsDisk()) return true;
  if (IsTape() && Unit.IsTape()) return true;
  if (IsNI()   && Unit.IsNI()  ) return true;
  return false;
}


bool CMBA::IsCompatible (uint8_t nIDT) const
{
  //++
  // Ditto, except that this version works from the IDT instead...
  //--
  if (IsDisk() && CDriveType::GetDriveType(nIDT)->IsDisk()) return true;
  if (IsTape() && CDriveType::GetDriveType(nIDT)->IsTape()) return true;
  if (IsNI  () && CDriveType::GetDriveType(nIDT)->IsNI  ()) return true;
  return false;
}


void CMBA::DoCommand (uint32_t lCommand)
{
  //++
  //   This method is called when we find a word in the MASSBUS command silo.
  // The FPGA stores the unit number of the addressed drive in bits 18..16 of
  // the silo longword - we extract these bits, find the corresponding unit,
  // and pass along the command for it to handle.
  //
  //   Note that the command silo longword contains other bits too, besides
  // just the MASSBUS command.  In particular for tapes it also contains the
  // address of the MASSBUS register, which tells us which unit is selected.
  // We don't have to worry about that here, but that's why we pass along all
  // 32 bits from the command silo to the device's DoCommand() method.
  //--
  assert(CDECUPE::IsCommandValid(lCommand));
  uint8_t nUnit = CDECUPE::ExtractUnit(lCommand);
  if (!UnitExists(nUnit)) {
    LOGF(WARNING, "received command (0x%08X) for non-existent unit %d", lCommand, nUnit);
  //   Note that tape drives accept many commands (e.g. READ SENSE, formatter
  // clear, etc) even while the unit is offline.  That's because the formatter
  // is online, even if the specific slave is not.
  } else if (!IsTape() && !m_apUnits[nUnit]->IsOnline()) {
    LOGF(WARNING, "received command (0x%08X) for offline unit %d", lCommand, nUnit);
  } else
    m_apUnits[nUnit]->DoCommand(lCommand);
}


void* THREAD_ATTRIBUTES CMBA::CommandLoop (void *pParam)
{
  //++
  //   This method is the background thread for this MASSBUS adapter. It reads
  // and executes commands more or less forever (well, until the m_fExitLoop
  // flag is set).  The thread running this procedure is created by the
  // BeginThread() method and terminated by the ExitThread() method. The latter
  // works by setting the m_fExitLoop flag to true and then waiting for this
  // procedure to exit.
  //--
  CThread *pThread = (CThread *) pParam;
  CMBA *pMBA = (CMBA *) pThread->GetParameter();
  LOGS(DEBUG, "thread for " << pThread->GetName() << " is running");
  while (!pThread->IsExitRequested()) {
    uint32_t cmd = pMBA->m_UPE.WaitCommand();
    if (cmd == CDECUPE::ERROR) break;
    if (cmd == CDECUPE::TIMEOUT) continue;
    pMBA->m_UIlock.Enter();
    pMBA->DoCommand(cmd);
    pMBA->m_UIlock.Leave();
  }
  LOGS(DEBUG, "thread for " << pThread->GetName() << " terminated");
  return pThread->End();
}


///////////////////////////////////////////////////////////////////////////////


CMBAs::~CMBAs()
{
  //++
  //   The dstructor for the MASSBUS collection just destroys all the MBA
  // objects, but we provide a little extra code for debugging messages...
  //--
  for (iterator it = begin();  it != end();  ++it) {
    char chBus = (*it)->GetName();
    delete *it;
    LOGS(DEBUG, "MASSBUS " << chBus << " disconnected");
  }
}


CMBA *CMBAs::FindUPE (const CDECUPE *pUPE) const
{
  //++
  //   Search the MBAs in this collection for the one that's attached to a
  // particular UPE ...
  //--
  for (const_iterator it = begin();  it != end();  ++it)
    if (&((*it)->GetUPE()) == pUPE) return *it;
  return NULL;
}


bool CMBAs::FindUnit(const string &strUnit, CMBA *&pBus, uint8_t &nUnit) const
{
  //++
  //   Search all the units on all the MBAs in this collection for one which
  // has the specified alias ...
  //--
  for (const_iterator it = begin();  it != end();  ++it) {
    pBus = *it;
    nUnit = pBus->FindUnit(strUnit);
    if (nUnit < CMBA::MAXUNIT) return true;
  }
  return false;
}


CBaseDrive *CMBAs::FindUnit(const string &strUnit) const
{
  //++
  // Same as before, but this time return a pointer to the unit directly ...
  //--
  uint8_t nUnit;  CMBA *pBus;
  if (!FindUnit(strUnit, pBus, nUnit)) return NULL;
  return pBus->Unit(nUnit);
}


CMBA *CMBAs::FindBus (char chBus) const
{
  //++
  // Search all MBAs for one with the specified name ...
  //--
  for (const_iterator it = begin();  it != end();  ++it)
    if ((*it)->GetName() == chBus) return *it;
  return NULL;
}


uint32_t CMBAs::UnitsConnected() const
{
  //++
  // Return the total number of units connected to all MBAs...
  //--
  uint32_t nCount = 0;
  for (const_iterator it = begin();  it != end();  ++it)
    nCount += (*it)->UnitsConnected();
  return nCount;
}


uint32_t CMBAs::UnitsOnline() const
{
  //++
  // Return the total number of units online on all MBAs ...
  //--
  uint32_t nCount = 0;
  for (const_iterator it = begin();  it != end();  ++it)
    nCount += (*it)->UnitsOnline();
  return nCount;
}


bool CMBAs::Create(char chBus, CDECUPE *pUPE, CMBA *&pMBA)
{
  //++
  //   This method will create a new MASSBUS (CMBA) object, connect it to the
  // UPE specified, add it to this collection, and then start the background
  // service thread running.
  //--
  if ((pMBA = FindBus(chBus)) != NULL) {
    LOGS(ERROR, "MASSBUS " << chBus << " is already in use");  return false;
  }

  // Create the CMBA object and add it to the collection ...
  pMBA = new CMBA(chBus, *pUPE);  Add(*pMBA);

  // Start the background service thread and we're done ...
  if (!pMBA->BeginThread()) return false;
  if (pUPE->IsOffline()) {
    LOGS(DEBUG, "offline MASSBUS " << pMBA->GetName() << " created");
  } else {
    LOGS(DEBUG, "MASSBUS " << pMBA->GetName() << " created on UPE " << *pUPE);
  }
  return true;
}

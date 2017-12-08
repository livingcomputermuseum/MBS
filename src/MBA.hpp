//++
// MBA.hpp -> CMBA (collection of MASSBUS drives) class
//            CMBAs (collection of CMBA objects) class
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
//   The CMBA class is primarily a collection class which contains all the
// drives attached to a single MASSBUS cable.  In addition, it contains a
// bunch of MASSBUS adapter global methods.
//
//   The CMBAS class is a collection of CMBA objects and describes all the
// MASSBUS adapters connected to this particular MBS instance.
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
using std::string;              // ...
using std::ostream;             // ...
class CDECUPE;                  // we need forward pointers for this class
class CBaseDrive;               //   ... and this one ....
#include "Mutex.hpp"            // we need the delaration for the CMutex class
#include "Thread.hpp"           //   ... and the CThread class ...


// CMBA class definition ...
class CMBA {
  //++
  //   This class is, for the most part, a collection of MASSBUS drives.
  // The CDiskDrive objects (and CTapeDrive, if they're ever invented) are
  // "owned" by this collection - they're created by AddUnit() and destroyed
  // by RemoveUnit() or when the class destructor is called.  We keep a simple
  // array of pointers to the units in the class member m_apDrives[]. Yes, we
  // could use an STL container like std::array for this, but unfortunately
  // array wasn't standard with Visual Studio 2008.
  //
  //   Note that the ordering of our m_apDrives[] array is not arbitrary - the
  // index corresponds directly to the drive's unit number. This has the effect
  // of making this collection "sparse" - not all indices valid at any time. 
  // For example, if units 0, 1 and 6 are connected then only subscripts [0],
  // [1] and [6] are legal.  The UnitExists() method can be used to figure out
  // which units are valid.
  //
  //   Lastly, notice that this class doesn't implement real STL iterators.
  // That's unfortunate, because we could make the iterators do useful things
  // like automatically skip over units that don't exist, but it's just too
  // much coding for now.  Instead we simply use a small integer to traverse
  // the collection -
  //
  //    for (uint8_t i = 0;  i < CMBA::MAXUNIT;  ++i) {
  //      if (!pMBA->UnitExists(i)) continue;
  //      ... work with pMBA->Unit(i) ...
  //    }
  //
  //  Not as cool as an iterator, but it gets the job done.
  //--

  // The maximum number of drives that can be attached to a MASSBUS ...
public:
  static const size_t MAXUNIT = 8;

  // Public MBA properties ...
public:
  // Return the name (e.g. A, B, C, D, etc) of this MBA ...
  char GetName() const {return m_chBus;}
  // Return the UPE attached to this MASSBUS ...
  CDECUPE &GetUPE() const {return m_UPE;}
  // Count units connected or online ...
  uint32_t UnitsConnected() const;
  uint32_t UnitsOnline() const;
  // Figure out what VHDL code this MASSBUS is running ...
  bool IsDisk() const {return m_UPE.IsDisk();}
  bool IsTape() const {return m_UPE.IsTape();}
  bool IsNI()   const {return m_UPE.IsNI();}
  bool IsCompatible (const CBaseDrive &Unit) const;
  bool IsCompatible (uint8_t nIDT) const;

  // Public MBA "global" (i.e. not unit specific) methods ...
public:
  // Initialize a new MBA collection object ...
  CMBA (char chBus, CDECUPE &upe);
  // Destroy an MBA collection and release any drives allocated ...
  virtual ~CMBA ();
  // Add a new disk or tape drive to this collection ...
  CBaseDrive &AddUnit(uint8_t nUnit, CBaseDrive &Unit);
  CBaseDrive &AddUnit(uint8_t nUnit, uint8_t nIDT);
  // Remove a drive from the collection ...
  void RemoveUnit(uint8_t nUnit);
  void RemoveUnit(CBaseDrive *pUnit);
  // Map the units connected ...
  void SetDriveMap() const;
  // Execute a MASSBUS command from the FPGA ...
  void DoCommand(uint32_t lCommand);
  // Start or stop the background thread for this MBA ...
  bool BeginThread() {return m_ChannelThread.Begin();}
  void ExitThread() {m_ChannelThread.WaitExit();}
  // Set or release the UI lock on this MBA ...
  void LockUI() {m_UIlock.Enter();}
  void UnlockUI() {m_UIlock.Leave();}

  // Public MBA methods to access individual units ...
public:
  // Test whether a particular unit exists ...
  bool UnitExists(uint8_t n) const
    {return (n < MAXUNIT) ? (m_apUnits[n] != NULL) : false;}
  // Return a pointer to a particular unit or NULL if it's disconnected.
  CBaseDrive *Unit(uint8_t n)
    {assert(n < MAXUNIT);  return m_apUnits[n];}
  const CBaseDrive *Unit(uint8_t n) const
    {assert(n < MAXUNIT);  return m_apUnits[n];}
  // Return a reference to a particular unit and fail if it's not connected.
  CBaseDrive& operator[] (uint8_t n)
    {assert((n < MAXUNIT) && (m_apUnits[n] != NULL));  return *m_apUnits[n];}
  const CBaseDrive& operator[] (uint8_t n) const
    {assert((n < MAXUNIT) && (m_apUnits[n] != NULL));  return *m_apUnits[n];}

  // Search for a drive with a particular alias ...
  uint8_t FindUnit(const string &strAlias) const;

  // Disallow copy and assignment operations with CMBA objects...
private:
  CMBA(const CMBA &mba) = delete;
  CMBA& operator= (const CMBA &mba) = delete;

  // Private internal MASSBUS adapter methods ...
private:
  // The background task that manages this MBA ...
  static void* THREAD_ATTRIBUTES CommandLoop (void *pParam);

  // Local members ...
protected:
  char         m_chBus;           // number of this MASSBUS
  CDECUPE      &m_UPE;            // UPE object associated with this bus
  CBaseDrive  *m_apUnits[MAXUNIT];// unit data blocks for each MASSBUS unit
  CMutex       m_UIlock;          // CRITICAL_SECTION lock for UI access
  CThread      m_ChannelThread;   // background thread to service this channel
};


class CMBAs
{
  //++
  //   The CMBAS class is a collection of all CMBA objects connected to this
  // MBS instance.  Unlike CMBA, this one uses the STL std::vector template
  // and exposes some standard container behaviors, like iterators ...
  //--

  // Public MBA collection properties ...
public:
  // Count the number of units attached or online ...
  uint32_t UnitsConnected() const;
  uint32_t UnitsOnline() const;
  // Find the MBA that owns a particular UPE or UNIT ...
  CMBA *FindUPE(const CDECUPE *pUPE) const;
  CMBA *FindBus(char chBus) const;
  CBaseDrive *FindUnit(const string &strUnit) const; 
  bool FindUnit(const string &strUnit, CMBA *&pBus, uint8_t &nUnit) const; 

  // Delegate iterators for this collection ...
public:
  typedef std::vector<CMBA *> CMBA_VECTOR;
  typedef CMBA_VECTOR::iterator iterator;
  typedef CMBA_VECTOR::const_iterator const_iterator;
  iterator begin() {return m_vecMBAs.begin();}
  const_iterator begin() const {return m_vecMBAs.begin();}
  iterator end() {return m_vecMBAs.end();}
  const_iterator end() const {return m_vecMBAs.end();}

  // Delegate array style addressing for this collection ...
public:
  // Notice that we use Bus() instead of at() and Count() instead of size() ...
  size_t Count() const {return m_vecMBAs.size();}
  CMBA &Bus(uint8_t n) {return *m_vecMBAs.at(n);}
  const CMBA &Bus(uint8_t n) const {return *m_vecMBAs.at(n);}
  CMBA& operator[] (uint32_t n) {return *m_vecMBAs.at(n);}
  const CMBA& operator[] (uint32_t n) const {return *m_vecMBAs.at(n);}

  // Public interface functions for the MBA collection ...
public:
  // CMBAs constructor and destructor ...
  CMBAs() {};
  virtual ~CMBAs();
  // Add CMBA instances ...
  CMBA &Add(CMBA &mba) {m_vecMBAs.push_back(&mba);  return mba;}
  CMBA &Add(char chBus, CDECUPE &upe) {return Add(*new CMBA(chBus, upe));}
  CMBA &Add(CDECUPE &upe) {return Add((char) ('A'+Count()), upe);}
  // Create a new MBA instance, add it to this collection, and start it..
  bool Create(char chBus, CDECUPE *pUPE, CMBA *&pMBA);

  // Disallow copy and assignment operations with CMBAs objects...
private:
  CMBAs(const CMBAs&) = delete;
  CMBAs& operator= (const CMBAs&) = delete;

  // Private member data ...
private:
  CMBA_VECTOR m_vecMBAs;    // collection of all known MASSBUS adapters
};

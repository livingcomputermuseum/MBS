//++
// BaseDrive.cpp -> CBaseDrive (basic MASSBUS disk or tape emulation methods)
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
//   The CBaseDrive class represents a single physical MASSBUS disk or tape
// drive (i.e. one single unit on the MASSBUS).  This class contains all methods
// that are common to both disk and tape - Attach/Detach, read only/read-write
// mode, etc.
//
//   A lot of shared code is possible because the m_pType and m_pImage members
// in this base class can point to EITHER a CDiskType object OR a CTapeType
// object (or CDiskImageFile vs CTapeImageFile in the case of m_pImage).
// Polymorphism at it's best...
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
// 23-APR-14  RLA   Split off from CDiskDrive (to add tape support)
//--
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "UPELIB.hpp"           // UPE library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "UPE.hpp"              // UPE library FPGA interface methods
#include "LogFile.hpp"          // UPE library message logging facility
#include "ImageFile.hpp"        // UPE library image file methods
#include "MBS.hpp"              // global declarations for this project
#include "MASSBUS.h"            // MASSBUS commands and registers
#include "DriveType.hpp"        // internal drive type class
#include "DECUPE.hpp"           // DEC specific UPE/FPGA class definitions
#include "BaseDrive.hpp"        // declarations for this module
#include "MBA.hpp"              // MASSBUS drive collection class



CBaseDrive::CBaseDrive (CMBA &mba, uint8_t nUnit, CDriveType const *pType, CImageFile *pImage)
  : m_MBA(mba), m_UPE(mba.GetUPE())
{
  //++
  //   The constructor simply creates an empty disk unit and initializes all
  // of the members.  It does NOT initialize any FPGA registers associated
  // with the drive NOR does it attach any image file.  That all happens later.
  //
  //   Note that the destructor for the derived class is responsible for
  // deleting the CImageFile object - we won't do it.  The CDriveType object
  // is actually compiled in and never goes away, so nobody needs to delete
  // that one.  The CMBA and CUPE objects all have a lifetime exceeding this
  // drive, so they should be left alone by the destructor.
  //--
  assert((pType != NULL) && (pImage != NULL));
  m_nUnit = nUnit;  m_nSerial = 0;
  m_fOnline = m_fReadOnly = false;
  m_pType = pType;  m_pImage = pImage;
}


string CBaseDrive::GetCU() const
{
  //++
  // Return this unit's name in the standard "CU" format ...
  //--
  string strCU;
  strCU.push_back(m_MBA.GetName());
  strCU.push_back(m_nUnit + '0');
  return strCU;
}


string CBaseDrive::GetName() const
{
  //++
  //   This method returns the unit name as a string in a pretty format.
  // If this unit has an alias then we return that, and if it doesn't we
  // return the MASSBUS name and unit number in the standard "cu" format.
  //--
  if (m_strAlias.empty())
    return GetCU();
  else
    return GetCU() + " (" + m_strAlias + ")";
}


bool CBaseDrive::Attach (const string &strFileName, bool fReadOnly, int nShareMode)
{
  //++
  //   This method will attach this drive to an image file. pszFileName should
  // be the full path and name of the host image file, and fReadOnly is the
  // initial read only state of the drive.  Note that the read only status may
  // be overridden if the host image file is read only.  If opening the image
  // file fails for any reason, then we return false...
  //
  // NOTE that this DOES NOT bring the drive online!
  //--
  assert(!strFileName.empty());
  if (IsAttached()) Detach();

  // Now try to open the image file and, if that fails, return false.
  if (!m_pImage->Open(strFileName, fReadOnly, nShareMode)) return false;

  //  It actually worked - initialize the rest of the drive's data.  Note that
  // we want to initialize the read only flag from the actual image file state
  // and not from the fReadOnly parameter - that's because the file may have 
  // been opened in read only mode IF the associated disk file is write protected.
  m_fReadOnly = m_pImage->IsReadOnly();  m_fOnline = false;
  LOGS(DEBUG, "unit " << *this << " attached to " << GetFileName()
    << " for " << (IsReadOnly() ? "read only" : "read/write"));
  Clear();
  return true;
}


void CBaseDrive::Detach()
{
  //++
  //   Close the image file associated with this unit...  Note that this DOES
  // NOT take the unit offline first - that's the caller's problem!
  //--
  if (IsAttached()) {
    if (IsOnline()) GoOffline();
    LOGS(DEBUG, "unit " << *this << " detached from " << GetFileName());
    m_pImage->Close();
  }
}


void CBaseDrive::SetReadOnly (bool fReadOnly)
{
  //++
  //   Set or clear this drive's read only status...   Note that this does
  // NOT actually change any drive status bits in the MASSBUS registers -
  // that's up to the caller!
  //--
  if (m_fReadOnly == fReadOnly) return;
  if (fReadOnly) {
    LOGS(DEBUG, "unit " << *this << " is read only");
  } else {
    LOGS(DEBUG, "unit " << *this << " is read/write");
  }
}


void CBaseDrive::SetSerialNumber (uint16_t nSerial)
{
  //++
  //   Change the drive's serial number...   Note that this just records
  // the serial number assigned in a class member - it doesn't actually do
  // anything to the MASSBUS registers.  That's up to the caller!
  //--
  if (nSerial == m_nSerial) return;
  m_nSerial = nSerial;
  LOGS(DEBUG, "unit " << *this << " serial number set to " << nSerial);
}


/*static*/ uint16_t CBaseDrive::ToBCD (uint16_t n)
{
  //++
  //   This routine will convert a binary value, 0..9999, into four BCD
  // digits.  Some drive types, like the TM78/TU78, want their serial
  // numbers in BCD.  This is kind of the brute force approach, but it
  // works!
  //-
  uint8_t d3 = (n / 1000) % 10; 
  uint8_t d2 = (n /  100) % 10; 
  uint8_t d1 = (n /   10) % 10; 
  uint8_t d0 = (n       ) % 10; 
  return (((((d3 << 4) | d2) << 4) | d1) << 4) | d0;
}



void CBaseDrive::DoCommand (uint32_t lCommand)
{
  //++
  // Execute one MASSBUS command.  For the base class, it does nothing!
  //--
  LOGF(WARNING, "unimplemented command %02o", (lCommand & 077));
}

//++
// DriveType.cpp -> CDriveType (generic MASSBUS drive characteristics)
//                  CDiskType  (MASSBUS disk drive characteristics)
//                  CTapeType  (MASSBUS tape drive characteristics)
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
//   The CDiskType class encapsulates all static information about a particular
// type of drive (say, an RP07).  That includes things like geometry, controller
// type and register set, MASSBUS drive type, etc.  It also includes methods
// to do calculations that are drive type specific, such as calculating an
// absolute sector number from the C/H/S address.
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
// 22-APR-14  RLA   Add tape support.
// 15-OCT-14  RLA   RP07s have 632 total cylinders, not 630.
//--
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "UPELIB.hpp"           // UPE library definitions
#include "MBS.hpp"              // global declarations for this project
#include "DriveType.hpp"        // declarations for this module


// Disk Type Definitions ...
                                  // name   MASSBUS  sectors  sectors   total    total           controller
                                  //         type    (16 bit) (18 bit)  heads  cylinders         type
static const CDriveType m_NullType (    "",  000,                                          CDriveType::NOCTRL);
static const CDiskType  m_RP04type ("RP04",  020,       22,      20,      19,       411,   CDriveType::RPCTRL);
static const CDiskType  m_RP06type ("RP06",  022,       22,      20,      19,       815,   CDriveType::RPCTRL);
static const CDiskType  m_RP07type ("RP07",  042,       50,      43,      32,       632,   CDriveType::RMCTRL);
static const CDiskType  m_RM03type ("RM03",  024,       32,      30,       5,       823,   CDriveType::RMCTRL);
static const CDiskType  m_RM05type ("RM05",  027,       32,      30,      19,       823,   CDriveType::RMCTRL);
static const CDiskType  m_RM80type ("RM80",  026,       31,      30,      14,       559,   CDriveType::RMCTRL);
static const CTapeType  m_TU78type ("TU78",  000,                                          CDriveType::TM78CTRL);
static const CTapeType  m_TU77type ("TU77",  000,                                          CDriveType::TM03CTRL);
static const CTapeType  m_TU45type ("TU45",  000,                                          CDriveType::TM03CTRL);
/*static*/ const CDriveType *CDriveType::m_apDriveTypes[CDriveType::NUMIDTS] = {
  &m_NullType,
  &m_RP04type, &m_RP06type, &m_RP07type,
  &m_RM03type, &m_RM05type, &m_RM80type,
  &m_TU78type, &m_TU77type, &m_TU45type
};


/*static*/ CDriveType const *CDriveType::GetDriveType (uint8_t nIDT)
{
  //++
  // Return a pointer to the CDriveType object for the specified type ...
  //--
  assert(nIDT < NUMIDTS);
  return m_apDriveTypes[nIDT];
}


/*static*/ CDiskType const *CDiskType::GetDiskType (uint8_t nIDT)
{
  //++
  // Identical to GetDriveType, except that the IDT MUST be a disk!
  //--
  const CDriveType *pType = CDriveType::GetDriveType(nIDT);
  assert(pType->IsDisk());
  return (const CDiskType *) pType;
}


/*static*/ CTapeType const *CTapeType::GetTapeType (uint8_t nIDT)
{
  //++
  // Identical to GetDriveType, except that the IDT MUST be a tape!
  //--
  const CDriveType *pType = CDriveType::GetDriveType(nIDT);
  assert(pType->IsTape());
  return (const CTapeType *) pType;
}


CDriveType::CDriveType (const char *pszName, uint16_t nMDT, CTRLTYPE nCtrlType)
{
  //++
  //   The generic constructor for all types just initializes the device name
  // (e.g. RP06, TU78, etc), MASSBUS drive type, and controller type (e.g.
  // RM, RP or TM).
  //--
  m_pszName = pszName;  m_nMDT = nMDT;  m_CtrlType = nCtrlType;
}


CDiskType::CDiskType (const char *pszName, uint16_t nMDT, uint8_t nSectors16, uint8_t nSectors18,
                      uint8_t nHeads, uint16_t nCylinders, CTRLTYPE nCtrlType)
         : CDriveType(pszName, nMDT, nCtrlType)
{
  //++
  // Initialize a new CDiskType object ...
  //--
  m_nSectors16 = nSectors16;  m_nSectors18 = nSectors18;
  m_nHeads = nHeads;  m_nCylinders = nCylinders;
}


CTapeType::CTapeType (const char *pszName, uint16_t nMDT, CTRLTYPE nCtrlType)
         : CDriveType(pszName, nMDT, nCtrlType)
{
  //++
  // In the case of a tape drive, there's nothing more to do...
  //--
}


bool CDiskType::IsValidCHS (uint16_t nCylinder, uint8_t nHead, uint8_t nSector, bool f18Bit) const
{
  //++
  // Return true if the C/H/S address is valid ...
  //--
  return    IsValidCylinder(nCylinder)
         && IsValidHead    (nHead)
         && IsValidSector  (nSector, f18Bit);
}


uint32_t CDiskType::CHStoLBA (uint16_t nCylinder, uint8_t nHead, uint8_t nSector, bool f18Bit) const
{
  //++
  // Convert a C/H/S address to an absolute sector number ...
  //--
  if (IsValidCHS(nCylinder, nHead, nSector, f18Bit))
    return ((nCylinder*GetHeads())+nHead)*GetSectors(f18Bit)+nSector; 
  else
    return INVALID_SECTOR;
}

void CDiskType::LBAtoCHS (uint32_t lLBA, uint16_t &nCylinder, uint8_t &nHead, uint8_t &nSector, bool f18Bit) const
{
  //++
  // Convert an absolute sector back to a cylinder, head and sector address...
  //--
  nSector = lLBA % GetSectors(f18Bit);  lLBA /= GetSectors(f18Bit);
  nHead = lLBA % GetHeads();  nCylinder = lLBA / GetHeads();
  if (!IsValidCHS(nCylinder, nHead, nSector)) {
    nHead = nSector = LOBYTE(INVALID_SECTOR);
    nCylinder = LOWORD(INVALID_SECTOR);
  }
}

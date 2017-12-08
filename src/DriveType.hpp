//++
// DriveType.hpp -> CDriveType (generic MASSBUS drive characteristics class)
//                  CDiskType  (MASSBUS disk drive characteristics class)
//                  CTapeType  (MASSBUS tape drive characteristics class)
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
// type of MASSBUS disk drive (say, an RP07), including things like geometry,
// controller type and register set, MASSBUS drive type, etc.  It also includes
// methods to do calculations that are drive type specific, such as calculating
// an absolute sector number from the C/H/S address.
//
//   The CTapeType class is similar, but it encapsulates all static information
// about a MASSBUS tape drive.  This is actually quite a bit simpler than the
// disk version, since tapes don't have geometry and all of the are pretty much
// the same!
//
//   The CDriveType class is the base class that's shared by both CDiskType and
// CTapeType.  It encapsulates all the information common to both...
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
// 22-OCT-14  RLA   Add tape support.
// 10-SEP-15  RLA   Add TM03 support with TU77 and TU45 drives.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
using std::string;              // ...
using std::ostream;             // ...

#ifdef _WIN32
// windows.h defines a macro called GetDriveType which hoses our CDriveType class!
#undef GetDriveType
#endif


// CDriveType base class definition ...
class CDriveType {
  //++
  //   CDriveType is the base class that contains all information common to
  // both disk and tape devices.  This is an abstract class and should never
  // be instantiated directly.
  //
  //   It's worth pointing out that all instances of CDriveType (and all derived
  // classes like CDiskType and CTapeType) are immutable (in other words, the
  // characteristics of an RP06 are fixed at compile time and can't ever be
  // changed dynamically).  That means all methods associated with this class
  // and all derived classes must be const!
  //--

public:
  // Drive Type Codes ...
  enum IDT {
    //   These are our own internal codes for various drives and have nothing
    // to do with the MASSBUS drive type codes.  WARNING - these constants are
    // used as indices into the m_aDriveTypes[] array in DriveType.cpp.  It goes
    // without saying that if you change one you must change the other!
    UNDEFINED= 0,         // undefined drive type
    RP04     = 1,         // RP04 removable pack drive
    RP06     = 2,         // RP06 removable pack drive
    RP07     = 3,         // RP07 fixed pack drive
    RM03     = 4,         // RM03 removable pack drive
    RM05     = 5,         // RM05 removable pack drive
    RM80     = 6,         // RM80 fixed pack drive
    TU78     = 7,         // TU78/TM78 9 track tape drive
    TU77     = 8,         // TU77/TM03 9 track tape drive
    TU45     = 9,         // TU45/TM03 9 track tape drive
    NUMIDTS  = 10         // number of IDTs defined
  };
  // Controller type ...
  enum CTRLTYPE {
    NOCTRL   = 0,         // used by the null device only
    RPCTRL   = 1,         // "RP" (RP04/RP06) style registers
    RMCTRL   = 2,         // "RM" register space
    TM78CTRL = 3,         // TM78 tape formatter
    TM03CTRL = 4,         // TM03 tape formatter
    NICTRL   = 5          // MEIS network interface
  };


  // Public CDriveType methods ...
public:
  // Construct an object with all the information for one drive type ...
  CDriveType (const char *pszName, uint16_t nMDT, CTRLTYPE nCtrlType);
  // The destructor doesn't do anything ...
  virtual ~CDriveType () {};
  // Return a pointer to the IDT for the specified type ...
  static const CDriveType *GetDriveType (uint8_t nIDT);
  // Return or test the controller type ...
  CTRLTYPE CtrlType() const {return m_CtrlType;}
  bool IsRMType() const {return m_CtrlType == RMCTRL;}
  bool IsRPType() const {return m_CtrlType == RPCTRL;}
  bool IsTMType() const {return (m_CtrlType==TM78CTRL) || (m_CtrlType==TM03CTRL);}
  // Return the disk type name (e.g. RP06 or TU78) ...
  const char *GetName() const {return m_pszName;}
  // Return the MASSBUS drive type ...
  uint16_t GetMDT() const {return m_nMDT;}
  // Return TRUE if this type is a disk drive ...
  bool IsDisk() const {return IsRMType() || IsRPType();}
  // Return TRUE if this type is a tape drive ...
  bool IsTape() const {return IsTMType();}
  // Return TRUE if this is a network interface (not yet implemented) ...
  bool IsNI() const {return false;}

  // Local members ...
private:
  const char *m_pszName;          // user friendly type mnemonic (e.g. "RP06")
  uint16_t    m_nMDT;             // MASSBUS drive type for MB_REG_DRIVE_TYPE
  CTRLTYPE    m_CtrlType;         // controller type (RP, RM or TM)

  // This is the table of drive types ...
  static const CDriveType *m_apDriveTypes[];
};


//   This inserter allows you to send a drive type directly to an I/O stream
// for error and debug messages ...
inline ostream& operator << (ostream &os, const CDriveType &type)
  {os << type.GetName();  return os;}
inline ostream& operator << (ostream &os, const CDriveType *ptype)
  {os << ptype->GetName();  return os;}


// CDiskType class definition ...
class CDiskType : public CDriveType {
  //++
  //   This is the class for all disk drive specific characteristics (e.g.
  // geometry, C/H/S to physical translation, etc)...
  //--

  // Public CDiskType methods ...
public:
  // Construct an object with all the information for one drive type ...
  CDiskType (const char *pszName, uint16_t nMDT, uint8_t nSectors16, uint8_t nSectors18,
             uint8_t nHeads, uint16_t nCylinders, CTRLTYPE nCtrlType);
  // The destructor doesn't do anything ...
  virtual ~CDiskType () {};
  // Return a pointer to the IDT for the specified type ...
  static const CDiskType *GetDiskType (uint8_t nIDT);
  // Return geometry information ...
  uint8_t  GetHeads()     const {return m_nHeads;}
  uint16_t GetCylinders() const {return m_nCylinders;}
  //   Note that returning the number of sectors requires us to know whether
  // it's an 18 bit or 16 bit pack!  That makes things a bit messy...
  uint8_t GetSectors (bool f18Bit=false) const
    {return f18Bit ? m_nSectors18 : m_nSectors16;}
  // Test C/H/S addresses for validity ...
  bool IsValidCylinder(uint16_t nCylinder) const {return nCylinder < GetCylinders();}
  bool IsValidHead(uint8_t nHead) const {return nHead < GetHeads();}
  bool IsValidSector(uint8_t nSector, bool f18Bit=false) const {return nSector < GetSectors(f18Bit);}
  // Convert a C/H/S address to an absolute sector number ...
  bool IsValidCHS(uint16_t nCylinder, uint8_t nHead, uint8_t nSector, bool f18Bit=false) const;
  uint32_t CHStoLBA (uint16_t nCylinder, uint8_t nHead, uint8_t nSector, bool f18Bit) const;
  void LBAtoCHS (uint32_t lLBA, uint16_t &nCylinder, uint8_t &nHead, uint8_t &nSector, bool f18Bit) const;
  enum {INVALID_SECTOR = 0xFFFFFFFFUL};

  // Local members ...
private:
  uint8_t     m_nSectors16;       // sectors per track (16 bit mode)
  uint8_t     m_nSectors18;       // sectors per track (18 bit mode)
  uint8_t     m_nHeads;           // surfaces (heads) per cylinder
  uint16_t    m_nCylinders;       // cylinders per drive
};


// CTapeType class definition ...
class CTapeType : public CDriveType {
  //++
  //   This is the class for all tape drive specific characteristics (right
  // now, there aren't many!) ...
  //--

  // Public CTapeType methods ...
public:
  // Construct an object with all the information for one drive type ...
  CTapeType (const char *pszName, uint16_t nMDT, CTRLTYPE nCtrlType);
  // The destructor doesn't do anything ...
  virtual ~CTapeType () {};
  // Return a pointer to the IDT for the specified type ...
  static const CTapeType *GetTapeType (uint8_t nIDT);

  // Local members ...
private:
};

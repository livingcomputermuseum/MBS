//++
// BaseDrive.hpp -> CBaseDrive (basic MASSBUS disk or tape unit emulation class)
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
//    The CBaseDrive class represents a single physical disk drive and is shared
// by both disk and tape drive emulation.  It contains all aspects of MASSBUS
// emulation that are common to both...
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
// 23-APR-14  RLA   Split from CDiskDrive (to add tape support)
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
using std::string;              // ...
using std::ostream;             // ...
class CDriveType;               // we need forward pointers for this class
class CImageFile;               //   ... and this one ....
class CDECUPE;                  //   ... and this ...
class CMBA;                     //   ... one more ...


class CBaseDrive {
  //++
  //   These are the methods and members common to both disk and tape drives.
  // This is an abstract class and should never be instantiated directly.
  //--

  // Constructor and destructor ...
public:
  CBaseDrive (CMBA &mba, uint8_t nUnit, CDriveType const *pType, CImageFile *pImage);
  virtual ~CBaseDrive() {};

  // Public basic drive properties ...
public:
  // Return the MBA, number, type, image file name, serial number, etc ...
  CMBA &GetMBA() const {return m_MBA;}
  CDECUPE &GetUPE() const {return m_UPE;}
  uint8_t GetUnit() const {return m_nUnit;}
  string GetName() const;
  string GetCU() const;
  const CDriveType *GetType() const {return m_pType;}
  string GetFileName() const
    {return IsAttached() ? m_pImage->GetFileName() : string();}
  uint16_t GetSerial() const {return m_nSerial;}
  // Test whether the drive is read only, online, attached ...
  bool IsAttached() const {return m_pImage->IsOpen();}
  bool IsOnline() const {return IsAttached() && m_fOnline;}
  bool IsReadOnly() const {return m_fReadOnly;}
  // Get and set the alias name ...
  string GetAlias() const {return m_strAlias;}
  void SetAlias(const string &strAlias) {m_strAlias = strAlias;}
  // Test whether this drive is a disk or tape ...
  //   Note that, in general, if IsTape() returns true then the code will
  // assume that this object is actually a CTapeDrive and is free to cast it
  // as such.  Likewise, if IsDisk() returns true then this object must be a
  // CDiskDrive object.  It's kind of a poor man's RTTI...
  bool IsDisk() const {return m_pType->IsDisk();}
  bool IsTape() const {return m_pType->IsTape();}
  bool IsNI() const {return false;}

  // Public basic drive methods ...
public:
  // Convert serial number to BCD ...
  static uint16_t ToBCD (uint16_t n);
  // Attach and detach this drive ...
  virtual bool Attach(const string &strFileName, bool fReadOnly=false, int nShareMode=0);
  virtual void Detach();
  // Initialize the drive MASSBUS registers ...
  virtual void Clear() {};
  // Put the drive online or take it offline ...
  virtual void GoOnline()   {m_fOnline = true ;}
  virtual void GoOffline()  {m_fOnline = false;}
  // Set and clear the read only flag ...
  virtual void SetReadOnly(bool fReadOnly);
  // Set the drive serial number
  virtual void SetSerialNumber(uint16_t nSerial);
  // Execute a MASSBUS command
  virtual void DoCommand (uint32_t lCommand);

  // Disallow copy and assignment operations with CBaseDrive objects...
private:
  CBaseDrive(const CBaseDrive &u) = delete;
  CBaseDrive& operator= (const CBaseDrive &unit) = delete;

  // Local device methods...
protected:

  // Local members ...
protected:
  uint8_t     m_nUnit;          // unit number 
  string      m_strAlias;       // alias name
  uint16_t    m_nSerial;        // serial number for this drive
  bool        m_fOnline;        // this drive is spinning and ready for I/O
  bool        m_fReadOnly;      // this drive is read only
  CMBA       &m_MBA;            // MASSBUS that owns this unit
  CDECUPE    &m_UPE;            // UPE/FPGA interface used by this unit
  //   This member, m_pImage, is set up by the constructor and is guaranteed
  // to actually point to either a CDiskImage or CTapeImage object.  
  CImageFile       *m_pImage;   // associated disk image file
  // Likewise, m_pType actually points to either a CDiskType or CTapeType object.
  CDriveType const *m_pType;    // drive type data for this unit
};


//   This inserter allows you to send an disk drive's identification (it
// prints the unit name and/or alias) directly to an I/O stream for error
// and debug messages ...
inline ostream& operator << (ostream &os, const CBaseDrive &drive)
  {os << drive.GetName();  return os;}

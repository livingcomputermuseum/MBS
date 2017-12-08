//++
// DiskDrive.hpp -> CDiskDrive (MASSBUS disk unit emulation class)
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
//   The CDiskErive class represents specifically a MASSBUS disk drive and 
// contains all the methods unique to disks.  It's derived from the CBaseDrive
// basic MASSBUS drive class...
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
// 23-APR-14  RLA   Split out for tape drive support.
// 15-OCT-14  RLA   Don't mask the cylinder, head or sector addresses.
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


class CDiskDrive : public CBaseDrive {
  //++
  // Methods and properties unique to disk drives ...
  //--

  // Constructor and destructor ...
public:
  CDiskDrive (CMBA &mba, uint8_t nUnit, uint8_t nIDT);
  virtual ~CDiskDrive ();

  // Public disk drive properties ...
public:
  // Return a type cast pointer to the CDiskType object for this drive ...
  const CDiskType *GetType() const {return (const CDiskType *) m_pType;}
  // Return a type cast pointer to the CDiskImageFile object for this drive ...
  CDiskImageFile *GetImage() {return (CDiskImageFile *) m_pImage;}
  // Test whether the drive is 18 bit formatted ...
  void Set18Bit (bool f18Bit = true);
  bool Is18Bit() const {return m_f18Bit;}

  // Public disk drive methods ...
public:
  // Attach and detach this drive ...
  virtual bool Attach(const string &strFileName, bool fReadOnly=false, bool f18Bit=false, int nShareMode=0);
  virtual void Detach();
  // Initialize the drive MASSBUS registers ...
  virtual void Clear();
  // Put the drive online or take it offline ...
  virtual void GoOnline()  {SpinUp()  ;}
  virtual void GoOffline() {SpinDown();}
  // Set and clear the read only flag ...
  virtual void SetReadOnly(bool fReadOnly);
  // Set the drive serial number
  virtual void SetSerialNumber(uint16_t nSerial);
  // Execute a MASSBUS command
  virtual void DoCommand (uint32_t lCommand);
  // Spin up and spin down ...
  void SpinUp();
  void SpinDown();
  // Read and Write sectors ...
  void DoRead(uint16_t wCommand);
  void DoWrite(uint16_t wCommand);
  // Read sectors in 16 or 18 bit mode ...
  //   Notice that these routines come in two flavors - a static method that
  // takes the image file and 18 bit flag as parameters, and a member method
  // that uses the current values from this object.  The static methods are
  // needed by the user interface for the DUMP DISK commands ...
  static bool ReadSector16(CDiskImageFile *pImage, uint32_t lLBA, uint32_t alData16[]);
  bool ReadSector16(uint32_t lLBA, uint32_t alData16[])
    {return ReadSector16(GetImage(), lLBA, alData16);}
  static bool ReadSector18(CDiskImageFile *pImage, uint32_t lLBA, uint32_t alData18[]);
  bool ReadSector18(uint32_t lLBA, uint32_t alData18[])
    {return ReadSector18(GetImage(), lLBA, alData18);}
  // Write sectors in 16 or 18 bit mode ...
  static bool WriteSector16(CDiskImageFile *pImage, uint32_t lLBA, const uint32_t alData16[]);
  bool WriteSector16(uint32_t lLBA, const uint32_t alData16[])
    {return WriteSector16(GetImage(), lLBA, alData16);}
  static bool WriteSector18(CDiskImageFile *pImage, uint32_t lLBA, const uint32_t alData18[]);
  bool WriteSector18(uint32_t lLBA, const uint32_t alData18[])
    {return WriteSector18(GetImage(), lLBA, alData18);}

  // Disallow copy and assignment operations with CDiskDrive objects...
private:
  CDiskDrive(const CDiskDrive &u) = delete;
  CDiskDrive& operator= (const CDiskDrive &unit) = delete;

  // Local device methods...
protected:
  // Get the desired cylinder, head and sector from the RPDC and RPDA registers.
  //
  //   Note that, per Rich, we no longer bother to mask the cylinder, sector or
  // track address at all.  This is to allow future expansion for the mythical
  // "RP99" of unlimited size ...
  uint16_t GetDesiredCylinder() const { return        m_UPE.ReadMBR(m_nUnit, RPDC); }
  uint8_t GetDesiredHead()      const { return HIBYTE(m_UPE.ReadMBR(m_nUnit, RPDA)); }
  uint8_t GetDesiredSector()    const { return LOBYTE(m_UPE.ReadMBR(m_nUnit, RPDA)); }
  // Use the above routines and compute the desired LBA ...
  uint32_t GetDesiredLBA() const;
#ifdef _DEBUG
  void DumpSector (uint32_t *plData, uint32_t clData);
#endif

  // Local members ...
protected:
  bool      m_f18Bit;         // the pack on this drive is 18 bit formatted
  uint32_t  m_nSectorSize;    // logical disk sector size in the image file
};

//++
// TapeDrive.hpp -> CTapeDrive (MASSBUS tape unit emulation class)
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
//   The CTapeDrive represents (you guessed it!) a MASSBUS tape drive and
// contains all the methods unique to tapes.  It's derived from the CBaseDrive
// basic MASSBUS drive class...
//
// Bob Armstrong <bob@jfcl.com>   [24-APR-2014]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
// 23-APR-14  RLA   Split from CDiskDrive (to add tape support!)
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
using std::string;              // ...
using std::ostream;             // ...
class CDriveType;               // we need forward pointers for this class
class CImageFile;               //  ...
class CDECUPE;                  //  ...
class CMBA;                     //  ...


class CTapeDrive : public CBaseDrive {
  //++
  // Methods and properties unique to tape drives ...
  //--

  // Constants ...
public:
  enum {
    //   MAXSKIP is the maximum value of the SKIP COUNT field - this is a
    // TM78 feature that's used to align the bit fiddler for odd length
    // records.  We need it here because we have to allocate enough extra
    // padding on our buffer to allow for skipped bytes.
    MAXSKIP = 10    // 10 bytes in high density core dump mode
  };

  // Constructor and destructor ...
public:
  CTapeDrive (CMBA &mba, uint8_t nUnit, uint8_t nIDT);
  virtual ~CTapeDrive ();
private:
  // Disallow copy and assignment operations with CTapeDrive objects...
  CTapeDrive(const CTapeDrive &u) = delete;
  CTapeDrive& operator= (const CTapeDrive &unit) = delete;

  // Public tape drive properties ...
public:
  // Return a type cast pointer to the CTapeType object for this drive ...
  const CTapeType *GetType() const {return (const CTapeType *) m_pType;}
  // Return a type cast pointer to the CTapeImageFile object for this drive ...
  CTapeImageFile *GetImage() {return (CTapeImageFile *) m_pImage;}

  // Public tape drive methods ...
public:
  // Attach and detach this drive ...
  //virtual bool Attach(const string &strFileName, bool fReadOnly=false, int nShareMode=0);
  //virtual void Detach();
  // Initialize the drive MASSBUS registers ...
  virtual void Clear();
  // Put the drive online or take it offline ...
  virtual void GoOnline();
  virtual void GoOffline();
  // Execute a MASSBUS command ...
  virtual void DoCommand (uint32_t lCommand);
  // Do a manual (i.e. operator initiated) rewind ...
  void ManualRewind();

  // Data conversion routines ...
  static uint32_t Fiddle8to18 (uint8_t bFormat, uint8_t abIn[], uint32_t alOut[], uint32_t cbIn, bool fReverse=false);
  static uint32_t Fiddle18to8 (uint8_t bFormat, uint32_t alIn[], uint8_t abOut[], uint32_t clIn);

  // Local device methods...
protected:
  // Set transport status bits ...
  void SetStatus(uint8_t nSlave=0);
  // Clear the GO bit from this command ...
  void ClearMotionGO(uint8_t nSlave=0);
  // Set motion interrupt or data interrupt registers ...
  void SetMotionInt(uint16_t nCode, uint8_t nSlave=0, uint16_t nFailure=TMFC_NONE);
  void SetDataInt(uint16_t nCode, uint8_t nSlave=0, uint16_t nFailure=TMFC_NONE);
  // Update the command repeat count ...
  void SetMotionCount (uint8_t nCount=0, uint8_t nSlave=0);
  // Check that the drive is online and writable ...
  bool CheckOnline (bool fMotion=true);
  bool CheckWritable (bool fMotion=true);
  // Read normal and extended sense data ...
  void DoReadSense(uint8_t nSlave=0);
  void DoReadExtendedSense();
  // Rewind, unload, and skip records and files ...
  void DoRewind();
  void DoUnload();
  void DoSpace (uint8_t nCount, bool fReverse=false, bool fFiles=false);
  // Write tape marks and gaps ...
  void DoWriteGap (uint8_t nCount=1);
  void DoWriteMark (uint8_t nCount=1);
  // Erase the remainder of the the tape ...
  void DoEraseTape();
  // Read and Write records ...
  void DoRead (bool fReverse, uint8_t bFormat, uint32_t lByteCount);
  void DoWrite (uint8_t bFormat, uint32_t lByteCount);
  // Execute motion and transfer commands ...
  void DoMotionCommand (uint8_t nSlave, uint8_t bFunction, uint8_t bCount);
  void DoTransferCommand (uint8_t bFunction);
#ifdef _DEBUG
  // Dump a tape record for debugging ...
  void DumpRecord (uint32_t *plData, uint32_t clData);
#endif

  // Local members ...
protected:
  //   These two members arrays of 8 bit bytes and 18 bit halfwords that are
  // used to buffer tape records.  We could just do these as locals, but they
  // are rather large for the stack.  Or, we could "new" and "delete" them
  // every time a buffer is needed, but that's kind of expensive.  Instead,
  // we allocate a permanent buffer to each formatter.
  //
  //   Note that the bit fiddler code (the "Fiddle??to??" routines) can
  // intentionally overrun the buffer size when the record size is not an
  // exact multiple of the fiddler's periodicity (e.g. 4 bytes for industry
  // compatible mode, or 5 bytes for core dump mode), so the MAXSKIP fudge is
  // added to the size of the byte buffer to allow for that.
  //
  //   The halfword buffer is never intentionally overrun, but size required
  // depends on the bit fiddler mode.  The worst case packing mode is one
  // byte per halfword, so the number of halfwords required is the same as the
  // longest tape record.
  uint8_t  m_abBuffer[CTapeImageFile::MAXRECLEN+MAXSKIP];
  uint32_t m_alBuffer[CTapeImageFile::MAXRECLEN];
};

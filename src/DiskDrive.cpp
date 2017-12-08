//++
// DiskDrive.cpp -> CDiskDrive (MASSBUS disk unit emulation methods)
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
// containes all the methods unique to disks.  This includes sector by sector
// random access I/O, spin up and spin down, etc.
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
//  6-DEC-13  RLA   SpinDown() forgets to clear the MOL bit ...
// 23-APR-14  RLA   Split off drom CBaseDrive (to add tape support)
// 25-APR-14  RLA   Update FPGA geometry when 18 bit flag changes!
// 15-OCT-14  RLA   Don't mask cylinder, head or sector addresses
//--
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "UPELIB.hpp"           // UPE library definitions
#include "UPE.hpp"              // UPE library FPGA interface methods
#include "LogFile.hpp"          // UPE library message logging facility
#include "ImageFile.hpp"        // UPE library image file methods
#include "MBS.hpp"              // global declarations for this project
#include "MASSBUS.h"            // MASSBUS commands and registers
#include "DriveType.hpp"        // internal drive type class
#include "DECUPE.hpp"           // DEC specific UPE/FPGA class definitions
#include "BaseDrive.hpp"        // common methods for all MASSBUS drives
#include "DiskDrive.hpp"        // declarations for this module
#include "MBA.hpp"              // MASSBUS drive collection class



CDiskDrive::CDiskDrive (CMBA &mba, uint8_t nUnit, uint8_t nIDT)
  //   Note that the sector size to give to the new CDiskImageFileobject is
  // a bit tricky, since it depends on the 18 bit flag, and we don't know what
  // that'll be at this point.  We cheat and always give a sector size of 512
  // bytes, but then that gets corrected in the Set18Bit() method ...
  : CBaseDrive(mba, nUnit, CDiskType::GetDiskType(nIDT), new CDiskImageFile(512))
{
  //++
  //   Initialize any disk specific members...  Note that GetDiskType() has
  // already asserted that nIDT corresponds to a disk type device!!
  //--
  m_f18Bit = false;  m_nSectorSize = 512;
}


CDiskDrive::~CDiskDrive()
{
  //++
  // Delete a disk drive and free any resources allocated to it ...
  //--
  if (IsOnline()) SpinDown();
  if (IsAttached()) Detach();
  delete (CDiskImageFile *) m_pImage;
}


bool CDiskDrive::Attach(const string &strFileName, bool fReadOnly, bool f18Bit, int nShareMode)
{
  //++
  // The disk specific Attach() method just handles the 18 bit flag ...
  //--
  if (!CBaseDrive::Attach(strFileName, fReadOnly, nShareMode)) return false;
  Set18Bit(f18Bit);
  return true;
}


void CDiskDrive::Detach()
{
  //++
  // The disk specific detach calls SpinDown() first ...
  //--
  SpinDown();
  CBaseDrive::Detach();
}


void CDiskDrive::Set18Bit (bool f18Bit)
{
  //++
  // Set or clear the 18 bit mode for this drive (disks only!) ...
  //--
  if (f18Bit == m_f18Bit) return;

  //   Calculate the correct sector size for this emulated drive. This depends
  // on both the physical disk sector size AND the way the data is stored in
  // the image file.  For VAX and PDP11 systems, this is a no brainer, since
  // 16 bit words are stored exactly in two disk bytes.  For -10 systems it's
  // a bit trickier - the only file format we currently support, simh, stores
  // one 36 bit PDP10 word in a 64 bit disk word (it wastes a lot of space!).
  const uint32_t nSectorSize = f18Bit
    ? (SECTOR_SIZE/2)*sizeof(uint64_t)  // 128 words * 8 bytes =  1K bytes/sector
    : (SECTOR_SIZE*2)*sizeof(uint8_t);  // 256 words * 2 bytes = 512 bytes/sector
  GetImage()->SetSectorSize(nSectorSize);  m_f18Bit = f18Bit;

  //   Note that changing the 18 bit flag changes the drive's geometry (the
  // number of sectors per track differ) and hence the FPGA needs to be told...
  m_UPE.SetGeometry(m_nUnit,
    GetType()->GetCylinders(), GetType()->GetHeads(), GetType()->GetSectors(m_f18Bit));
}


void CDiskDrive::Clear()
{
  //++
  //   The Clear() method resets all this disk drive's MASSBUS registers to
  // their default state. It does the equivalent of a DRIVE CLEAR function -
  // it's not clear that the FPGA/UPE will actually let us do this, but we
  // give it a try just to ensure everything is in a known state.  
  //
  //   This routine also initializes the write locked and 18 bit format bits,
  // as well as the MASSBUS drive type register.  And lastly it defines the
  // drive's geometry for the FPGA.
  //
  //   Note that this method CLEARS both the MOL and VV bits - that means
  // it'll bring the drive offline.  Normally it's only called once, just
  // before spinning up the drive, so that's not an issue.
  //--
  CBaseDrive::Clear();

  // Set the drive geometry for the FPGA ...
  m_UPE.SetGeometry(m_nUnit,
    GetType()->GetCylinders(), GetType()->GetHeads(), GetType()->GetSectors(m_f18Bit));

  // Initialize the drive status register ...
  m_UPE.WriteMBR(m_nUnit, RPDS, RPDS_DRY | (m_fReadOnly ? RPDS_WLK : 0));

  // Initialize the serial number and drive type registers ...
  m_UPE.WriteMBR(m_nUnit, RPDT, RPDT_MOH | m_pType->GetMDT());
  SetSerialNumber(m_nSerial);

  // Initialize the offset register and set the format bit ...
  m_UPE.WriteMBR(m_nUnit, RPOF, m_f18Bit ? RPOF_FMT22 : 0);
}


void CDiskDrive::SetReadOnly (bool fReadOnly)
{
  //++
  // This disk specific version sets or clears the WLK bit in the RPDS ...
  //--
  CBaseDrive::SetReadOnly(fReadOnly);
  if (IsReadOnly()) {
    m_UPE.SetBitMBR(m_nUnit, RPDS, RPDS_WLK);  m_fReadOnly = true;
  } else {
    m_UPE.ClearBitMBR(m_nUnit, RPDS, RPDS_WLK);  m_fReadOnly = false;
  }
}


void CDiskDrive::SetSerialNumber (uint16_t nSerial)
{
  //++
  // This disk specific version sets the RPSN register ...
  //--
  CBaseDrive::SetSerialNumber(nSerial);
  m_UPE.WriteMBR(m_nUnit, RPSN, GetSerial());
}


void CDiskDrive::SpinUp()
{
  //++
  //   This method will bring a unit online by setting the MOL (medium online)
  // bit in the drive status register.  The FPGA is supposed to notice this 
  // 0 -> 1 transition of the MOL bit and generate the corresponding attention
  // interrupt on the MASSBUS.   We'll keep our fingers crossed...
  //--
  assert(IsAttached());
  if (IsOnline()) return;
  //Clear();
  //   Note that SpinUp() DOES NOT set volume valid (VV) - that can only be
  // set when the host issues a pack acknowledge command ...
  m_UPE.SetBitMBR(m_nUnit, RPDS, RPDS_MOL);
  LOGS(DEBUG, "unit " << *this << " online");
  CBaseDrive::GoOnline();
}


void CDiskDrive::SpinDown()
{
  //++
  //   This method takes this unit offile - it's equivalent to spinning down
  // the pack on a real drive.  If the drive isn't currently online, then 
  // nothing happens.
  //--
  assert(IsAttached());
  if (!IsOnline()) return;
  //Clear();
  m_UPE.ClearBitMBR(m_nUnit, RPDS, RPDS_MOL|RPDS_VV);
  LOGS(DEBUG, "unit " << *this << " offline");
  CBaseDrive::GoOffline();
}


uint32_t CDiskDrive::GetDesiredLBA() const
{
  //++
  // Return the desired C/H/S address as a LBA ...
  //--
  //uint32_t rpda = m_UPE.ReadMBR(m_nUnit, RPDA);
  //uint32_t rpdc = m_UPE.ReadMBR(m_nUnit, RPDC);
  uint16_t c = GetDesiredCylinder();
  uint8_t  h = GetDesiredHead();
  uint8_t  s = GetDesiredSector();
  LOGF(TRACE, "GetDesiredLBA() RPDC=0%06o, RPDA=0%06o, c/h/s = %d/%d/%d",
    m_UPE.ReadMBR(m_nUnit, RPDC), m_UPE.ReadMBR(m_nUnit, RPDA), c, h, s);
  return GetType()->CHStoLBA(c, h, s, m_f18Bit);
}


/* static */ bool CDiskDrive::ReadSector18 (CDiskImageFile *pImage, uint32_t lLBA, uint32_t alData18[])
{
  //++
  //   This method will read one sector from the image file in 18 bit, KL10
  // or KS10, format.  In this case the image data is stored in "simh" style
  // with one 36 bit KL/KS word stored right aligned in an 8 byte, 64 bit,
  // quadword.  It wastes a lot of space this way, but it makes the disk I/O
  // fairly simple.  
  //
  //   One complication is that the MASSBUS and the RP/RM drives don't really
  // deal with 36 bit words - instead they deal with two 18 bit words.  This 
  // routine therefore has to read 128 36 bit words from the image and split
  // them up into 256 18 bit words.
  //--
  uint64_t aqData[SECTOR_SIZE/2];

  // Read the sector - this going to read 1K bytes from the file ...
  if (!pImage->ReadSector(lLBA, &aqData)) return false;

  // Now unpack the 36 bit data into two 18 bit words ...
  for (uint32_t i = 0; i < SECTOR_SIZE/2; ++i) {
    alData18[2*i] = LH36(aqData[i]);
    alData18[2*i+1] = RH36(aqData[i]);
  }
  return true;
}


/* static */ bool CDiskDrive::ReadSector16 (CDiskImageFile *pImage, uint32_t lLBA, uint32_t alData16[])
{
  //++
  //   And this method will read a single sector from the image file in 16
  // bit format.  There's a catch, though - notice that the data is actually
  // RETURNED AS AN ARRAY OF 32 BIT LONGWORDS!  That effectively means that
  // every 16 bit data word gets padded with an extra two bytes of zeros!
  // This is obviously not the most convenient way to deal with 16 bit data,
  // but there are two advantages - first, it's consistent with the way 18
  // bit images are handled.  Second (and more importantly) it's the way the
  // FPGA wants to see the data, so we're stuck with repacking it eventually.
  // Might as well deal with it now...
  //--
  uint16_t awData[SECTOR_SIZE];

  // Read the sector - this will read 512 bytes from the file!
  if (!pImage->ReadSector(lLBA, &awData)) return false;

  // And unpack the 16 bit data into 32 bit words ...
  for (uint32_t i = 0; i < SECTOR_SIZE; ++i)
    alData16[i] = MKLONG(0, awData[i]);
  return true;
}


/* static */ bool CDiskDrive::WriteSector18 (CDiskImageFile *pImage, uint32_t lLBA, const uint32_t alData18[])
{
  //++
  //   This routine will write one sector to the image file in 18 bit, KL10
  // or KS10, format.  It has all the same issues to contend with that you
  // saw in mbReadSector18() - the simh file format using 36 bit/8 byte words,
  // packing from 18 bit MASSBUS words into 36 bit simh words, and of course
  // the whole endian thing, which hasn't been dealt with.
  //--
  uint64_t aqData[SECTOR_SIZE/2];
  for (uint32_t i = 0; i < SECTOR_SIZE/2; ++i) {
    uint32_t h = alData18[2*i];
    uint32_t l = alData18[2*i+1];
    aqData[i]  = MK36(h, l);
  }
  return pImage->WriteSector(lLBA, &aqData);
}


/* static */ bool CDiskDrive::WriteSector16 (CDiskImageFile *pImage, uint32_t lLBA, const uint32_t alData16[])
{
  //++
  //   This method writes a single sector to the image file in 16 bit format.
  // Just like ReadSector16(), notice that the data supplied by the caller is
  // an ARRAY OF 32 BIT LONGWORDS!  The low order 16 bits of each word is 
  // written to the file and the upper 16 bits of each word are discarded.
  // Yes, that's an awkward way to handle 16 bit data, but it's just the way
  // the FPGA gives it to us.  We're forced to repack it eventually, and now
  // is the time.  Besides, this neatly parallels the way 18 bit data is handled.
  //--
  uint16_t awData[SECTOR_SIZE];

  // Repack the 32 bit words into 16 bit words ...
  for (uint32_t i = 0; i < SECTOR_SIZE; ++i)
    awData[i] = LOWORD(alData16[i]);

  // And write the sector ...
  return pImage->WriteSector(lLBA, &awData);
}


void CDiskDrive::DoRead(uint16_t wCommand)
{
  //++
  //   This method handles the MASSBUS READ, READ WITH HEADER, WRITE CHECK
  // and WRITE CHECK WITH HEADER commands.  Believe it or not, these are all
  // the same as far as we're concerned - the FPGA and/or the RH20 controller
  // takes care of all the differences.  We simply read the sector and pump
  // the data into the FPGA.
  //
  //   Note that a read can fail in a number of ways.  Firstly, the FPGA is
  // supposed to range check the RPDC and RPDA registers before we ever get
  // here, but it's possible that the FPGA failed to live up to its end of the
  // deal.  It's also possible for the FPGA data transfer to fail, or for
  // the disk I/O to fail.  It's not clear what we should do in any of these
  // cases, but for now we print an error message and mark the disk offline.
  //--
  assert(IsOnline());
  uint32_t alSector[SECTOR_SIZE];

  // Figure out which sector we want to read ...
  uint32_t lLBA = GetDesiredLBA();
  if (lLBA == CDiskType::INVALID_SECTOR) {
    LOGS(WARNING, "unit " << *this << " invalid sector address, C/H/S = "
      << GetDesiredCylinder() << "/" << GetDesiredHead() << "/" << GetDesiredSector());
    goto offline;
  }
  LOGS(TRACE, "unit " << *this << " read sector, C/H/S = "
      << GetDesiredCylinder() << "/" << GetDesiredHead() << "/" << GetDesiredSector()
      <<", LBA = " << lLBA);

  // Read the image file ...
  if (Is18Bit()) {
    if (!ReadSector18(lLBA, alSector)) goto offline;
  } else {
    if (!ReadSector16(lLBA, alSector)) goto offline;
  }

  // Then stuff the data into the FPGA and we're done ...
  m_UPE.WriteData(alSector, SECTOR_SIZE);
  return;

offline:
  LOGS(ERROR, "unit " << *this << " offline due to errors");
  SpinDown();
}


void CDiskDrive::DoWrite (uint16_t wCommand)
{
  //++
  //   And this method handles the MASSBUS WRITE and WRITE WITH HEADER 
  // commands.  It's pretty much the obvious complement of DoRead().
  //--
  assert(IsOnline());
  uint32_t alSector[SECTOR_SIZE];

  // Figure out which sector we want to write ...
  uint32_t lLBA = GetDesiredLBA();
  if (lLBA == CDiskType::INVALID_SECTOR) {
    LOGS(WARNING, "unit " << *this << " invalid sector address, C/H/S = "
      << GetDesiredCylinder() << "/" << GetDesiredHead() << "/" << GetDesiredSector());
    goto offline;
  }
  LOGS(TRACE, "unit " << *this << " write sector, C/H/S = "
      << GetDesiredCylinder() << "/" << GetDesiredHead() << "/" << GetDesiredSector()
      <<", LBA = " << lLBA);

  // Now get data from the FPGA and ...
  if (!m_UPE.ReadData(alSector, SECTOR_SIZE)) goto offline;
  if (IsReadOnly()) {
    LOGS(WARNING, "unit " << *this << " write to read only unit");
    goto offline;
  }

  // And write it to the image file ...
  if (Is18Bit()) {
    if (!WriteSector18(lLBA, alSector)) goto offline;
  } else {
    if (!WriteSector16(lLBA, alSector)) goto offline;
  }
  return;

offline:
  LOGS(ERROR, "unit " << *this << " offline due to errors");
  SpinDown();
}


void CDiskDrive::DoCommand (uint32_t lCommand)
{
  //++
  //   This method will execute one MASSBUS command, where lCommand is the
  // 32 bit command FIFO longword from the FPGA.  In the case of disk drives,
  // all we need out of lCommand is the lower 16 bits, which are the contents
  // of the RPCR register when it was written.  Tape drives are harder :-)
  //
  //   The only MASSBUS commands we need to worry about are the ones that
  // actually transfer data - READ and WRITE, in their several forms.  The FPGA
  // takes care of everything else.
  //--
  uint16_t wCommand = CDECUPE::ExtractCommand(lCommand);
  switch (wCommand & RPCMD_MASK) {
    case RPCMD_READ:
    case RPCMD_RHEADER:
    case RPCMD_WCHECK:
    case RPCMD_WHCHECK:
      DoRead(wCommand);
      break;
    case RPCMD_WRITE:
    case RPCMD_WHEADER:
      DoWrite(wCommand);
      break;
    default:
      LOGF(WARNING, "unimplemented command %02o", (wCommand & RPCMD_MASK));
      break;
  }
}

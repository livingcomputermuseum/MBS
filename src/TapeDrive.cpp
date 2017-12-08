//++
// TapeDrive.cpp -> CTapeDrive (MASSBUS tape unit emulation methods)
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
//   The CTapeDrive class represents (you guessed it!) a MASSBUS tape drive
// and contains all the methods unique to tapes.  This includes things like
// veriable length record I/O, rewind and unload, space forward and backward,
// etc.
//
//   One important note about tape drives - tapes differ from disks in that 
// the MASSBUS unit select actually selects the formatter, not the drive. The
// formatter is somewhat analogous to the drive control logic box on the RP04
// or RP06, HOWEVER each formatter potentially supports more than one slave
// tape drive.  In addition to the MASSBUS unit select, each formatter has its
// own internal slave transport select. This gives a second level of indirection
// to tape drives that disks don't have and would potentially make things more
// complicated.
//
//   The good news is that we currently just don't implement that option.  We
// enforce a one to one relationship between formatters and slaves, so that
// each slave has, in effect, its own formatter.  That's a valid and supported
// by DEC configuration, albeit somewhat extravagant.  It makes our life easier,
// though, and now tape drives look more like disk drives...
//
//   If we ever did want to implement multiple transports per formatter, then
// probably the easiest way to handle it would be to have four (or eight) image
// files associated with this object rather than just one.  Outside of the tape
// position and contents, which are encapsulated in the CTapeImageFile, there
// isn't much transport specific context otherwise.  Pretty much everything is
// controller specific not transport specific, so just keeping multiple image
// files might be enough.
//
// RESTRICTIONS ON THE CURRENT IMPLEMENTATION
//
// * Timing is not modeled at all, and in general commands complete as soon as
// they are issued.  Functions that should take several seconds or even minutes
// to complete (e.g. rewind) will actually complete in a few hundred micro-
// seconds.  This is true for the disk emulation as well, however in real life
// tapes are much slower than disks and the errors are more egregious in this
// case.  
//
// * Up to eight formatters are allowed per MASSBUS, however each formatter
// allows exactly one drive/transport.  The MBS "unit number" is actually the
// MASSBUS unit number, which selects a formatter.  The TU78 transport drive
// select address (what people would usually think of as the "unit number" in
// this case) is always zero.
//
// * The UPE FPGA bitstream required for tape emulation differs from the one
// used for disk emulation.  That means disks and tapes cannot be combined on
// the same MASSBUS.
//
// * Only a "TM03 compatible" subset of TM78 functions are implemented. These
// seem to be the only ones actually used by TOPS10 or TOPS20.  The specific
// TM78 functions implemented are -
//
//      -> SENSE (011) and EXTENDED SENSE (073)
//      -> READ FORWARD (071) and READ REVERSE (077)
//      -> WRITE PE (061) and WRITE GCR (063)
//      -> WRITE MARK PE (015) and WRITE MARK GCR (17)
//      -> SPACE FORWARD RECORD (021) and SPACE REVERSE RECORD (023)
//      -> REWIND (007) and UNLOAD (005)
//      -> ERASE GAP (035)
//      -> DATA SECURITY ERASE (013)
//
// * The only byte assembly (aka "bit fiddler") modes implemented are "10 
// COMPATIBLE" and "10 CORE DUMP".  In particular, none of the "HIGH DENSITY"
// modes nor any PDP-11 or PDP-15 modes are implemented.
//
// * The TM78 "SKIP COUNT" field, which is used to adjust the bit fiddler for
// odd length records, is not implemented.
//
// * Media errors are not modeled.  All tapes are "error free" to the host.
//
// * Tape density is not modeled.  All "WRITE PE" and "WRITE GCR" functions
// do exactly the same thing.  The status register always reports the tape
// density as PE.
//
// * Tape length is not modeled.  Tape images are treated as arbitrary length
// and arbitrary capacity.  EOT is never reported while writing.
//
// * Attempts to write in the middle of an already populated tape image are
// not modeled accurately. In our case, any write operation simply truncates
// the tape image after that.
//
// * Blank tape is not modeled.  The "ERASE GAP" function simply truncates
// the tape image file to that point.
//
// * The "DATA SECURITY ERASE" function simply truncates the image file.
//
// * The drive error log is not implemented.
//
// * The EXTENDED SENSE command returns a block of all zeros (neither errors
// nor any internal drive data paths are modeled).
//
// * The microprocessor control functions (registers 020 and 021) of the TM78 are
// not implemented and are ignored.  Needless to say, downloading firmware is
// not implemented.
//
// * All maintenance registers (3, 11, 12) are not implemented and read as zeros.
//
// Bob Armstrong <bob@jfcl.com>   [24-APR-2014]
//
// REVISION HISTORY:
// 23-APR-14  RLA   New file.
// 18-JUN-15  RLA   Add ManualRewind() for UI
//--
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // memset(), strlen(), etc ...
#include "UPELIB.hpp"           // UPE library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "UPE.hpp"              // UPE library FPGA interface methods
#include "LogFile.hpp"          // UPE library message logging facility
#include "ImageFile.hpp"        // UPE library image file methods
#include "MBS.hpp"              // global declarations for this project
#include "MASSBUS.h"            // MASSBUS commands and registers
#include "DriveType.hpp"        // internal drive type class
#include "DECUPE.hpp"           // DEC specific UPE/FPGA class definitions
#include "BaseDrive.hpp"        // basic MASSBUS drive emulation
#include "TapeDrive.hpp"        // declarations for this module
#include "MBA.hpp"              // MASSBUS drive collection class


/*static*/ uint32_t CTapeDrive::Fiddle8to18(uint8_t bFormat, uint8_t abIn[], uint32_t alOut[], uint32_t cbIn, bool fReverse)
{
  //++
  //   This routine will convert a block of 8 bit data (a tape record) to PDP-10
  // words using either the DEC "industry compatible" algorithm or the DEC-10
  // "core dump" algorithm.  These two modes are essentially identical except 
  // the first packs four 8 bit bytes into one 36 bit word; the low order 4 bits
  // of the result are zero.  The latter mode packs FIVE 8 bit bytes into one
  // 36 bit word, and the low order 4 bits of the last byte are ignored.  One
  // preserves all the bits in the tape record, and the other preserves all the
  // bits in the -10 word.  Simple :-)
  //
  //   Remember that tape records may be read in either the forward or the reverse
  // direction and that has to be taken into account.  The real TM03/TM78 bit
  // fiddler is designed so that reading a record in reverse will produce the same
  // sequence of 18 bit MASSBUS data words, but in reverse order.  The RH20 had
  // a "read reverse" operation, and combining that with the tape's read reverse
  // function would actually produce the exact same sequence of 36 bit words in
  // the -10's memory.
  //
  //   Unfortunately this doesn't mean that the bytes are simply processed in
  // reverse order.  Instead they have to be taken in groups of 4 or 5, converted
  // to 36 bits, and then the order of the two halfwords is reversed.  On the TM03
  // this would really only work if the record was an exact multiple of 4 or 5 bytes
  // long, but the TM78 has a "skip" feature that allows the host to shift the
  // alignment of the first word. It's cool, but we don't implement that feature.
  // The TM03 didn't have it and neither TOPS10 nor TOPS20 used it.
  //
  //   Also, remember that the tape image file I/O routines always read records
  // in the forward direction, so even when fReverse is true the bytes in abIn 
  // are still "forward".  This is not how a real tape drive would work, but
  // it's the way we do.  That means that in reverse mode we have to go thru the
  // abIn array backwards.
  //
  //   The return value of this function is the number of 18 bit words written
  // to alOut.  Note that we do not check the size of the output buffer - we
  // assume the caller has allocated enough space!
  //
  //   And lastly, note that this routine can sometimes touch bytes that are
  // beyond the official end (i.e. at subscripts greater than cbIn) in the abIn
  // array.  This happens when cbIn is not a multiple of 4 or 5.  This is a bit
  // uncool, but it works because the caller always allocates a multiple of 4
  // or 5 bytes for the record size!
  //
  //   REMEMBER! alOut and the return value are in HALFWORDS, not FULLWORDS!
  //--
  uint32_t clOut = 0;  uint64_t w36;

  if (bFormat == TMAM_10_CORE_DUMP) {
    uint32_t n = fReverse ? (((cbIn+4)  / 5) * 5) : 0;
    cbIn = ((cbIn * 5) + 4) / 5;
    //LOGF(TRACE, "  >> Fiddle8to18 CORE DUMP - fReverse=%d, cbIn=%d, n=%d", fReverse, cbIn, n);
    for (; cbIn > 0; cbIn -= 5) {
      uint64_t b0 = abIn[n+0], b1 = abIn[n+1], b2 = abIn[n+2];
      uint64_t b3 = abIn[n+3], b4 = abIn[n+4] & 017;
      w36 = (b0<<28) | (b1<<20) | (b2<<12) | (b3<<4) | b4;
      //  LOGF(TRACE, "  >> Fiddle8to18 abIn = 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X, w36=0x%016I64x %06o %06o",
      //    abIn[n + 0], abIn[n + 1], abIn[n + 2], abIn[n + 3], abIn[n + 4], w36, LH36(w36), RH36(w36));
      if (fReverse) {
        alOut[clOut++] = RH36(w36);  alOut[clOut++] = LH36(w36);  n -= 5;
      } else {
        alOut[clOut++] = LH36(w36);  alOut[clOut++] = RH36(w36);  n += 5;
      }
    }

  } else if (bFormat == TMAM_10_COMPATIBLE) {
    uint32_t n = fReverse ? ((cbIn+3) & ~3) : 0;
    cbIn = (cbIn+3) & ~3;
    //LOGF(TRACE, "  >> Fiddle8to18 INDUSTRY COMPATIBLE - fReverse=%d, cbIn=%d, n=%d", fReverse, cbIn, n);
    for (; cbIn > 0; cbIn -= 4) {
      uint64_t b0 = abIn[n+0], b1 = abIn[n+1], b2 = abIn[n+2], b3 = abIn[n+3];
      w36 = (b0<<28) | (b1<<20) | (b2<<12) | (b3<<4);
      if (fReverse) {
        alOut[clOut++] = RH36(w36);  alOut[clOut++] = LH36(w36);  n -= 4;
      } else {
        alOut[clOut++] = LH36(w36);  alOut[clOut++] = RH36(w36);  n += 4;
      }
    }

  } else
    LOGF(ERROR, "UNSUPPORTED BIT FIDDLER FORMAT %d", bFormat);

  return clOut;
}


/*static*/ uint32_t CTapeDrive::Fiddle18to8(uint8_t bFormat, uint32_t alIn[], uint8_t abOut[], uint32_t clIn)
{
  //++
  //   This routine is the reverse bit fiddler - it converts an array of 18 bit
  // PDP-10 words into an array of 8 bit tape frames using either the "industry
  // compatible" or the "core dump" algorithm.  This is quite a bit easier, 
  // because we don't have to worry about working in reverse this time.  Why
  // not?  Because this conversion is only used for writing, and there's no
  // "write reverse" function.
  //
  //   It's assumed that the number of 18 bit data words in alIn is even - the
  // MASSBUS simply can't transfer an odd number of 18 bit words on the -10.
  // The return value of this routine is the number of bytes written to abOut.
  // Note that we don't check that the abOut array is big enough - it's the
  // caller's job to ensure that it is.
  //
  //   REMEMBER!  alIn and clIn are in HALFWORDS, not FULLWORDS!
  //--
  assert((clIn & 1) == 0);
  uint32_t cbOut = 0;  uint32_t n = 0;  uint64_t w36;

  if (bFormat == TMAM_10_CORE_DUMP) {
    for (; clIn > 0; clIn -= 2) {
      w36 = MK36(alIn[n+0], alIn[n+1]);  n += 2;
      abOut[cbOut+0] = (w36>>28) & 0xFF;
      abOut[cbOut+1] = (w36>>20) & 0xFF;
      abOut[cbOut+2] = (w36>>12) & 0xFF;
      abOut[cbOut+3] = (w36>> 4) & 0xFF;
      abOut[cbOut+4] = (w36) & 0x0F;
      cbOut += 5;
    }
  } else if (bFormat == TMAM_10_COMPATIBLE) {
    for (; clIn > 0; clIn -= 2) {
      w36 = MK36(alIn[n+0], alIn[n+1]);  n += 2;
      abOut[cbOut+0] = (w36>>28) & 0xFF;
      abOut[cbOut+1] = (w36>>20) & 0xFF;
      abOut[cbOut+2] = (w36>>12) & 0xFF;
      abOut[cbOut+3] = (w36>> 4) & 0xFF;
      cbOut += 4;
    }
  } else
    LOGF(ERROR, "UNSUPPORTED BIT FIDDLER FORMAT %d", bFormat);

  return cbOut;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


CTapeDrive::CTapeDrive(CMBA &mba, uint8_t nUnit, uint8_t nIDT)
  : CBaseDrive(mba, nUnit, CTapeType::GetTapeType(nIDT), new CTapeImageFile())
{
  //++
  //   Initialize any tape specific members...  Note that GetTapeType() has
  // already asserted that nIDT corresponds to a tape type device!!
  //
  //   REMEMBER - in this instance, nUnit is the MASSBUS unit number of the TM78
  // formatter, NOT the unit number of the slave transport!
  //--
}


CTapeDrive::~CTapeDrive()
{
  //++
  // Delete a tape drive and free any resources allocated to it ...
  //--
  if (IsOnline()) DoRewind();
  if (IsAttached()) Detach();
  delete (CTapeImageFile *) m_pImage;
}


void CTapeDrive::ClearMotionGO(uint8_t nSlave)
{
  //++
  //   This routine will clear the MASSBUS GO bit from the current command
  // register.  It's the last step in processing any command, and tells the
  // host that it's free to write a new command.  In the case of tape drives
  // this is slightly messy because there's more than one command register,
  // depending on whether the command being completed is a data transfer or
  // motion command.  There's no way to guess - the caller has to tell us.
  //
  //   WARNING - major hack ahead!  Bruce's FPGA clears the GO bit in the
  // data command register after the data transfer finishes, BUT the FPGA
  // DOES NOT clear the GO bit in the motion command register after a motion
  // command finishes.  That means this routine should be called only for
  // motion commands and NOT for data transfer commands!  
  //--
  assert(nSlave <= 3);
  m_UPE.ClearBitMBR(m_nUnit, TMMCR0+nSlave, 1);
}


void CTapeDrive::SetMotionInt(uint16_t nCode, uint8_t nSlave, uint16_t nFailure)
{
  //++
  //   This routine will set the Tm78 motion interrupt (TMMIR) register.
  // Writing a non-zero value to this register should cause the FPGA to set
  // the ATTENTION bit for this MASSBUS unit and that'll interrupt the host
  // via the RH20.  The only required argument is the interrupt code (one of
  // the TMIC_xyz constants).  The other two arguments are the slave number
  // (defaults to zero) and the failure code.  The latter provides extended
  // interrupt indentification to the host and is almost always zero.
  //
  //   Note that although we really only support slave #0, this particular
  // method gets called for other slave numbers.  This happens all the time
  // when the host does a READ STATUS command on another slave to figure out
  // if that slave exists in the first place!
  //--
  uint16_t nMIR = MK_TMMIR(nCode, nSlave, nFailure);
  LOGF(TRACE, "SetMotionInt - nSlave=%d, nCode=%03o, nFailure=%03o (TMMIR=%06o)", nSlave, nCode, nFailure, nMIR);
  m_UPE.WriteMBR(m_nUnit, TMMIR, nMIR);
}


void CTapeDrive::SetDataInt(uint16_t nCode, uint8_t nSlave, uint16_t nFailure)
{
  //++
  //   This routine sets the data interrupt register (TMDIR, register 1), but
  // unlike SetMotionInt(), this DOES NOT generate a KL10 interrupt.  Data
  // transfers on the TM78 are a bit odd - the actual interrupt comes from the
  // RH20 data channel, and the formatter NEVER generates any interrupt.
  //
  //   That pretty much means that anything which calls this routine needs to
  // also transfer data or generate a null transfer via CDECUPE::EmptyTransfer().
  //--
  uint16_t nDIR = MK_TMDIR(nCode, nFailure)|(nSlave==0 ? TMDIR_DPR : 0);
  LOGF(TRACE, "SetDataInt - Slave=%d, nCode=%03o, nFailure=%03o, (TMDIR=%06o)", nSlave, nCode, nFailure, nDIR);
  m_UPE.WriteMBR(m_nUnit, TMDIR, nDIR);
}


void CTapeDrive::SetMotionCount(uint8_t nCount, uint8_t nSlave)
{
  //++
  //   This routine updates the command count field (the left byte) in the
  // motion command register.  The TM78 uses this to tell the host the number
  // of operations NOT completed successfully.  For example, if the host wants
  // to skip four records and we only skip one, this field will be three when
  // the command completes.
  //--
  uint16_t wMCR = MKWORD(nCount, LOBYTE(m_UPE.ReadMBR(m_nUnit, TMMCR0+nSlave)));
  LOGF(TRACE, "SetMotionCount - Slave=%d, nCount=%d, (TMMCR%d=%06o)", nSlave, nCount, nSlave, wMCR);
  m_UPE.WriteMBR(m_nUnit, TMMCR0+nSlave, wMCR);
}


void CTapeDrive::SetStatus(uint8_t nSlave)
{
  //++
  //   This routine will set the unit status (TMUS), drive type (TMDT) and 
  // serial number (TMSN) registers for the specified slave transport.
  // Remember that right now we only support one slave drive, #0, so that's
  // the only one that produces meaningful data.  Any other slave just clears
  // the unit status and serial number registers, which should let the host
  // know that this slave doesn't exist.
  //
  //   This operation is used by the READ SENSE command and it's also used
  // by certain drive generated interrupts (e.g. drive online).
  //--
  assert(nSlave <= 3);

  //   I'm not clear what's supposed to happen with the TMDT register when a
  // non-existent slave is selected.  It could be that it's zero, but that
  // seems to make TOPS10 unhappy.  OTOH, since the TM78 only ever supported
  // one type of slave, the TU78, it's also possible that this register is
  // hardwired to that value regardless of the slave selected.  That seems to
  // work better, so we're going with that for now.
  m_UPE.WriteMBR(m_nUnit, TMDT, TMDT_TM78 | TMDT_TU78);

  if (nSlave == 0) {
    // For slave 0, put real values in the TMUS and TMSN registers ...
    uint16_t usr = TMUS_AVAIL|TMUS_PRES|TMUS_PE;
    if (IsOnline()) {
      usr |= TMUS_ONL | TMUS_RDY;
      if (GetImage()->IsBOT())      usr |= TMUS_BOT;
      if (GetImage()->IsEOT())      usr |= TMUS_EOT;
      if (GetImage()->IsReadOnly()) usr |= TMUS_FPT;
    }
    m_UPE.WriteMBR(m_nUnit, TMUS, usr);
    m_UPE.WriteMBR(m_nUnit, TMSN, CBaseDrive::ToBCD(m_nSerial));
  } else {
    // For all other slaves, just clear TMUS and TMSN ...
    m_UPE.WriteMBR(m_nUnit, TMUS, 0);
    m_UPE.WriteMBR(m_nUnit, TMSN, 0);
  }
}


void CTapeDrive::Clear()
{
  //++
  //   This clears and initializes the TM78 (not just the transport, but the
  // entire formatter) to a known state. It's the equivalent of a MASSBUS INIT
  // or of the host setting the TM_CLR bit in the hardware control (TMHCR)
  // register.
  //--
  CBaseDrive::Clear();

  //   This should clear the MASSBUS ATTN bit, but that's the FPGA's job.
  // Let's hope Bruce knows that!

  //   Clear the two interrupt registers, the command registers (all of them!),
  // and set the available bit.
  m_UPE.WriteMBR(m_nUnit, TMDCR, TMCMD_DVA);
  m_UPE.WriteMBR(m_nUnit, TMDIR, TMDIR_DPR);
  m_UPE.WriteMBR(m_nUnit, TMMCR0+0, 0);
  m_UPE.WriteMBR(m_nUnit, TMMCR0+1, 0);
  m_UPE.WriteMBR(m_nUnit, TMMCR0+2, 0);
  m_UPE.WriteMBR(m_nUnit, TMMCR0+3, 0);
  m_UPE.WriteMBR(m_nUnit, TMMIR, 0);

  //   Initialize the drive type register (we have to do that so that the host
  // knows there's a TM78 here!), but clear the unit status and serial number.
  m_UPE.WriteMBR(m_nUnit, TMDT, TMDT_TM78|TMDT_TU78);
  m_UPE.WriteMBR(m_nUnit, TMUS, 0);
  m_UPE.WriteMBR(m_nUnit, TMSN, 0);
}


void CTapeDrive::GoOnline()
{
  //++
  //   This routine puts the transport "on line".  It updates the unit status
  // register and then generates an "ON LINE" motion interrupt.  This is the
  // tape drive equivalent of the disk SpinUp() method.  Note that it is not
  // done by any host action - the only way to put a tape online is with an
  // explicit UI command.
  //
  //   Note that GoOnline() and GoOffline() do not explicitly rewind the tape.
  // It's possible to take a drive offline and then put it back online without
  // changing the logical tape position.  That's just like a real TU78 drive.
  //--
  assert(IsAttached());
  if (IsOnline()) return;
  CBaseDrive::GoOnline();
  //   Note that this routine does NOT do a SetStatus nor does it change the
  // TMUS register directly - that could screw up an command that's currently
  // in progress.  Instead, it's up to the host to notice this interrupt and
  // then do an explicit READ STATUS command.
  SetMotionInt(TMIC_ONLINE);
  LOGS(DEBUG, "unit " << *this << " online");
}


void CTapeDrive::GoOffline()
{
  //++
  //   And this routine takes the specified tape drive offline.  This really
  // does nothing of consequence except to clear the "online" bit which makes
  // this transport inaccessible for future operations.  It doesn't detach
  // the image file, and it's possible for the operator to put this drive back
  // online with the appropriate UI command.
  //
  //   Note that, like GoOnline(), this does not rewind the tape!  The UNLOAD
  // command does, but that's not this function.
  //--
  assert(IsAttached());
  if (!IsOnline()) return;
  CBaseDrive::GoOffline();
  //   As far as I can determine, the TM78 doesn't generate any "OFFLINE"
  // interrupt in this situation.  The drive just silently disappears, and
  // the host doesn't find out unless and until it actually tries to access
  // the drive.  That'll generate an OFFLINE error interrupt.
  LOGS(DEBUG, "unit " << *this << " offline");
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


bool CTapeDrive::CheckOnline(bool fMotion)
{
  //++
  //   This routine will check that the tape drive is attached and online.  If
  // it is, then TRUE is returned and nothing else happens, but if the drive
  // isn't online then an "OFF LINE" interrupt will be generated and FALSE
  // returned.  This function is called by most tape motion or data transfer
  // commands.
  //
  //   The fMotion parameter tells us which OFF LINE interrupt to generate if
  // it should be necessary - fMotion=TRUE says to generate a motion interrupt,
  // and fMotion=FALSE generates a data transfer interrupt.
  //--
  if (IsOnline()) return true;
  if (fMotion) {
    ClearMotionGO();  SetMotionInt(TMIC_OFFLINE);
  } else {
    SetDataInt(TMIC_OFFLINE);  m_UPE.EmptyTransfer(true);
  }
  return false;
}


bool CTapeDrive::CheckWritable(bool fMotion)
{
  //++
  //   This method checks the that drive is online with the CheckOnline()
  // routine, but then further checks that the tape is writable.  If not,
  // it generates a FILE PROTECT interrupt and returns FALSE.  As you can
  // probably guess, this routine is used by any function that writes to the
  // tape.
  //
  //   Note that there's still an fMotion parameter needed here - there are
  // some commands like WRITE MARK and WRITE GAP that are considered motion
  // commands but which still write to the tape.  Not all write commands are
  // actually data transfer commands!
  //--
  if (!CheckOnline(fMotion)) return false;
  if (!IsReadOnly()) return true;
  if (fMotion) {
    ClearMotionGO();  SetMotionInt(TMIC_FILE_PROTECT);
  } else {
    SetDataInt(TMIC_FILE_PROTECT);  m_UPE.EmptyTransfer(true);
  }
  return false;
}


void CTapeDrive::DoReadSense(uint8_t nSlave)
{
  //++
  //   This TM78 function updates the unit status (TMUS), drive type (TMDT) and
  // serial number (TMUS) registers for the selected drive and then generates
  // a DONE motion control interrupt.  Note that this is the only function that
  // can be successfully executed for ANY slave, not just #0...
  //--
  LOGF(DEBUG, "READ SENSE on slave #%d", nSlave);
  SetStatus(nSlave);
  ClearMotionGO(nSlave);  SetMotionInt(TMIC_DONE, nSlave, 0);
}


void CTapeDrive::DoRewind()
{
  //++
  //   Rewind the selected tape to the load point.  In real life this command
  // takes a while to executed (a couple minutes, possibly) and the TM78 is
  // free to process other commands while the tape is rewinding.  In our case
  // rewind is infintely fast and we just do it, right here and right now. This
  // is pretty hokey, but empirically it doesn't cause any problems.
  //
  //   According to the manual, a real TM78 always interrupts when the rewind
  // finishes and "sometimes interrupts when rewind starts".  I'm not clear what
  // "sometimes" means in this case, but we just always do a DONE interrupt and
  // nothing more.
  //--
  if (!CheckOnline()) return;
  LOGS(DEBUG, "REWIND on " << *this);
  GetImage()->Rewind();
  SetMotionCount(0);  ClearMotionGO();  SetMotionInt(TMIC_DONE);
}


void CTapeDrive::ManualRewind()
{
  //++
  //   This routine is invoked by the UI REWIND command and it's supposed to
  // be equivalent to an operator initiated manual rewind of a real drive.
  // The drive is taken offline, rewound, and then placed back online.  The
  // latter operation (online) will generate an unsolicited motion status
  // interrupt to the host, just as real drive would when it was placed online.
  // AFAIK, this is equivalent to the way an operator would rewind a real
  // drive.
  //--
  bool fWasOnline = IsOnline();
  if (fWasOnline) GoOffline();
  GetImage()->Rewind();
  LOGS(DEBUG, "unit " << *this << " rewound");
  //   Notice that rewinding DOES NOT call SetStatus() nor do anything to update
  // the TMUS register.  Doing so now, asynchronously, could screw up another
  // command in progress.  Instead, going online will generate a unit attention
  // interrupt, and after that it's up to the host to explicitly ask for the
  // drive's status.
  if (fWasOnline) GoOnline();
}


void CTapeDrive::DoUnload()
{
  //++
  //   Unload rewinds the tape takes the drive offline.  The only way to get
  // it back online is with an MBS operator command.  In our case rewinding
  // the tape is probably pointless, but we'll do it just for completeness.
  //
  //   FWIW, on a real TM78 the UNLOAD command interrupts immediately (before
  // tape finishes rewinding) and the REWIND command interrupts after the tape
  // stops moving.  Since we don't simulate any delay for rewinding this point
  // is kind of moot, but it's worth mentioning.  
  //--
  if (!CheckOnline()) return;
  LOGS(DEBUG, "UNLOAD on "<<*this);
  SetMotionCount(0);  ClearMotionGO();  SetMotionInt(TMIC_DONE);
  GoOffline();
  Detach();
}


void CTapeDrive::DoSpace(uint8_t nCount, bool fReverse, bool fFiles)
{
  //++
  //   This method spaces the tape forward or backward by one or more records
  // or files.  The space record operation will stop prematurely at any tape
  // mark, and either space record or space file will be stopped by an error
  // or by the BOT/EOT marker.  If the skip operation stops early, then the
  // repeat count byte in the MASSBUS motion command register is updated to
  // show the number of skips NOT completed.  For example, if the host told us
  // to skip 10 records and we only skip one, then the repeat count would be
  // updated to 9.  Note that an initial count of either zero or one will both
  // skip exactly one file or record.
  //--
  int32_t nRet;
  if (!CheckOnline()) return;
  LOGS(DEBUG, "SPACE " << (fReverse ? "REVERSE " : "FORWARD ") << nCount <<
       " " << (fFiles ? "FILES" : "RECORDS") << " on " << *this);
  do {
    nRet = fFiles ? (fReverse ? GetImage()->SpaceReverseFile()
                     : GetImage()->SpaceForwardFile())
                     : (fReverse ? GetImage()->SpaceReverseRecord()
                     : GetImage()->SpaceForwardRecord());
    if ((nRet > 0) && (nCount > 0)) --nCount;
  } while ((nRet > 0) && (nCount > 0));
  SetMotionCount(nCount);  ClearMotionGO();
  if (nRet == CTapeImageFile::BADTAPE)
    SetMotionInt(TMIC_BAD_TAPE);
  else if (nRet == CTapeImageFile::TAPEMARK)
    SetMotionInt(TMIC_TAPE_MARK);
  else if (nRet == CTapeImageFile::EOTBOT)
    SetMotionInt(fReverse ? TMIC_BOT : TMIC_EOT);
  else
    SetMotionInt(TMIC_DONE);
}


void CTapeDrive::DoWriteMark(uint8_t nCount)
{
  //++
  //   This method writes a tape mark to the virtual tape.  If this occurs
  // in the middle of a previously recorded tape, then any data after the
  // current position is lost forever.  The nCount parameter is the repeat
  // count from the original command - it's not clear whether the real TM78
  // used this to write multiple tape marks with one operation, but we'll
  // go with that option.
  //--
  bool fError = false;
  if (!CheckWritable()) return;
  LOGS(DEBUG, "WRITE "<<nCount<<" TAPE MARK(S) on "<<*this);
  do {
    fError = GetImage()->WriteMark();
    if (!fError && (nCount > 0)) --nCount;
  } while ((nCount > 0) && !fError);
  SetMotionCount(nCount);  ClearMotionGO();
  SetMotionInt(fError ? TMIC_BAD_TAPE : TMIC_DONE);
}


void CTapeDrive::DoWriteGap(uint8_t nCount)
{
  //++
  //   This method simulates writing blank tape.  When it's used in the middle
  // of a previously recorded tape this would have rendered all or part of the
  // remaining data unreadable, but we have no good way to emulate that so we
  // just erase all the remaining data by truncating the tape image to this
  // point.  If it's used at the end of a tape then it effectively does
  // nothing from our point of view, so this is harmless.
  //
  //   Note that this function ignores the repeat count - the way we do things,
  // repeating a write gap would be pointless.  The repeat count is zeroed if
  // it succeeds, and is unchaged if it fails.
  //--
  if (!CheckWritable()) return;
  LOGS(DEBUG, "WRITE GAP on " << *this);
  GetImage()->Truncate();
  SetMotionCount(0);  ClearMotionGO();  SetMotionInt(TMIC_DONE);
}


void CTapeDrive::DoEraseTape()
{
  //++
  //   This method does a data security erase, which is supposed to erase all
  // data on the tape following the current point.  In our case that's exactly
  // the same thing that write gap does, so we just hand it off to that code.
  //--
  LOGS(TRACE, "ERASE TAPE on " << *this);
  DoWriteGap();
}


void CTapeDrive::DoReadExtendedSense()
{
  //++
  //   This command reads the extended sense data from the TM78's microprocessor.
  // The extended sense data is transferred just like tape data and this command
  // works something like the READ DATA FORWARD function, except that the data
  // length is fixed (it's always 30 half words) and the format, skip count, byte
  // count, etc fields in the TCR and BCR registers are ignored.
  //
  //   Right now extended sense data isn't implemented and this function is
  // hard coded to always return a buffer of all zeros.  TOPS10 doesn't seem to
  // care what the data actually is - it just writes it to the error log.
  //--
  uint32_t alSense[TMES_LENGTH];
  LOGS(TRACE, "READ EXTENDED SENSE on " << *this);
  memset(alSense, 0, sizeof(alSense));
  SetDataInt(TMIC_DONE);
  m_UPE.WriteData(alSense, TMES_LENGTH);
}


void CTapeDrive::DoRead(bool fReverse, uint8_t bFormat, uint32_t lByteCount)
{
  //++
  //  Handle tape read operations, both forward and backward.  This hasn't been
  // very extensively tested, but this seems to be the basic  idea!
  //--
  if (!CheckOnline(false)) return;
  LOGS(DEBUG, "READ RECORD " << (fReverse ? "REVERSE" : "FORWARD") << " on " << *this);
  LOGF(TRACE, "  >> Format=%o, Byte Count=%d", bFormat, lByteCount);

  // A "READ REVERSE" operation at BOT is an immediate failure ...
  if (fReverse && GetImage()->IsBOT()) {
    LOGF(WARNING, "READ REVERSE AT BOT!!");
    SetDataInt(TMIC_BOT);  m_UPE.EmptyTransfer(true);
    return;
  }

  // Try to read the record ...
  int32_t cbRecord = GetImage()->ReadForwardRecord(m_abBuffer, CTapeImageFile::MAXRECLEN);
  if (cbRecord <= 0) {
    if (cbRecord == CTapeImageFile::TAPEMARK) {
      // Here if a tape mark is found during a read operation ...
      LOGS(TRACE, "<TAPE MARK> on " << *this);
      //   Note that this code (and the other two subsequent error cases) clear
      // the byte count register to indicate that zero bytes were actually
      // transferred.  I'm not completely sure that's the right behavior (a byte
      // count of zero indicates a 64K record), but Bruce and Rich persuaded me
      // to give it a try.  After further experimentation, this appears to make
      // WAITS happy, so beware if you take this out.
      m_UPE.WriteMBR(m_nUnit, TMBCR, 0);
      SetDataInt(TMIC_TAPE_MARK); m_UPE.EmptyTransfer(true);
      return;
    } else if (cbRecord == CTapeImageFile::EOTBOT) {
      // Here if end of tape is found during a read operation ...
      LOGS(TRACE, "<END OF TAPE> on " << *this);
      m_UPE.WriteMBR(m_nUnit, TMBCR, 0);
      SetDataInt(TMIC_EOT);  m_UPE.EmptyTransfer(true);
      return;
    } else {
      // Here for any other kind of tape read error ...
      LOGS(WARNING, "TAPE ERROR (" << cbRecord << ") on " << *this);
      m_UPE.WriteMBR(m_nUnit, TMBCR, 0);
      SetDataInt(TMIC_UNREADABLE, 0, 1);  m_UPE.EmptyTransfer(true);
      return;
    }
  }

  //   Update the record, byte count, interrupt and drive status registers.
  // It may seem wrong to do this BEFORE we transfer data, but that's the way
  // it needs to be.  Remember that in the case of data transfers, the TM78
  // does not actually generate any interrupt to the host - the "done"
  // interrupt comes from the RH20 data channel.  That'll happen a few micro-
  // seconds after we fill the FIFO, so we've got to have all the other
  // registers up to date before then.
  //m_UPE.ClearBitMBR(m_nUnit, TMDCR, 1);
  m_UPE.ClearBitMBR(m_nUnit, TMTCR, TMTCR_M_REC_COUNT);
  m_UPE.WriteMBR(m_nUnit, TMBCR, LOWORD(cbRecord));
  LOGF(TRACE, "  >> cbRecord=%d, TMTCR=%06o, TMBCR=0%06o",
       LOWORD(cbRecord), m_UPE.ReadMBR(m_nUnit, TMTCR), m_UPE.ReadMBR(m_nUnit, TMBCR));
  if ((uint32_t) cbRecord < lByteCount)
    SetDataInt(TMIC_SHORT_RECORD);
  else if ((uint32_t) cbRecord > lByteCount)
    SetDataInt(TMIC_LONG_RECORD);
  else
    SetDataInt(TMIC_DONE);

  // Finally, unpack the data and send it to the host...
  uint32_t clRecord = Fiddle8to18(bFormat, m_abBuffer, m_alBuffer, cbRecord, fReverse);
  LOGS(DEBUG, "READ RECORD on " << *this << ", format " << bFormat << ", " << cbRecord << " bytes, " << clRecord << " halfwords");
  //DumpRecord(m_alBuffer, clRecord);
  //   This is an odd case - the TM78 manual says, verbatim - "All interrupt codes,
  // except DONE, are accompaniend by the DEE bit in the RH20."  From that we infer
  // that either a long or short read is also an exception, so that's what we'll
  // do.  I'm not absolutely sure this is the right thing, since DEE aborts any
  // RH20 command list and short records aren't at all unusual when reading.
  m_UPE.WriteData(m_alBuffer, clRecord, ((uint32_t) cbRecord != lByteCount));
}

//   The way writing records works, at least in our implementation, is a little
// odd.  First, it's split into two distinct parts - in the first part, the
// host initializes the TMBCR and TMTCR registers and then loads some kind of
// WRITE RECORD command into the TMDCR.  We figure out how many 18 bit half
// words we expect to transfer from the host to the tape and then tell the
// FPGA about this value.  So far so good.
//
//   The FPGA then transfers the data from the host and puts it in the data
// FIFO without our help.  
//
//   One subtle but really, really important point is that it's the RH20 that
// interrupts the host for data transfers, NOT us.  That means that as soon as
// we finish reading the data record from the host the RH20 will interrupt the
// KL and tell it that the transfer is finished.  At that point TOPS10 will
// think that it can go and read the TMDIR register to get the status from the
// write operation.  
//
//   That means that we have to load the TMDIR BEFORE we transfer data from the
// host, and that puts us in the odd position of needing to give the completion
// status of the write operation BEFORE we actually receive the data to write!
// That's strange but it doesn't cause us any fundamental problems.  The only
// errors that can occur _during_ a write (as opposed to, say, offile or file
// protect errors, which occur _before_ the write starts) are end of tape and
// bad tape.  Neither of those are conditions we simulate, so we don't have to
// worry about 'em.
void CTapeDrive::DoWrite(uint8_t bFormat, uint32_t lByteCount)
{
  //++
  //   THIS WAS NEVER REALLY FINISHED AND HASN'T BEEN TESTED MUCH, BUT HERE'S
  // THE BASIC IDEA!
  //--
  if (!CheckWritable(false)) return;
  uint32_t clRecord = (bFormat==TMAM_10_COMPATIBLE) ? (lByteCount*2/4) : (lByteCount*2/5);
  LOGS(TRACE, "WRITE RECORD on " << *this);
  LOGF(TRACE, "  >> Format=%o, Byte Count=%d, Halfword Count=%d", bFormat, lByteCount, clRecord);

  m_UPE.ClearBitMBR(m_nUnit, TMTCR, TMTCR_M_REC_COUNT);
  SetDataInt(TMIC_DONE);

  if (m_UPE.ReadData(m_alBuffer, clRecord)) {
    //  DumpRecord(m_alBuffer, clRecord);
    uint32_t cbRecord = Fiddle18to8(bFormat, m_alBuffer, m_abBuffer, clRecord);
    GetImage()->WriteRecord(m_abBuffer, cbRecord);
  } else {
    LOGF(TRACE, "  >> ERROR READING DATA FROM FIFO!!!");
  }

}

//////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


void CTapeDrive::DoMotionCommand(uint8_t nSlave, uint8_t bFunction, uint8_t bCount)
{
  //++
  //--

  //   Right now we only implement slave #0 and the only legal command
  // for any other slave is READ SENSE.  We have to implement that one,
  // because that's how TOPS10 knows which slaves exist!
  if (nSlave != 0) {
    if (bFunction==TMCMD_SENSE) {
      DoReadSense(nSlave);
    } else {
      LOGF(DEBUG, "motion command for non-existent slave #%d", nSlave);
      ClearMotionGO(nSlave);  SetMotionInt(TMIC_NOT_AVAIL, nSlave, 0);
    }
    return;
  }

  // Handle all motion commands for slave 0 ...
  if (bCount == 0) bCount = 1;
  switch (bFunction) {
    case TMCMD_SENSE:       DoReadSense();                  break;
    case TMCMD_WTM_PE:      DoWriteMark(bCount);            break;
    case TMCMD_WTM_GCR:     DoWriteMark(bCount);            break;
    case TMCMD_SP_FWD_REC:  DoSpace(bCount, false, false);  break;
    case TMCMD_SP_REV_REC:  DoSpace(bCount, true, false);  break;
    case TMCMD_SP_FWD_FILE: DoSpace(bCount, false, true);  break;
    case TMCMD_SP_REV_FILE: DoSpace(bCount, true, true);  break;
    case TMCMD_REWIND:      DoRewind();                     break;
    case TMCMD_UNLOAD:      DoUnload();                     break;
    case TMCMD_ERG_PE:      DoWriteGap(bCount);             break;
    case TMCMD_ERG_GCR:     DoWriteGap(bCount);             break;
    case TMCMD_DSE:         DoEraseTape();                  break;
    default:
      LOGF(WARNING, "unimplemented tape motion command %03o", bFunction);
      ClearMotionGO();  SetMotionInt(TMIC_TM_FAULT_A);
      break;
  }
}


void CTapeDrive::DoTransferCommand(uint8_t bFunction)
{
  //++
  //--

  // Read and decode the tape control and byte count registers ...
  uint16_t nTCR = m_UPE.ReadMBR(m_nUnit, TMTCR);
  uint8_t  bFormat =      (nTCR & TMTCR_M_FORMAT)     >> TMTCR_V_FORMAT;
  uint8_t  bSkipCount =   (nTCR & TMTCR_M_SKIP_COUNT) >> TMTCR_V_SKIP_COUNT;
  uint8_t  bRecordCount = (nTCR & TMTCR_M_REC_COUNT)  >> TMTCR_V_REC_COUNT;
  uint8_t  bSlave =       (nTCR & TMTCR_M_CMD_ADDR)   >> TMTCR_V_CMD_ADDR;
  uint32_t lByteCount = MKLONG(0, m_UPE.ReadMBR(m_nUnit, TMBCR));
  if (lByteCount == 0)  lByteCount = 65536UL;

  //   We actually only implement a fairly small subset of all the possible bit
  // fiddle functions; we don't implement the skip count field, and we can only
  // read one record at a time.  We can check for all these requirements and bail
  // immediately if they're not met.
  if (bSlave != 0) {
    LOGF(WARNING, "DATA TRANSFER ON SLAVE %d NOT IMPLEMENTED!!", bSlave);  goto errret;
  }
  if ((bFormat != TMAM_10_COMPATIBLE)  &&  (bFormat != TMAM_10_CORE_DUMP)) {
    LOGF(WARNING, "BIT FIDDLER FORMAT %03o NOT IMPLEMENTED!!", bFormat);  goto errret;
  }
  if (bSkipCount != 0) {
    LOGF(WARNING, "SKIP COUNT .GT. 0 NOT IMPLEMENTED!!");  goto errret;
  }
  if (bRecordCount > 1) {
    LOGF(WARNING, "RECORD COUNT .GT. 1 NOT IMPLEMENTED!!");  goto errret;
  }

  //(bool fReverse, uint8_t bFormat, uint16_t wByteCount)
  switch (bFunction) {
    case TMCMD_RD_FWD:    DoRead(false, bFormat, lByteCount);  break;
    case TMCMD_RD_REV:    DoRead(true, bFormat, lByteCount);  break;
    case TMCMD_WRT_PE:    DoWrite(bFormat, lByteCount);  break;
    case TMCMD_WRT_GCR:   DoWrite(bFormat, lByteCount);  break;
    case TMCMD_RD_EXSNS:  DoReadExtendedSense();                break;
    default:
      LOGF(WARNING, "unimplemented tape transfer command %03o", bFunction);
      goto errret;
  }
  return;

  //   Here for an unimplemented read operation.  This aborts the read with a
  // TM_FAULT_A error status ("illegal command code").  This probably won't be
  // what the host is expecting, but it's the best we can do.
errret:
  SetDataInt(TMIC_TM_FAULT_A);  m_UPE.EmptyTransfer(true);
}


void CTapeDrive::DoCommand(uint32_t lCommand)
{
  //++
  //   This method will execute one MASSBUS command, where lCommand is the
  // 32 bit command FIFO longword from the FPGA.  In the case of tape drives,
  // particularly the TM78, we need to know not only the 16 bit command code,
  // but also the address of the MASSBUS register it was written to.  That's
  // because the TM78 actually implements five separate MASSBUS command
  // registers!  Fortunately the FPGA has captured this information for us
  // and it's included in the lCommand longword.
  //--
  uint16_t wCommand  = CDECUPE::ExtractCommand(lCommand);
  uint8_t  bRegister = CDECUPE::ExtractRegister(lCommand);
  uint8_t  bFunction = wCommand & TMCMD_M_MASK;

  if (CDECUPE::IsEndofBlock(lCommand)) {
    // Handle the FPGA's EBL signal (currently unused) ...
    LOGS(TRACE, "END OF BLOCK ignored on " << *this);
  } else if (bRegister == TMHCR) {
    // Handle TM78 reset - TBA!
    if (ISSET(wCommand, TMHCR_CLEAR)) {
      LOGS(DEBUG, "FORMATTER RESET on " << *this << " (IGNORED)");
    }
  } else if ((bRegister & TMMCR0) == TMMCR0) {
    // Handle motion control (non-data transfer) commands ...
    uint8_t bCount = HIBYTE(wCommand);
    uint8_t bSlave = bRegister & 3;
    DoMotionCommand(bSlave, bFunction, bCount);
  } else if (bRegister == TMDCR) {
    // Handle data transfer commands ...
    DoTransferCommand(bFunction);
    return;
  } else {
    // Anything else is a screwed up FPGA!
    LOGF(WARNING, "received command (%07o) via unknown register (%03o)", wCommand, bRegister);
  }
}


#ifdef _DEBUG
void CTapeDrive::DumpRecord(uint32_t *plData, uint32_t clData)
{
  //++
  //   This routine will dump a tape record in a readable format for debugging.
  // The data is always printed as 18 bit octal halfwords.
  //--
  char szLine[CLog::MAXMSG];  uint32_t i;
  LOGF(TRACE, "  >> DUMP OF TAPE DATA (record length=%d halfwords)", clData);
  for (i = 0, szLine[0] = 0; i < clData; ++i) {
    char sz[10];
    if ((i & 7) == 0) {
      sprintf_s(sz, sizeof(sz), "%06o/ ", i);
      strcat_s(szLine, sizeof(szLine), sz);
      //    sprintf_s(szLine+strlen(szLine), sizeof(sz), "%06lo/ ", i);
    }
    sprintf_s(sz, sizeof(sz), " %06o", (plData[i] & 0777777));
    strcat_s(szLine, sizeof(szLine), sz);
    if ((i & 7) == 7) {
      LOGF(TRACE, "  >> %s", szLine);  szLine[0] = 0;
    }
  }
  if (strlen(szLine) > 0) LOGF(TRACE, "  >> %s", szLine);
}
#endif

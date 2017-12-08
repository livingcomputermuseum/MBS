//++
// decupe.c -> DEC specific UPE interface methods for MASSBUS server 
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
//   A CUPE object is an interface to one FPGA board running the UPE Xilinx
// bitstream.  The actual physical interface between the two is via the PLX
// 9054 PCI to localbus bridge chip, and you'll notice that this module is
// mostly based on the PLX API library.
//
//   Our interface to the the FPGA is actually fairly simple  - there's only a
// shared memory window which is used for all communications, and an interrupt.
// That's it - no I/O ports are used.  The memory window contains a number of
// items, including a copy of all the MASSBUS registers, a command queue,
// configuration information, and a data transfer buffer.  The whole thing is
// described by the UPE_MEMORY structure.  A single interrupt is used by the
// FPGA to signal the PC every time somethingis added to the command queue,
// and that's it.
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
// 15-NOV-13  RLA   Add multiple FPGA/UPE support.
//  7-DEC-13  RLA   SetBitMBR() and ToggleBitMBR() have the wrong operator!
// 11-MAY-15  RLA   Add "from PC" FIFO full handshaking.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <PlxApi.h>             // PLX 9054 PCI interface API declarations
#include "UPELIB.hpp"           // UPE library definitions
#include "UPE.hpp"              // UPE library FPGA interface methods
#include "LogFile.hpp"          // UPE library message logging facility
#include "MBS.hpp"              // global declarations for this project
#include "DECUPE.hpp"           // and declarations for this module


CUPE *NewDECUPE (const PLX_DEVICE_KEY *pplxKey)
{
  //++
  //   This little routine is a CDECUPE "object factory".  All it does is to
  // create new instances of CDECUPE objects - it's used by the library CUPEs
  // collection class to create new instances of application specific UPE
  // objects...
  //--
  return (CUPE *) DBGNEW CDECUPE(pplxKey);
}


bool CDECUPE::Initialize()
{
  //++
  //   This routine should be called after successfully locking the FPGA.  It
  // will initialize any DEC specific UPE control registers and enable PC
  // interrupts ...
  //--
  assert(IsOpen());
  assert(sizeof(SHARED_MEMORY) == CUPE::SHARED_MEMORY_SIZE);

  // Temporary!
  //LOGF(DEBUG, "UPE offset of lWordCount = 0x%04X", offsetof(_UPE_MEMORY, lWordCount));
  //LOGF(DEBUG, "UPE offset of lDataFIFO  = 0x%04X", offsetof(_UPE_MEMORY, lDataFIFO));

  // Capture generic local bus -> PCI interrupts ...
  if (!CUPE::Initialize()) return false;
  if (RegisterInterrupt() != ApiSuccess) return false;

  // Initialize the FPGA registers ...
  GetWindow()->lDrivesAttached = 0;
  if (!IsCableConnected())
    LOGS(WARNING, "MASSBUS cable disconnected on " << *this);
//GetWindow()->lDataClock = ???
//GetWindow()->lTransferDelay = ???
//GetWindow()->lControlErrors = 0;
//GetWindow()->lDataErrors = 0;
  return true;
}


uint16_t CDECUPE::ReadMBR (uint8_t nUnit, uint8_t nRegister) const
{
  //++
  //   This method reads, via the UPE, and returns the contents of the
  // specified MASSBUS register for the specified unit.  Remember that there
  // can be up to 8 devices on the MASSBUS and each one has its own separate
  // and independent register file.  
  //--
  assert((GetWindow() != NULL) && (nUnit < 8) && (nRegister < 32));
  return LOWORD(GetWindow()->alRegisters[nUnit][nRegister]);
}


void CDECUPE::WriteMBR (uint8_t nUnit, uint8_t nRegister, uint16_t wValue)
{
  //++
  //   Write (via the FPGA) the contents of a MASSBUS register ...
  // This is the logical complement to ReadMBR() ...
  //--
  assert((GetWindow() != NULL) && (nUnit < 8) && (nRegister < 32));
  GetWindow()->alRegisters[nUnit][nRegister] = wValue;
#ifdef _DEBUG
  uint16_t wNew = ReadMBR(nUnit, nRegister);
  if (wNew != wValue)
    LOGF(WARNING, "WriteMBR() failed - nUnit=%d, nRegister=%d, wValue=%06o, register=%06o", nUnit, nRegister, wValue, wNew);
#endif
}


uint16_t CDECUPE::ClearBitMBR (uint8_t nUnit, uint8_t nRegister, uint16_t wMask)
{
  //++
  // Clear bits (under mask) in the MASSBUS register ...
  //--
  assert((GetWindow() != NULL) && (nUnit < 8) && (nRegister < 32));
#ifdef _DEBUG
  uint16_t wOld = ReadMBR(nUnit, nRegister);
#endif
  GetWindow()->alRegisters[nUnit][nRegister] &= MKLONG(0, ~wMask);
#ifdef _DEBUG
  uint16_t wNew = ReadMBR(nUnit, nRegister);
  if ((wNew & wMask) != 0)
    LOGF(WARNING, "ClearBitMBR() failed - nUnit=%d, nRegister=%d, wMask=%06o, before=%06o, after=%06o",
    nUnit, nRegister, wMask, wOld, wNew);
#endif
  return LOWORD(GetWindow()->alRegisters[nUnit][nRegister]);
}


uint16_t CDECUPE::SetBitMBR (uint8_t nUnit, uint8_t nRegister, uint16_t wMask)
{
  //++
  // Set bits (under mask) in the MASSBUS register ...
  //--
  assert((GetWindow() != NULL) && (nUnit < 8) && (nRegister < 32));
#ifdef _DEBUG
  uint16_t wOld = ReadMBR(nUnit, nRegister);
#endif
  GetWindow()->alRegisters[nUnit][nRegister] |= MKLONG(0, wMask);
#ifdef _DEBUG
  uint16_t wNew = ReadMBR(nUnit, nRegister);
  if ((wNew & wMask) != wMask)
    LOGF(WARNING, "SetBitMBR() failed - nUnit=%d, nRegister=%d, wMask=%06o, before=%06o, after=%06o",
    nUnit, nRegister, wMask, wOld, wNew);
#endif
  return LOWORD(GetWindow()->alRegisters[nUnit][nRegister]);
}


uint16_t CDECUPE::ToggleBitMBR (uint8_t nUnit, uint8_t nRegister, uint16_t wMask)
{
  //++
  // Toggle bits (under mask) in the MASSBUS register ...
  //--
  assert((GetWindow() != NULL) && (nUnit < 8) && (nRegister < 32));
  GetWindow()->alRegisters[nUnit][nRegister] ^= MKLONG(0, wMask);
  return LOWORD(GetWindow()->alRegisters[nUnit][nRegister]);
}


uint32_t CDECUPE::WaitCommand (uint32_t lTimeout)
{
  //++
  //   This method will wait for a command to show up in the UPE's FIFO and,
  // when it finds one, it returns the UPE command longword.  The low order 16
  // bits of this longword are simply the contents MASSBUS control and status
  // register, and the upper bits contain some UPE specific flags.  lTimeout
  // specifies how long (in milliseconds) that we should wait for a command.
  // If the timeout period expires, then zero (which would be an invalid
  // command) is returned.
  //
  //   Note that reading the UPE FIFO is a tricky thing - the very same PCI
  // bus transaction that reads the FIFO also clears it, so we only get exactly
  // one chance to read it!  If you read it a second time, you're guaranteed
  // not to get the same result...
  //--
  uint32_t cmd, ret;
  assert(IsOpen());

  //   If we're offline, then just sleep for the timeout period and then
  // return CMD_TIMEOUT.  That's all we know how to do!
  if (IsOffline()) {_sleep_ms(lTimeout);  return TIMEOUT;}

  // If there's a valid command in the queue now, then just return it.
  cmd = GetWindow()->lCommandFIFO;
  if (IsCommandValid(cmd)) goto gotcmd;

  //   There's no command waiting, so we'll have to block until something
  // shows up.  The order of operations here is tricky - if the FPGA asserts
  // an interrupt reqest BEFORE we've called PlxPci_EnableInterrupt, then the
  // PLX driver will lose that interrupt.  The solution is to enable PLX
  // interrupts first, AND THEN set the FPGA interrupt enable. This guarantees
  // that the PC won't see an interrupt request before the PLX driver is ready.
  if (EnableInterrupt() != ApiSuccess) return ERROR;
  //SetControl(INTERRUPT_ENABLE); ** NOT YET IMPLEMENTED ON MASSBUS!!!
  ret = WaitInterrupt(lTimeout);
  //ClearControl(INTERRUPT_ENABLE); ** NOT YET IMPLEMENTED ON MASSBUS!!!
  if ((ret == ApiWaitTimeout) || (ret == ApiWaitCanceled)) return TIMEOUT;
  if (ret != ApiSuccess)  return ERROR;

  // And now there should be a command in the queue!
  cmd = GetWindow()->lCommandFIFO;
  if (!IsCommandValid(cmd)) {
    LOGF(WARNING, "FPGA interrupted but no command found");  return TIMEOUT;
  }

  // Here if we have a good command ...
gotcmd:
  LOGF(TRACE, "Command 0x%08x (reg=%02o, unit=%d, cmd=%06o) received by %s", 
    cmd, ExtractRegister(cmd), ExtractUnit(cmd), ExtractCommand(cmd), GetBDF().c_str());
  return cmd;
}


bool CDECUPE::ReadData (uint32_t *plData, uint32_t clData)
{
  //++
  //   This method will read disk sector data from the UPE's data FIFO.  This
  // FIFO works much like the command queue - there's just a single long word
  // that we read over and over, and each read gets us the next data word in
  // the queue.  Even though the FIFO access is a longword (32 bits) wide, the
  // data itself is at most 18 bits.
  //
  //   Like the command FIFO, there's always the possibility that we might read
  // faster than the data arrives, and in which case the FIFO might be empty.
  // Like the command FIFO, the data FIFO sets the MSB (0x80000000) bit of every
  // valid word, so any word we read WITHOUT this bit set is invalid. Unlike the
  // command FIFO, however, the data FIFO has no hardware interrupt to let us
  // know when more data is ready - we simply spin here polling the FIFO until
  // we get what we want.
  //
  //   This is a little hokey, but remember that the real MASSBUS has to
  // transfer data fast enough to keep up with the spinning disk.  That means
  // there's an upper limit on how long it can take to transfer a sector and
  // we're guaranteed that we can't wait forever.  Just in case something goes
  // wrong, however, we also implement a simple timeout that aborts the read
  // operation if things drag on too long.
  //
  //   The only thing that can go wrong here is a timeout reading data, and if
  // that happens then false is returned.
  //--
  uint32_t i, tmo, data;
  if (IsOffline()) return false;
  assert(IsOpen() && (plData != NULL) && (clData > 0));

  // For tapes, tell the FPGA how many words to expect ...
  if (IsTape()) {
    LOGF(TRACE, "  >> reading %d halfwords from FIFO", clData);
    GetWindow()->lSendCount = clData;
  }

  //   And now read the expected number of words from the FIFO.  Spin wait, in a
  // tight little loop here, if data is not available (but don't wait too long!).
  for (i = 0;  i < clData;  ++i) {
    for (tmo = 0;  ;  ++tmo) {
      data = GetWindow()->lDataFIFO;
      if (IsDataValid (data)) break;
      if (tmo >= DATA_TIMEOUT) {
        LOGS(WARNING, "data FIFO timeout on " << *this);  return false;
      }
    }
    plData[i] = MASK18(data);
  }

  // Success!
  return true;
}


bool CDECUPE::WriteData (const uint32_t *plData, uint32_t clData, bool fException)
{
  //++
  //   This method writes a buffer of data to the FPGA's data FIFO. For disk
  // drives this is pretty easy because the sector size is fixed and known in
  // advance, and the FIFO is plenty big enough to hold an entire sector.  We
  // can just dump the data in there and let the FPGA take care of it.
  //
  //   For tapes it's a lot harder because the sector size varies and may even
  // be bigger than the FPGA's FIFO.  We have to let the FPGA know how many
  // words to transfer by writing the word count to a special location in the
  // shared memory map.  There's also the possibility that we could actually be
  // faster than the RH20 and that the FIFO could overflow for long records.
  // To avoid data loss, we have to poll the FIFO's status and make sure
  // there's room before we write a word.
  //--
  assert(IsOpen()  &&  (plData != NULL)  &&  (clData > 0));

  //   Tape records are variable length, and for those we have to tell the
  // FPGA how many words to expect ...
  if (IsTape()) {
  }

  //   Put the data in the FIFO ...  Notice that the tape situation is much more
  // complex because a) records there are variable length, and b) tape records
  // can be very (very!) long and we have to use care not to overflow the FIFO.
  // Neither of these are a concern for disk drives, because the record length
  // is always exactly one sector (1024 halfwords).
  if (IsTape()) {
    //LOGF(TRACE, "  >> writing %d halfwords to FIFO", clData);
    //   If fException is true, then set the FORCE_EXCEPTION bit in the word
    // count register.  This tells the FPGA that it shold assert the MASSBUS EXC
    // (exception) signal, which tells the RH20 that an error occurred.  This in
    // turn sets the DEE (drive exception error) bit in the RH20 status and
    // aborts any RH20 command list in progress...
    GetWindow()->lSendCount = clData | (fException ? FORCE_EXCEPTION : 0);
    for (uint32_t i = 0;  i < clData;  ++i) {
      //   If the "from PC" FIFO is almost full, then just spin in a tight loop
      // waiting for some of the data to clear out.  Don't wait forever, though!
      if (ISSET(GetWindow()->lFIFOstatus, FROMPC_ALMOST_FULL)) {
        //LOGF(TRACE, "  >> FIFO STATUS 0x%08x .. waiting", GetWindow()->lFIFOstatus);
        for (uint32_t tmo = 0;  !ISSET(GetWindow()->lFIFOstatus, FROMPC_ALMOST_EMPTY);  ++tmo) {
          if (tmo >= DATA_TIMEOUT) {
            LOGS(WARNING, "data FIFO timeout on " << *this);  return false;
          }
        }
        //LOGF(TRACE, "  >> FIFO STATUS 0x%08x .. ready", GetWindow()->lFIFOstatus);
      }
      // Stuff the next word into the FIFO ...
      GetWindow()->lDataFIFO = MASK18(plData[i]);
    }
  } else {
    // For the disk case, we can just let 'er rip!
    for (uint32_t i = 0;  i < clData;  ++i) 
      GetWindow()->lDataFIFO = MASK18(plData[i]);
  }

  // Success!
  return true;
}


void CDECUPE::EmptyTransfer (bool fException)
{
  //++
  //   This method sends a "null" (i.e. zero length) data record to the host.
  // This is a special case for tape emulation when an error or tape mark is
  // found during an operation.
  //--
  assert(IsOpen());
  //   If fException is true, then set the FORCE_EXCEPTION bit in the word
  // count register.  This tells the FPGA that it shold assert the MASSBUS EXC
  // (exception) signal, which tells the RH20 that an error occurred.  This in
  // turn sets the DEE (drive exception error) bit in the RH20 status and
  // aborts any RH20 command list in progress...
  GetWindow()->lSendCount = fException ? FORCE_EXCEPTION : 0;
  //   Even though we are transferring zero words, Bruce's FPGA state machine
  // needs to find something in the data FIFO or else it will hang up.  Bruce
  // swears that this word will be flushed and not actually sent to the host.
  GetWindow()->lDataFIFO = 0;
}


void CDECUPE::SetGeometry (uint8_t nUnit, uint16_t nCylinders, uint8_t nHeads, uint8_t nSectors)
{
  //++
  //   This method will tell the FPGA about the geometry of the specified 
  // MASSBUS unit.  The FPGA uses this information to set various error bits
  // and to handle spiral read/writes.
  //--
  assert(IsOpen()  &&  (nUnit < 8));
  GetWindow()->alGeometry[nUnit] = 
      ((nCylinders - 1)  <<  16)
    | ((nHeads     - 1)  <<   8)
    | ( nSectors   - 1);
}


void CDECUPE::SetDrivesAttached (uint32_t nMap)
{
  //++
  //   This method sets the bitmap of connected drives in the FPGA.  This is
  // eight bits, one for each drive, with a 1 meaning that the corresponding
  // drive is connected to the MASSBUS.  Bit 0 is unit 0, bit 1 is unit 1, etc.
  //--
  assert(IsOpen()  &&  (nMap < 256));
  GetWindow()->lDrivesAttached = nMap;
  LOGF(DEBUG, "drive map set to 0x%02X", nMap);
}

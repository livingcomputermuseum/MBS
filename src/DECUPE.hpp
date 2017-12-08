//++
// decupe.hpp -> CDECUPE (DEC specific FPGA/UPE) interface class declarations
//
//       COPYRIGHT (C) 2015-2017 Vulcan Inc.
//       Developed by Living Computers: Museum+Labs
//
// DESCRIPTION:
//   The CDECUPE class wraps the generic CUPE class implemented in the UPELIB.
// CDECUPE implements the DEC specific shared memory layoput and all the DEC
// specific functions.
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
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
//  3-JUN-15  RLA   Adapt to use UPELIB
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
using std::string;              // ...
using std::ostream;             // ...


// CDECUPE class definition ...
class CDECUPE : public CUPE {
  //++
  // The CUPE class describes the interface to a single FPGA/UPE board ...
  //--

  // Constants and parameters ...
  public:
    enum {
      COMMAND_TIMEOUT =  1000UL,  // upeWaitCommand() timeout (in ms)
      DATA_TIMEOUT    = 77777UL   // data transfer timeout (iterations)
    };

    // Shared Memory Map
    //   This structure defines the contents of the memory region that's shared
    // between the FPGA/UPE and this server.  Use caution when modifying it - you
    // pretty much CANNOT change anything here without making corresponding changes
    // the the VHDL for the UPE...
    //
    //   One "odd" thing to keep in mind is that every datum in the UPE memory is
    // a 32 bit longword.  This is always true, even for things where the actual
    // data is distinctly shorter (e.g. MASSBUS registers, clock divisors, etc).
    // The actual data is always right justified and zero filled.
    //
    //  Another odd thing is that the UPE memory map is incompletely decoded.  There
    // are many single byte/word values (e.g. transfer delay, drive config, etc)
    // that occupy a 256 byte chunk of addresses.  The real truth is that the lower
    // eight bits of the address are simply being ignored, and that's why you see so
    // many "filler" arrays in this structure ...
private:
#pragma pack(push, 4)
    typedef struct _SHARED_MEMORY {
      uint32_t alRegisters[8][32];      // 0x00xx all MASSBUS registers (8 drives, 32 regs)
      uint32_t lDrivesAttached;         // 0x04xx bitmap of connected drives
      uint32_t filler_1[255];           //
      uint32_t lDataClock;	        // 0x08xx MASSBUS data transfer clock divisor
      uint32_t filler_2[255];           //
      uint32_t lTransferDelay;          // 0x0Cxx MASSBUS R/W delay  71 - 48MHz/72 for SCLK half cycle
      uint32_t filler_3[255];           //
      uint32_t lCommandFIFO;            // 0x10xx next command (the top of the FIFO)
      uint32_t filler_4[255];           //
      uint32_t alGeometry[8];           // 0x14xx drive geometry
      uint32_t filler_5[248];           //
      // 0x18xx control and counter registers
      uint32_t lControlErrors;          //   count of control bus parity errors
      uint32_t lDataErrors;             //   count of data bus parity errors
      uint32_t lFIFOstatus;             //   FIFO status bits
      uint32_t lwhatever;               //   don't know what Bruce put here!
      uint32_t lVHDL;                   //   VHDL version number
      uint32_t filler_6[251];           //
      uint32_t filler_7[256];           // 0x1Cxx unused ...
      uint32_t filler_8[256];           // 0x20xx unused ...
      uint32_t lSendCount;              // 0x2400 count of words to be sent to the host
      uint32_t lReceiveCount;           // 0x2404 count of words received from the host
      uint32_t filler_9[254];           // ...
      uint32_t filler_10[5632];         // 0x28xx .. 0x7C00 unused
      uint32_t lDataFIFO;	        // 0x8xxx data transfer buffer
      uint32_t filler_11[8191];         //
    } SHARED_MEMORY;
#pragma pack(pop)

    // FPGA flags and special bits ...
public:
  enum {
    // Magic bits and fields in the UPE command FIFO ...
    VALID               = 0x80000000UL, // this silo entry is valid (ignore otherwise)
    END_OF_BLOCK        = 0x01000000UL, // end of data block
    TOPC_EMPTY          = 0x00000001UL, // to PC FIFO empty
    TOPC_ALMOST_EMPTY   = 0x00000002UL, //  "  "   "  almost empty
    TOPC_ALMOST_FULL    = 0x00000004UL, //  "  "   "  almost full
    TOPC_FULL           = 0x00000008UL, //  "  "   "  full
    FROMPC_EMPTY        = 0x00000100UL, // from PC FIFO empty
    FROMPC_ALMOST_EMPTY = 0x00000200UL, //  "   "   "   almost empty
    FROMPC_ALMOST_FULL  = 0x00000400UL, //  "   "   "   almost full
    FROMPC_FULL         = 0x00000800UL, //  "   "   "   full
    // Magic bits in the UPE word count (m_lSendCount) register ...
    FORCE_EXCEPTION     = 0x01000000UL, // force a drive exception error
    // VHDL type codes (from the lVHDL field in the memory map) ...
    TYPE_DISK           = 0,            // MASSBUS disk emulation
    TYPE_TAPE           = 1,            // MASSBUS tape emulation
    TYPE_MEIS           = 2,            // MASSBUS NI   emulation
    // Magic bits in the lDrivesAttached register ...
    MASSBUS_FAIL        = 0x00000100UL, // MASSBUS cable disconnected
  };

  // Magic routines to decode the UPE command FIFO ...
public:
  static bool IsCommandValid (uint32_t l)  {return ISSET(l, VALID);}
  static bool IsDataValid    (uint32_t l)  {return ISSET(l, VALID);}
  static bool IsEndofBlock   (uint32_t l)  {return ISSET(l, END_OF_BLOCK);}
  static uint32_t ExtractCommand  (uint32_t cmd)  {return    cmd     & 0777777;}
  static uint32_t ExtractUnit     (uint32_t cmd)  {return  ((cmd >> 16) & 007);}
  static uint32_t ExtractRegister (uint32_t cmd)  {return  ((cmd >> 19) & 037);}

  // Public properties for the UPE ...
public:
  //   Return the address of the shared memory window. Note that the underlying
  // CUPE code takes care of the window - all we have to do is cast it to the
  // correct type!
  volatile SHARED_MEMORY *GetWindow() const {return (volatile SHARED_MEMORY *) CUPE::GetWindow();}
  // Get or set the VHDL type and/or version number ...
  //   NOTE - setting these values is only possible for offline interfaces!
  //   For online interfaces, the value is determined by the VHDL code loaded
  //   into the FPGA and there's no way we can change that!
  uint16_t GetRevision() const {return IsOpen() ? LOWORD(GetWindow()->lVHDL) : 0;}
  uint8_t  GetVHDLtype() const {return IsOpen() ? HIWORD(GetWindow()->lVHDL) & 7 : 0;}
  void SetVHDLtype (uint8_t  nType) {
    if (IsOffline()) GetWindow()->lVHDL = MKLONG((nType & 7), LOWORD(GetWindow()->lVHDL));
  }
  // Get or set the owner PID of this UPE ...
//virtual PROCESS_ID GetOwner() const  {return IsOpen() ? GetWindow()->lOwner : 0;}
//virtual void SetOwner(PROCESS_ID nPID)  {if (IsOpen()) GetWindow()->lOwner = nPID;}
  // Get or set the MASSBUS speed parameters ...
  uint8_t GetDataClock() const {assert(IsOpen());  return LOBYTE(GetWindow()->lDataClock);}
  uint8_t GetTransferDelay() const {assert(IsOpen());  return LOBYTE(GetWindow()->lTransferDelay);}
  void SetDataClock (uint8_t bClock) {assert(IsOpen());  GetWindow()->lDataClock = bClock;}
  void SetTransferDelay (uint8_t bDelay) {assert(IsOpen());  GetWindow()->lTransferDelay = bDelay;}
  // Return TRUE if the MASSBUS cable is connected
  bool IsCableConnected() const {
    assert(IsOpen());  return !ISSET(GetWindow()->lDrivesAttached, MASSBUS_FAIL);
  }
  // Return the VHDL type (e.g. disk, tape, MEIS, etc) ...
  bool IsDisk() const {return (GetVHDLtype() == TYPE_DISK);}
  bool IsTape() const {return (GetVHDLtype() == TYPE_TAPE);}
  bool IsNI()   const {return (GetVHDLtype() == TYPE_MEIS);}

  // CUPE constructor and destructor ...
public:
  CDECUPE (const PLX_DEVICE_KEY *pplxKey) : CUPE(pplxKey) {};
  //   Note that this destructor should explicitly Close() the UPE if it has
  // been opened.  Why?  It's complicated, but the comments in the CUPE::Close
  // method will tell you more...
  virtual ~CDECUPE() {if (IsOpen())  Close();};
  // Initialize the DEC specific state ...
  virtual bool Initialize();

  // Disallow copy and assignment operations with CUPE objects...
  //   There's locally allocated data in m_pplxData and m_pUPE, and it's not
  // worth the trouble of actually making this work correctly!
private:
//CDECUPE(const CDECUPE&) = delete;
//CDECUPE& operator= (const CDECUPE&) = delete;

  // Public UPE MASSBUS functions ...
public:
  // Read, write, set, clear and complement bits in MASSBUS registers ...
  uint16_t ReadMBR (uint8_t nUnit, uint8_t nRegister) const;
  void WriteMBR (uint8_t nUnit, uint8_t nRegister, uint16_t wValue);
  uint16_t ClearBitMBR (uint8_t nUnit, uint8_t nRegister, uint16_t wMask);
  uint16_t SetBitMBR (uint8_t nUnit, uint8_t nRegister, uint16_t wMask);
  uint16_t ToggleBitMBR(uint8_t nUnit, uint8_t nregister, uint16_t wMask);
  // Wait for a command in the command FIFO ...
  uint32_t WaitCommand (uint32_t lTimeout=COMMAND_TIMEOUT);
  // Special values returned by WaitCommand() for timeout and errors.
  enum {TIMEOUT = 0x00000000UL, ERROR = 0x0FFFFFFF};
  // Read and write blocks of data to/from the FIFO ...
  bool ReadData (uint32_t alData[], uint32_t clData);
  bool WriteData (const uint32_t alData[], uint32_t clData, bool fException = false);
  void EmptyTransfer (bool fException = false);
  // Tell the FPGA about mapped drives and emulated geometry ...
  void SetDrivesAttached (uint32_t nMap);
  void SetGeometry (uint8_t nUnit, uint16_t nCylinders, uint8_t nHeads, uint8_t nSectors);

  // Private methods ...
private:

  // Private member data ...
private:
};


//   This little routine is a CDECUPE "object factory".  All it does is to
// create new instances of CDECUPE objects - it's used by the library CUPEs
// collection class to create new instances of application specific UPE
// objects...
extern "C" CUPE *NewDECUPE (const PLX_DEVICE_KEY *pplxKey);

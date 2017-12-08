//++
// MASSBUS.h -> MASSBUS bits and register definitions
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
//   This header defines mnemonics for various MASSBUS registers and bits.
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
//  5-DEC-13  RLA   Change RPSN to 010 - Apparently the DEC manuals are wrong!
// 16-OCT-14  RLA   Do away with RPDC and RPDA masks.
// 18-Nov-14  RLA   Add TM78/TU78 tape definitions.
//--
#pragma once

//   What follows is a bunch of MASSBUS register and bit definitions.  The
// astute reader will notice immediately that these are not inside a class.
// That's probably not ideal, but these mnemonics are used by several classes
// (CMBA, CDiskDrive, CTapeDrive, etc) and always prefixing them with a class
// name gets old.
//
//  Another issue is that, sadly, the RP and RM MASSBUS controllers ARE NOT
// the same.  Many of the registers and bits are common, but many are not and
// there are significant differences.  This is why you'll notice so many names
// prefixed with either RP... or RM...

// MASSBUS disk register addresses ...
#define RPCR	           000  // control and command register
#define RPDS	           001  // drive status
#define RPER1	           002  // primary error register
#define RPMR	           003  // primary maintenance register
#define RPAS	           004  // attention summary
#define RPDA	           005  // desired sector/track address
#define RPDT	           006  // drive type
#define RPLA	           007  // look ahead
#define RPSN	           010  // serial number
//#define RPER2              010  // secondary error register
#define RPOF	           011  // offset
#define RPDC	           012  // desired cylinder address
#define RPCC	           013  // current cylinder address
#define RPER3              015  // secondary error register
#define RPEC1	           016  // ECC position
#define RPEC2	           017  // ECC pattern

// MASSBUS disk commands ...
//   Note that MASSBUS commands are six bits, however the LSB is the "GO"
// bit and is always one.  These cconstants are defined to include all six
// bits and hence they're always odd!
#define RPCMD_MASK         077  // mask for command bits in RPCR
#define RPCMD_NOP	   001  // no operation
#define RPCMD_UNLOAD       003  // unload
#define RPCMD_SEEK         005  // seek
#define RPCMD_RECAL        007  // recalibrate
#define RPCMD_CLEAR        011  // drive clear
#define RPCMD_RELEASE      013  // port release
#define RPCMD_OFFSET       015  // offset
#define RPCMD_RETURN       017  // return to center
#define RPCMD_READIN       021  // read-in preset
#define RPCMD_PACKACK      023  // pack acknowledge
#define RPCMD_SEARCH       031  // search for a sector
#define RPCMD_WCHECK       051  // write check
#define RPCMD_WHCHECK      053  // write check w/header
#define RPCMD_WRITE        061  // write
#define RPCMD_WHEADER      063  // write w/header
#define RPCMD_READ         071  // read
#define RPCMD_RHEADER      073  // read w/header

// MASSBUS disk status bits (RPDS register) ...
#define RPDS_ATA       0100000  // attention active
#define RPDS_ERR       0040000  // OR of all error bits
#define RPDS_PIP       0020000  // position in progress
#define RPDS_MOL       0010000  // medium online
#define RPDS_WLK       0004000  // write locked
#define RPDS_LBT       0002000  // last block transferred
#define RPDS_PGM       0001000  // programmable (for dual port)
#define RPDS_DPR       0000400  // drive present
#define RPDS_DRY       0000200  // drive ready
#define RPDS_VV        0000100  // volume valid
 
// MASSBUS sector, track and cylinder masks (RPDA and RPDC registers) ...
// Note that these are no longer used - see DiskDrive.hpp ...
//#define RPDA_TRKMSK        037  // track address is 5 bits
//#define RPDA_SECMSK        077  // sector address is 6 bits (for RP07!)
//#define RPDC_CYLMSK      01777  // cylinder address is 10 bits

// MASSBUS disk drive type bits (RPDT register) ...
#define RPDT_NBA       0100000  // not block addressed
#define RPDT_TAP       0040000  // tape drive
#define RPDT_MOH       0020000  // moving head drive
#define RPDT_DRQ       0004000  // drive request required
#define RPDT_TYPE      0000777  // drive type code

// MASSBUS disk format bits (RPOF register) ...
#define RPOF_SNGCG     0100000  // sign change flag
#define RPOF_FMT22     0010000  // 18 bit format flag
#define RPOF_ECI       0004000  // ECC inhibit
#define RPOF_HCI       0002000  // header compare inhibit
#define RPOF_OFFSET    0000377  // offset field

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// MASSBUS tape register addresses ...
#define TMDCR	           000  // data transfer control register
#define TMDIR	           001  // data transfer interrupt register
#define TMTCR	           002  // tape control register
#define TMMR1	           003  // maintenance register
#define TMAS	           004  // attention summary
#define TMBCR	           005  // byte count register
#define TMDT	           006  // drive type register
#define TMUS	           007  // unit status register
#define TMSN	           010  // serial number (BCD)
#define TMMR2	           011  // maintenance register
#define TMMR3	           012  // maintenance register
#define TMMIR	           013  // motion interrupt register
#define TMMCR0             014  // motion control register unit 0
#define TMMCR1             015  //   "     "   "   "   "     "  1
#define TMMCR2             016  //   "     "   "   "   "     "  2
#define TMMCR3             017  //   "     "   "   "   "     "  3
#define TMIAR              020  // internal microprocessor address
#define TMHCR              021  // hardware control register

// MASSBUS tape function codes ...
//   Note that in the case of tape, there are actually two distinct sets of
// function codes - motion codes and data transfer codes. These actually appear
// in different registers - motion codes in TMMCRn and transfer codes in TMDCR)
// but the numeric values are non-overlapping so we don't differentiate them
// here.
//
//   Also note that, just like disk fucntions, these definitions all include
// the GO bit and therefore should always be odd!
#define TMCMD_M_MASK        077 // mask for command bits in TMMCR/TMDCR
#define TMCMD_V_MASK          0 // ...
#define TMCMD_NOP	    003 // no operation
#define TMCMD_UNLOAD        005 // unload tape
#define TMCMD_REWIND        007 // rewind to load point
#define TMCMD_SENSE         011 // read status information
#define TMCMD_DSE           013 // data security erase
#define TMCMD_WTM_PE        015 // write tape mark (1600 BPI)
#define TMCMD_WTM_GCR       017 //   "    "    "   (6250 BPI)
#define TMCMD_SP_FWD_REC    021 // space forward record
#define TMCMD_SP_REV_REC    023 //   "   reverse   "
#define TMCMD_SP_FWD_FILE   025 // space forward file
#define TMCMD_SP_REV_FILE   027 //   "   reverse   "
#define TMCMD_SP_FWD_EITHER 031 // space forward either record or file
#define TMCMD_SP_REV_EITHER 033 //   "   reverse   "       "   "    "
#define TMCMD_ERG_PE        035 // erase record gap (1600 BPI)
#define TMCMD_ERG_GCR       037 //   "      "    "  (6250 BPI)
#define TMCMD_CLOSE_PE      041 // close file (1600 BPI)
#define TMCMD_CLOSE_GCR     043 //   "    "   (6250 BPI)
#define TMCMD_SP_LEOT       045 // space forward to logical EOT
#define TMCMD_SP_FILE_LEOT  047 // space forward one for or to logical EOT
#define TMCMD_WRT_CK_FWD    051 // write check forward
#define TMCMD_WRT_CK_REV    057 // write check reverse
#define TMCMD_WRT_PE        061 // write forward (1600 BPI)
#define TMCMD_WRT_GCR       063 //   "      "    (6250 BPI)
#define TMCMD_RD_FWD        071 // read forward
#define TMCMD_RD_EXSNS      073 // read error log (extended sense) data
#define TMCMD_RD_REV        077 // read reverse

// Other bits in the command register ...
#define TMCMD_DVA        004000 // drive/formatter available

// MASSBUS tape interrupt register bits (TMDIR and TMMIR registers) ...
#define TMDIR_M_FC      0176000 // transfer failure code
#define TMDIR_V_FC           10 // ...
#define TMDIR_DPR       0000400 // drive/formatter present (always 1!)
#define TMDIR_M_IC      0000077 // transfer interrupt code
#define TMDIR_V_IC            0 // ...
#define TMMIR_M_FC      0176000 // motion failure code
#define TMMIR_V_FC           10 // ...
#define TMMIR_M_UNIT    0001400 // motion interrupt unit
#define TMMIR_V_UNIT          8 // ...
#define TMMIR_M_IC      0000077 // motion interrupt code
#define TMMIR_V_IC            0 // ...
#define MK_TMDIR(ic,fc)   ((uint16_t) ((((fc) << TMDIR_V_FC) & TMDIR_M_FC) | (((ic) << TMDIR_V_IC) & TMDIR_M_IC)))
#define MK_TMMIR(ic,u,fc) ((uint16_t) ((((fc) << TMMIR_V_FC) & TMMIR_M_FC) | (((ic) << TMMIR_V_IC) & TMMIR_M_IC) | (((u) << TMMIR_V_UNIT) & TMMIR_M_UNIT)))

// MASSBUS tape interrupt codes ...
//   Like the TM commands, these codes can appear in two different registers -
// either in the TMDIR in response to a data transfer command, or the TMMIR
// in response to a motion command.  But once again the numeric values don't
// overlap, so we can group them all together here.  It's up to the code to
// use the appropriately!
#define TMIC_DONE           001 // attention active
#define TMIC_TAPE_MARK      002 // found tape mark
#define TMIC_BOT            003 // found beginning of tape marker
#define TMIC_EOT            004 // found end of tape marker
#define TMIC_LEOT           005 // found logical EOT (two tape marks)
#define TMIC_NOP            006 // NOP command completed
#define TMIC_REWINDING      007 // rewind in progress
#define TMIC_FILE_PROTECT   010 // no ring (write protected tape)
#define TMIC_NOT_READY      011 // drive not ready
#define TMIC_NOT_AVAIL      012 // drive not available
#define TMIC_OFFLINE        013 // drive offline
#define TMIC_NOT_EXEC       014 // command not executable
#define TMIC_NOT_CAPABLE    015 // unsupported density or format
#define TMIC_ONLINE         017 // drive has come online
#define TMIC_LONG_RECORD    020 // record longer than byte count
#define TMIC_SHORT_RECORD   021 //   "    shorter  "    "    "
#define TMIC_RETRY          022 // read failure (software should retry)
#define TMIC_READ_OPP       023 // read error (software should read in reverse)
#define TMIC_UNREADABLE     024 // read failure (do not retry)
#define TMIC_READ_ERROR     025 // read failure (undefined)
#define TMIC_EOT_ERROR      026 // EOT marker found while writing
#define TMIC_BAD_TAPE       027 // tape position lost
#define TMIC_TM_FAULT_A     030 // TM78 hardware fault
#define TMIC_TU_FAULT       031 // TU78 hardware fault
#define TMIC_TM_FAULT_B     032 // TM78 hardware fault
#define TMIC_MB_FAULT       034 // MASSBUS fault

// MASSBUS tape failure codes ...
#define TMFC_NONE           000 // none
 
// MASSBUS tape drive type bits (TMDT register) ...
#define TMDT_NBA       0100000  // not block addressed
#define TMDT_TAP       0040000  // tape drive
#define TMDT_TM78      0142000  // magic bits for TM78 (NBA+TAPE+???)
#define TMDT_M_TYPE    0000777  // drive type code
#define TMDT_V_TYPE          0  // ...
#define   TMDT_TU78       0101  // drive type for a TU78

// MASSBUS tape status bits (TMUS register) ...
#define TMUS_RDY      0100000   // unit ready
#define TMUS_PRES     0040000   // unit present
#define TMUS_ONL      0020000   // unit online
#define TMUS_REW      0010000   // reqind in progress
#define TMUS_PE       0004000   // 1600 BPI
#define TMUS_BOT      0002000   // tape positioned at load point
#define TMUS_EOT      0001000   //   "    "    "   "   end point
#define TMUS_FPT      0000400   // file protect
#define TMUS_AVAIL    0000200   // unit available to this MASSBUS port
#define TMUS_SHR      0000100   //   "    "    "  "  both  "   "  ports
#define TMUS_MAINT    0000040   // unit is in maintenance mode
#define TMUS_DSE      0000020   // data security erase in progress

// MASSBUS tape control bits (TMTCR register) ...
#define TMTCR_SER          0100000 // 
#define TMTCR_M_FORMAT     0070000 // assembly format mask
#define TMTCR_V_FORMAT          12 // ...
#define TMTCR_M_SKIP_COUNT 0007400 // byte skip count
#define TMTCR_V_SKIP_COUNT       8 // ...
#define TMTCR_M_REC_COUNT  0000374 // record count
#define TMTCR_V_REC_COUNT        2 // ...
#define TMTCR_M_CMD_ADDR   0000003 // unit number
#define TMTCR_V_CMD_ADDR         0 // ...
      
// MASSBUS tape assembly mode bits (TMTCR register, TMTC_FORMAT field) ...
#define TMAM_11_NORMAL        0 // PDP-11 native mode
#define TMAM_15_NORMAL        1 // PDP-15 native mode
#define TMAM_10_COMPATIBLE    2 // PDP-10 industry compatible mode
#define TMAM_10_CORE_DUMP     3 // PDP-10 native RIM mode
#define TMAM_10_HD_COMPATIBLE 4 // PDP-10 high density industry compatible
#define TMAM_IMAGE            5 // image mode
#define TMAM_10_HD_DUMP       6 // PDP-10 high density native mode

// MASSBUS hardware control register magic bits (TMHCR, register 21) ...
#define TMHCR_CLEAR      040000 // master clear of the TM78

// MASSBUS TM78 extended sense data definitions ...
#define TMES_LENGTH          30 // length of extended sense buffer (halfwords)


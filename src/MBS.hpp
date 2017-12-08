//++
// mbs.hpp -> Global Declarations for the MASSBUS Server project
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
//   This file contains global constants (e.g. the version number of the
// MASSBUS server) and universal macros (e.g. make nibbles, words, bytes,
// etc).  
//
// Bob Armstrong <bob@jfcl.com>   [1-OCT-2013]
//
// REVISION HISTORY:
//  1-OCT-13  RLA   New file.
//--
#pragma once

// Global complilation parameters ...
#define PROGRAM   "mbs"       // used in prompts and error messages 
#define MBSVER      53        // version number of this release

//   All RP and RM disks have, so far as I know, a sector size of exactly
// 256 "words", but the catch is that a word may be either 16 bits (for VAX
// and PDP11 systems) or 18 bits (for DECsystem-10 and DECSYSTEM-20 systems).
// In the former case the host sees the sector size as 512 bytes (VAX and PDP11
// systems) and in the latter case it's 128 words (36 bit words, for -10 and
// -20 systems). 
#define SECTOR_SIZE    256   // and either way it's 256 16/18 bit words

// Assemble and disassemble 36 bit words and 18 bit halfwords ...
//   Note that internally we store 36 bit words in a uint64 type (i.g. 8 bytes
// or a quadword) - that's a bit wasteful, but it's convenient because it's
// the same format used by simh.
#define MASK18(x)       ((uint32_t) ((x) &       0777777UL ))
#define MASK36(x)       ((uint64_t) ((x) & 0777777777777ULL))
#define RH36(x) 	((uint32_t) ( (x) & 0000000777777ULL)       )
#define LH36(x)		((uint32_t) (((x) & 0777777000000ULL) >> 18))
#define MK36(h,l)	((uint64_t) (( (uint64_t) ((h) & 0777777ULL) << 18) | (uint64_t) ((l) & 0777777ULL)))

//   These globals are declarated in MBS.cpp and are used more or less
// everywhere in the MBS program ...
//extern class CLog     *g_pLog;      // message logging object (including console!)
extern class CUPEs      *g_pUPEs;     // collection of all known UPEs on this PC
extern class CMBAs      *g_pMBAs;     // collection of all MASSBUS adapters created



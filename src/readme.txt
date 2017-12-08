README for MBS v53 and UPELIB v48 Release
Bob Armstrong  <bob@jfcl.com>  [8-DEC-2017]


1. Introduction and Overview
============================

   MBS is the software component of the Living Computer Museum's MASSBUS
simulator project.  The source code is split into two distinct parts - MBS is
the specific to the MASSBUS simulator, and UPELIB ("Universal Peripheral
Emulator Library") is code which is used by MBS but also shared by other LCM
projects.

  The hardware component of MBS is a MESA Electronics 5i22 PCI FPGA board

  http://store.mesanet.com/index.php?route=product/product&product_id=63

  The FPGA on this board requires programming and you will need the VHDL source
code for that as well as the Xilinx tool chain to generate the necessary
bitstream.  And of course you will need a suitable interface to connect the I/Os
on the MESA card to the physical MASSBUS cable.  The FPGA source code as well
as the schematics and layout for the interface board are included in this
release.

1.1 What's Emulated
-------------------

  MBS is currently able to emulate -

  * RP04, RP06, RP07, RM03, RM05, and RM80 disk drives with either 16 bit or
    18 bit packs (so MBS works for both 36 bit and 16/32 bit hardware)
  * the TU78 tape transport with a TM78 formatter

  Note that the FPGA bitstream required for tape emulation differs from the one
used for disks, and emulating both tape and disk drives will require at least
two MESA cards and two MASSBUS interfaces.

  The container files used by MBS for tape images use the standard TAP file
format, and the disk image files are just a linear array of sectors images.
Both of these file formats are compatible with simh and other simulators.

1.2 What's Not
--------------

  Many things are only partially emulated or are not emulated at all -

  * Only the TM78 functions that appear to actually be used by TOPS10 or TOPS20
    are actually emulated.  See the comments at the top of TapeDrive.cpp for a
    more complete list.
  * Dual ported drives (disk or tape) are not simulated.  In particular, this
    means that the CFE on a KL will require its own dedicated disk drive.
  * There are hooks in the code for emulation of the TM03 formatter and TU45 or
    TU77 transports, but this code is incomplete.
  * There are hooks in the code for emulating the MEIS MASSBUS Ethernet
    interface, but this code is incomplete.
  * Probably a lot of other stuff too, that I've forgotten...


2.0 Requirements
================

  MBS works on either Ubuntu/Debian Linux or Windows.  The software is written
in C++ and for Windows Visual Studio 2013 was used to build the executable. 
On Linux the Code::Blocks IDE was used, however there is an untested Makefile
which should allow you to compile MBS directly using gcc and no IDE.

  For either platform you will also need a copy of the PLX library to build MBS.
The PLX9054 is the PCI interface chip used on the MESA card, and the chip
manufacturer provides a library for accessing the card and the FPGA.

	https://docs.broadcom.com/docs/SDK-Complete-Package


3.0 Using MBS
=============

  MBS uses a command line interface - there is no GUI - and it runs in a normal
console/terminal window.  It also works quite well with ssh or the "screen"
program on Linux.  It has a scripting feature built in, and generally you would
create one or more scripts that define your emulation configuration.  These
scripts would then be run automatically when you start MBS.  It also has some
extensive logging capabilities that you can use to monitor operation or track
down hardware problems.

  When using MBS the general idea is to first create a simulated MASSBUS
instance and connect it to a specific MESA FPGA card.  For example, the command

	 mbs> create A disk A:0.0 /configuration=MDE2ATOP.BIT

would create a simulated MASSBUS called "A", connect it to the FPGA card with
PCI BDF address 0A:00.0, and then download the FPGA bitstream from MDE2ATOP.BIT.

  Next you would connect one or more simulated drives to this MASSBUS

	 mbs> connect A0 RP04 /serial=1234 /alias=RPA0
	 mbs> connect A1 RP06 /serial=5678 /alias=RPA1

This would connect one emulated RP04 drive with serial number 1234 as unit 0 on
MASSBUS A, and one RP06 drive, serial 5678, as unit 1.  The alias names are
arbitrary and can be used in subsequent MBS commands to refer to this specific
drive instance.

  Once connected, a drive is visible to the MASSBUS and the host, but is offline
(spun down) until you attach it to an image file.  For example the command

	 mbs> attach RPA0 tops10.rp04 /bits=18 /write /online 

would attach the PC image file "tops10.rp04" to RPA0 with a 18 bit format pack,
write enable it, a then spin it up (place it online).  At this point RPA0 would
be accessible to the host system.

  To see the current status you could give the MBS "show all" command

	mbs>show all

	MASSBUS Disk and Tape Emulator v53 UPE Library v48

	Default console message level set to TRACE
	Default log file message level set to TRACE
	Logging to file D:\Gizmos\Consulting\LCM\MBS\examples\test.log


	UPE     Bus Slot   PLX     VHDL TDLY DCLK Type  Status    MBA  Units
	------- --- ---- --------  ---- ---- ---- ----  --------  ---  -----
	0A:00.0   A    0  9054 AC  1234 0x80 0x80 DISK  ONLINE     A    1/1

	1 UPE/FPGA boards connected


	Unit Alias       Type    S/N   Status      Port  Image File
	---- ----------  ----  ------  ----------  ----  ----------------------------
	 A0  RPA0        RP04    1234  ONL RW 18b  A/B   D:\Gizmos\...\tops10.rp04
	 A1  RPA1        RP06    5678  OFL RW 16b  A/B

	2 drives connected

	No command aliases defined


  MBS has many other commands and this very brief introduction.  In MBS you can
always type "help" to get a list of all commands, and "help <command>" (where
"<command>" is the name of any command) to get a list of the arguments and 
options for that command.


4.0 Additional Reading Materials
================================

  An MBS specification document is also included which gives more details about
MBS operation and commands, HOWEVER be aware that this specification was written
before the software was created and it HAS NOT been kept up to date.  Much of
what it says is still true, however it is far behind the current software,
especially in the list of commands.  The source code file UserInterface.cpp has
a bunch of notes in the comments at the top that describe some of the CLI
changes that have occurred since the specification was written.

  The Bitsavers DEC archive at http://www.bitsavers.org/pdf/dec/ has just an
enormous collection of DEC documents, including the MASSBUS specification and
technical manuals for the various tape and disk drive models.  DEC hardware
tends to be pretty well documented and there wasn't a lot of "secret" stuff that
needed to be reverse engineered for MBS.
         
5.0 Known Issues
================

  TBA.


6.0 Reporting Bugs
==================

  TBA.

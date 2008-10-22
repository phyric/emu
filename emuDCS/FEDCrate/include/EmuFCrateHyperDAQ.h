/*****************************************************************************\
* $Id: EmuFCrateHyperDAQ.h,v 3.53 2008/10/22 20:23:57 paste Exp $
*
* $Log: EmuFCrateHyperDAQ.h,v $
* Revision 3.53  2008/10/22 20:23:57  paste
* Fixes for random FED software crashes attempted.  DCC communication and display reverted to ancient (pointer-based communication) version at the request of Jianhui.
*
* Revision 3.52  2008/10/04 18:44:04  paste
* Fixed bugs in DCC firmware loading, altered locations of files and updated javascript/css to conform to WC3 XHTML standards.
*
* Revision 3.51  2008/09/22 14:31:53  paste
* /tmp/cvsY7EjxV
*
* Revision 3.50  2008/08/25 12:25:49  paste
* Major updates to VMEController/VMEModule handling of CAEN instructions.  Also, added version file for future RPMs.
*
* Revision 3.49  2008/08/15 10:40:20  paste
* Working on fixing CAEN controller opening problems
*
* Revision 3.47  2008/08/15 08:35:50  paste
* Massive update to finalize namespace introduction and to clean up stale log messages in the code.
*
*
\*****************************************************************************/
/** XDAQ-based interface for controlling and debugging FED Crates.
*	Please, PLEASE leave the editing of this file to the experts!
*
*	@author Jason Gilmore     <gilmore@mps.ohio-state.edu>
*	@author Jianhui Gu        <gujh@mps.ohio-state.edu>
*	@author Stan Durkin       <durkin@mps.ohio-state.edu>
*	@author Phillip Killewald <paste@mps.ohio-state.edu>
**/

#ifndef __EMUFCRATEHYPERDAQ_H__
#define __EMUFCRATEHYPERDAQ_H__

#include <string>
#include <vector>
#include <utility> // pair

#include "xdaq/Application.h"
#include "xgi/Utils.h"
#include "xgi/Method.h"
#include "xdata/String.h"

#include "EmuFEDApplication.h"
#include "FEDException.h"
#include "FEDCrate.h"

class EmuFCrateHyperDAQ: public EmuFEDApplication
{
private:

	xdata::String svfFile_;
	xdata::String xmlFile_;
	std::vector<emu::fed::FEDCrate*> crateVector;
	std::string DDUBoardID_[9];
	std::string DCCBoardID_[9];
	/*
	int DCC_ratemon[50][12];
	int DCC_ratemon_cnt;
	int DCC_ratemon_ch;
	*/
	std::string fcState_;

public:

	XDAQ_INSTANTIATOR();

	/** Default Constructor **/
	EmuFCrateHyperDAQ(xdaq::ApplicationStub * s);


	/** Default page when XDAQ loads.  Sets configuration if required and
	*	bounces the user to the main page.
	*
	*	@param *in is a pointer to a standard xgi input object (for passing
	*	things like POST and GET variables to the function.)
	*	@param *out is the xgi output (basically, a stream that outputs to the
	*	browser window.
	*
	*	@throws xgi::exception::Exception for great justice!
	*
	*	@note The *in and *out parameters and the xgi::exception::Exception
	*	throw are common to all xgi-bound functions, and will herein not be
	*	included in the documentation.
	**/
	void Default(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Page listing all the DDUs and the DCC along with their various options.
	*
	**/
	void mainPage(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Page listing all the configuration options. **/
	void configurePage(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Sets to which crate this EmuFCrateHyperDAQ application is talking.
	*	The original idea was to have each EmuFCrateHyperDAQ (and each FCrate)
	*	application talk to two different crates, selectable in some way.  This
	*	was because the original design called for one computer to talk to two
	*	FED crates, and it was seen as sort of rediculous to have two seperate
	*	processes to accomplish this.  However, we may eventually switch back to
	*	the one-crate-per-application model, which would instantly depricate
	*	this function.
	**/
	void setCrate(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Loads the configuration file from raw textarea input. **/
	void setRawConfFile(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Sets the configuration file from a text input std::string (should point to a
	*	file on the server-side disk that XDAQ can access.)
	**/
	void setConfFile(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Uploads the configuration file from a file input (should point to a
	*	file on the client-side disk.)
	**/
	void UploadConfFile(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Actually configures the application to talk to the apporpriate crate
	*	(crates?).
	*
	**/
	void Configuring();

	/** Load the firmware to the DDUs.  This is a universal loader, and reads
	*	from the xgi input which firmwares to load, etc.  Perhaps this can be
	*	broken into multiple methods?
	**/
	/*
	void DDUFirmware(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);
	*/

	/** Page listing the DDU firmware broadcast options. **/
	void DDUBroadcast(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Uploads the broadcastable firmware from the client computer to the
	*	server computer.
	**/
	void DDULoadBroadcast(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Broadcasts firmware to all DDUs simultaneously. **/
	void DDUSendBroadcast(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Resets a particular Crate either cheating by using TTC, or the "real"
	*	way, by requesting a global reset.  The global reset method may or may
	*	not work ever.
	**/
	void DDUReset(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);
		
	/** Resets a particular Crate either cheating by using TTC, or the "real"
	*	way, by requesting a global reset.  The global reset method may or may
	*	not work ever.
	**/
	void DCCReset(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Shows General DDU debugging information. **/
	void DDUDebug(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Shows Expert DDU debugging information and commands. **/
	void DDUExpert(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Shows the DDU InFPGA 0 and 1 status page and various communication options. **/
	void InFpga(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Traps and displays INFPGA debugging information (?)
	*
	*	@param lcode An array corresponding to the status of the INFPGAs (?)
	**/
	//void DDUinTrapDecode(xgi::Input * in, xgi::Output * out,  unsigned long int lcode[10])
		//throw (xgi::exception::Exception);

	/** Page for VME parallel register reading/writing. **/
	void VMEPARA(xgi::Input * in, xgi::Output * out )
    	throw (xgi::exception::Exception);

	/** Page for VEM Serial register reading/writing. **/
	void VMESERI(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Load data into the DDU from text (?) **/
	void DDUTextLoad(xgi::Input * in, xgi::Output * out )
    	throw (xgi::exception::Exception);

	/** Starts/stops/monitors the IRQ interrupt handler. **/
	void VMEIntIRQ(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Page listing the DCC firmware broadcast options. **/
	//void DCCBroadcast(xgi::Input *in, xgi::Output *out)
	//	throw (xgi::exception::Exception);
	
	/** Uploads the broadcastable firmware from the client computer to the
	*	server computer.
	**/
	//void DCCLoadBroadcast(xgi::Input *in, xgi::Output *out)
	//	throw (xgi::exception::Exception);
	
	/** Broadcasts firmware to all DCCs simultaneously. **/
	//void DCCSendBroadcast(xgi::Input *in, xgi::Output *out)
	//	throw (xgi::exception::Exception);

	/** Page for DCC firmware loading/checking. **/
	void DCCFirmware(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Routine for actually loading the DCC firmware. **/
	void DCCLoadFirmware(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Debugging routine for XML configuration file loading (?) **/
	void LoadXMLconf(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Page for reading general debug information from the DCC. **/
	//void DCCDebug(xgi::Input * in, xgi::Output * out )
	//	throw (xgi::exception::Exception);

	/** Page for reading and setting expert registers on the DCC. **/
	//void DCCExpert(xgi::Input * in, xgi::Output * out )
	//	throw (xgi::exception::Exception);

	/** Page for sending commands to the DCC. **/
	void DCCCommands(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Load data into the DCC from text (?) **/
	void DCCTextLoad(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);	

	/** Hard-reset the DCC to reset and load firmware. **/
	void DCCFirmwareReset(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** Live DDU voltage monitoring page. **/
	void DDUVoltMon(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);

	/** HTTP redirection page using status code 303. **/
	void webRedirect(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception);

	/** Live DCC data rate monitoring. **/
	/*
	void DCCRateMon(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);
	*/
	/** Display a static page with a graph for the DCC data rate off the first
	*	S-Link channel.
	**/
	/*
	void getDataDCCRate0(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);
	*/
	/** Display a static page with a graph for the DCC data rate off the second
	*	S-Link channel.
	**/
	/*
	void getDataDCCRate1(xgi::Input * in, xgi::Output * out )
		throw (xgi::exception::Exception);
	*/
	/** Redirect the user so that the form information is cleared from the URL.
	*
	*	@author Phillip Killewald
	**/
	void webRedirect(xgi::Output *out, std::string location = "");

	/** My patented Select-a-crate/board
	*
	*	@author Phillip Killewald
	**/
	std::string selectACrate(std::string location, std::string what, unsigned int index, unsigned int crateIndex = 0);

	/** A way to easily get the selected crate from the CGI variables
	 *
	 * @param cgi The Cgicc object which to parse.
	 * @returns a pair of the cgi integer value parsed from cgi and a
	 * pointer to the target FEDCrate object
	 * 
	 * @author Phillip Killewald
	 **/
	std::pair<unsigned int, emu::fed::FEDCrate *> getCGICrate(cgicc::Cgicc cgi)
		throw (emu::fed::FEDException);

	/** A way to easily get the selected DDU/DCC from the CGI variables
	 *
	 * @param cgi The Cgicc object which to parse.
	 * @returns a pair of the cgi integer value parsed from cgi and a
	 * pointer to the target VMEModule object
	 *
	 * @author Phillip Killewald
	 **/
	template<class T>
		std::pair<unsigned int, T *> getCGIBoard(cgicc::Cgicc cgi)
		throw (emu::fed::FEDException) {

			// Start with the crate
			std::pair<unsigned int, emu::fed::FEDCrate *> cratePair = getCGICrate(cgi);
			emu::fed::FEDCrate *crate = cratePair.second;

			// Now get the vector of boards.
			std::vector<T *> boardVector = crate->getBoards<T>();

			// Die if there are no boards from which to choose.
			if (boardVector.size() == 0) {
				XCEPT_RAISE(emu::fed::FEDException, "Cannot select a board when there are no boards to select!");
			}
			

			// Then get the board number
			cgicc::form_iterator name = cgi.getElement("board");
			unsigned int cgiBoard = 0;
			if(name != cgi.getElements().end()) {
				cgiBoard = cgi["board"]->getIntegerValue();
			}
			
			// Warn if the number from cgi is out of bounds of the board vector
			if (cgiBoard >= boardVector.size() || cgiBoard < 0) {
				LOG4CPLUS_WARN(getApplicationLogger(), "Board " << cgiBoard << " is out-of-bounds");
				cgiBoard = 0;
			}
			
			return std::pair<unsigned int, T *> (cgiBoard, boardVector[cgiBoard]);
		}

};

#endif

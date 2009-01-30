/*****************************************************************************\
* $Id: EmuFCrateManager.cc,v 1.28 2009/01/30 19:14:16 paste Exp $
*
* $Log: EmuFCrateManager.cc,v $
* Revision 1.28  2009/01/30 19:14:16  paste
* New emu::base namespace and emu::base::Supervised inheritance added.
*
* Revision 1.27  2009/01/29 15:31:24  paste
* Massive update to properly throw and catch exceptions, improve documentation, deploy new namespaces, and prepare for Sentinel messaging.
*
* Revision 1.26  2008/11/03 23:33:47  paste
* Modifications to fix "missing stylesheet/javascript" problem.
*
* Revision 1.25  2008/10/29 16:01:44  paste
* Updated interoperability with primative DCC commands, added new xdata variables for future use.
*
* Revision 1.24  2008/10/22 20:23:58  paste
* Fixes for random FED software crashes attempted.  DCC communication and display reverted to ancient (pointer-based communication) version at the request of Jianhui.
*
* Revision 1.23  2008/10/15 00:46:56  paste
* Attempt to solve certain crashes on Enable/Disable commands.
*
* Revision 1.22  2008/10/09 11:21:19  paste
* Attempt to fix DCC MPROM load.  Added debugging for "Global SOAP death" bug.  Changed the debugging interpretation of certain DCC registers.  Added inline SVG to EmuFCrateManager page for future GUI use.
*
* Revision 1.21  2008/10/04 18:44:06  paste
* Fixed bugs in DCC firmware loading, altered locations of files and updated javascript/css to conform to WC3 XHTML standards.
*
* Revision 1.20  2008/08/25 12:25:49  paste
* Major updates to VMEController/VMEModule handling of CAEN instructions.  Also, added version file for future RPMs.
*
* Revision 1.19  2008/08/18 08:30:15  paste
* Update to fix error propagation from IRQ threads to EmuFCrateManager.
*
* Revision 1.18  2008/08/15 08:35:51  paste
* Massive update to finalize namespace introduction and to clean up stale log messages in the code.
*
*
\*****************************************************************************/
#include "EmuFCrateManager.h"

#include <vector>
#include <sstream>

#include "xgi/Method.h"
#include "cgicc/HTMLClasses.h"
#include "xdaq2rc/RcmsStateNotifier.h"

XDAQ_INSTANTIATOR_IMPL(emu::fed::EmuFCrateManager);

emu::fed::EmuFCrateManager::EmuFCrateManager(xdaq::ApplicationStub *stub):
Application(stub),
soapConfigured_(false)
{
	// The name of the "endcap," which determines certain file names
	endcap_ = "Manager";

	// Variables that are to be made available to other applications
	xdata::InfoSpace *infoSpace = getApplicationInfoSpace();
	infoSpace->fireItemAvailable("ttsID", &ttsID_);
	infoSpace->fireItemAvailable("ttsBits", &ttsBits_);

	// HyperDAQ pages
	xgi::bind(this,&emu::fed::EmuFCrateManager::webDefault, "Default");

	// SOAP call-back functions which fire the transitions to the FSM
	BIND_DEFAULT_SOAP2FSM_ACTION(EmuFCrateManager, Configure);
	BIND_DEFAULT_SOAP2FSM_ACTION(EmuFCrateManager, Enable);
	BIND_DEFAULT_SOAP2FSM_ACTION(EmuFCrateManager, Disable);
	BIND_DEFAULT_SOAP2FSM_ACTION(EmuFCrateManager, Halt);

	// Other SOAP call-back functions
	xoap::bind(this, &emu::fed::EmuFCrateManager::onSetTTSBits, "SetTTSBits", XDAQ_NS_URI);

	// FSM state definitions and state-change call-back functions
	fsm_.addState('H', "Halted", this, &emu::fed::EmuFCrateManager::stateChanged);
	fsm_.addState('C', "Configured", this, &emu::fed::EmuFCrateManager::stateChanged);
	fsm_.addState('E', "Enabled", this, &emu::fed::EmuFCrateManager::stateChanged);
	fsm_.addState('U', "Undefined", this, &emu::fed::EmuFCrateManager::stateChanged);

	// FSM transition definitions
	fsm_.addStateTransition('H', 'C', "Configure", this, &emu::fed::EmuFCrateManager::configureAction); // valid
	fsm_.addStateTransition('C', 'C', "Configure", this, &emu::fed::EmuFCrateManager::configureAction); // valid
	fsm_.addStateTransition('E', 'C', "Configure", this, &emu::fed::EmuFCrateManager::configureAction); // invalid
	fsm_.addStateTransition('U', 'C', "Configure", this, &emu::fed::EmuFCrateManager::configureAction); // valid
	fsm_.addStateTransition('F', 'C', "Configure", this, &emu::fed::EmuFCrateManager::configureAction); // invalid

	fsm_.addStateTransition('H', 'C', "Disable", this, &emu::fed::EmuFCrateManager::disableAction); // invalid
	fsm_.addStateTransition('C', 'C', "Disable", this, &emu::fed::EmuFCrateManager::disableAction); // invalid
	fsm_.addStateTransition('E', 'C', "Disable", this, &emu::fed::EmuFCrateManager::disableAction); // valid
	fsm_.addStateTransition('U', 'C', "Disable", this, &emu::fed::EmuFCrateManager::disableAction); // invalid
	fsm_.addStateTransition('F', 'C', "Disable", this, &emu::fed::EmuFCrateManager::disableAction); // invalid
	
	fsm_.addStateTransition('H', 'E', "Enable", this, &emu::fed::EmuFCrateManager::enableAction); // invalid
	fsm_.addStateTransition('C', 'E', "Enable", this, &emu::fed::EmuFCrateManager::enableAction); // valid
	fsm_.addStateTransition('E', 'E', "Enable", this, &emu::fed::EmuFCrateManager::enableAction); // invalid
	fsm_.addStateTransition('U', 'E', "Enable", this, &emu::fed::EmuFCrateManager::enableAction); // invalid
	fsm_.addStateTransition('F', 'E', "Enable", this, &emu::fed::EmuFCrateManager::enableAction); // invalid
	
	fsm_.addStateTransition('H', 'H', "Halt", this, &emu::fed::EmuFCrateManager::haltAction); // valid
	fsm_.addStateTransition('C', 'H', "Halt", this, &emu::fed::EmuFCrateManager::haltAction); // valid
	fsm_.addStateTransition('E', 'H', "Halt", this, &emu::fed::EmuFCrateManager::haltAction); // valid
	fsm_.addStateTransition('U', 'H', "Halt", this, &emu::fed::EmuFCrateManager::haltAction); // valid
	fsm_.addStateTransition('F', 'H', "Halt", this, &emu::fed::EmuFCrateManager::haltAction); // valid

	fsm_.addStateTransition('H', 'U', "Unknown", this, &emu::fed::EmuFCrateManager::unknownAction); // invalid
	fsm_.addStateTransition('C', 'U', "Unknown", this, &emu::fed::EmuFCrateManager::unknownAction); // invalid
	fsm_.addStateTransition('E', 'U', "Unknown", this, &emu::fed::EmuFCrateManager::unknownAction); // invalid
	fsm_.addStateTransition('U', 'U', "Unknown", this, &emu::fed::EmuFCrateManager::unknownAction); // invalid
	fsm_.addStateTransition('F', 'U', "Unknown", this, &emu::fed::EmuFCrateManager::unknownAction); // invalid

	fsm_.setInitialState('H');
	fsm_.reset();

	state_ = fsm_.getStateName(fsm_.getCurrentState());

}


// HyperDAQ pages
void emu::fed::EmuFCrateManager::webDefault(xgi::Input *in, xgi::Output *out)
{
	
	// This header manipulation will make inline SVG possible, I think.
	/*
	cgicc::HTTPResponseHeader newHeader("HTTP/1.1",200,"OK");
	newHeader.addHeader("Content-Type","application/xhtml+xml");
	out->setHTTPResponseHeader(newHeader);
	*/

	std::vector<std::string> jsFileNames;
	jsFileNames.push_back("errorFlasher.js");
	jsFileNames.push_back("reload.js");
	*out << Header("EmuFCrateManager", jsFileNames);

	// Manual state changing
	*out << cgicc::fieldset()
		.set("class","fieldset") << std::endl;
	*out << cgicc::div("Manual state changes")
		.set("class","legend") << std::endl;

	*out << cgicc::div();
	*out << "Present state: ";
	*out << cgicc::span(state_.toString())
		.set("class",state_.toString()) << std::endl;
	*out << cgicc::div() << std::endl;

	// PGK You can't change states if you have been configured from above.
	*out << cgicc::div() << std::endl;
	if (!soapConfigured_) {
		*out << cgicc::form()
			.set("style","display: inline;")
			.set("action","/" + getApplicationDescriptor()->getURN() + "/Fire")
			.set("method","GET") << std::endl;
		if (state_.toString() == "Halted" || state_.toString() == "Configured") {
			*out << cgicc::input()
				.set("name","action")
				.set("type","submit")
				.set("value","Configure") << std::endl;
		}
		if (state_.toString() == "Configured") {
			*out << cgicc::input()
				.set("name","action")
				.set("type","submit")
				.set("value","Enable") << std::endl;
		}
		if (state_.toString() == "Enabled") {
			*out << cgicc::input()
				.set("name","action")
				.set("type","submit")
				.set("value","Disable") << std::endl;
		}
		*out << cgicc::input()
			.set("name","action")
			.set("type","submit")
			.set("value","Halt") << std::endl;
		*out << cgicc::form() << std::endl;

	} else {
		*out << "EmuFCrateManager has been configured through SOAP." << std::endl;
		*out << cgicc::br() << "Send the Halt signal through SOAP to manually change states." << std::endl;
	}
	*out << cgicc::div() << std::endl;
	*out << cgicc::fieldset() << std::endl;


	// EmuFCrate states
	*out << cgicc::fieldset()
		.set("class","fieldset") << std::endl;
	*out << cgicc::div("EmuFCrate states")
		.set("class","legend") << std::endl;

	std::set<xdaq::ApplicationDescriptor * > descriptors =
		getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("EmuFCrate");

	std::set <xdaq::ApplicationDescriptor *>::iterator itDescriptor;
    for ( itDescriptor = descriptors.begin(); itDescriptor != descriptors.end(); itDescriptor++ ) {

		// PGK ping the EmuFCrates for their informations.
		//  This will be used from here on out instead of the status table.
		xoap::MessageReference reply;
		try {
			reply = getParameters((*itDescriptor));
		} catch (emu::fed::SOAPException &e) {
			std::ostringstream error;
			error << "Unable to get parameters from application '" << (*itDescriptor)->getClassName() << "' instance " << (*itDescriptor)->getInstance();
			LOG4CPLUS_ERROR(getApplicationLogger(), error.str());
			XCEPT_DECLARE_NESTED(emu::fed::SOAPException, e2, error.str(), e);
			notifyQualified("ERROR", e2);
		}

		xdata::String endcap = "?";
		try {
			endcap = readParameter<xdata::String>(reply,"endcap");
		} catch (emu::fed::SOAPException &e) {
			std::ostringstream error;
			error << "Unable to read parameter 'endcap' from application '" << (*itDescriptor)->getClassName() << "' instance " << (*itDescriptor)->getInstance();
			LOG4CPLUS_WARN(getApplicationLogger(), error.str());
			XCEPT_DECLARE_NESTED(emu::fed::SOAPException, e2, error.str(), e);
			notifyQualified("WARN", e2);
		}
		
		std::stringstream className;
		className << (*itDescriptor)->getClassName() << "(" << (*itDescriptor)->getInstance() << ") " << endcap.toString();
		std::stringstream url;
		url << (*itDescriptor)->getContextDescriptor()->getURL() << "/" << (*itDescriptor)->getURN();

		*out << cgicc::div()
			.set("style","clear: both");

		*out << cgicc::a(className.str())
			.set("href",url.str()) << std::endl;

		*out << " Present state: " << std::endl;
		xdata::String currentState = "Unknown";
		try {
			currentState = readParameter<xdata::String>(reply,"State");
		} catch (emu::fed::SOAPException &e) {
			std::ostringstream error;
			error << "Unable to read parameter 'State' from application '" << (*itDescriptor)->getClassName() << "' instance " << (*itDescriptor)->getInstance();
			LOG4CPLUS_WARN(getApplicationLogger(), error.str());
			XCEPT_DECLARE_NESTED(emu::fed::SOAPException, e2, error.str(), e);
			notifyQualified("WARN", e2);
		}
		*out << cgicc::span(currentState)
			.set("class",currentState) << std::endl;

		xdata::String xmlFileName = "?";
		try {
			xmlFileName = readParameter<xdata::String>(reply,"xmlFileName");
		} catch (emu::fed::SOAPException &e) {
			std::ostringstream error;
			error << "Unable to read parameter 'xmlFileName' from application '" << (*itDescriptor)->getClassName() << "' instance " << (*itDescriptor)->getInstance();
			LOG4CPLUS_WARN(getApplicationLogger(), error.str());
			XCEPT_DECLARE_NESTED(emu::fed::SOAPException, e2, error.str(), e);
			notifyQualified("WARN", e2);
		}
		*out << cgicc::br() << std::endl;
		*out << cgicc::span("Configuration located at " + xmlFileName.toString())
		.set("style","color: #A00; font-size: 10pt;") << std::endl;
		*out << cgicc::br() << std::endl;

		*out << cgicc::div();

		// Print a table of all the DCC input and output rates.
		/*
		if (currentState == "Enabled") {

			xdata::String errorChambers = readParameter<xdata::String>(reply,"errorChambers");

			//LOG4CPLUS_DEBUG(getApplicationLogger(),"I think that there are " << errorChambers.size() << " errors to read out.");

			if (errorChambers != "") {
				*out << cgicc::div()
					<< "Chambers in an error state: " << std::endl;
				*out << cgicc::span()
					.set("class","error") << std::endl;
				*out << errorChambers.toString();
				*out << cgicc::span() << std::endl;
				*out << cgicc::div() << std::endl;
			}

			// PGK The reply is a std::vector of std::vectors of integers.
			// The outer std::vector is indexed by crates.
			// The inner std::vector contains the data from the DCC, with element [0]
			//  being the crate number,
			//  element [1] being the Slink output,
			//  elements [2] through [7] being the fiber inputs to the DCC.
			//
			// The math here is wicked-squiggly, so don't try to follow it.
			//  I should probably clean it up.
			xdata::Vector<xdata::Vector<xdata::UnsignedInteger> > dccInOut = readParameter<xdata::Vector<xdata::Vector<xdata::UnsignedInteger> > >(reply,"dccInOut");

			xdata::Vector<xdata::Vector<xdata::UnsignedInteger> >::iterator iCrate;
			for (iCrate = dccInOut.begin(); iCrate != dccInOut.end(); iCrate++) {
				std::stringstream style;
				style << "margin: 10px auto 10px auto; width: 45%; float: " << ((*iCrate)[0] % 2 ? "left;" : "right;");
				*out << cgicc::div()
					.set("style", style.str()) << std::endl;
				*out << cgicc::table()
					.set("style","border-collapse: collapse; border: solid 2px #000; width: 100%;") << std::endl;
				*out << cgicc::tr()
					.set("style","background-color: #000; color: #FFF; text-align: center; border-bottom: solid 1px #000; font-size: 14pt; font-weight: bold;")  << std::endl;
				*out << cgicc::td()
					.set("colspan","5") << std::endl;
				*out << "Data Rates for Crate " << (*iCrate)[0] << cgicc::td() << std::endl;
				*out << cgicc::tr() << std::endl;
				*out << cgicc::tr() << std::endl;
				for (unsigned int i=2; i<7; i++) {
					unsigned int slot = (i % 2) ? 15 - (i+1)/2 : i/2 + 2;
					*out << cgicc::td()
						.set("style","border: solid 1px #000; background-color: #FFF;") << std::endl;
					*out << "Slot " << slot << cgicc::br() << (*iCrate)[i] << std::endl;
					*out << cgicc::td() << std::endl;
				}
				*out << cgicc::tr() << std::endl;
				*out << cgicc::tr()
					.set("style","background-color: #FFF; text-align: center; border-bottom: solid 1px #000; font-weight: bold;") << std::endl;
				*out << cgicc::td()
					.set("colspan","5") << std::endl;
				*out << "Slink Output 1: " << (*iCrate)[1] << cgicc::td() << cgicc::tr() << std::endl;
				*out << cgicc::tr() << std::endl;
				for (unsigned int i=8; i<13; i++) {
					unsigned int slot = (i % 2) ? (i-1)/2 + 2 : 15 - i/2;
					*out << cgicc::td()
						.set("style","border: solid 1px #000; background-color: #FFF;") << std::endl;
					*out << "Slot " << slot << cgicc::br() << (*iCrate)[i] << std::endl;
					*out << cgicc::td() << std::endl;
				}
				*out << cgicc::tr() << std::endl;
				*out << cgicc::tr()
					.set("style","background-color: #FFF; text-align: center; border-bottom: solid 1px #000; font-weight: bold;") << std::endl;
				*out << cgicc::td()
					.set("colspan","5") << std::endl;
				*out << "Slink Output 2: " << (*iCrate)[7] << cgicc::td() << cgicc::tr() << std::endl;
				*out << cgicc::table() << std::endl;
				*out << cgicc::div() << std::endl;
			}
		}
		*/
		
		*out << cgicc::br() << std::endl;
	}

	*out << cgicc::fieldset() << std::endl;

	// Testing SVG
	/*
	// Radius of an individual station in px
	unsigned int stationRadius = 100;
	
	// Spacing between stations (and sides of SVG canvas) in px
	unsigned int stationSpacing = 10;
	
	// Proportions of chamber sizes in "normal" stations, inside out
	std::vector<float> normalSizes;
	normalSizes.push_back(0.8);
	normalSizes.push_back(1);
	
	// Proportions of chamber sizes in ME1/1 stations, inside out
	std::vector<float> me11Sizes;
	me11Sizes.push_back(0.8);
	me11Sizes.push_back(0.9);
	me11Sizes.push_back(1);
	
	// Proportions of misc. other things
	float centerSize = 0.5;
	float ringSpacing = 0.05;
	
	// Draw the canvas
	*out << "<!--svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << (4 * stationRadius * 2 + 5 * stationSpacing) << "\" height=\"" << (4 * stationRadius + 3 * stationSpacing) << "\" style=\"margin: 5px auto 5px auto;\">" << std::endl;
	
	// Loop over endcaps
	for (unsigned int iEndcap = 1; iEndcap <= 2; iEndcap++) {
		
		// Loop over stations
		for (unsigned int iStation = 1; iStation <= 4; iStation++) {
			
			// Draw the ring itself
			unsigned int centerX = (stationRadius * 2 + stationSpacing) * iStation - stationRadius;
			unsigned int centerY = (stationRadius * 2 + stationSpacing) * iEndcap - stationRadius;
			
			*out << "<circle cx=\"" << centerX << "\" cy=\"" << centerY << "\" r=\"" << stationRadius << "\" stroke=\"#000\" stroke-width=\"3\" fill=\"#FFF\" />" << std::endl;
			
			// Calculate sizes of things
			std::vector<float> radii;
			
			// Station 1 is different
			std::vector<float> sizes;
			if (iStation == 1) sizes = me11Sizes;
			else sizes = normalSizes;
			
			float total = 0;
			for (std::vector<float>::iterator iSize = sizes.begin(); iSize != sizes.end(); iSize++) {
				total += (*iSize);
			}
			
			float totalSizes = centerSize + ringSpacing * (sizes.size() - 1) + total;
			
			float radiusCache = centerSize/totalSizes * stationRadius;
			
			for (std::vector<float>::iterator iSize = sizes.begin(); iSize != sizes.end(); iSize++) {

				radii.push_back(radiusCache);
				radii.push_back(radiusCache + (*iSize)/totalSizes * stationRadius);
				
				radiusCache += ringSpacing/totalSizes * stationRadius + (*iSize)/totalSizes * stationRadius;
			}
			
			// Start drawing chambers
			for (unsigned int iRing = 1; iRing <= sizes.size(); iRing++) {
				
				// Number of chambers in this ring.
				unsigned int nChambers = 18;
				if (iStation == 1 || iRing == 2) {
					nChambers = 36;
				}
				
				// The verticies of the trapizoid
				std::vector<float> xPoints(1,0);
				std::vector<float> yPoints(1,0);
				xPoints.reserve(4);
				yPoints.reserve(4);
				
				xPoints.push_back(cos(3.14159265/nChambers) * sin(3.14159265/nChambers) * 2 * radii[2 * (iRing - 1) + 1]);
				yPoints.push_back(sin(3.14159265/nChambers) * sin(3.14159265/nChambers) * 2 * radii[2 * (iRing - 1) + 1]);
				
				xPoints.push_back(cos(3.14159265/nChambers) * sin(3.14159265/nChambers) * 2 * radii[2 * (iRing - 1)]);
				yPoints.push_back(sin(3.14159265/nChambers) * sin(3.14159265/nChambers) * 2 * radii[2 * (iRing - 1)] + radii[2 * (iRing - 1) + 1] - radii[2 * (iRing - 1)]);
				
				xPoints.push_back(0);
				yPoints.push_back(radii[2 * (iRing - 1) + 1] - radii[2 * (iRing - 1)]);
				
				// The starting angle for chamber 1
				float angularOffset = 90;
				if (nChambers == 18) {
					angularOffset -= 10;
				}
				
				// Draw the chamber and move it to the appropriate location
				for (unsigned int iChamber = 1; iChamber <= nChambers; iChamber++) {
					
					// Set the status
					std::string status = "killed";
					if (iStation == 4 && iRing == 2) {
						status = "undefined";
					}
					
					float angle = angularOffset - (360/nChambers * (iChamber - 1));
					
					// Rotate and move the group
					*out << "<g transform=\"rotate(" << angle << " " << centerX << " " << centerY << ") translate(" << centerX << " " << ((stationRadius * 2 + stationSpacing) * iEndcap - stationRadius - radii[2 * (iRing - 1) + 1]) << ")\" >" << std::endl;
					
					// Draw the chamber
					*out << "<polygon id=\"" << (iEndcap == 1 ? "p" : "m") << iStation << "_" << iRing << "_" << iChamber << "\" stroke=\"#000\" stroke-width=\"1\" class=\"" << status << "\" points=\"";
					for (unsigned int iPoint = 0; iPoint < 4; iPoint++) {
						*out << xPoints[iPoint] << "," << yPoints[iPoint] << " ";
					}
					*out << "\" />" << std::endl;
					*out << "</g>" << std::endl;
				}
			}
		}
		
	}
	
	*out << "</svg-->" << std::endl;
	*/
	*out << Footer();

}



void emu::fed::EmuFCrateManager::configureAction(toolbox::Event::Reference event)
throw (toolbox::fsm::exception::Exception)
{
	LOG4CPLUS_DEBUG(getApplicationLogger(), "FSM transition received:  Configure");

	if (soapLocal_) {
		soapLocal_ = false;
		soapConfigured_ = false;
	} else {
		soapConfigured_ = true;
	}

	if (state_.toString() != "Halted") {
		std::ostringstream error;
		error << state_.toString() << "->Configured via action 'Configure' is not valid:  transitioning through 'Halted' first";
		LOG4CPLUS_WARN(getApplicationLogger(), error.str());
		XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
		notifyQualified("WARN", e);

		fireEvent("Halt");
	}

	try{
		sendSOAPCommand("Configure","EmuFCrate");
	} catch (emu::fed::Exception &e) {
		std::ostringstream error;
		error << "Exception in configuring EmuFCrateManager";
		LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
		XCEPT_DECLARE_NESTED(emu::fed::FSMException, e2, error.str(), e);
		notifyQualified("FATAL", e2);
		XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e2);
	}

	std::string underlyingStates = getUnderlyingStates();
	if (underlyingStates != state_.toString() || underlyingStates == "Failed") {
		std::ostringstream error;
		error << "Failure in achieving consistant underlying FSM states";
		LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
		XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
		notifyQualified("FATAL", e);
		XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
	} else if (underlyingStates == "Unknown") {
		try {
			fireEvent("Unknown");
		} catch (toolbox::fsm::exception::Exception &e) {
			std::ostringstream error;
			error << "Exception transitioning to 'Unknown' state.  What the . . . ?";
			LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
			notifyQualified("FATAL", e);
			XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
		}
	}
}



void emu::fed::EmuFCrateManager::enableAction(toolbox::Event::Reference event)
throw (toolbox::fsm::exception::Exception)
{
	LOG4CPLUS_DEBUG(getApplicationLogger(), "FSM transition received:  Enable");
	soapLocal_ = false;

	if (state_.toString() != "Configured") {
		std::ostringstream error;
		error << state_.toString() << "->Enabled via action 'Enable' is not valid:  transitioning through 'Halted' and 'Configured' first";
		LOG4CPLUS_WARN(getApplicationLogger(), error.str());
		XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
		notifyQualified("WARN", e);
		
		fireEvent("Halt");
		fireEvent("Configure");
	}

	LOG4CPLUS_DEBUG(getApplicationLogger(), "The run number is " << runNumber_.toString());

	// PGK Now send the run number to the EmuFCrates.
	try {
		setParameter("EmuFCrate", "runNumber", "xsd:unsignedLong", runNumber_.toString());
	} catch (emu::fed::SOAPException &e) {
		std::ostringstream error;
		error << "Unable to send runNumber to EmuFCrates";
		LOG4CPLUS_ERROR(getApplicationLogger(), error.str());
		XCEPT_DECLARE_NESTED(emu::fed::SoftwareException, e2, error.str(), e);
		notifyQualified("ERROR", e2);
	}

	try{
		sendSOAPCommand("Enable","EmuFCrate");
	} catch (emu::fed::SOAPException &e) {
		std::ostringstream error;
		error << "Exception in enabling EmuFCrates";
		LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
		XCEPT_DECLARE_NESTED(emu::fed::FSMException, e2, error.str(), e);
		notifyQualified("FATAL", e2);
		XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e2);
	}

	std::string underlyingStates = getUnderlyingStates();
	if (underlyingStates != state_.toString() || underlyingStates == "Failed") {
		std::ostringstream error;
		error << "Failure in achieving consistant underlying FSM states";
		LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
		XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
		notifyQualified("FATAL", e);
		XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
	} else if (underlyingStates == "Unknown") {
		try {
			fireEvent("Unknown");
		} catch (toolbox::fsm::exception::Exception &e) {
			std::ostringstream error;
			error << "Exception transitioning to 'Unknown' state.  What the . . . ?";
			LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
			notifyQualified("FATAL", e);
			XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
		}
	}
}



void emu::fed::EmuFCrateManager::disableAction(toolbox::Event::Reference event)
throw (toolbox::fsm::exception::Exception)
{
	LOG4CPLUS_DEBUG(getApplicationLogger(), "FSM transtion received:  Disable");
	soapLocal_ = false;

	if (state_.toString() != "Enabled") {
		std::ostringstream error;
		error << state_.toString() << "->Configured via action 'Disable' is not valid:  doing 'Halted'->'Configured' instead";
		LOG4CPLUS_WARN(getApplicationLogger(), error.str());
		XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
		notifyQualified("WARN", e);

		fireEvent("Halt");
		fireEvent("Configure");

	} else {

		try{
			sendSOAPCommand("Disable","EmuFCrate");
		} catch (emu::fed::SOAPException &e) {
			std::ostringstream error;
			error << "Exception in disabling EmuFCrates";
			LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
			XCEPT_DECLARE_NESTED(emu::fed::SoftwareException, e2, error.str(), e);
			notifyQualified("FATAL", e2);
			XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e2);
		}
	}

	std::string underlyingStates = getUnderlyingStates();
	if (underlyingStates != state_.toString() || underlyingStates == "Failed") {
		std::ostringstream error;
		error << "Failure in achieving consistant underlying FSM states";
		LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
		XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
		notifyQualified("FATAL", e);
		XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
	} else if (underlyingStates == "Unknown") {
		try {
			fireEvent("Unknown");
		} catch (toolbox::fsm::exception::Exception &e) {
			std::ostringstream error;
			error << "Exception transitioning to 'Unknown' state.  What the . . . ?";
			LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
			notifyQualified("FATAL", e);
			XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
		}
	}
}



void emu::fed::EmuFCrateManager::haltAction(toolbox::Event::Reference event)
throw (toolbox::fsm::exception::Exception)
{
	LOG4CPLUS_DEBUG(getApplicationLogger(), "FSM transition received:  Halt");
	soapLocal_ = false;
	soapConfigured_ = false;

	try{
		sendSOAPCommand("Halt","EmuFCrate");
	} catch (emu::fed::SOAPException &e) {
		std::ostringstream error;
		error << "Exception in halting EmuFCrates";
		LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
		XCEPT_DECLARE_NESTED(emu::fed::SoftwareException, e2, error.str(), e);
		notifyQualified("FATAL", e2);
		XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e2);
	}

	std::string underlyingStates = getUnderlyingStates();
	if (underlyingStates != state_.toString() || underlyingStates == "Failed") {
		std::ostringstream error;
		error << "Failure in achieving consistant underlying FSM states";
		LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
		XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
		notifyQualified("FATAL", e);
		XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
	} else if (underlyingStates == "Unknown") {
		try {
			fireEvent("Unknown");
		} catch (toolbox::fsm::exception::Exception &e) {
			std::ostringstream error;
			error << "Exception transitioning to 'Unknown' state.  What the . . . ?";
			LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
			notifyQualified("FATAL", e);
			XCEPT_RETHROW(toolbox::fsm::exception::Exception, error.str(), e);
		}
	}
}



void emu::fed::EmuFCrateManager::unknownAction(toolbox::Event::Reference event)
{
	std::ostringstream error;
	error << "FSM transition to Unknown encountered";
	LOG4CPLUS_WARN(getApplicationLogger(), error.str());
	XCEPT_DECLARE(emu::fed::FSMException, e, error.str());
	notifyQualified("WARN", e);
	
	soapLocal_ = false;
	soapConfigured_ = false;
}



std::string emu::fed::EmuFCrateManager::getUnderlyingStates()
{
	// Check to see if I should fail based on the statuses of the EmuFCrates.
	std::set<xdaq::ApplicationDescriptor *> apps;
	try {
		apps = getApplicationContext()->getDefaultZone()->getApplicationDescriptors("EmuFCrate");
	} catch (xdaq::exception::ApplicationDescriptorNotFound &e) {
		std::ostringstream error;
		error << "EmuFCrate applications not found";
		LOG4CPLUS_WARN(getApplicationLogger(), error.str());
		XCEPT_DECLARE_NESTED(emu::fed::SoftwareException, e2, error.str(), e);
		notifyQualified("WARN", e2);
		return state_.toString();
	}
	
	// This map is to make sure everybody is in the same state.
	std::map<std::string, unsigned int> stateMap;
	for (std::set<xdaq::ApplicationDescriptor *>::iterator iApp = apps.begin(); iApp != apps.end(); iApp++) {
		xdata::String currentState = "";
		try {
			xoap::MessageReference reply = getParameters((*iApp));
			currentState = readParameter<xdata::String>(reply,"State");
		} catch (emu::fed::SOAPException &e) {
			std::ostringstream error;
			error << "Error in reading state from one or more EmuFCrate application";
			LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
			XCEPT_DECLARE_NESTED(emu::fed::SoftwareException, e2, error.str(), e);
			notifyQualified("FATAL", e2);
			return "Failed";
		}
		
		LOG4CPLUS_DEBUG(getApplicationLogger(),(*iApp)->getClassName() << "(" << (*iApp)->getInstance() << ") shows status: " << currentState.toString());
		stateMap[currentState.toString()]++;
		
		if (currentState == "Failed") {
			std::ostringstream error;
			error << "One or more EmuFCrate application is in a failed state";
			LOG4CPLUS_FATAL(getApplicationLogger(), error.str());
			XCEPT_DECLARE(emu::fed::SoftwareException, e, error.str());
			notifyQualified("FATAL", e);
			return "Failed";
		}
	}
	
	if (stateMap.size() > 1) {
		std::ostringstream error;
		error << "Inconsistant state across EmuFCrate applications";
		XCEPT_DECLARE(emu::fed::SoftwareException, e, error.str());
		LOG4CPLUS_WARN(getApplicationLogger(), error.str());
		notifyQualified("WARN", e);
		return "Unknown";
	}

	return state_.toString();
}



xoap::MessageReference emu::fed::EmuFCrateManager::onSetTTSBits(xoap::MessageReference message)
{
	LOG4CPLUS_DEBUG(getApplicationLogger(), "Remote SOAP command received: SetTTSBits, ttsID_=" << ttsID_ << ", ttsBits_=" << std::hex << ttsBits_ << std::dec);
	
	// JRG, decode the Source ID into Crate/Slot locations
	// PGK The ttsID_ is given to us by the CSCSV, so we don't have to do
	//  anything to get it.
	
	xdata::UnsignedInteger crateNumber = 0;
	xdata::UnsignedInteger slotNumber = 0;
	int ttsID = ttsID_; // Cached for easier manipulations
	
	if (ttsID == 760) { // TF-DDU
		crateNumber = 5;
		slotNumber = 2;
		
	} else if ((ttsID >= 750 && ttsID <= 756) || (ttsID >= 880 && ttsID <= 886)) { // DCCs
		// Determine if this is a master or slave DCC (for super-LHC)
		if (ttsID > 756) slotNumber = 17;
		else slotNumber = 8;
		// Determine crate
		crateNumber = (ttsID%10) / 2 + 1;
		
	} else if (ttsID >= 831 && ttsID <= 869 && ttsID%10) { // DDUs
		// Determine slot number.  Note that there is no super-LHC checking here!
		slotNumber = ttsID%10 + 2;
		if ((unsigned int) slotNumber > 7) slotNumber++; // slot 8 is DCC
		// Determine crate number.  This is rediculous.
		if (ttsID >= 831 && ttsID <= 839) crateNumber = 2;
		else if (ttsID >= 841 && ttsID <= 849) crateNumber = 1;
		else if (ttsID >= 851 && ttsID <= 859) crateNumber = 4;
		else crateNumber = 3;
		
	} else {
		std::ostringstream error;
		error << "ttsID_=" << ttsID_.toString() << " is out-of-bounds for the CSC FEDs";
		XCEPT_DECLARE(emu::fed::OutOfBoundsException, e, error.str());
		notifyQualified("WARN", e);
		LOG4CPLUS_WARN(getApplicationLogger(), error.str());
		return createReply(message);
	}

	try {

		// This will send the paramters to all the EmuFCrate applications.
		setParameter("EmuFCrate", "ttsCrate", "xsd:unsignedInt", crateNumber.toString());
		setParameter("EmuFCrate", "ttsSlot", "xsd:unsignedInt", slotNumber.toString());
		setParameter("EmuFCrate", "ttsBits", "xsd:int", ttsBits_.toString());
		
		// The EmuFCrate applications will decide if they should do anything based on the crates they conmmand.
		sendSOAPCommand("SetTTSBits", "EmuFCrate");
		
	} catch (emu::fed::SoftwareException &e) {
		std::ostringstream error;
		error << "Exception caught in completing setTTSBits";
		XCEPT_DECLARE_NESTED(emu::fed::OutOfBoundsException, e2, error.str(), e);
		notifyQualified("ERROR", e2);
		LOG4CPLUS_ERROR(getApplicationLogger(), error.str());
	}
	
	return createReply(message);
	
}


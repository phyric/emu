/// $Id: EmuFCrateManager.cc,v 1.8 2008/06/25 17:43:32 paste Exp $

/*************************************************************************
 * XDAQ Components for Distributed Data Acquisition                      *
 * Copyright (C) 2000-2004, CERN.			                 *
 * All rights reserved.                                                  *
 * Authors: J. Gutleber and L. Orsini					 *
 *                                                                       *
 * For the licensing terms see LICENSE.		                         *
 * For the list of contributors see CREDITS.   			         *
 *************************************************************************/

#include "EmuFCrateManager.h"

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <unistd.h> // for sleep()
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include <time.h>

#include "xoap/DOMParser.h"
#include "xoap/domutils.h"
#include "xdata/xdata.h"

using namespace cgicc;
using namespace std;

XDAQ_INSTANTIATOR_IMPL(EmuFCrateManager);

EmuFCrateManager::EmuFCrateManager(xdaq::ApplicationStub * s):
    //    throw (xdaq::exception::Exception) :
	LocalEmuApplication(s),
	//state_table_(this),
	soapConfigured_(false),
	soapLocal_(false)
{
	xdata::InfoSpace *i = getApplicationInfoSpace();
	i->fireItemAvailable("ttsID", &tts_id_);
	i->fireItemAvailable("ttsCrate", &tts_crate_);
	i->fireItemAvailable("ttsSlot", &tts_slot_);
	i->fireItemAvailable("ttsBits", &tts_bits_);

	xgi::bind(this,&EmuFCrateManager::webDefault, "Default");
	xgi::bind(this,&EmuFCrateManager::webFire, "Fire");
	//xgi::bind(this,&EmuFCrateManager::MainPage, "MainPage");
	//xgi::bind(this,&EmuFCrateManager::SendSOAPMessageConfigure, "SendSOAPMessageConfigure");
	//xgi::bind(this,&EmuFCrateManager::SendSOAPMessageConfigureXRelay, "SendSOAPMessageConfigureXRelay");

	// SOAP call-back functions, which relays to *Action method.
	xoap::bind(this, &EmuFCrateManager::onConfigure, "Configure", XDAQ_NS_URI);
	xoap::bind(this, &EmuFCrateManager::onEnable,    "Enable",    XDAQ_NS_URI);
	xoap::bind(this, &EmuFCrateManager::onDisable,   "Disable",   XDAQ_NS_URI);
	xoap::bind(this, &EmuFCrateManager::onHalt,      "Halt",      XDAQ_NS_URI);
	xoap::bind(this, &EmuFCrateManager::onSetTTSBits, "SetTTSBits", XDAQ_NS_URI);
	//xoap::bind(this, &EmuFCrateManager::onSetTTSBitsResponse, "SetTTSBitsResponse", XDAQ_NS_URI);

	//	xoap::bind(this, &CSCSupervisor::onSetTTS,    "SetTTS",    XDAQ_NS_URI);

	// fsm_ is defined in EmuApplication
	fsm_.addState('H', "Halted",     this, &EmuFCrateManager::stateChanged);
	fsm_.addState('C', "Configured", this, &EmuFCrateManager::stateChanged);
	fsm_.addState('E', "Enabled",    this, &EmuFCrateManager::stateChanged);

	fsm_.addStateTransition(
		'H', 'C', "Configure", this, &EmuFCrateManager::configureAction); // valid
	fsm_.addStateTransition(
		'C', 'C', "Configure", this, &EmuFCrateManager::configureAction); // valid

	fsm_.addStateTransition(
		'C', 'E', "Enable",    this, &EmuFCrateManager::enableAction); // valid
	fsm_.addStateTransition(
		'E', 'E', "Enable",    this, &EmuFCrateManager::enableAction); // valid

	fsm_.addStateTransition(
		'E', 'C', "Disable",   this, &EmuFCrateManager::disableAction); // valid

	fsm_.addStateTransition(
		'C', 'H', "Halt",      this, &EmuFCrateManager::haltAction); // valid
	fsm_.addStateTransition(
		'E', 'H', "Halt",      this, &EmuFCrateManager::haltAction); // valid
	fsm_.addStateTransition(
		'H', 'H', "Halt",      this, &EmuFCrateManager::haltAction); // valid
	fsm_.addStateTransition(
		'F', 'H', "Halt",      this, &EmuFCrateManager::haltAction); // valid

	// PGK We don't need these to be state transitions.
	/*
	fsm_.addStateTransition(
		'E', 'E', "SetTTSBits",this, &EmuFCrateManager::setTTSBitsAction);
	fsm_.addStateTransition(
		'E', 'E', "SetTTSBitsResponse",this, &EmuFCrateManager::setTTSBitsResponseAction);
	*/

	fsm_.setInitialState('H');
	fsm_.reset();

	ConfigureState_ = "None";

	// state_ is defined in EmuApplication
	state_ = fsm_.getStateName(fsm_.getCurrentState());

	//state_table_.addApplication("EmuFCrate");

	// Logger/Appender
	// log file format: EmuFEDYYYY-DOY-HHMMSS_rRUNNUMBER.log
	char datebuf[55];
	char filebuf[255];
	time_t theTime = time(NULL);

	strftime(datebuf, sizeof(datebuf), "%Y-%j-%H%M%S", localtime(&theTime));
	sprintf(filebuf,"EmuFCrateManager-%s.log",datebuf);

	log4cplus::SharedAppenderPtr myAppend = new FileAppender(filebuf);
	myAppend->setName("EmuFCrateManagerAppender");

	//Appender Layout
	std::auto_ptr<Layout> myLayout = std::auto_ptr<Layout>(new log4cplus::PatternLayout("%D{%m/%d/%Y %j-%H:%M:%S.%q} %-5p %c, %m%n"));
	// for date code, use the Year %Y, DayOfYear %j and Hour:Min:Sec.mSec
	// only need error data from Log lines with "ErrorData" tag
	myAppend->setLayout( myLayout );

	getApplicationLogger().addAppender(myAppend);

}


// HyperDAQ pages
void EmuFCrateManager::webDefault(xgi::Input * in, xgi::Output * out ) throw (xgi::exception::Exception)
{

	*out << Header("EmuFCrateManager");

	// Manual state changing
	*out << fieldset()
		.set("class","fieldset") << endl;
	*out << cgicc::div("Manual state changes")
		.set("class","legend") << endl;

	*out << cgicc::div();
	*out << "Present state: ";
	*out << cgicc::span(state_.toString())
		.set("class",state_.toString()) << endl;
	*out << cgicc::div() << endl;

	// PGK You can't change states if you have been configured from above.
	*out << cgicc::div() << endl;
	if (!soapConfigured_) {
		*out << cgicc::form()
			.set("style","display: inline;")
			.set("action","/" + getApplicationDescriptor()->getURN() + "/Fire")
			.set("method","GET") << endl;
		if (state_.toString() == "Halted" || state_.toString() == "Configured") {
			*out << cgicc::input()
				.set("name","action")
				.set("type","submit")
				.set("value","Configure") << endl;
		}
		if (state_.toString() == "Configured") {
			*out << cgicc::input()
				.set("name","action")
				.set("type","submit")
				.set("value","Enable") << endl;
		}
		if (state_.toString() == "Enabled") {
			*out << cgicc::input()
				.set("name","action")
				.set("type","submit")
				.set("value","Disable") << endl;
		}
		if (state_.toString() == "Halted" || state_.toString() == "Configured" || state_.toString() == "Enabled" || state_.toString() == "Failed" || state_.toString() == STATE_UNKNOWN) {
			*out << cgicc::input()
				.set("name","action")
				.set("type","submit")
				.set("value","Halt") << endl;
		}
		*out << cgicc::form() << endl;

	} else {
		*out << "EmuFCrateManager has been configured through SOAP." << endl;
		*out << cgicc::br() << "Send the Halt signal to manually change states." << endl;
	}
	*out << cgicc::div() << endl;
	*out << cgicc::fieldset() << endl;


	// EmuFCrate states
	*out << fieldset()
		.set("class","fieldset") << endl;
	*out << cgicc::div("EmuFCrate states")
		.set("class","legend") << endl;

	std::set<xdaq::ApplicationDescriptor * > descriptors =
		getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("EmuFCrate");

	std::set <xdaq::ApplicationDescriptor *>::iterator itDescriptor;
    for ( itDescriptor = descriptors.begin(); itDescriptor != descriptors.end(); itDescriptor++ ) {

		// PGK ping the EmuFCrates for their informations.
		//  This will be used from here on out instead of the status table.
		xoap::MessageReference reply = getParameters((*itDescriptor));
		
		ostringstream className;
		className << (*itDescriptor)->getClassName() << "(" << (*itDescriptor)->getInstance() << ")";
		ostringstream url;
		url << (*itDescriptor)->getContextDescriptor()->getURL() << "/" << (*itDescriptor)->getURN();

		*out << cgicc::div()
			.set("style","clear: both");

		*out << cgicc::a(className.str())
			.set("href",url.str())
			.set("target","_blank") << endl;

		*out << " Present state: " << endl;
		xdata::String currentState = readParameter<xdata::String>(reply,"State");
		*out << cgicc::span(currentState)
			.set("class",currentState) << endl;

		xdata::String xmlFileName = readParameter<xdata::String>(reply,"xmlFileName");
		*out << cgicc::br() << endl;
		*out << cgicc::span("Configuration located at " + xmlFileName.toString())
		.set("style","color: #A00; font-size: 10pt;") << endl;
		*out << cgicc::br() << endl;

		*out << cgicc::div();

		// Print a table of all the DCC input and output rates.
		if (currentState == "Enabled") {

			xdata::Vector<xdata::String> errorChambers = readParameter<xdata::Vector<xdata::String> >(reply,"errorChambers");
			if (errorChambers.size()) {
				*out << cgicc::div()
					<< "Chambers in an error state: " << endl;
				*out << cgicc::span()
					.set("class","error") << endl;
				for (unsigned int iChamber = 0; iChamber < errorChambers.size(); iChamber++) {
					*out << errorChambers[iChamber].toString() << " ";
				}
				*out << cgicc::span() << endl;
				*out << cgicc::div() << endl;

			}

			// PGK The reply is a vector of vectors of integers.
			// The outer vector is indexed by crates.
			// The inner vector contains the data from the DCC, with element [0]
			//  being the crate number,
			//  element [1] being the Slink output,
			//  elements [2] through [7] being the fiber inputs to the DCC.
			//
			// The math here is wicked-squiggly, so don't try to follow it.
			//  I should probably clean it up.
			xdata::Vector<xdata::Vector<xdata::UnsignedInteger> > dccInOut = readParameter<xdata::Vector<xdata::Vector<xdata::UnsignedInteger> > >(reply,"dccInOut");

			xdata::Vector<xdata::Vector<xdata::UnsignedInteger> >::iterator iCrate;
			for (iCrate = dccInOut.begin(); iCrate != dccInOut.end(); iCrate++) {
				stringstream style;
				style << "margin: 10px auto 10px auto; width: 45%; float: " << ((*iCrate)[0] % 2 ? "right;" : "left;");
				*out << cgicc::div()
					.set("style", style.str()) << endl;
				*out << cgicc::table()
					.set("style","border-collapse: collapse; border: solid 2px #000; width: 100%;") << endl;
				*out << cgicc::tr()
					.set("style","background-color: #000; color: #FFF; text-align: center; border-bottom: solid 1px #000; font-size: 14pt; font-weight: bold;")  << endl;
				*out << cgicc::td()
					.set("colspan","5") << endl;
				*out << "Data Rates for Crate " << (*iCrate)[0] << cgicc::td() << endl;
				*out << cgicc::tr() << endl;
				*out << cgicc::tr() << endl;
				for (unsigned int i=2; i<7; i++) {
					unsigned int slot = (i % 2) ? 15 - (i+1)/2 : i/2 + 2;
					*out << cgicc::td()
						.set("style","border: solid 1px #000; background-color: #FFF;") << endl;
					*out << "Slot " << slot << cgicc::br() << (*iCrate)[i] << endl;
					*out << cgicc::td() << endl;
				}
				*out << cgicc::tr() << endl;
				*out << cgicc::tr()
					.set("style","background-color: #FFF; text-align: center; border-bottom: solid 1px #000; font-weight: bold;") << endl;
				*out << cgicc::td()
					.set("colspan","5") << endl;
				*out << "Slink Output 1: " << (*iCrate)[1] << cgicc::td() << cgicc::tr() << endl;
				*out << cgicc::tr() << endl;
				for (unsigned int i=8; i<13; i++) {
					unsigned int slot = (i % 2) ? (i-1)/2 + 2 : 15 - i/2;
					*out << cgicc::td()
						.set("style","border: solid 1px #000; background-color: #FFF;") << endl;
					*out << "Slot " << slot << cgicc::br() << (*iCrate)[i] << endl;
					*out << cgicc::td() << endl;
				}
				*out << cgicc::tr() << endl;
				*out << cgicc::tr()
					.set("style","background-color: #FFF; text-align: center; border-bottom: solid 1px #000; font-weight: bold;") << endl;
				*out << cgicc::td()
					.set("colspan","5") << endl;
				*out << "Slink Output 2: " << (*iCrate)[7] << cgicc::td() << cgicc::tr() << endl;
				*out << cgicc::table() << endl;
				*out << cgicc::div() << endl;
			}
		}
		
		*out << cgicc::br() << endl;
	}

	*out << cgicc::fieldset() << endl;

	//LOG4CPLUS_INFO(getApplicationLogger(), "XRelay");
	// XRelays?
	std::set<xdaq::ApplicationDescriptor * > xrdescriptors =
		getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("XRelay");

	if (xrdescriptors.size()) {

		*out << fieldset()
			.set("class","fieldset") << endl;
		*out << cgicc::div("XRelays")
			.set("class","legend") << endl;

		std::set <xdaq::ApplicationDescriptor *>::iterator itDescriptor;
		for ( itDescriptor = xrdescriptors.begin(); itDescriptor != xrdescriptors.end(); itDescriptor++ ) {
			ostringstream className;
			className << (*itDescriptor)->getClassName() << "(" << (*itDescriptor)->getInstance() << ")";
			ostringstream url;
			url << (*itDescriptor)->getContextDescriptor()->getURL() << "/" << (*itDescriptor)->getURN();

			*out << cgicc::a(className.str())
				.set("href",url.str()) << endl;

			*out << cgicc::br() << endl;
		}

		*out << cgicc::fieldset() << endl;
	}

	*out << Footer();

}



void EmuFCrateManager::webFire(xgi::Input *in, xgi::Output *out)
	throw (xgi::exception::Exception)
{
	cgicc::Cgicc cgi(in);
	soapLocal_ = true;

	string action = "";
	cgicc::form_iterator name = cgi.getElement("action");
	if(name != cgi.getElements().end()) {
		action = cgi["action"]->getValue();
		cout << "webFire action: " << action << endl;
		ostringstream log;
		log << "Local FSM state change requested: " << action;
		LOG4CPLUS_INFO(getApplicationLogger(), log.str());
		fireEvent(action);
	}

	webRedirect(in, out);
}



void EmuFCrateManager::webRedirect(xgi::Input *in, xgi::Output *out)
	throw (xgi::exception::Exception)
{
	string url = in->getenv("PATH_TRANSLATED");

	HTTPResponseHeader &header = out->getHTTPResponseHeader();

	header.getStatusCode(303);
	header.getReasonPhrase("See Other");
	header.addHeader("Location",url.substr(0, url.find("/" + in->getenv("PATH_INFO"))));
}



// PGK this is removed in favor of not having state transitions
/*
void EmuFCrateManager::setTTSBitsResponseAction(toolbox::Event::Reference e)
  throw (toolbox::fsm::exception::Exception)
{
      //
      LOG4CPLUS_INFO(getApplicationLogger(), "Received Message SetTTSBitsResponse");
      //
//JRG  Jason's failed attempt to send positive/negative result info to cscSV
	const string sv_app = "bad_idea_CSCSupervisor_skipit";
	cout << "*** EmuFCrateManager: inside setTTSBitsResponseAction" << endl;

	try {
		int instance = 0;
		cout << " ** EmuFCrateManager: inside setTTSBitsResponseAction, try sendCommand instance=" << instance << sv_app << endl;
		sendCommand("SetTTSBitsResponse", sv_app, instance);

	} catch (xoap::exception::Exception e) {
		XCEPT_RETHROW(toolbox::fsm::exception::Exception,
				"SOAP fault was returned", e);
		cout << "*!* EmuFCrateManager: inside setTTSBitsResponseAction, setParameter fault" << endl;
	} catch (xdaq::exception::Exception e) {
		XCEPT_RETHROW(toolbox::fsm::exception::Exception,
				"Failed to send a command", e);
		cout << "*!* EmuFCrateManager: inside setTTSBitsResponseAction, setParameter failed" << endl;
	}
	cout << "*** EmuFCrateManager: end of setTTSBitsResponseAction" << endl ;
}
*/

void EmuFCrateManager::configureAction(toolbox::Event::Reference e)
	throw (toolbox::fsm::exception::Exception)
{
	cout << "  inside EmuFCrateManager::configureAction " << endl;
	LOG4CPLUS_INFO(getApplicationLogger(), "Received SOAP message: Configure");

	if (soapLocal_) {
		soapLocal_ = false;
		soapConfigured_ = false;
	} else {
		soapConfigured_ = true;
	}

	// PGK This is given to us from the CSCSV.  This will determine our logging
	//  preferences.
	LOG4CPLUS_INFO(getApplicationLogger(), "Run type is " << runType_.toString());
	if (runType_.toString() == "Debug") {
		getApplicationLogger().setLogLevel(DEBUG_LOG_LEVEL);
	} else {
		getApplicationLogger().setLogLevel(INFO_LOG_LEVEL);
	}

	// PGK Now send it to the EmuFCrates.
	setParameter("EmuFCrate","runType","xsd:string",runType_.toString());

	try{
		PCsendCommand("Configure","EmuFCrate");
	} catch (xdaq::exception::Exception e) {
		XCEPT_RAISE(toolbox::fsm::exception::Exception, "error in EmuFCrateManager::configureAction");
	}
}


void EmuFCrateManager::enableAction(toolbox::Event::Reference e)
	throw (toolbox::fsm::exception::Exception)
{
	LOG4CPLUS_INFO(getApplicationLogger(), "Received SOAP message: Enable");
	soapLocal_ = false;

	// PGK If the run number is not set, this is a debug run.
	LOG4CPLUS_INFO(getApplicationLogger(), "The run number is " << runNumber_.toString());
	if (runNumber_.toString() == "" || runNumber_.toString() == "0") {
		getApplicationLogger().setLogLevel(DEBUG_LOG_LEVEL);
	}
	// PGK Now send the run number to the EmuFCrates.
	setParameter("EmuFCrate","runNumber","xsd:unsignedLong",runNumber_.toString());

	try{
		PCsendCommand("Enable","EmuFCrate");
	} catch (xdaq::exception::Exception e) {
		XCEPT_RAISE(toolbox::fsm::exception::Exception, "error in EmuFCrateManager::enableAction");
	}
}


void EmuFCrateManager::disableAction(toolbox::Event::Reference e)
	throw (toolbox::fsm::exception::Exception)
{
	LOG4CPLUS_INFO(getApplicationLogger(), "Received SOAP message: Disable");
	soapLocal_ = false;

	try{
		PCsendCommand("Disable","EmuFCrate");
	} catch (xdaq::exception::Exception e) {
		XCEPT_RAISE(toolbox::fsm::exception::Exception, "error in EmuFCrateManager::disableAction");
	}
}


void EmuFCrateManager::haltAction(toolbox::Event::Reference e)
	throw (toolbox::fsm::exception::Exception)
{
	LOG4CPLUS_INFO(getApplicationLogger(), "Received SOAP message: Halt");
	soapLocal_ = false;
	soapConfigured_ = false;
	try{
		PCsendCommand("Halt","EmuFCrate");
	} catch (xdaq::exception::Exception e) {
		XCEPT_RAISE(toolbox::fsm::exception::Exception, "error in EmuFCrateManager::haltAction");
	}
}


void EmuFCrateManager::stateChanged(toolbox::fsm::FiniteStateMachine &fsm)
	throw (toolbox::fsm::exception::Exception)
{
	EmuApplication::stateChanged(fsm);

	//state_table_.refresh();

	// Check to see if I should fail based on the statuses of the EmuFCrates.
	std::set<xdaq::ApplicationDescriptor *> apps;
	try {
		apps = getApplicationContext()->getDefaultZone()->getApplicationDescriptors("EmuFCrate");
	} catch (xdaq::exception::ApplicationDescriptorNotFound &e) {
		// PGK WOAH!  Proper exception handling!
		LOG4CPLUS_ERROR(getApplicationLogger(), e.what());
		XCEPT_RAISE(xdaq::exception::Exception, "Application class name not found.  Can't continue.");
	}

	std::set<xdaq::ApplicationDescriptor *>::iterator i;
	bool failure = false;
	for (i = apps.begin(); i != apps.end(); ++i) {
		// PGK Use this instead of the state table.
		xoap::MessageReference reply = getParameters((*i));
		xdata::String currentState = readParameter<xdata::String>(reply,"State");
		
		if (currentState == "Failed") {
			failure = true;
			LOG4CPLUS_ERROR(getApplicationLogger(),(*i)->getClassName() << "(" << (*i)->getInstance() << ") shows status: Failed");
		}
	}
	if (failure) {
		//state_ = "Failed";
		XCEPT_RAISE(toolbox::fsm::exception::Exception, "failure in one of the EmuFCrates");
	}

	LOG4CPLUS_INFO(getApplicationLogger(), "FSM state changed to " << state_.toString());
	
}




string EmuFCrateManager::extractRunNumber(xoap::MessageReference message)
{
      xoap::SOAPElement root = message->getSOAPPart()
	.getEnvelope().getBody().getChildElements(*(new xoap::SOAPName("ParameterGetResponse", "", "")))[0];
      xoap::SOAPElement properties = root.getChildElements(*(new xoap::SOAPName("properties", "", "")))[0];
      xoap::SOAPElement state = properties.getChildElements(*(new xoap::SOAPName("RunNumber", "", "")))[0];

      return state.getValue();
}


  //
  // Create a XRelay SOAP Message
  //
/*
xoap::MessageReference EmuFCrateManager::QueryLTCInfoSpace()
{
      xoap::MessageReference message = xoap::createMessage();
      xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
      envelope.addNamespaceDeclaration("xsi", "http://www.w3.org/2001/XMLSchema-instance");
      //
      xoap::SOAPName command    = envelope.createName("ParameterGet","xdaq", "urn:xdaq-soap:3.0");
      xoap::SOAPName properties = envelope.createName("properties",  "LTCControl", "urn:xdaq-application:LTCControl");
      xoap::SOAPName parameter  = envelope.createName("stateName",   "LTCControl", "urn:xdaq-application:LTCControl");
      xoap::SOAPName parameter2 = envelope.createName("RunNumber",   "LTCControl", "urn:xdaq-application:LTCControl");
      xoap::SOAPName xsitype    = envelope.createName("type", "xsi", "http://www.w3.org/2001/XMLSchema-instance");
      //
      xoap::SOAPElement properties_e = envelope.getBody()
	.addBodyElement(command)
	.addChildElement(properties);
      properties_e.addAttribute(xsitype, "soapenc:Struct");
      //
      xoap::SOAPElement parameter_e = properties_e.addChildElement(parameter);
      parameter_e.addAttribute(xsitype, "xsd:string");
      //
      xoap::SOAPElement parameter_e2 = properties_e.addChildElement(parameter2);
      parameter_e2.addAttribute(xsitype, "xsd:unsignedLong");
      //
      return message;
}
*/


xoap::MessageReference EmuFCrateManager::QueryFCrateInfoSpace()
{
      xoap::MessageReference message = xoap::createMessage();
      xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
      envelope.addNamespaceDeclaration("xsi", "http://www.w3.org/2001/XMLSchema-instance");

      xoap::SOAPName command = envelope.createName("ParameterGet", "xdaq", "urn:xdaq-soap:3.0");
      xoap::SOAPName properties = envelope.createName("properties", "EmuFCrate", "urn:xdaq-application:EmuFCrate");
      xoap::SOAPName parameter   = envelope.createName("stateName", "EmuFCrate", "urn:xdaq-application:EmuFCrate");
      xoap::SOAPName parameter2  = envelope.createName("CalibrationState", "EmuFCrate", "urn:xdaq-application:EmuFCrate");
      xoap::SOAPName xsitype    = envelope.createName("type", "xsi", "http://www.w3.org/2001/XMLSchema-instance");

      xoap::SOAPElement properties_e = envelope.getBody()
	.addBodyElement(command)
	.addChildElement(properties);
      properties_e.addAttribute(xsitype, "soapenc:Struct");

      xoap::SOAPElement parameter_e = properties_e.addChildElement(parameter);
      parameter_e.addAttribute(xsitype, "xsd:string");

      parameter_e = properties_e.addChildElement(parameter2);
      parameter_e.addAttribute(xsitype, "xsd:string");

      return message;
}




xoap::MessageReference EmuFCrateManager::onConfigure (xoap::MessageReference message) throw (xoap::exception::Exception)
{

	LOG4CPLUS_INFO(getApplicationLogger(), "Remote SOAP state change requested: Configure");

	// PGK I avoid errors at all cost.
	if (state_.toString() == "Enabled" || state_.toString() == "Failed") {
		LOG4CPLUS_WARN(getApplicationLogger(), state_.toString() <<"->Configured is not a valid transition.  Fixing by going to Halted first.");
		fireEvent("Halt");
	}

	fireEvent("Configure");

	return createReply(message);
}



xoap::MessageReference EmuFCrateManager::onEnable (xoap::MessageReference message) throw (xoap::exception::Exception)
{

	LOG4CPLUS_INFO(getApplicationLogger(), "Remote SOAP state change requested: Enable");

	// PGK I avoid errors at all cost.
	if (state_.toString() == "Halted" || state_.toString() == "Failed") {
		LOG4CPLUS_WARN(getApplicationLogger(), state_.toString() <<"->Enabled is not a valid transition.  Fixing by going to Halted->Configured first.");
		fireEvent("Halt");
		fireEvent("Configure");
	}

	fireEvent("Enable");

	return createReply(message);
}

xoap::MessageReference EmuFCrateManager::onSetTTSBits(xoap::MessageReference message) throw (xoap::exception::Exception)
{
	LOG4CPLUS_INFO(getApplicationLogger(), "Remote SOAP command received: SetTTSBits");

	// PGK This doesn't need to be a state transition.
	//fireEvent("SetTTSBits");

	const string fed_app = "EmuFCrate";

	// JRG, decode the Source ID into Crate/Slot locations
	// PGK The tts_id_ is given to us by the CSCSV, so we don't have to do
	//  anything to get it.
	unsigned int srcID = tts_id_;
	if (srcID<748) srcID = 748;
	printf(", srcID=%d\n",srcID);

	if (srcID>830&&srcID<840) {       // crate 2 DDUs, S1-G06g, ME+
		tts_crate_ = 2;
		unsigned int islot = srcID-827; // srcID-831+4
		if (islot>7) islot++;
		tts_slot_ = islot;
	}
	else if (srcID>840&&srcID<850) {  // crate 1 DDUs, S1-G06i, ME+
		tts_crate_ = 1;
		unsigned int islot = srcID-837; // srcID-841+4
		if (islot>7) islot++;
		tts_slot_ = islot;
	}
/*
	else if(srcID>850&&srcID<860){  // crate 4 DDUs, S1-G08g, ME-
	  tts_crate_=4;
	  unsigned int islot=srcID-847; // srcID-851+4
	  if(islot>7)islot++;
	  tts_slot_=islot;
	}
	else if(srcID>860&&srcID<870){  // crate 3 DDUs, S1-G08i, ME-
	  tts_crate_=3;
	  unsigned int islot=srcID-857; // srcID-861+4
	  if(islot>7)islot++;
	  tts_slot_=islot;
	}
*/
	else if (srcID==760) { //crate ? TF-DDU, S1-?
		tts_crate_ = 3;  // JRG temp!  Later should be 5!  After ME- installed.
		tts_slot_ = 2;   // check...!
	}

	else { // set crates/slot for DCCs,
		unsigned int icrate = (srcID-748)/2; // will work for both S-Link IDs
		if (icrate>0&&icrate<5) {
			tts_crate_ = icrate;
			tts_slot_ = 8;
		}
	}
/*  better way used above ^^^^
	if(srcID==752){  //crate 2 DCC, S1-G06g
	  tts_crate_=2;
	  tts_slot_=8;
	}
	if(srcID==750){  //crate 1 DCC, S1-G06i
	  tts_crate_=1;
	  tts_slot_=8;
	}
	if(srcID==756){  //crate 4 DCC, S1-G08g
	  tts_crate_=4;
	  tts_slot_=8;
	}
	if(srcID==754){  //crate 3 DCC, S1-G08i
	  tts_crate_=3;
	  tts_slot_=8;
	}
*/

	try {
// JRG: this is the instance for the FED application, NOT really the CrateID
//		int instance = (tts_crate_ == "1") ? 0 : 1;
		int instance = 0;
		xdata::UnsignedInteger ui_diff = 1;

// JRG 9/29/07: need to have unique instance for each crate fed_app process
//		if(tts_crate_>0)instance=tts_crate_ - ui_diff;
		instance=tts_crate_;
		if(instance>0)instance--;
		if(srcID==760)tts_crate_=5;
/* JRG, for case of 2 FED crates in a single config (2 crates per EmuFCrate):
		if(instance>2)instance=1;
		else instance=0;
*/
		setParameter(fed_app,"ttsCrate","xsd:unsignedInt",tts_crate_.toString());
		setParameter(fed_app,"ttsSlot", "xsd:unsignedInt",tts_slot_.toString());
		setParameter(fed_app,"ttsBits", "xsd:unsignedInt",tts_bits_.toString());
//		cout << "inside setTTSAction" << tts_crate_.str() << tts_slot_.str() << tts_bits_.str() << endl;

		LOG4CPLUS_DEBUG(getApplicationLogger(), "TTSBits being sent to " << fed_app << "(" << instance << ")");
		LOG4CPLUS_DEBUG(getApplicationLogger(), "Crate " << tts_crate_.toString() << " Slot " << tts_slot_.toString() << " Bits " << tts_bits_.toString());
		//cout << " ** EmuFCrateManager: inside setTTSBitsAction, setParameter tried, now sendCommand instance=" << instance << fed_app << endl;

		sendCommand("SetTTSBits", fed_app, instance);

	} catch (xoap::exception::Exception e) {
		XCEPT_RETHROW(toolbox::fsm::exception::Exception,"SOAP fault was returned", e);
		LOG4CPLUS_ERROR(getApplicationLogger(), "setParameter fault");
		//cout << "*!* EmuFCrateManager: inside setTTSBitsAction, setParameter fault" << endl;
	} catch (xdaq::exception::Exception e) {
		XCEPT_RETHROW(toolbox::fsm::exception::Exception,"Failed to send a command", e);
		LOG4CPLUS_ERROR(getApplicationLogger(), "setParameter failed");
		//cout << "*!* EmuFCrateManager: inside setTTSBitsAction, setParameter failed" << endl;
	}
	//cout << "*** EmuFCrateManager: end of setTTSBitsAction" << endl;

	//SendSOAPMessageXRelaySimple("SetTTSBits","");

	//cout << "*** EmuFCrateManager: end of onSetTTSBits, so return" << endl;
	return createReply(message);

}


// This is not used.
/*
xoap::MessageReference EmuFCrateManager::onSetTTSBitsResponse(xoap::MessageReference message) throw (xoap::exception::Exception)
{
	cout << "*** EmuFCrateManager: inside onSetTTSBitsResponse" << endl ;

	// PGK We don't have to do make this a state transitions.
	//fireEvent("SetTTSBitsResponse");

	LOG4CPLUS_INFO(getApplicationLogger(), "Received Message SetTTSBitsResponse");
	//JRG  Jason's failed attempt to send positive/negative result info to cscSV
	const string sv_app = "bad_idea_CSCSupervisor_skipit";
	cout << "*** EmuFCrateManager: inside setTTSBitsResponseAction" << endl;

	try {
		int instance = 0;
		cout << " ** EmuFCrateManager: inside setTTSBitsResponseAction, try sendCommand instance=" << instance << sv_app << endl;
		sendCommand("SetTTSBitsResponse", sv_app, instance);

	} catch (xoap::exception::Exception e) {
		XCEPT_RETHROW(toolbox::fsm::exception::Exception,
				"SOAP fault was returned", e);
		cout << "*!* EmuFCrateManager: inside setTTSBitsResponseAction, setParameter fault" << endl;
	} catch (xdaq::exception::Exception e) {
		XCEPT_RETHROW(toolbox::fsm::exception::Exception,
				"Failed to send a command", e);
		cout << "*!* EmuFCrateManager: inside setTTSBitsResponseAction, setParameter failed" << endl;
	}
	cout << "*** EmuFCrateManager: end of setTTSBitsResponseAction" << endl ;

	//	SendSOAPMessageXRelayReturn("SetTTSBitsResponse","");

	//cout << "*** EmuFCrateManager: end of onSetTTSBitsResponse, so return" << endl ;
	return createReply(message);

}
*/




/*
xoap::MessageReference EmuFCrateManager::onEnableCalCFEBComp (xoap::MessageReference message) throw (xoap::exception::Exception)
{
    float dac, threshold;
    int nsleep = 100, highthreshold;
    cout << "inside onEnableCalCFEBComp" << endl;
    ostringstream test;
    message->writeTo(test);
    cout << test.str() <<endl;

    calsetup++;

    //implement the comparator setup process:
    cout << "DMB setup for CFEB Comparator, calsetup= " <<calsetup<< endl;

    //Start the setup process:

    //    if (calsetup==1) broadcastTMB->EnableCLCTInputs(0x1f); //enable TMB's CLCT inputs

    int thresholdsetting =((calsetup-1)%35);   //35 Comparator threshold setting for each channel
    int nstrip=(calsetup-1)/35;           //16 channels, total loop: 32*35=1120
    highthreshold=nstrip/16;
    dac=0.15+0.2*highthreshold;
    nstrip=nstrip%16;
    threshold=0.003*thresholdsetting+0.013+0.036*highthreshold;
    cout<<" calsetup: "<<calsetup<<" strip: "<<nstrip<<" DAC: "<<dac<<" Threshold: "<<threshold<<endl;
    if (!thresholdsetting) {
      broadcastDMB->buck_shift_comp_bc(nstrip);
      if (!nstrip) broadcastDMB->set_cal_dac(dac,dac);
    }
    broadcastDMB->set_comp_thresh_bc(threshold);
    cout <<" The strip was set to: "<<nstrip<<" DAC was set to: "<<dac <<endl;
    usleep(nsleep);
    //    fireEvent("Enable");

    return createReply(message);
}


xoap::MessageReference EmuFCrateManager::onEnableCalCFEBGain (xoap::MessageReference message) throw (xoap::exception::Exception)
{
    float dac;
    int nsleep = 100;
    //  cout<< "This is a checking printing for OnEnableCalCFEBGain"<<endl;
    ostringstream test;
    message->writeTo(test);
    cout << test.str() <<endl;

    calsetup++;

    //implement the cal0 setup process:
    cout << "DMB setup for CFEB Gain, calsetup= " <<calsetup<< endl;

    //Start the setup process:
    int gainsetting =((calsetup-1)%10);
    int nstrip=(calsetup-1)/10;
    if (!gainsetting) broadcastDMB->buck_shift_ext_bc(nstrip);
    dac=0.2+0.2*gainsetting;
    broadcastDMB->set_cal_dac(dac,dac);
    cout <<" The strip was set to: "<<nstrip<<" DAC was set to: "<<dac <<endl;
    usleep(nsleep);
    //    fireEvent("Enable");

    return createReply(message);
}


xoap::MessageReference EmuFCrateManager::onEnableCalCFEBTime (xoap::MessageReference message) throw (xoap::exception::Exception)
{
    int nsleep = 100;
    //
    cout<< "This is a checking printing for OnEnableCalCFEBTime"<<endl;
    ostringstream test;
    message->writeTo(test);
    cout << test.str() <<endl;

    calsetup++;

    //implement the cal0 setup process:
    cout << "DMB setup for CFEB Time, calsetup= " <<calsetup<< endl;

    //Start the setup process:
    int timesetting =((calsetup-1)%20);
    int nstrip=(calsetup-1)/20;
    if (!timesetting) broadcastDMB->buck_shift_ext_bc(nstrip);
    broadcastDMB->set_cal_tim_pulse(timesetting);
    cout <<" The strip was set to: "<<nstrip<<" Time was set to: "<<timesetting <<endl;
    usleep(nsleep);
    //    fireEvent("Enable");

    return createReply(message);
}



xoap::MessageReference EmuFCrateManager::onEnableCalCFEBPed (xoap::MessageReference message) throw (xoap::exception::Exception)
{
    float dac;
    int nsleep = 100;
    //
    cout<< "This is a checking printing for OnEnableCalCFEBPed"<<endl;
    ostringstream test;
    message->writeTo(test);
    cout << test.str() <<endl;

    calsetup++;

    //implement the CFEB_Pedestal setup process:
    cout << "DMB setup for CFEB Pedestal, calsetup= " <<calsetup<< endl;

    //Start the setup process: Set all channel to normal, DAC to 0:
    broadcastDMB->buck_shift_ext_bc(-1);
    dac=0.0;
    broadcastDMB->set_cal_dac(dac,dac);
    cout <<" The strip was set to: -1, " <<" DAC was set to: "<<dac <<endl;
    usleep(nsleep);
    //    fireEvent("Enable");

    return createReply(message);
}
*/



xoap::MessageReference EmuFCrateManager::onDisable (xoap::MessageReference message) throw (xoap::exception::Exception)
{

	LOG4CPLUS_INFO(getApplicationLogger(), "Remote SOAP state change requested: Disable");

	// PGK I avoid errors at all cost.
	if (state_.toString() != "Enabled") {
		LOG4CPLUS_WARN(getApplicationLogger(), state_.toString() <<"->Configured via \"Disable\" is not a valid transition.  Fixing by doing Halted->Configured instead.");
		fireEvent("Halt");
		fireEvent("Configure");
	} else {
		fireEvent("Disable");
	}

	return createReply(message);
}
  //


xoap::MessageReference EmuFCrateManager::onHalt (xoap::MessageReference message) throw (xoap::exception::Exception)
{

	LOG4CPLUS_INFO(getApplicationLogger(), "Remote SOAP state change requested: Halt");

	fireEvent("Halt");

	return createReply(message);
}
 //


xoap::MessageReference EmuFCrateManager::createXRelayMessage(const string & command, const string & setting,
 std::set<xdaq::ApplicationDescriptor * > descriptor )
{
  cout << "  * EmuFCrateManager: inside createXRelayMessage" << endl ;
      // Build a SOAP msg with the Xrelay header:
  xoap::MessageReference msg  = xoap::createMessage();
    //
    string topNode = "relay";
    string prefix = "xr";
    string httpAdd = "http://xdaq.web.cern.ch/xdaq/xsd/2004/XRelay-10";
    xoap::SOAPEnvelope envelope = msg->getSOAPPart().getEnvelope();
    xoap::SOAPName envelopeName = envelope.getElementName();
    xoap::SOAPHeader header = envelope.addHeader();
    xoap::SOAPName relayName = envelope.createName(topNode, prefix,  httpAdd);
    xoap::SOAPHeaderElement relayElement = header.addHeaderElement(relayName);

    // Add the actor attribute
    xoap::SOAPName actorName = envelope.createName("actor", envelope.getElementName().getPrefix(),
						   envelope.getElementName().getURI());
    relayElement.addAttribute(actorName,httpAdd);

    // Add the "to" node
    string childNode = "to";
    // Send to all the destinations:
    //
     std::set<xdaq::ApplicationDescriptor * >  descriptorsXrelays =
      getApplicationContext()->getDefaultZone()->getApplicationGroup("broker")->getApplicationDescriptors("XRelay");
    // xdata::UnsignedIntegerT lid4=4;
    // std::set<xdaq::ApplicationDescriptor * >  descriptorsXrelays =
    //     getApplicationContext()->getZone("default")->getApplicationGroup("broker")->getApplicationDescriptors("XRelay");

   //
    cout << "  * EmuFcrateManager: descriptorXrelays size = " << descriptorsXrelays.size() << endl;
    //
    std::set<xdaq::ApplicationDescriptor * >::iterator  itDescriptorsXrelays = descriptorsXrelays.begin();

    std::set <xdaq::ApplicationDescriptor *>::iterator itDescriptor;

    for ( itDescriptor = descriptor.begin(); itDescriptor != descriptor.end(); itDescriptor++ )
      {
        string classNameStr = (*itDescriptor)->getClassName();
	//
	string url = (*itDescriptor)->getContextDescriptor()->getURL();
	string urn = (*itDescriptor)->getURN();
	//
	string urlXRelay = (*itDescriptorsXrelays)->getContextDescriptor()->getURL();
 	string urnXRelay = (*itDescriptorsXrelays)->getURN();
	itDescriptorsXrelays++;
	if (itDescriptorsXrelays ==  descriptorsXrelays.end()) itDescriptorsXrelays=descriptorsXrelays.begin();
	//
	xoap::SOAPName toName = envelope.createName(childNode, prefix, " ");
	xoap::SOAPElement childElement = relayElement.addChildElement(toName);
	xoap::SOAPName urlName = envelope.createName("url");
	xoap::SOAPName urnName = envelope.createName("urn");
	childElement.addAttribute(urlName,urlXRelay);
	childElement.addAttribute(urnName,urnXRelay);
	xoap::SOAPElement childElement2 = childElement.addChildElement(toName);
	childElement2.addAttribute(urlName,url);
	childElement2.addAttribute(urnName,urn);
	//
      }
    //
    // Create body
    //
    xoap::SOAPBody body = envelope.getBody();
    xoap::SOAPName cmd  = envelope.createName(command,"xdaq","urn:xdaq-soap:3.0");
    xoap::SOAPElement queryElement = body.addBodyElement(cmd);
    //
    if(setting != "" ) {
      xoap::SOAPName att  = envelope.createName("Setting");
      queryElement.addAttribute(att,setting);
    }
    //
    //
    // msg->writeTo(cout);
    //
    return msg;
    //
}


  // Post XRelay SOAP message to XRelay application
void EmuFCrateManager::relayMessage (xoap::MessageReference msg) throw (xgi::exception::Exception)
{
    // Retrieve the list of applications expecting this command and build the XRelay header
    xoap::MessageReference reply;
    try
      {
	// Get the Xrelay application descriptor and post the message:
	xdaq::ApplicationDescriptor * xrelay = getApplicationContext()->getDefaultZone()->
	  getApplicationGroup("broker")->getApplicationDescriptor(getApplicationContext()->getContextDescriptor(),4);

	// Depricated
	//reply = getApplicationContext()->postSOAP(msg, xrelay);
	reply = getApplicationContext()->postSOAP(msg, *getApplicationDescriptor(), *xrelay);
	xoap::SOAPBody body = reply->getSOAPPart().getEnvelope().getBody();
	if (body.hasFault()) {
	  cout << "EmuFcrateManager: No connection. " << body.getFault().getFaultString() << endl;
	} else {
	  //reply->writeTo(cout);
	  //cout << endl;
	}
      }
    catch (xdaq::exception::Exception& e)
      {
	XCEPT_RETHROW (xgi::exception::Exception, "Cannot relay message", e);
      }

     cout << " ** EmuFcrateManager: Finish relayMessage" << endl;

}




/*
void EmuFCrateManager::SendSOAPMessageConfigureLTC(xgi::Input * in, xgi::Output * out )
  throw (xgi::exception::Exception)
{
      cout << "SendSOAPMessage Configure LTC" << endl;
      xoap::MessageReference msg = xoap::createMessage();
      xoap::SOAPPart soap = msg->getSOAPPart();
      xoap::SOAPEnvelope envelope = soap.getEnvelope();
      xoap::SOAPBody body = envelope.getBody();
      xoap::SOAPName command = envelope.createName("Configure","xdaq", "urn:xdaq-soap:3.0");
      body.addBodyElement(command);
      //
      printf(" EmuFCrateManager: ConfigureLTC \n");
      try
	{
	  xdaq::ApplicationDescriptor * d =
	    getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptor("LTCControl", 0);
	  xoap::MessageReference reply    = getApplicationContext()->postSOAP(msg, *getApplicationDescriptor(), *d);
	  xoap::SOAPBody body = reply->getSOAPPart().getEnvelope().getBody();
	  reply->writeTo(cout);
	  cout << endl;
	  if (body.hasFault()) {
	    cout << "Fault = " << body.getFault().getFaultString() << endl;
	  }
	  //
	}
      catch (xdaq::exception::Exception& e)
	{
	  XCEPT_RETHROW (xgi::exception::Exception, "Cannot send message", e);
	}
      //
      this->Default(in,out);
      //
}


void EmuFCrateManager::SendSOAPMessageExecuteSequence(xgi::Input * in, xgi::Output * out )
  throw (xgi::exception::Exception)
{
      //
      cout << "SendSOAPMessage Execute Sequence" << endl;
      //
      xoap::MessageReference msg = xoap::createMessage();
      xoap::SOAPPart soap = msg->getSOAPPart();
      xoap::SOAPEnvelope envelope = soap.getEnvelope();
      xoap::SOAPBody body = envelope.getBody();
      xoap::SOAPName command = envelope.createName("ExecuteSequence","xdaq", "urn:xdaq-soap:3.0");
      xoap::SOAPBodyElement elem = body.addBodyElement(command);
      xoap::SOAPName name = xoap::SOAPName("Param", "xdaq", XDAQ_NS_URI);
      elem.addAttribute(name, "Dmb_cfeb_calibrate0");
      //
      try
	{
	  xdaq::ApplicationDescriptor * d =
	    getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptor("LTCControl", 0);
	  xoap::MessageReference reply    = getApplicationContext()->postSOAP(msg, *getApplicationDescriptor(), *d);
	  xoap::SOAPBody body = reply->getSOAPPart().getEnvelope().getBody();
	  cout << endl;
	  if (body.hasFault()) {
	    cout << "Fault = " << body.getFault().getFaultString() << endl;
	  } else {
	    reply->writeTo(cout);
	  }
	  //
	}
      catch (xdaq::exception::Exception& e)
	{
	  XCEPT_RETHROW (xgi::exception::Exception, "Cannot send message", e);
	}
      //
      printf(" SendSoapMessageExecuteSequence \n");
      this->Default(in,out);
      //this->SendSOAPMessageExecuteSequence(in,out);
      //
}



xoap::MessageReference EmuFCrateManager::ExecuteCommandMessage(string port)
{
      xoap::MessageReference msg = xoap::createMessage();
      xoap::SOAPPart soap = msg->getSOAPPart();
      xoap::SOAPEnvelope envelope = soap.getEnvelope();
      xoap::SOAPBody body = envelope.getBody();
      xoap::SOAPName command  = envelope.createName("executeCommand","xdaq", "urn:xdaq-soap:3.0");
      xoap::SOAPName user     = envelope.createName("user", "", "http://www.w3.org/2001/XMLSchema-instance");
      xoap::SOAPName argv     = envelope.createName("argv", "", "http://www.w3.org/2001/XMLSchema-instance");
      xoap::SOAPName execPath = envelope.createName("execPath", "", "http://www.w3.org/2001/XMLSchema-instance");
      //
      xoap::SOAPName ldLibraryPath = envelope.createName("LD_LIBRARY_PATH", "", "http://www.w3.org/2001/XMLSchema-instance");
      xoap::SOAPName xdaqRoot = envelope.createName("XDAQ_ROOT", "", "http://www.w3.org/2001/XMLSchema-instance");
      xoap::SOAPName home = envelope.createName("HOME", "", "http://www.w3.org/2001/XMLSchema-instance");
      xoap::SOAPName environment = envelope.createName("EnvironmentVariable","","http://www.w3.org/2001/XMLSchema-instance");
      //
      xoap::SOAPBodyElement itm = body.addBodyElement(command);
      itm.addAttribute(execPath,"/home/meydev/DAQkit/3.9/TriDAS/daq/xdaq/bin/linux/x86/xdaq.exe");
      itm.addAttribute(user,"meydev");
      ostringstream dummy;
      dummy << "-p " << port << " -c /home/cscpc/DAQkit/v3.9/TriDAS/emu/emuDCS/FCrate/xml/EmuCluster.xml";
      itm.addAttribute(argv,dummy.str());
      xoap::SOAPElement itm2 = itm.addChildElement(environment);
      itm2.addAttribute(home,"/home/meydev");
      itm2.addAttribute(xdaqRoot,"/home/meydev/DAQkit/3.9/TriDAS");
      itm2.addAttribute(ldLibraryPath,"/home/meydev/DAQkit/3.9/TriDAS/emu/emuDCS/FCrate/lib/linux/x86:/lib/linux/x86:/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/xerces/linuxx86/lib:/home/meydev/DAQkit/3.9/TriDAS/daq/exter:/home/meydev/DAQkit/3.9/TriDAS/daq/xdaq/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/xdata/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/log4cplus/linuxx86/lib:/home/meydev/DAQkit/3.9/TriDAS/daq/toolbox/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/xoap/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/cgicc/linuxx86/lib:/home/meydev/DAQkit/3.9/TriDAS/daq/xcept/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/xgi/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/pt/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/mimetic/linuxx86/lib:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/log4cplus/xmlappender/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/log4cplus/udpappender/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/pt/soap/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/pt/tcp/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/emu/extern/dim/linuxx86/linux:/home/meydev/DAQkit/3.9/TriDAS/emu/emuDCS/e2p/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/emu/cscSV/lib/linux/x86:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/asyncresolv/linuxx86/lib:/home/meydev/DAQkit/3.9/TriDAS/daq/extern/oracle/linuxx86");
      //
      return msg;
      //
}


void EmuFCrateManager::SendSOAPMessageConfigureXRelay(xgi::Input * in, xgi::Output * out )
  throw (xgi::exception::Exception)
{
      //
//JRGtry:      SendSOAPMessageXRelaySimple("Configure","");
      //
      // Now check
      //
      ConfigureState_ = "Failed";
      //
      for(int i=0;i<20; i++) {
	int compare=-1;
	compare = CompareEmuFCrateState("Configured");
	//
	std::set<xdaq::ApplicationDescriptor * >  descriptor =
	  getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("EmuFCrate");
	//
	if ( compare == (int) descriptor.size() ) {
	  ConfigureState_ = "Configured";
	  break;
	}
	//
      }
      //
      this->Default(in,out);
      //
}
  //


void EmuFCrateManager::SendSOAPMessageCalibrationXRelay(xgi::Input * in, xgi::Output * out )
  throw (xgi::exception::Exception)
{
      //
      SendSOAPMessageXRelaySimple("Calibration","Reset Now");
      //
      for (int i=0; i<20; i++) {
	int compare = -1;
	ostringstream output;
	output << "Next Setting " << i ;
	SendSOAPMessageXRelaySimple("Calibration",output.str());
	while (compare!=1){
	  compare = CompareEmuFCrateCalibrationState(output.str());
	  ostringstream compare_string;
	  compare_string << "compare " <<  compare << endl;
	  LOG4CPLUS_INFO(getApplicationLogger(), compare_string.str());
	}
        LTCDone=0;
	this->SendSOAPMessageExecuteSequence(in,out);
	// while(LTCDone==0){
        //  output << "waiting for LTCResponse" << endl;
	//  ::usleep(200);
        // }
      }
      //
      this->Default(in,out);
      //
}
*/



void EmuFCrateManager::SendSOAPMessageXRelaySimple(string command,string setting)
{
    //
  cout << " ** EmuFCrateManager: inside SendSOAPMessageXRelaySimple" << endl ;
      std::set<xdaq::ApplicationDescriptor * >  descriptors =
      getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("EmuFCrate");
    //
  cout << " ** EmuFCrateManager: SendSOAPMessageXRelaySimple, got EmuFCrate App" << endl ;
      xoap::MessageReference configure = createXRelayMessage(command,setting,descriptors);
  cout << " ** EmuFCrateManager: SendSOAPMessageXRelaySimple, created XRelayMessage" << endl ;
  //  cout << configure.toString() << endl;

      this->relayMessage(configure);
}



void EmuFCrateManager::SendSOAPMessageXRelayReturn(string command,string setting)
{
    //
  cout << " ** EmuFCrateManager: inside SendSOAPMessageXRelayReturn" << endl ;
      std::set<xdaq::ApplicationDescriptor * >  descriptors =
      getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("CSCSupervisor");
    //
  cout << " ** EmuFCrateManager: SendSOAPMessageXRelayReturn, got EmuFCrate App" << endl ;
      xoap::MessageReference configure = createXRelayMessage(command,setting,descriptors);
  cout << " ** EmuFCrateManager: SendSOAPMessageXRelayReturn, created XRelayMessage" << endl ;
  //  cout << configure.toString() << endl;

      this->relayMessage(configure);
}



void EmuFCrateManager::SendSOAPMessageConfigure(xgi::Input * in, xgi::Output * out )
  throw (xgi::exception::Exception)
{
    //
    cout << "EmuFcrateManager: SendSOAPMessage Configure" << endl;
    //
    xoap::MessageReference msg = xoap::createMessage();
    xoap::SOAPPart soap = msg->getSOAPPart();
    xoap::SOAPEnvelope envelope = soap.getEnvelope();
    xoap::SOAPBody body = envelope.getBody();
    xoap::SOAPName command = envelope.createName("Configure","xdaq", "urn:xdaq-soap:3.0");
    body.addBodyElement(command);
    //
    try
      {
	std::set<xdaq::ApplicationDescriptor * >  descriptors =
	  getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("EmuFCrate");
	//
	std::set <xdaq::ApplicationDescriptor *>::iterator itDescriptor;
	for ( itDescriptor = descriptors.begin(); itDescriptor != descriptors.end(); itDescriptor++ )
	  {
	  	// Depricated
	    //xoap::MessageReference reply    = getApplicationContext()->postSOAP(msg, (*itDescriptor));
	    xoap::MessageReference reply    = getApplicationContext()->postSOAP(msg, *getApplicationDescriptor(), **itDescriptor);
	  }
	//
      }
    catch (xdaq::exception::Exception& e)
      {
	XCEPT_RETHROW (xgi::exception::Exception, "Cannot send message", e);
      }
    //
    this->Default(in,out);
    //
}


  //
//This is copied from CSCSupervisor::sendcommand;
void EmuFCrateManager::PCsendCommand(string command, string klass)
	throw (xoap::exception::Exception, xdaq::exception::Exception)
{
	// Exceptions:
	// xoap exceptions are thrown by analyzeReply() for SOAP faults.
	// xdaq exceptions are thrown by postSOAP() for socket level errors.

	// find applications
	std::set<xdaq::ApplicationDescriptor *> apps;
	try {
		apps = getApplicationContext()->getDefaultZone()->getApplicationDescriptors(klass);
	} catch (xdaq::exception::ApplicationDescriptorNotFound e) {
		return; // Do nothing if the target doesn't exist
	}

	// prepare a SOAP message
	xoap::MessageReference message = PCcreateCommandSOAP(command);
	xoap::MessageReference reply;

	// send the message one-by-one
	std::set<xdaq::ApplicationDescriptor *>::iterator i = apps.begin();
	for (; i != apps.end(); ++i) {
		// postSOAP() may throw an exception when failed.
		// Depricated
		//reply = getApplicationContext()->postSOAP(message, *i);
		reply = getApplicationContext()->postSOAP(message, *getApplicationDescriptor(), **i);

		//      PCanalyzeReply(message, reply, *i);
	}
}


//
//This is copied from CSCSupervisor::createCommandSOAP
xoap::MessageReference EmuFCrateManager::PCcreateCommandSOAP(string command)
{
    xoap::MessageReference message = xoap::createMessage();
    xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
    xoap::SOAPName name = envelope.createName(command, "xdaq", "urn:xdaq-soap:3.0");
    envelope.getBody().addBodyElement(name);

    return message;
}


void EmuFCrateManager::sendCommand(string command, string klass, int instance)
		throw (xoap::exception::Exception, xdaq::exception::Exception)
{
	// Exceptions:
	// xoap exceptions are thrown by analyzeReply() for SOAP faults.
	// xdaq exceptions are thrown by postSOAP() for socket level errors.

	// find applications
  cout << "  * EmuFCrateManager: inside sendCommand" << endl;
  xdaq::ApplicationDescriptor *app;
  try {
    app = getApplicationContext()->getDefaultZone()
      ->getApplicationDescriptor(klass, instance);
    cout << "  * EmuFCrateManager: sendCommand, got application " << klass << endl;
  } catch (xdaq::exception::ApplicationDescriptorNotFound e) {
    cout << "  * EmuFCrateManager: sendCommand, application not found! " << klass << endl;
    return; // Do nothing if the target doesn't exist
  }

	// prepare a SOAP message
  xoap::MessageReference message = createCommandSOAP(command);
  cout << "  * EmuFCrateManager: sendCommand, created Soap message" << endl;
  xoap::MessageReference reply;

	// send the message
	// postSOAP() may throw an exception when failed.
  cout << "  * EmuFCrateManager: sendCommand, sending Soap message" << endl;
  // Depricated
  //reply = getApplicationContext()->postSOAP(message, app);
  reply = getApplicationContext()->postSOAP(message, *getApplicationDescriptor(), *app);
  cout << "  * EmuFCrateManager: sendCommand, got Soap reply " << endl;
  cout << "            tts_bits_=" << tts_bits_.toString() << endl;

  //analyzeReply(message, reply, app);
  cout << "  * EmuFCrateManager: sendCommand, analyzed message,reply,app" << endl;
  cout << "            tts_bits_=" << tts_bits_.toString() << endl;
}


xoap::MessageReference EmuFCrateManager::createCommandSOAP(string command)
{
  //cout << "  - EmuFCrateManager:  inside createCommandSOAP " << endl;
	xoap::MessageReference message = xoap::createMessage();
	xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
	xoap::SOAPName name = envelope.createName(command, "xdaq", XDAQ_NS_URI);
	envelope.getBody().addBodyElement(name);

	return message;
}

/* PRK Replaced in LocalEmuApplication.h
void EmuFCrateManager::setParameter(
		string klass, string name, string type, unsigned int value)
{
  // need to convert value to a string:
  //        string value_str = "";
        char value_str[35];
	sprintf(value_str,"%d",value);
	// find applications
	std::set<xdaq::ApplicationDescriptor *> apps;
	try {
		apps = getApplicationContext()->getDefaultZone()
				->getApplicationDescriptors(klass);
	} catch (xdaq::exception::ApplicationDescriptorNotFound e) {
		return; // Do nothing if the target doesn't exist
	}

	// prepare a SOAP message
	xoap::MessageReference message = createParameterSetSOAP(
			klass, name, type, value_str);
	xoap::MessageReference reply;

	// send the message one-by-one
	std::set<xdaq::ApplicationDescriptor *>::iterator i = apps.begin();
	for (; i != apps.end(); ++i) {
		// Depricated
		//reply = getApplicationContext()->postSOAP(message, *i);
		reply = getApplicationContext()->postSOAP(message, *getApplicationDescriptor(), **i);
		analyzeReply(message, reply, *i);
	}
}

xoap::MessageReference EmuFCrateManager::createParameterSetSOAP(
		string klass, string name, string type, string value)
{
	xoap::MessageReference message = xoap::createMessage();
	xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
	envelope.addNamespaceDeclaration("xsi", NS_XSI);

	xoap::SOAPName command = envelope.createName(
			"ParameterSet", "xdaq", XDAQ_NS_URI);
	xoap::SOAPName properties = envelope.createName(
			"properties", klass, "urn:xdaq-application:" + klass);
	xoap::SOAPName parameter = envelope.createName(
			name, klass, "urn:xdaq-application:" + klass);
	xoap::SOAPName xsitype = envelope.createName("type", "xsi", NS_XSI);

	xoap::SOAPElement properties_e = envelope.getBody()
			.addBodyElement(command)
			.addChildElement(properties);
	properties_e.addAttribute(xsitype, "soapenc:Struct");

	xoap::SOAPElement parameter_e = properties_e.addChildElement(parameter);
	parameter_e.addAttribute(xsitype, type);
	parameter_e.addTextNode(value);

	return message;
}

void EmuFCrateManager::analyzeReply(
		xoap::MessageReference message, xoap::MessageReference reply,
		xdaq::ApplicationDescriptor *app)
{
	string message_str, reply_str;
	//cout << "  - EmuFCrateManager:  inside analyzeReply " << endl;

	reply->writeTo(reply_str);
	ostringstream s;
	s << "Reply from "
			<< app->getClassName() << "(" << app->getInstance() << ")" << endl
			<< reply_str;
	//	last_log_.add(s.str());
	LOG4CPLUS_DEBUG(getApplicationLogger(), reply_str);

	xoap::SOAPBody body = reply->getSOAPPart().getEnvelope().getBody();

	// do nothing when no fault
	if (!body.hasFault()) {
	  //cout << "  - EmuFCrateManager:  analyzeReply, body OK " << endl;
	  return;
	}
	//cout << "  - EmuFCrateManager:  analyzeReply, body has fault " << endl;

	ostringstream error;

	error << "SOAP message: " << endl;
	message->writeTo(message_str);
	error << message_str << endl;
	error << "Fault string: " << endl;
	error << reply_str << endl;

	LOG4CPLUS_ERROR(getApplicationLogger(), error.str());
	XCEPT_RAISE(xoap::exception::Exception, "SOAP fault: \n" + reply_str);
	cout << "  - EmuFCrateManager:  analyzeReply, wrote to Error Log" << endl;

	return;
}
*/


//
/*This is copied from CSCSupervisor::analyzeReply
  void EmuFCrateManager::PCanalyzeReply(
		xoap::MessageReference message, xoap::MessageReference reply,
		xdaq::ApplicationDescriptor *app)
  {
    string message_str, reply_str;

    reply->writeTo(reply_str);
    ostringstream s;
    s << "Reply from "
      << app->getClassName() << "(" << app->getInstance() << ")" << endl
      << reply_str;
    last_log_.add(s.str());
    LOG4CPLUS_DEBUG(getApplicationLogger(), reply_str);

    xoap::SOAPBody body = reply->getSOAPPart().getEnvelope().getBody();

    // do nothing when no fault
    if (!body.hasFault()) { return; }

    ostringstream error;

    error << "SOAP message: " << endl;
    message->writeTo(message_str);
    error << message_str << endl;
    error << "Fault string: " << endl;
    error << reply_str << endl;

    LOG4CPLUS_ERROR(getApplicationLogger(), error.str());
    XCEPT_RAISE(xoap::exception::Exception, "SOAP fault: \n" + reply_str);

    return;
  }
*/

void EmuFCrateManager::CheckEmuFCrateState() {
  //
  cout << " inside EmuFCrateManager::CheckEmuFCrateState " << endl ;

  std::set<xdaq::ApplicationDescriptor * >  descriptor =
    getApplicationContext()->getDefaultZone()->getApplicationGroup("default")->getApplicationDescriptors("EmuFCrate");
  //
  std::set<xdaq::ApplicationDescriptor *>::iterator itDescriptor;
  for ( itDescriptor = descriptor.begin(); itDescriptor != descriptor.end(); itDescriptor++ ) {
    string classNameStr = (*itDescriptor)->getClassName();
    cout << classNameStr << " " << endl ;
    string url = (*itDescriptor)->getContextDescriptor()->getURL();
    cout << url << " " << endl;
    string urn = (*itDescriptor)->getURN();
    cout << urn << endl;
    //
    xoap::MessageReference reply;
    //
    bool failed = false ;
    //

    cout << " EmuFCrateManager::CheckEmuFCrateState-- about to try Query" << endl;
    try {
      xoap::MessageReference msg   = QueryFCrateInfoSpace();
      // Depricated
      //reply = getApplicationContext()->postSOAP(msg, (*itDescriptor));
      reply = getApplicationContext()->postSOAP(msg, *getApplicationDescriptor(), **itDescriptor);
      cout << " EmuFCrateManager::CheckEmuFCrateState-- try Query OK? " << endl;
    }
    //
    catch (xdaq::exception::Exception& e) {
      cout << " EmuFCrateManager::CheckEmuFCrateState-- try Query Exception! " << endl;
      // *out << cgicc::span().set("style","color:red");
      cout << "(Not running)"<<endl;
      // *out << cgicc::span();
      failed = true;
    }
    //
    if(!failed) {
      //
      cout << " EmuFCrateManager::CheckEmuFCrateState-- inside Query OK " << endl;
      xoap::SOAPBody body = reply->getSOAPPart().getEnvelope().getBody();
      if (body.hasFault()) {
	cout << "No connection. " << body.getFault().getFaultString() << endl;
      } else {
      cout << " EmuFCrateManager::CheckEmuFCrateState-- inside Query Failed " << endl;
	//	*out << cgicc::span().set("style","color:green");
	cout << "(" << extractState(reply) << ")";
	cout << cgicc::span();
      }
      //
    }
    //
    // *out << cgicc::br();
    //
  }
  //
}

string EmuFCrateManager::extractState(xoap::MessageReference message) {
  //
  //LOG4CPLUS_INFO(getApplicationLogger(), "extractState");
  //
  xoap::SOAPElement root = message->getSOAPPart().getEnvelope().getBody().getChildElements(*(new xoap::SOAPName("ParameterGetResponse", "", "")))[0];
  xoap::SOAPElement properties = root.getChildElements(*(new xoap::SOAPName("properties", "", "")))[0];
  xoap::SOAPElement state = properties.getChildElements(*(new xoap::SOAPName("stateName", "", "")))[0];
  //
  return state.getValue();
}

// PGK Stolen from CSCSupervisor.cc
// PGK We don't need this with the new getParameter in LocalEmuAppliction!
/*
EmuFCrateManager::StateTable::StateTable(EmuFCrateManager *fedmgr) : fedmgr_(fedmgr) {}

void EmuFCrateManager::StateTable::addApplication(string klass)
{
	// find applications
  cout << " enter: StateTable::addApplication " << klass << endl;
        std::set<xdaq::ApplicationDescriptor *> apps;
	try {
		apps = fedmgr_->getApplicationContext()->getDefaultZone()->getApplicationDescriptors(klass);
	} catch (xdaq::exception::ApplicationDescriptorNotFound e) {
		return; // Do nothing if the target doesn't exist
	}
	cout << " StateTable::addApplication adding to table " << endl;
        // add to the table
	std::set<xdaq::ApplicationDescriptor *>::iterator i;
	for (i = apps.begin(); i != apps.end(); ++i) {
		table_.push_back(pair<xdaq::ApplicationDescriptor *, string>(*i, "NULL"));
	}
}


void EmuFCrateManager::StateTable::refresh()
{
	//cout << "<PGK> StateTable::refresh()" << endl;

	string klass = "";
	xoap::MessageReference message, reply;

	std::map<string, int> statusCount;

	vector<pair<xdaq::ApplicationDescriptor *, string> >::iterator i;
	for (i = table_.begin(); i != table_.end(); ++i) {
		// PGK All of this is replaced below with a super convenient function.
		try {
			reply = fedmgr_->getParameters(i->first);
			i->second = (fedmgr_->readParameter<xdata::String>(reply,"State")).toString();
		} catch (xdaq::exception::Exception e) {
			i->second = STATE_UNKNOWN;
			fedmgr_->state_ = STATE_UNKNOWN;
		} catch (...) {
			LOG4CPLUS_DEBUG(fedmgr_->getApplicationLogger(), "Exception with " << i->first->getClassName());
			i->second = STATE_UNKNOWN;
			fedmgr_->state_ = STATE_UNKNOWN;
		}
		// PGK My status should be the consensus of the FCrates I control.
		// If the states don't match, then flag an error or something.
		statusCount[i->second]++;
	}

	if (statusCount.size() != 1) {
		fedmgr_->state_ = STATE_UNKNOWN;
	}

}

string EmuFCrateManager::StateTable::getState(string klass, unsigned int instance)
{
	string state = "";

	vector<pair<xdaq::ApplicationDescriptor *, string> >::iterator i =
			table_.begin();
	for (; i != table_.end(); ++i) {
		if (klass == i->first->getClassName()
				&& instance == i->first->getInstance()) {
			state = i->second;
			break;
		}
	}

	return state;
}

bool EmuFCrateManager::StateTable::isValidState(string expected)
{
	bool is_valid = true;

	vector<pair<xdaq::ApplicationDescriptor *, string> >::iterator i = table_.begin();
	for (; i != table_.end(); ++i) {
		string checked = expected;
		string klass = i->first->getClassName();

		if (klass == "TTCciControl" || klass == "LTCControl") {
			if (expected == "Configured") { checked = "Ready"; }
		}

		if (i->second != checked) {
			is_valid = false;
			break;
		}
	}

	return is_valid;
}

void EmuFCrateManager::StateTable::webOutput(xgi::Output *out, string fedmgr_state)
		throw (xgi::exception::Exception)
{
	refresh();
	*out << table() << tbody() << endl;

	// My state
	*out << tr();
	*out << td() << "EmuFCrateManager" << "(" << "0" << ")" << td();
	*out << td().set("class", fedmgr_state) << fedmgr_state << td();
	*out << tr() << endl;

	// Applications
	vector<pair<xdaq::ApplicationDescriptor *, string> >::iterator i =
			table_.begin();
	for (; i != table_.end(); ++i) {
		string klass = i->first->getClassName();
		int instance = i->first->getInstance();
		string state = i->second;

		*out << tr();
		*out << td() << klass << "(" << instance << ")" << td();
		*out << td().set("class", state) << state << td();
		*out << tr() << endl;
	}

	*out << tbody() << table() << endl;
}

xoap::MessageReference EmuFCrateManager::StateTable::createStateSOAP(
		string klass)
{
	xoap::MessageReference message = xoap::createMessage();
	xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
	envelope.addNamespaceDeclaration("xsi", NS_XSI);

	xoap::SOAPName command = envelope.createName(
			"ParameterGet", "xdaq", XDAQ_NS_URI);
	xoap::SOAPName properties = envelope.createName(
			"properties", klass, "urn:xdaq-application:" + klass);
	xoap::SOAPName parameter = envelope.createName(
			"stateName", klass, "urn:xdaq-application:" + klass);
	xoap::SOAPName xsitype = envelope.createName("type", "xsi", NS_XSI);

	xoap::SOAPElement properties_e = envelope.getBody()
			.addBodyElement(command)
			.addChildElement(properties);
	properties_e.addAttribute(xsitype, "soapenc:Struct");

	xoap::SOAPElement parameter_e = properties_e.addChildElement(parameter);
	parameter_e.addAttribute(xsitype, "xsd:string");

	return message;
}

string EmuFCrateManager::StateTable::extractState(xoap::MessageReference message, string klass)
{
	xoap::SOAPElement root = message->getSOAPPart()
			.getEnvelope().getBody().getChildElements(
			*(new xoap::SOAPName("ParameterGetResponse", "", "")))[0];
	xoap::SOAPElement properties = root.getChildElements(
			*(new xoap::SOAPName("properties", "", "")))[0];
	xoap::SOAPElement state = properties.getChildElements(
			*(new xoap::SOAPName("stateName", "", "")))[0];

	//cout << "<PGK> StateTable::extractState from klass " << klass << ": " << state.getValue() << endl;
	return state.getValue();
}
*/



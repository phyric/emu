// $Id: EmuPeripheralCrateCommand.cc

#include "EmuPeripheralCrateCommand.h"

#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <unistd.h> // for sleep()
#include <sstream>
#include <cstdlib>
#include <iomanip>
#include <time.h>

using namespace cgicc;
using namespace std;

/////////////////////////////////////////////////////////////////////
// Instantiation and main page
/////////////////////////////////////////////////////////////////////
EmuPeripheralCrateCommand::EmuPeripheralCrateCommand(xdaq::ApplicationStub * s): EmuApplication(s)
{	
  //
  MyController = 0;
  //thisTMB = 0;
  //thisDMB = 0;
  thisCCB = 0;
  thisMPC = 0;
  rat = 0;
  alct = 0;
  nTrigger_ = 100;
  MenuMonitor_ = 2;
  //
  tmb_vme_ready = -1;
  //
  all_crates_ok = -1;
  for (int i=0; i<60; i++) {
    crate_check_ok[i] = -1;
    ccb_check_ok[i] = -1;
    mpc_check_ok[i] = -1;
    for (int j=0; j<9; j++) {
      alct_check_ok[i][j] = -1;
      tmb_check_ok[i][j] = -1;
      dmb_check_ok[i][j] = -1;
    }
  }
  //
  xgi::bind(this,&EmuPeripheralCrateCommand::Default, "Default");
  xgi::bind(this,&EmuPeripheralCrateCommand::MainPage, "MainPage");
  //
  //------------------------------------------------------
  // bind buttons -> other pages
  //------------------------------------------------------
  xgi::bind(this,&EmuPeripheralCrateCommand::CheckCrates, "CheckCrates");
  xgi::bind(this,&EmuPeripheralCrateCommand::CheckCratesConfiguration, "CheckCratesConfiguration");
  xgi::bind(this, &EmuPeripheralCrateCommand::FastConfigCrates, "FastConfigCrates");

  // SOAP call-back functions, which relays to *Action method.
  //-----------------------------------------------------------
  xoap::bind(this, &EmuPeripheralCrateCommand::onConfigure, "Configure", XDAQ_NS_URI);
  xoap::bind(this, &EmuPeripheralCrateCommand::onEnable,    "Enable",    XDAQ_NS_URI);
  xoap::bind(this, &EmuPeripheralCrateCommand::onDisable,   "Disable",   XDAQ_NS_URI);
  xoap::bind(this, &EmuPeripheralCrateCommand::onHalt,      "Halt",      XDAQ_NS_URI);
  //
  //-------------------------------------------------------------
  // fsm_ is defined in EmuApplication
  //-------------------------------------------------------------
  fsm_.addState('H', "Halted",     this, &EmuPeripheralCrateCommand::stateChanged);
  fsm_.addState('C', "Configured", this, &EmuPeripheralCrateCommand::stateChanged);
  fsm_.addState('E', "Enabled",    this, &EmuPeripheralCrateCommand::stateChanged);
  //
  fsm_.addStateTransition('H', 'C', "Configure", this, &EmuPeripheralCrateCommand::configureAction);
  fsm_.addStateTransition('C', 'C', "Configure", this, &EmuPeripheralCrateCommand::reConfigureAction);
  fsm_.addStateTransition('C', 'E', "Enable",    this, &EmuPeripheralCrateCommand::enableAction);
  fsm_.addStateTransition('E', 'E', "Enable",    this, &EmuPeripheralCrateCommand::enableAction);
  fsm_.addStateTransition('E', 'C', "Disable",   this, &EmuPeripheralCrateCommand::disableAction);
  fsm_.addStateTransition('C', 'H', "Halt",      this, &EmuPeripheralCrateCommand::haltAction);
  fsm_.addStateTransition('E', 'H', "Halt",      this, &EmuPeripheralCrateCommand::haltAction);
  fsm_.addStateTransition('H', 'H', "Halt",      this, &EmuPeripheralCrateCommand::haltAction);
  //
  fsm_.setInitialState('H');
  fsm_.reset();    
  //
  // state_ is defined in EmuApplication
  state_ = fsm_.getStateName(fsm_.getCurrentState());
  //
  //----------------------------
  // initialize variables
  //----------------------------
  myParameter_ =  0;
  //
  xmlFile_ = "config.xml" ;
  //
  for(unsigned int dmb=0; dmb<9; dmb++) {
    L1aLctCounter_.push_back(0);
    CfebDavCounter_.push_back(0);
    TmbDavCounter_.push_back(0);
    AlctDavCounter_.push_back(0);
  }
  //
  CCBRegisterValue_ = -1;
  Operator_ = "Operator";
  RunNumber_= "-1";
  CalibrationState_ = "None";
  //
  for(int i=0; i<9;i++) {
    OutputStringDMBStatus[i] << "DMB-CFEB Status " << i << " output:" << std::endl;
    OutputStringTMBStatus[i] << "TMB-RAT Status " << i << " output:" << std::endl;
  }
  CrateTestsOutput << "Crate Tests output:" << std::endl;
  //
  this->getApplicationInfoSpace()->fireItemAvailable("runNumber", &runNumber_);
  this->getApplicationInfoSpace()->fireItemAvailable("xmlFileName", &xmlFile_);
  
  // for XMAS minotoring:

  Monitor_On_ = false;
  Monitor_Ready_ = false;

  global_config_states[0]="UnConfiged";
  global_config_states[1]="Configuring";
  global_config_states[2]="Configed";
  global_run_states[0]="Halted";
  global_run_states[1]="Enabled";
  current_config_state_=0;
  current_run_state_=0;
  total_crates_=0;
  this_crate_no_=0;

  parsed=0;
}

void EmuPeripheralCrateCommand::MainPage(xgi::Input * in, xgi::Output * out ) 
{
  //
  std::string LoggerName = getApplicationLogger().getName() ;
  std::cout << "Name of Logger is " <<  LoggerName <<std::endl;
  //
  LOG4CPLUS_INFO(getApplicationLogger(), "EmuPeripheralCrate ready");
  //
  MyHeader(in,out,"EmuPeripheralCrateCommand");

  if(!parsed) ParsingXML();

  *out << "Total Crates : ";
  *out << total_crates_ << cgicc::br() << std::endl ;
  unsigned int active_crates=0;
  for(unsigned i=0; i<crateVector.size(); i++)
     if(crateVector[i]->IsAlive()) active_crates++;
  if( active_crates <= total_crates_) 
     *out << cgicc::b(" Active Crates: ") << active_crates << cgicc::br() << std::endl ;
 
 // Crate Status
  *out << cgicc::span().set("style","color:blue");
  *out << cgicc::b(cgicc::i("System Status: ")) ;
  *out << global_config_states[current_config_state_] << "  ";
  *out << global_run_states[current_run_state_]<< cgicc::br() << std::endl ;
  *out << cgicc::span() << std::endl ;
  //
  *out << cgicc::table().set("border","0");
    //
  *out << cgicc::td();
  std::string FastConfigureAll = toolbox::toString("/%s/FastConfigCrates",getApplicationDescriptor()->getURN().c_str());
  *out << cgicc::form().set("method","GET").set("action",FastConfigureAll) << std::endl ;
  *out << cgicc::input().set("type","submit").set("value","Crates Power-up Init") << std::endl ;
  *out << cgicc::form() << std::endl ;
  *out << cgicc::td();

  *out << cgicc::td();
  std::string CheckCrates = toolbox::toString("/%s/CheckCrates",getApplicationDescriptor()->getURN().c_str());
  *out << cgicc::form().set("method","GET").set("action",CheckCrates) << std::endl ;
  *out << cgicc::input().set("type","submit").set("value","Check Crate Controllers") << std::endl ;
  *out << cgicc::form() << std::endl ;
  *out << cgicc::td();

  *out << cgicc::td();
  std::string CheckCratesConfiguration = toolbox::toString("/%s/CheckCratesConfiguration",getApplicationDescriptor()->getURN().c_str());
  *out << cgicc::form().set("method","GET").set("action",CheckCratesConfiguration) << std::endl ;
  if (all_crates_ok == 1) {
    *out << cgicc::input().set("type","submit").set("value","Check configuration of crates").set("style","color:green") << std::endl ;
  } else if (all_crates_ok == 0) {
    *out << cgicc::input().set("type","submit").set("value","Check configuration of crates").set("style","color:red") << std::endl ;
  } else if (all_crates_ok == -1) {
    *out << cgicc::input().set("type","submit").set("value","Check configuration of crates").set("style","color:blue") << std::endl ;
  }
  *out << cgicc::form() << std::endl ;
  *out << cgicc::td();

  *out << cgicc::table();
  //
  int initial_crate = current_crate_;
  //
  if (all_crates_ok == 0) {
    //
    for(unsigned crate_number=0; crate_number< crateVector.size(); crate_number++) {
      //
      SetCurrentCrate(crate_number);
      //
      *out << crateVector[crate_number]->GetLabel() << std::endl;
      //
      if (crate_check_ok[current_crate_] == 0) {
	//
	*out << cgicc::br() << cgicc::span().set("style","color:red");
	//
	if (ccb_check_ok[current_crate_] == 0) *out << "Config problem for CCB/TTC " << cgicc::br() << std::endl ;
	if (mpc_check_ok[current_crate_] == 0) *out << "Config problem for MPC " << cgicc::br() << std::endl ;
        //
	bool alct_ok = true;
	bool tmb_ok = true;
	bool dmb_ok = true;
	//
	for (unsigned chamber_index=0; chamber_index<(tmbVector.size()<9?tmbVector.size():9) ; chamber_index++) {
	  if (alct_check_ok[current_crate_][chamber_index] == 0) alct_ok = false;
	  if (tmb_check_ok[current_crate_][chamber_index] == 0)  tmb_ok = false;
	  if (dmb_check_ok[current_crate_][chamber_index] == 0)  dmb_ok = false;
	}
	//
	if (!alct_ok) {
	  //
	  *out << "Config problems for ALCT: " ;
	  for (unsigned chamber_index=0; chamber_index<(tmbVector.size()<9?tmbVector.size():9) ; chamber_index++) 
	    if (alct_check_ok[current_crate_][chamber_index] == 0) 
	      *out << thisCrate->GetChamber(tmbVector[chamber_index]->slot())->GetLabel().c_str() << ", ";
	  //
	  *out << cgicc::br() << std::endl ;
	}
	//
	if (!tmb_ok) {
	  //
	  *out << "Config problems for TMB: " ;
	  for (unsigned chamber_index=0; chamber_index<(tmbVector.size()<9?tmbVector.size():9) ; chamber_index++) 
	    if (tmb_check_ok[current_crate_][chamber_index] == 0) 
	      *out << thisCrate->GetChamber(tmbVector[chamber_index]->slot())->GetLabel().c_str() << ", ";
	  //
	  *out << cgicc::br() << std::endl ;
	}
	//
	if (!dmb_ok) {
	  //
	  *out << "Config problems for DMB: " ;
	  for (unsigned chamber_index=0; chamber_index<(tmbVector.size()<9?tmbVector.size():9) ; chamber_index++) 
	    if (dmb_check_ok[current_crate_][chamber_index] == 0) 
	      *out << thisCrate->GetChamber(tmbVector[chamber_index]->slot())->GetLabel().c_str() << ", ";
	  //
	  *out << cgicc::br() << std::endl ;
	}
	//
      } else if (crate_check_ok[current_crate_] == 1) {
	//
	*out << cgicc::span().set("style","color:green");
	*out << " OK" << cgicc::br();
      } else if (crate_check_ok[current_crate_] == -1) {
	//
	*out << cgicc::span().set("style","color:blue");
	*out << " Not checked" << cgicc::br();
      }
      *out << cgicc::span() << std::endl ;
    }
  }
  //
  SetCurrentCrate(initial_crate);
  //

  *out << cgicc::br() << cgicc::br() << std::endl; 
  *out << cgicc::b(cgicc::i("Configuration filename : ")) ;
  *out << xmlFile_.toString() << cgicc::br() << std::endl ;
  //
  //
}

// 
void EmuPeripheralCrateCommand::MyHeader(xgi::Input * in, xgi::Output * out, std::string title ) 
  throw (xgi::exception::Exception) {
  //
  *out << cgicc::HTMLDoctype(cgicc::HTMLDoctype::eStrict) << std::endl;
  *out << cgicc::html().set("lang", "en").set("dir","ltr") << std::endl;
  //
  //*out << cgicc::title(title) << std::endl;
  //*out << "<a href=\"/\"><img border=\"0\" src=\"/daq/xgi/images/XDAQLogo.gif\" title=\"XDAQ\" alt=\"\" style=\"width: 145px; height: 89px;\"></a>" << std::endl;
  //
  std::string myUrn = getApplicationDescriptor()->getURN().c_str();
  xgi::Utils::getPageHeader(out,title,myUrn,"","");
  //
}
//
void EmuPeripheralCrateCommand::Default(xgi::Input * in, xgi::Output * out ) 
  throw (xgi::exception::Exception) {
  *out << "<meta HTTP-EQUIV=\"Refresh\" CONTENT=\"0; URL=/" <<getApplicationDescriptor()->getURN()<<"/"<<"MainPage"<<"\">" <<endl;
}
//
/////////////////////////////////////////////////////////////////////
// SOAP Callback  
/////////////////////////////////////////////////////////////////////
//
xoap::MessageReference EmuPeripheralCrateCommand::onConfigure (xoap::MessageReference message) 
  throw (xoap::exception::Exception) {
  std::cout << "SOAP Configure" << std::endl;
  //
  fireEvent("Configure");
  //
  VerifyCratesConfiguration();
  //
  return createReply(message);
}
//
xoap::MessageReference EmuPeripheralCrateCommand::onEnable (xoap::MessageReference message) 
  throw (xoap::exception::Exception) {
  std::cout << "SOAP Enable" << std::endl;
  //
  current_run_state_ = 1;
  fireEvent("Enable");
  //
  return createReply(message);
}
//
xoap::MessageReference EmuPeripheralCrateCommand::onDisable (xoap::MessageReference message) 
  throw (xoap::exception::Exception) {
  std::cout << "SOAP Disable" << std::endl;
  //
  current_run_state_ = 0;
  fireEvent("Disable");
  //
  return createReply(message);
}
//
xoap::MessageReference EmuPeripheralCrateCommand::onHalt (xoap::MessageReference message) 
  throw (xoap::exception::Exception) {
  std::cout << "SOAP Halt" << std::endl;
  //
  fireEvent("Halt");
  //
  return createReply(message);
}
//
void EmuPeripheralCrateCommand::configureAction(toolbox::Event::Reference e) 
  throw (toolbox::fsm::exception::Exception) {
  //
  // currently do nothing
  //
  LOG4CPLUS_INFO(getApplicationLogger(), "Configure");
  //
}
//
void EmuPeripheralCrateCommand::configureFail(toolbox::Event::Reference e) 
  throw (toolbox::fsm::exception::Exception) {
  //
  // currently do nothing
  //
  LOG4CPLUS_INFO(getApplicationLogger(), "Failed");
  //
}
//
void EmuPeripheralCrateCommand::reConfigureAction(toolbox::Event::Reference e) 
  throw (toolbox::fsm::exception::Exception) {
  //
  //
  LOG4CPLUS_INFO(getApplicationLogger(), "reConfigure");
  std::cout << "reConfigure" << std::endl ;
  //
}
//
void EmuPeripheralCrateCommand::enableAction(toolbox::Event::Reference e) 
  throw (toolbox::fsm::exception::Exception) {
  //
  // currently do nothing
  //
  std::cout << "Received Message Enable" << std::endl ;
  LOG4CPLUS_INFO(getApplicationLogger(), "Received Message Enable");
}
//
void EmuPeripheralCrateCommand::disableAction(toolbox::Event::Reference e) 
  throw (toolbox::fsm::exception::Exception) {
  //
  // currently do nothing
  //
  std::cout << "Received Message Disable" << std::endl ;
  LOG4CPLUS_INFO(getApplicationLogger(), "Received Message Disable");
}  
//
void EmuPeripheralCrateCommand::haltAction(toolbox::Event::Reference e) 
  throw (toolbox::fsm::exception::Exception) {
  //
  // currently do nothing
  // 
  std::cout << "Received Message Halt" << std::endl ;
  LOG4CPLUS_INFO(getApplicationLogger(), "Received Message Halt");
}  

void EmuPeripheralCrateCommand::stateChanged(toolbox::fsm::FiniteStateMachine &fsm)
  throw (toolbox::fsm::exception::Exception) {
  EmuApplication::stateChanged(fsm);
}

void EmuPeripheralCrateCommand::actionPerformed (xdata::Event& e) {
  //
}

  void EmuPeripheralCrateCommand::FastConfigCrates(xgi::Input * in, xgi::Output * out ) 
    throw (xgi::exception::Exception)
  {
     std::cout << "Button: FastConfigCrates" << std::endl;
     ConfigureInit(2);
     this->Default(in,out);
  }


  void EmuPeripheralCrateCommand::ConfigureInit(int c)
  {

    if(!parsed) ParsingXML();
    
    if( MyController )
      {
        current_config_state_=1;
	MyController->configure(c);
        current_config_state_=2;
      }
    //
  }

  bool EmuPeripheralCrateCommand::ParsingXML(){
    //
    LOG4CPLUS_INFO(getApplicationLogger(),"Parsing Configuration XML");
    //
    // Check if filename exists
    //
    if(xmlFile_.toString().find("http") == string::npos) 
    {
      std::ifstream filename(xmlFile_.toString().c_str());
      if(filename.is_open()) {
	filename.close();
      }
      else {
	LOG4CPLUS_ERROR(getApplicationLogger(), "Filename doesn't exist");
	XCEPT_RAISE (toolbox::fsm::exception::Exception, "Filename doesn't exist");
	return false;
      }
    }
    //
    //cout <<"Start Parsing"<<endl;
    if ( MyController != 0 ) {
      LOG4CPLUS_INFO(getApplicationLogger(), "Delete existing controller");
      delete MyController ;
    }
    //
    MyController = new EmuController();

    MyController->SetConfFile(xmlFile_.toString().c_str());
    MyController->init();
    MyController->NotInDCS();
    //
    emuEndcap_ = MyController->GetEmuEndcap();
    if(!emuEndcap_) return false;
    crateVector = emuEndcap_->crates();
    //
    total_crates_=crateVector.size();
    if(total_crates_<=0) return false;
    this_crate_no_=0;

    for(unsigned crate_number=0; crate_number< crateVector.size(); crate_number++) {
      //
      SetCurrentCrate(crate_number);
      for(int i=0; i<9;i++) {
	OutputDMBTests[i][current_crate_] << "DMB-CFEB Tests " 
					  << thisCrate->GetChamber(dmbVector[i]->slot())->GetLabel().c_str() 
					  << " output:" << std::endl;
	OutputTMBTests[i][current_crate_] << "TMB-RAT Tests " 
					  << thisCrate->GetChamber(tmbVector[i]->slot())->GetLabel().c_str() 
					  << " output:" << std::endl;
	ChamberTestsOutput[i][current_crate_] << "Chamber-Crate Phases " 
					      << thisCrate->GetChamber(tmbVector[i]->slot())->GetLabel().c_str() 
					      << " output:" << std::endl;
      }
    }
    //
    SetCurrentCrate(this_crate_no_);
    //
    std::cout << "Parser Done" << std::endl ;
    //
    parsed=1;
    return true;
  }

  void EmuPeripheralCrateCommand::SetCurrentCrate(int cr)
  {  
    if(total_crates_<=0) return;
    thisCrate = crateVector[cr];

    if ( ! thisCrate ) {
      std::cout << "Crate doesn't exist" << std::endl;
      assert(thisCrate);
    }
    
    ThisCrateID_=thisCrate->GetLabel();
    thisCCB = thisCrate->ccb();
    thisMPC = thisCrate->mpc();
    tmbVector = thisCrate->tmbs();
    dmbVector = thisCrate->daqmbs();
    chamberVector = thisCrate->chambers();
    //  
    current_crate_ = cr;
  }

  void EmuPeripheralCrateCommand::CheckCrates(xgi::Input * in, xgi::Output * out )
    throw (xgi::exception::Exception)
  {  
    std::cout << "Button: Check Crate Controllers" << std::endl;
    if(total_crates_<=0) return;
    bool cr;
    for(unsigned i=0; i< crateVector.size(); i++)
    {
        cr = (crateVector[i]->vmeController()->SelfTest()) && (crateVector[i]->vmeController()->exist(13));
        crateVector[i]->SetLife( cr );
        if(!cr) std::cout << "Exclude Crate " << crateVector[i]->GetLabel() << std::endl;
    }

    this->Default(in, out);
  }

void EmuPeripheralCrateCommand::CheckCratesConfiguration(xgi::Input * in, xgi::Output * out )
  throw (xgi::exception::Exception) {
  //
  std::cout << "Button:  Check Configuration of All Active Crates" << std::endl;
  //
  VerifyCratesConfiguration();
  //
  this->Default(in, out);
}


int EmuPeripheralCrateCommand::VerifyCratesConfiguration()
{
  if(!parsed) ParsingXML();

  if(MyController==NULL || total_crates_<=0) return 0;
  //
  all_crates_ok = 1;
  //
  for(unsigned i=0; i< crateVector.size(); i++) {
    //
    if ( crateVector[i]->IsAlive() ) {
      //
      SetCurrentCrate(i);	
      //
      CheckPeripheralCrateConfiguration();
      //
      all_crates_ok &= crate_check_ok[i];
    }
  }
  return all_crates_ok;
}

// Another method which would be better in another class... let's make it work, first....
void EmuPeripheralCrateCommand::CheckPeripheralCrateConfiguration() {
  //
  std::cout << "Configuration check for " << thisCrate->GetLabel() << std::endl;
  //
  std::cout << " .... TMBs received hard resets... "; 
  for (unsigned int tmb=0; tmb<(tmbVector.size()<9?tmbVector.size():9) ; tmb++) {
    std::cout << std::dec << tmbVector[tmb]->ReadRegister(0xE8) << " ";
  }
  std::cout << " seconds ago" << std::endl;
  //
  crate_check_ok[current_crate_] = 1;
  //
  ccb_check_ok[current_crate_] = thisCrate->ccb()->CheckConfig();
  crate_check_ok[current_crate_] &=  ccb_check_ok[current_crate_];  
  //
  mpc_check_ok[current_crate_] = thisCrate->mpc()->CheckConfig();
  crate_check_ok[current_crate_] &=  mpc_check_ok[current_crate_];  
  //
  for (unsigned int chamber_index=0; chamber_index<(tmbVector.size()<9?tmbVector.size():9) ; chamber_index++) {
    //	
    Chamber * thisChamber     = chamberVector[chamber_index];
    TMB * thisTMB             = tmbVector[chamber_index];
    ALCTController * thisALCT = thisTMB->alctController();
    DAQMB * thisDMB           = dmbVector[chamber_index];
    //
    std::cout << "Configuration check for " << thisCrate->GetLabel() << ", " << (thisChamber->GetLabel()).c_str() << std::endl;
    //
    thisTMB->CheckTMBConfiguration();
    tmb_check_ok[current_crate_][chamber_index]  = (int) thisTMB->GetTMBConfigurationStatus();
    //
    thisALCT->CheckALCTConfiguration();
    alct_check_ok[current_crate_][chamber_index] = (int) thisALCT->GetALCTConfigurationStatus();
    //
    dmb_check_ok[current_crate_][chamber_index]  = (int) thisDMB->checkDAQMBXMLValues();
    //
    crate_check_ok[current_crate_] &= tmb_check_ok[current_crate_][chamber_index];
    crate_check_ok[current_crate_] &= alct_check_ok[current_crate_][chamber_index];
    crate_check_ok[current_crate_] &= dmb_check_ok[current_crate_][chamber_index];
    //
  }
  //
  return;
}
//

// sending and receiving soap commands
////////////////////////////////////////////////////////////////////
void EmuPeripheralCrateCommand::PCsendCommand(string command, string klass)
  throw (xoap::exception::Exception, xdaq::exception::Exception){
  //
  //This is copied from CSCSupervisor::sendcommand;
  //
  // Exceptions:
  // xoap exceptions are thrown by analyzeReply() for SOAP faults.
  // xdaq exceptions are thrown by postSOAP() for socket level errors.
  //
  // find applications
  std::set<xdaq::ApplicationDescriptor *> apps;
  //
  try {
    apps = getApplicationContext()->getDefaultZone()->getApplicationDescriptors(klass);
  }
  // 
  catch (xdaq::exception::ApplicationDescriptorNotFound e) {
    return; // Do nothing if the target doesn't exist
  }
  //
  // prepare a SOAP message
  xoap::MessageReference message = PCcreateCommandSOAP(command);
  xoap::MessageReference reply;
  xdaq::ApplicationDescriptor *ori=this->getApplicationDescriptor();
  //
  // send the message one-by-one
  std::set<xdaq::ApplicationDescriptor *>::iterator i = apps.begin();
  for (; i != apps.end(); ++i) {
    // postSOAP() may throw an exception when failed.
    reply = getApplicationContext()->postSOAP(message, *ori, *(*i));
    //
    //      PCanalyzeReply(message, reply, *i);
  }
}

xoap::MessageReference EmuPeripheralCrateCommand::PCcreateCommandSOAP(string command) {
  //
  //This is copied from CSCSupervisor::createCommandSOAP
  //
  xoap::MessageReference message = xoap::createMessage();
  xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
  xoap::SOAPName name = envelope.createName(command, "xdaq", "urn:xdaq-soap:3.0");
  envelope.getBody().addBodyElement(name);
  //
  return message;
}

// provides factory method for instantion of HellWorld application
//
XDAQ_INSTANTIATOR_IMPL(EmuPeripheralCrateCommand)
//

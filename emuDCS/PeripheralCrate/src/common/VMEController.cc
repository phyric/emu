
#ifndef OSUcc

//----------------------------------------------------------------------
// $Id: VMEController.cc,v 2.10 2005/12/15 14:32:21 mey Exp $
// $Log: VMEController.cc,v $
// Revision 2.10  2005/12/15 14:32:21  mey
// Update
//
// Revision 2.9  2005/12/05 13:21:14  mey
// Update
//
// Revision 2.8  2005/12/05 13:10:50  mey
// Update
//
// Revision 2.7  2005/12/05 08:59:45  mey
// Update
//
// Revision 2.6  2005/12/02 18:12:30  mey
// get rid of D360
//
// Revision 2.5  2005/11/30 16:26:53  mey
// DMB Firmware upload delay
//
// Revision 2.4  2005/11/30 12:59:59  mey
// DMB firmware loading
//
// Revision 2.3  2005/11/25 23:43:00  mey
// Update
//
// Revision 2.2  2005/11/21 15:48:24  mey
// Update
//
// Revision 2.1  2005/11/02 16:16:24  mey
// Update for new controller
//
// Revision 2.0  2005/04/12 08:07:06  geurts
// *** empty log message ***
//
// Revision 1.25  2004/07/22 18:52:38  tfcvs
// added accessor functions for DCS integration
//
//----------------------------------------------------------------------
#include "VMEController.h"
#include "VMEModule.h"
#include "Crate.h"
#include <cmath>
#include <string>
#include <stdio.h>
#include <iostream>
#include <unistd.h> // read and write

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#ifndef debugV //silent mode
#define PRINT(x) 
#define PRINTSTRING(x)  
#else //verbose mode
#define PRINT(x) std::cout << #x << ":\t" << x << std::endl; 
#define PRINTSTRING(x) std::cout << #x << std::endl; 
#endif



VMEController::VMEController(int crate, std::string ipAddr, int port): 
 theSocket(0), ipAddress_(ipAddr), port_(port), theCurrentModule(0),
 indian(SWAP),  max_buff(0), tot_buff(0), Tbytes(0), crate_(crate)
{
  /*
  * Fill "serv_addr" with the address of the server we want to connect with
  */
  bzero( (char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ipAddr.c_str());
  serv_addr.sin_port        = htons(port);
  //
  int socket = openSocket();
  //
}


VMEController::~VMEController(){
  std::cout << "VMEController: destructing  ... closing socket " << std::endl;
  closeSocket();
}


void VMEController::start(VMEModule * module) {
  if(theCurrentModule != module) {
    PRINTSTRING(OVAL: start method defined in VMEController.cc is starting )
    end();
    PRINTSTRING(OVAL: starting current module);
    module->start();
    PRINTSTRING(OVAL: current module was started);
    theCurrentModule = module;
  }
}



void VMEController::end() {
  if(theCurrentModule != 0) {
    theCurrentModule->end();
    theCurrentModule = 0;
  }
  assert(plev !=2);
  idevo = 0;
  feuseo = 0;
}


void VMEController::send_last() {
  char rcvx[2];
  char sndx[2];
  
  if(plev==2){
    scan(TERMINATE,sndx,0,rcvx,2);
  }
  plev=1;
}


int VMEController::openSocket() {
  if(theSocket == 0) {
    // open a TCP socket ( an internet stream socket )
    // cout<<"VMEController: open socket stream" << endl;

#ifndef DUMMY
    if( (theSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
      std::cerr<<"VMEController: error opening stream socket" << theSocket <<std::endl;

    // connect to the server
    std::cout <<"VMEController: crate " <<  crate_ << " connecting to server " 
	<<  ipAddress_.c_str() << ":" << port_ << std::endl;
    int stat = connect(theSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if( stat < 0) {
      std::cerr<<"VMEController: ERROR in connection - " << strerror(errno) << std::endl;
    }
#endif
  }

  return theSocket;
}


void VMEController::closeSocket() {
#ifndef DUMMY
  close(theSocket);
#endif
  theSocket = 0;
}

// changed return code to nrcv
int VMEController::readn(char *line)
{
  int nrcv = 0;
#ifndef DUMMY
  nrcv = read(theSocket, line, MAXLINE);
#endif
  // OVAL is a CMS validation tool.  We'd just grep for this line
  // we want to make sure every byte read and written from the
  // crate is identical to other versions of the code.
  #ifdef debugV 
    std::cout << "VMEController: readn sock " << theSocket;
    for(int i = 0; i < nrcv; ++i) {
      std::cout << " " << (int) line[i];
    }
    std::cout << std::endl;
  #endif

  /* Bytes swap is done here rather than at VME -Jan/20/2000 */
  /*  strcopy11(line,0,line2,n); */         /* swap */

  if(nrcv < 0) {
    std::cerr <<"VMEController: read ERROR - " << strerror(errno) << std::endl;
  }
  return nrcv;
}


int VMEController::writen(register const char *ptr, register int nbytes)
{
  #ifdef debugV
    std::cout << "VMEController: writen " << std::dec << theSocket << " " ;
    for(int i = 0; i < nbytes; ++i) {
      std::cout << (int) ptr[i] << " " ;
    }
    std::cout << std::endl;
  #endif
  int nleft = 0;
#ifndef DUMMY
  nleft  =  nbytes;
  while (nleft >0) {
    int nwritten = write(theSocket,ptr,nleft);

    if(nwritten <=0) {
      std::cerr << "VMEController: write ERROR -  " << strerror(errno) << std::endl;
      return nwritten;  /*error*/
    }

    nleft -= nwritten;
    ptr   += nwritten;
  }
#endif
  if(nleft > 0) {
     std::cout << "VMEController: write ERROR - nleft= " << nleft << std::endl;
  }
  return(nbytes - nleft);
}


int VMEController::writenn(const char *ptr,int nbytes)
{
  char  size[4];
  int nwritten = 0;
  int inttmp = nbytes;
  char * sizec = (char *) &inttmp;  // size of command

  /* endian format for VME computer, so byte swap is done here
     rather than in VME --- Jan-8-99 Chang*/
  if(indian==SWAP){
  size[0] = sizec[1];
  size[1] = sizec[0];
  size[2] = sizec[3];
  size[3] = sizec[2];
  }else{
  size[0] = sizec[0];
  size[1] = sizec[1];
  size[2] = sizec[2];
  size[3] = sizec[3];
  }
#ifndef DUMMY
  int nleft  =  nbytes;
  while (nleft >0) {
    // printf(" debug: send four bytes %d %02x %02x %02x %02x \n",nbytes,size[0]&0xff,size[1]&0xff,size[2]&0xff,size[3]&0xff);
    int ntmp = write(theSocket, size,4); // first 4 bytes for size of things  
#ifdef debugV
     printf("VMEController::writenn() - written for size %d\n", ntmp ); 
#endif
    if(nleft>20000) {
      nwritten = write(theSocket, ptr,20000);
      if(nwritten != 20000) {
        std::cerr<<"VMEController::writenn() - ERROR for write: nwritenn != 20000" << std::endl;
        return(nwritten);  /*error*/
      }
      nleft = nleft - 20000;
      ptr   = ptr + 20000;
    }

    nwritten = write(theSocket, ptr,nleft);

#ifdef debugV
    printf("writenn :  nbytes=%d string=%s \n", nbytes, ptr);
    std::cout << "writen ";
    for(int i = 0; i < nbytes; ++i) {
      std::cout << (int) ptr[i] << " " ;
    }
    std::cout << std::endl;

    printf("writenn : written for nbytes=%d nwritten=%d, left=%d\n",nbytes,nwritten,nleft);
#endif
    if(nwritten <=0)
      return(nwritten);  /*error*/

    nleft -= nwritten;
    ptr   += nwritten;
  }

  return(nbytes - nleft);
#endif
return nbytes;
}


VMEModule* VMEController::getTheCurrentModule(){
 return theCurrentModule;
}

#else

//----------------------------------------------------------------------
// $Id: VMEController.cc,v 2.10 2005/12/15 14:32:21 mey Exp $
// $Log: VMEController.cc,v $
// Revision 2.10  2005/12/15 14:32:21  mey
// Update
//
// Revision 2.9  2005/12/05 13:21:14  mey
// Update
//
// Revision 2.8  2005/12/05 13:10:50  mey
// Update
//
// Revision 2.7  2005/12/05 08:59:45  mey
// Update
//
// Revision 2.6  2005/12/02 18:12:30  mey
// get rid of D360
//
// Revision 2.5  2005/11/30 16:26:53  mey
// DMB Firmware upload delay
//
// Revision 2.4  2005/11/30 12:59:59  mey
// DMB firmware loading
//
// Revision 2.3  2005/11/25 23:43:00  mey
// Update
//
// Revision 2.2  2005/11/21 15:48:24  mey
// Update
//
// Revision 2.1  2005/11/02 16:16:24  mey
// Update for new controller
//
// Revision 1.25  2004/07/22 18:52:38  tfcvs
// added accessor functions for DCS integration
//
//
//----------------------------------------------------------------------
#include "VMEController.h"
#include "VMEModule.h"
#include "Crate.h"
#include <cmath>
#include <string>
#include <stdio.h>
#include <iostream>
#include <unistd.h> // read and write
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>

#include <sys/socket.h>
#include <unistd.h>

#define SCHAR_IOCTL_BASE	0xbb
#define SCHAR_RESET     	_IO(SCHAR_IOCTL_BASE, 0)
#define SCHAR_END		_IOR(SCHAR_IOCTL_BASE, 1, 0)
#define SCHAR_BLOCKON		_IOR(SCHAR_IOCTL_BASE, 2, 0)
#define SCHAR_BLOCKOFF		_IOR(SCHAR_IOCTL_BASE, 3, 0)
#define SCHAR_DUMPON		_IOR(SCHAR_IOCTL_BASE, 4, 0)
#define SCHAR_DUMPOFF		_IOR(SCHAR_IOCTL_BASE, 5, 0)

#define        Set_FF_VME 0x02
#define       MRst_Ext_FF 0xE6


// #define MAX_DATA 8990
#define MAX_DATA 8900
#define VME_CMDS 0x20
#define ACNLG 0x20
#define ACNLG_LOOP 0x60
#define LOOP_CNTRL 0xff
#define ATYPE 2    // VME A24 bit transfer
#define TSIZE 1    // VME 16 bit data transfer
#define TTYPE 0    // VME single transfer mode
#define PACKETOUTDUMP 0   // to print dump set 1


#ifndef debugV //silent mode
#define PRINT(x) 
#define PRINTSTRING(x)  
#else //verbose mode
#define PRINT(x) cout << #x << ":\t" << x << endl; 
#define PRINTSTRING(x) cout << #x << endl; 
#endif


VMEController::VMEController(int crate, string ipAddr, int port): 
 theSocket(0), ipAddress_(ipAddr), port_(port), theCurrentModule(0),
 indian(SWAP),  max_buff(0), tot_buff(0), crate_(crate),
 plev(1), idevo(0)
{
  //
  fpacket_delay = 0;
  packet_delay = 0;
  packet_delay_flg = 0;
  //
  DELAY2 = 0.016;
  DELAY3 = 16.384;
  //
  usedelay_ = false ;
  int socket = openSocket();
  cout << "VMEController opened socket = " << socket << endl;
  cout << "VMEController opened port   = " << port << endl;
}


VMEController::~VMEController(){
  cout << "destructing VMEController .. closing socket " << endl;
  closeSocket();
}


void VMEController::start(VMEModule * module) {
  if(theCurrentModule != module) {
    PRINTSTRING(OVAL: start method defined in VMEController.cc is starting )
    end();
    PRINTSTRING(OVAL: starting current module);
    module->start();
    PRINTSTRING(OVAL: current module was started);
    theCurrentModule = module;
    board=module->boardType();
    vmeadd=(module->slot())<<19;
  }
}



void VMEController::end() {
  if(theCurrentModule != 0) {
    theCurrentModule->end();
    theCurrentModule = 0;
  }
//  assert(plev !=2);
//  idevo = 0;
//  feuseo = 0;
}


void VMEController::send_last() {
}


int VMEController::openSocket() {

  char schardev_name[12]="/dev/schar0";
  schardev_name[11]=0;
  if(port_ >0 || port_ <10)  schardev_name[10] += port_;
  theSocket = open(schardev_name, O_RDWR);
  if (theSocket == -1) {
    perror("open");
    return 1;
  }
  get_macaddr();
  // eth_enableblock();
  eth_reset();
  mrst_ff();
  set_VME_mode();   
  return theSocket;
}


void VMEController::closeSocket() {
#ifndef DUMMY
  close(theSocket);
#endif
  theSocket = 0;
}



void VMEController::goToScanLevel(){
}

void VMEController::release_plev(){
}

VMEModule* VMEController::getTheCurrentModule(){
 return theCurrentModule;
}

int udelay(long int itim)
{
  usleep(300);
  std::cout << "Udelay..." << std::endl;
  std::cout << "Waiting...." << std::endl;
  std::cout << "udelay..." << itim << std::endl;;
  struct  timeval tp;
  long usec1,usec2;
  long int loop;
  int tim;
  int i,j,k;
  static int inter=1000;
  float xinter;
  static int mdelay;
  /* calibrate on first entry */
  if(inter==1000){
    mdelay=0;
    for(j=0;j<10;j++){
    RETRY:
      usleep(1);
      gettimeofday (&tp,NULL);
      usec1=tp.tv_usec;
      for(k=0;k<100;k++)for(i=0;i<inter;i++);
      gettimeofday (&tp,NULL);
      usec2=tp.tv_usec;
      tim=usec2-usec1;
      if(tim<0)goto RETRY;
      // printf(" inter tim %d %d \n",inter,tim);
      xinter=inter*110./tim;
      inter=xinter+1;
      if(j>3&&inter>mdelay)mdelay=inter;
    }
    printf(" udelay calibration: %d loops for 1 usec \n",mdelay);
  }
  /* now do loop delay */
  //printf(" loop itim loop %d %d \n",loop,itim);
  for(j=0;j<itim;j++){
      for(i=0;i<mdelay;i++);
  } 
}

void VMEController::sdly()
{
  unsigned char tmp[1]={0x00};
  unsigned short int tmp2[2]={0,0};
  unsigned short int *ptr;
  tmp2[0]=50;  // 50x16=800ns delay
 //       vme_controller(6,ptr,tmp2,tmp);
}


void  VMEController::sleep_vme(const char *outbuf)   // in usecs (min 16 usec)
{
unsigned short int *time;
unsigned long tmp_time;
char tmp[1]={0x00};
unsigned short int tmp2[2]={0,0};
unsigned short int *ptr;
       time = (unsigned short int *) outbuf;
       tmp_time=time[0]*1000+15; // in nsec
       tmp_time >>= 4; // in 16 nsec
       tmp2[0]=tmp_time & 0xffff;
       tmp2[1]=(tmp_time >> 16) & 0xffff;
       vme_controller(6,ptr,tmp2,tmp);
}

void  VMEController::sleep_vme2(unsigned short int time) // time in usec
{
unsigned long tmp_time;
char tmp[1]={0x00};
unsigned short int tmp2[2]={0,0};
unsigned short int *ptr;
       tmp_time=time*1000+15; // in nsec
       tmp_time >>= 4; // in 16 nsec
       tmp2[0]=tmp_time & 0xffff;
       tmp2[1]=(tmp_time >> 16) & 0xffff;
       vme_controller(6,ptr,tmp2,tmp);
}

void  VMEController::long_sleep_vme2(float time)   // time in usec
{
unsigned long tmp_time;
char tmp[1]={0x00};
unsigned short int tmp2[2]={0,0};
unsigned short int *ptr;
       tmp_time=(int)(time*1000)+15; // in nsec
       tmp_time >>= 4; // in 16 nsec
       tmp2[0]=tmp_time & 0xffff;
       tmp2[1]=(tmp_time >> 16) & 0xffff;
       vme_controller(6,ptr,tmp2,tmp);
}

void VMEController::handshake_vme()
{
/* no such thing as handshake. The following code is meaningless.

  char tmp[1]={0x00};
  unsigned short int tmp2[1]={0x0000};
  unsigned short int *ptr;
  add_control_r=msk_control_r;   
  ptr=(unsigned short int *)add_control_r;
  vme_controller(4,ptr,tmp2,tmp);
  vme_controller(5,ptr,tmp2,tmp);

*/
}

void VMEController::flush_vme()
{
  // should never been used.
  //char tmp[1]={0x00};
  //unsigned short int tmp2[1]={0x0000};
  //unsigned short int *ptr;
  // printf(" flush buffers to VME \n");
  //vme_controller(4,ptr,tmp2,tmp); // flush
  //
}

int VMEController::eth_reset(void)
{ 
  if(ioctl(theSocket,SCHAR_RESET)==-1){
    printf(" error in SCHAR_RESET \n");
  }

  return 0;
}

int VMEController::eth_read()
{  int err;
int size;
int loopcnt;
 
 loopcnt=0;
 size=0;
 GETMORE: 
 size=read(theSocket,rbuf,nrbuf);
        if(size<0)return size;
        if(size<7){
           if(rbuf[0]==0x03&&loopcnt<10){usleep(1000);loopcnt=loopcnt+1;goto GETMORE;}
        }
   return size;
}

void VMEController::mrst_ff()
{
  int n;
  int l,lcnt;
  wbuf[0]=0x00;
  wbuf[1]=MRst_Ext_FF;
  nwbuf=2;
  n=eth_write();
  printf("Full reset of FIFO done.\n");
  for(l=0;l<8000;l++)lcnt++;
  return;
}

void VMEController::set_VME_mode()
{
  int n;
  int l,lcnt;
  wbuf[0]=0x00;
  wbuf[1]=Set_FF_VME;
  nwbuf=2;
  n=eth_write();
  printf("Controller is in VME mode.\n");
  for(l=0;l<8000;l++)lcnt++;
  return;
}

void VMEController::get_macaddr()
{
  int msock_fd;
  struct ifreq mifr;

  char eth[5]="eth2";

   if(port_ >=0 && port_ <10) eth[3] = '0' + port_; 
   //create socket
   if((msock_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
     fprintf(stderr, "Error in call: socket()\n");
     
   //get MAC address
   strcpy(mifr.ifr_name, eth);
   if(ioctl(msock_fd,SIOCGIFHWADDR,&mifr) < 0)
     fprintf(stderr, "Error in call ioctl(socket, SIOCGIFHWADDR)\n");
   
   memcpy(hw_source_addr,mifr.ifr_addr.sa_data, ETH_ALEN);
   memcpy(ether_header.h_source, hw_source_addr, ETH_ALEN);

   sscanf(ipAddress_.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
       hw_dest_addr, hw_dest_addr+1, hw_dest_addr+2,
       hw_dest_addr+3, hw_dest_addr+4, hw_dest_addr+5);
   memcpy(ether_header.h_dest, hw_dest_addr, ETH_ALEN);
}

int VMEController::eth_write()
{  char *msg;
  int msg_size;
  int nwritten;
  int i;
   //Creating the packet
     ether_header.h_proto = htons(nwbuf);
     //   ether_header.h_proto = htons(0xfff);

   msg_size = sizeof(ether_header) + nwbuf;
   if((msg = (char *)malloc(msg_size*sizeof(unsigned char))) == NULL){ 
           perror("rp_send: main(): malloc(): No memory available");
           exit(1);
   }
   memcpy(msg, &ether_header, sizeof(ether_header));
   memcpy(msg + sizeof(ether_header), wbuf, nwbuf); 
// Jinghua Liu to debug
//   printf("****");for(i=0;i<msg_size;i++)printf("%02X ",msg[i]&0xff);printf("\n");
//   printf("Len : %4d\n",((msg[12]&0xff)<<8)|(msg[13]&0xff));
//
   nwritten = write(theSocket, (const void *)msg, msg_size);
   free(msg);
   return nwritten; 

}

void VMEController::vme_controller(int irdwr,unsigned short int *ptr,unsigned short int *data,char *rcv)
{
  /* irdwr:   
     0 bufread
     1 bufwrite 
     2 bufread snd  
     3 bufwrite snd 
     4 flush to VME
     5 loop back 
     6 delay
  */
  
  const char a_mask[8]={0x00,0x20,0x40,0x50,0x80,0x90,0x00,0x00};
  const char r_mask=0x00;
  const char w_mask=0x10;
  const char ts_mask[4]={0x00,0x04,0x08,0x0c};
  const char ts_size[4]={1,2,4,8};
  const char tt_mask[4]={0x00,0x01,0x02,0x03};;
  const char delay_mask[8]={0,1,2,3,4,5,6,7};

  int debug = 0;

  static unsigned short int LRG_read_flag=0;
  static unsigned short int LRG_read_pnt=0;
  static unsigned short int LRG_read_flag2=0;

  static int nvme;
  static int nread=0;
  unsigned char *radd_to;
  unsigned char *radd_from;
  unsigned char *nbytet;
  unsigned short int r_nbyte;
  unsigned char *r_head0;
  unsigned char *r_head1;
  unsigned char *r_head2;
  unsigned char *r_head3;
  unsigned short r_num;
  unsigned char *r_datat;
  int size,nwrtn;
  int i;
  unsigned long int ptrt;
  static int istrt=0;  
/* initialize */
  if(istrt==0){
    nwbuf=4;
    nvme=0;
    istrt=1;
  }
  // Jinghua Liu to debug
  if ( debug ) {
    printf("vme_control: %02x %08x",irdwr, (unsigned long int)ptr);
    if(irdwr==1 || irdwr==3) printf(" %04X",data[0]);
    printf("\n");
  }
  //
  /* flush to vme */
  if(irdwr==4){      
    // printf(" flush to vme \n");
    if(nvme==0)return;
    irdwr=3;
    goto Process;
  }

  ptrt=(unsigned long int)ptr;
  // Jinghua Liu:
  // VME address higher than 0xC00000 is for broadcasting,
  // READ is not allowed in the software. 
  if((irdwr==0 || irdwr==2) && ptrt >= 0xC00000) return;
     
  /*  fill buffer  */
  nvme=nvme+1;
 //  VME command function code
  // wbuf[0]=ACNLG;
  wbuf[0]=0x00;
  wbuf[1]=VME_CMDS;
  // LOOP back to/from Controller
  if(irdwr==5){/* printf(" controller loop back \n"); */ wbuf[0]=ACNLG_LOOP;wbuf[1]=LOOP_CNTRL;irdwr=2;}
  wbuf[nwbuf+0]=0x00;
  // VME Read/Write 
  
  if(irdwr==0||irdwr==2){wbuf[nwbuf+1]=a_mask[ATYPE]|r_mask|ts_mask[TSIZE]|tt_mask[TTYPE];nread=nread+ts_size[TSIZE];}
  if(irdwr==1||irdwr==3){wbuf[nwbuf+1]=a_mask[ATYPE]|w_mask|ts_mask[TSIZE]|tt_mask[TTYPE];} 
  if(irdwr<=3){
    wbuf[nwbuf+2]=0x00;
    // Jinghua Liu: 
    // to prevent the OSU controller hanging up on invalid VME address
    if(ptrt<0x80000) 
      {  printf("VME ADDRESS ERROR: %06X\n",ptrt);
         exit(-1);
      }
    wbuf[nwbuf+3]=(ptrt&0xff0000)>>16;
    wbuf[nwbuf+4]=(ptrt&0xff00)>>8;
    wbuf[nwbuf+5]=(ptrt&0xff);
    // Jinghua Liu: no byte swap for CCB,MPC,TMB 
//    wbuf[nwbuf+6]=(*data&0xff);
//    wbuf[nwbuf+7]=(*data&0xff00)>>8;
    wbuf[nwbuf+6]=(*data&0xff00)>>8;
    wbuf[nwbuf+7]=(*data&0xff);
    // end byte swap
    if(irdwr==1||irdwr==3)nwbuf=nwbuf+8;
    if(irdwr==0||irdwr==2)nwbuf=nwbuf+6;   
    /* check for overflow */
    LRG_read_flag2=0;
    if(nwbuf>MAX_DATA){
      // printf(" nwbuf %d MAX_DATA %d \n",nwbuf,MAX_DATA);
      LRG_read_flag2=1;
       if(irdwr==1)irdwr=3;
       if(irdwr==0)irdwr=2;
       if(LRG_read_flag==0){
         LRG_read_flag=1;    // turn on large read
         LRG_read_pnt=0;
         // printf(" large read flag on \n");
       }
    }
  } 
  // delay
  if(irdwr==6){
    // only use delay type 2 and 5 (in 16 ns)
    int delay_type=2;
    if(data[1]) delay_type=5;
    wbuf[nwbuf+0]=delay_mask[delay_type];
    wbuf[nwbuf+1]=0x00;
    if(delay_type<=3){
      wbuf[nwbuf+3]=(data[0]&0xff);
      wbuf[nwbuf+2]=(data[0]&0xff00)>>8;
      nwbuf=nwbuf+4;
      if(delay_type==2)fpacket_delay=fpacket_delay+(*data)*DELAY2;
      if(delay_type==3)fpacket_delay=fpacket_delay+(*data)*DELAY3;
    }else{
      wbuf[nwbuf+5]=(data[0]&0xff);
      wbuf[nwbuf+4]=(data[0]&0xff00)>>8;
      wbuf[nwbuf+3]=(data[1]&0xff);
      wbuf[nwbuf+2]=(data[1]&0xff00)>>8;
      nwbuf=nwbuf+6;
    }
  } 
  /* write VME commands to vme */
 Process:
  if(irdwr==2||irdwr==3){
    if(nread>0&&wbuf[1]!=0x1f)wbuf[0]=ACNLG;
    wbuf[2]=(nvme&0xff00)>>8;
    wbuf[3]=nvme&0xff;
    if(PACKETOUTDUMP!=0)dump_outpacket(nvme);
    nwrtn=eth_write();
    //
    packet_delay=fpacket_delay+1;
    packet_delay=packet_delay+15; 
    if ( usedelay_ ) udelay(packet_delay);
    //
    fpacket_delay=0.0;
    packet_delay=0;
    //
    // printf(" nwrtn %d nwbuf %d \n",nwrtn,nwbuf);
    //
    nwbuf=4;
    nvme=0;
  }
 
 /* read back bytes from vme */
 
  if((irdwr==2||irdwr==3)&&nread>0){
READETH:
    nrbuf=nread;
    size=eth_read();
    if(size<10)
         {  printf(" no data read back \n");
            system("cat /proc/sys/dev/schar/0");
            exit(0);
         }
      radd_to=(unsigned char *)rbuf;
      radd_from=(unsigned char *)rbuf+6;
// Check if the packet is expected. To reject unwanted broadcast packets.
// Don't like GOTO, just keep it for the time being.
      
      if(memcmp(radd_to, hw_source_addr, 6) || memcmp(radd_from, hw_dest_addr,6))
        { 
printf("From %02X:%02X:%02X:%02X:%02X:%02X, need %02X:%02X:%02X:%02X:%02X:%02X\n",
radd_from[0],radd_from[1],radd_from[2],radd_from[3],radd_from[4], radd_from[5],
hw_dest_addr[0],hw_dest_addr[1],hw_dest_addr[2],hw_dest_addr[3],hw_dest_addr[4], hw_dest_addr[5]);

printf("To %02X:%02X:%02X:%02X:%02X:%02X, need %02X:%02X:%02X:%02X:%02X:%02X\n",
radd_to[0],radd_to[1],radd_to[2],radd_to[3],radd_to[4],radd_to[5],
hw_source_addr[0],hw_source_addr[1],hw_source_addr[2],hw_source_addr[3],hw_source_addr[4],hw_source_addr[5]);

          goto READETH;
        }
      nbytet=(unsigned char *)rbuf+12;
      r_nbyte=((nbytet[0]<<8)&0xff00)|(nbytet[1]&0xff);
      r_head0=(unsigned char *)rbuf+14;
      r_head1=(unsigned char *)rbuf+16;
      r_head2=(unsigned char *)rbuf+18;
      r_head3=(unsigned char *)rbuf+20;
      r_datat=(unsigned char *)rbuf+22;
      r_num=((r_head3[0]<<8)&0xff00)|(r_head3[1]&0xff);  
// Jinghua Liu to debug
//      printf("Read back size %d \n",size);
//      for(i=0;i<size;i++)printf("%02X ",rbuf[i]&0xff);printf("\n");
//

// Jinghua Liu: add the byte swap back:
    for(i=0;i<r_num;i++){rcv[2*i+LRG_read_pnt]=r_datat[2*i+1];rcv[2*i+1+LRG_read_pnt]=r_datat[2*i];}
//    for(i=0;i<r_num;i++){rcv[2*i+LRG_read_pnt]=r_datat[2*i];rcv[2*i+1+LRG_read_pnt]=r_datat[2*i+1];}
//end byte swap

    if(LRG_read_flag==1)LRG_read_pnt=LRG_read_pnt+2+2*r_num-2;
  ENDL: 
    if(LRG_read_flag2==0){
      LRG_read_flag=0;     // turn off large read
      LRG_read_pnt=0;
      // printf(" large read flag off %d \n",nwbuf);
    }
    nread=0;
  }
}

/* dump specific to A24/1/0 for now */

void VMEController::dump_outpacket(int nvme)
{
int nwbuft,nwbufto,i;
 printf(" Header %02x%02x   #Cmds  %02x%02x \n",wbuf[0]&0xff,wbuf[1]&0xff,wbuf[2]&0xff,wbuf[3]&0xff);
 if(wbuf[1]==VME_CMDS){
    nwbuft=4;
    for(i=0;i<nvme;i++){
      nwbufto=nwbuft;
      if(wbuf[nwbufto]==0){
      if(wbuf[1+nwbufto]==0x54){
	printf(" %d. W %02x%02x %02x%02x%02x%02x %02x%02x \n",i,wbuf[0+nwbuft]&0xff,wbuf[1+nwbuft]&0xff,wbuf[2+nwbuft]&0xff,wbuf[3+nwbuft]&0xff,wbuf[4+nwbuft]&0xff,wbuf[5+nwbuft]&0xff,wbuf[6+nwbuft]&0xff,wbuf[7+nwbuft]&0xff);
      nwbuft=nwbuft+8;}
      if(wbuf[1+nwbufto]==0x44){
         printf(" %d. R %02x%02x %02x%02x%02x%02x  \n",i,wbuf[0+nwbuft]&0xff,wbuf[1+nwbuft]&0xff,wbuf[2+nwbuft]&0xff,wbuf[3+nwbuft]&0xff,wbuf[4+nwbuft]&0xff,wbuf[5+nwbuft]&0xff);
      nwbuft=nwbuft+6;}
      }else{
	if(wbuf[nwbufto]<=3){
	   printf(" %d. D %02x%02x %02x%02x \n",i,wbuf[0+nwbuft]&0xff,wbuf[1+nwbuft]&0xff,wbuf[2+nwbuft]&0xff,wbuf[3+nwbuft]&0xff);
           nwbuft=nwbuft+4;
        }else{
           printf(" %d. D %02x%02x %02x%02x%02x \n",i,wbuf[0+nwbuft]&0xff,wbuf[1+nwbuft]&0xff,wbuf[2+nwbuft]&0xff,wbuf[3+nwbuft]&0xff,wbuf[4+nwbuft]&0xff,wbuf[5+nwbuft]&0xff);
           nwbuft=nwbuft+6;
        }
      }
    }
 }
}

#endif

//*****************************************************************************
// ConsumerCanvas.cc
//
// Description: canvas for the consumers
//
//
// Author List:
//   Hartmut Stadie  IEKP Karlsruhe
//=============================================================================
//*****************************************************************************
// RCS Current Revision Record
//-----------------------------------------------------------------------------
// $Source: /afs/cern.ch/project/cvs/reps/tridas/TriDAS/emu/dqm/common/src/common/ConsumerCanvas.cc,v $
// $Revision: 1.2 $
// $Date: 2012/04/23 14:42:57 $
// $Author: barvic $
// $State: Exp $
// $Locker:  $
//*****************************************************************************

#include <sstream>
#include "unistd.h"

#include <TPaveLabel.h>
#include <TPad.h>
#include <TROOT.h>

#include "emu/dqm/common/ConsumerCanvas.hh"

ConsumerCanvas::ConsumerCanvas(const char* name, const char* title,
                               const char* title2,int nx, int ny, int width, int height)
  :TCanvas(name,title, width, height), _pads(NULL), _n(0), runNumber("")
{
  SetFillColor(0);
  _title = new TPaveLabel(0.005, 0.95, 0.995, 0.995,title2);
  _title->SetFillColor(0);

  _counter = new TPaveLabel(0.005,0.92,0.995,0.945,"Run:       Time:");
  _counter->SetFillColor(0);

  TCanvas::cd();
  _title->Draw();
  _counter->Draw();

  Divide(nx,ny);
}

ConsumerCanvas::~ConsumerCanvas()
{
  //std::cout << "ConsumerCanvas::~ConsumerCanvas()" << std::endl;
  delete _title;
  delete _counter;
  for (int i = 0 ; i < _n ; ++i)
    delete _pads[i];
  delete[] _pads;
  //std::cout << "end of ConsumerCanvas::~ConsumerCanvas()" << std::endl;
}

void ConsumerCanvas::Divide(Int_t nx, Int_t ny, Float_t xmargin, Float_t ymargin,Int_t color)
{
  // delete old pads!
  for (int i = 0 ; i < _n ; ++i)
    delete _pads[i];
  delete[] _pads;
  //create new pads taken from TCanvas::Divide()
  if (nx <= 0) nx = 1;
  if (ny <= 0) ny = 1;
  Int_t ix,iy;
  Double_t x1,y1,x2,y2;
  Double_t dy = 1/Double_t(ny);
  Double_t dx = 1/Double_t(nx);

  _pads = new TPad*[nx*ny];

  _n = 0;
  _nx = nx;
  _ny = ny;

  for ( iy=0 ; iy < ny ; ++iy)
  {
    y2 = 0.92 - iy*dy - ymargin;
    y1 = y2 - dy + 2*ymargin;
    if (y1 < 0) y1 = 0;
    if (y1 > y2) continue;
    for ( ix = 0 ; ix < nx; ++ix)
    {
      x1 = ix*dx + xmargin;
      x2 = x1 +dx -2*xmargin;
      if (x1 > x2) continue;
      _n++;
      std::ostringstream padname;
      padname << "pad" << _n << std::ends;
      _pads[_n-1]  = new TPad(padname.str().c_str(),padname.str().c_str(),
                              x1,y1,x2,y2);
      _pads[_n-1]->SetFillColor(0);
      _pads[_n-1]->SetCanvasSize(10,20);
    }
  }
  TCanvas::cd();
  //std::cout << "drawing" << std::endl;
  for (int i = 0 ; i < _n ; ++i)
    _pads[i]->Draw();
}

/* void ConsumerCanvas::cd(Int_t subpadnumber)
{
  if(subpadnumber == 0) TCanvas::cd();
  else _pads[subpadnumber-1]->cd();
} */

void ConsumerCanvas::cd(Int_t subpadnumber, Int_t gridX, Int_t gridY)
{
  if (subpadnumber == 0) TCanvas::cd();
  else
  {
    _pads[subpadnumber-1]->cd();
    _pads[subpadnumber-1]->SetGridx(gridX);
    _pads[subpadnumber-1]->SetGridy(gridY);

  }
}

TPad* ConsumerCanvas::GetPad(Int_t subpadnumber)
{
  if (subpadnumber == 0) return this;
  else return _pads[subpadnumber-1];
}

void ConsumerCanvas::Streamer(TBuffer& b)
{
  std::ostringstream label;
  time_t t2 = time(0);
  tm* t22 = localtime(&t2);
  label << "Run#: " << runNumber << "  Time: "
        << asctime(t22)  << std::ends;
  bool batch = gROOT->IsBatch();
  gROOT->SetBatch(kTRUE);
  _counter->SetLabel(label.str().c_str());
  Modified();
  Update();
  gROOT->SetBatch(batch);
  TCanvas::Streamer(b);
}

void ConsumerCanvas::Draw()
{
  std::ostringstream label;
  time_t t2 = time(0);
  tm* t22 = localtime(&t2);
  label << "Run#: " << runNumber << "  Time: "
        << asctime(t22)  << std::ends;
  _counter->SetLabel(label.str().c_str());
  TCanvas::Draw();

}


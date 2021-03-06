#include <emu/dqm/common/CSCReadoutMappingFromFile.h>
#include <FWCore/MessageLogger/interface/MessageLogger.h>
// #include <FWCore/ParameterSet/interface/FileInPath.h>
#include <iostream>
#include <fstream>
#include <sstream>

CSCReadoutMappingFromFile::CSCReadoutMappingFromFile( std::string mapfile )
{
  fill( mapfile );
}

CSCReadoutMappingFromFile::~CSCReadoutMappingFromFile() {}

void CSCReadoutMappingFromFile::fill( std::string mapfile )
{
  theMappingFile = mapfile;
  std::ifstream in( theMappingFile.c_str() );
  std::string line;
  const std::string commentFlag = "#";
  if ( !in )
  {
    edm::LogError("CSCMapping") << " Failed to open file " << theMappingFile << " containing mapping.";
  }
  else
  {
    edm::LogInfo("CSCMapping") << " Opened file " << theMappingFile << " containing mapping.";

    while ( getline(in, line) )   // getline() from <string>
    {
      // LogDebug("CSC") << line;
      if ( line[0] != commentFlag[0] )
      {
        int i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11, i12;
        std::istringstream is( line );
        is >> i1 >> i2 >> i3 >> i4 >> i5 >> i6 >> i7 >> i8 >> i9 >> i10 >> i11 >> i12;
        // LogDebug("CSC") << i1 << " " << i2 << " " << i3 << " " << i4 << " " <<
        //    i5 << " " << i6 << " " << i7 << " " << i8 << " " << i9;
        addRecord( i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i12 );
      }
    }

  }

  return;
}


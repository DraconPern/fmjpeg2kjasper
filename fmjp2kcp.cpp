#include "dcmtk/config/osconfig.h"
#include "fmjp2kcp.h"

FMJP2KCodecParameter::FMJP2KCodecParameter(
    OFBool pVerbose,
    OFBool pCreateSOPInstanceUID,
    Uint32 pFragmentSize,
    OFBool pCreateOffsetTable,
    OFBool pConvertToSC)
: DcmCodecParameter()
, fragmentSize(pFragmentSize)
, createOffsetTable(pCreateOffsetTable)
, convertToSC(pConvertToSC)
, createInstanceUID(pCreateSOPInstanceUID)
, verboseMode(pVerbose)
{
}


FMJP2KCodecParameter::FMJP2KCodecParameter(const FMJP2KCodecParameter& arg)
: DcmCodecParameter(arg)
, fragmentSize(arg.fragmentSize)
, createOffsetTable(arg.createOffsetTable)
, convertToSC(arg.convertToSC)
, createInstanceUID(arg.createInstanceUID)
, verboseMode(arg.verboseMode)
{
}

FMJP2KCodecParameter::~FMJP2KCodecParameter()
{
}
  
DcmCodecParameter *FMJP2KCodecParameter::clone() const
{
  return new FMJP2KCodecParameter(*this);
} 

const char *FMJP2KCodecParameter::className() const
{
  return "FMJPEG2KCodecParameter";
}


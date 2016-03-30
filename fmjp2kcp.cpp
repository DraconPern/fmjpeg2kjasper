#include "dcmtk/config/osconfig.h"
#include "fmjpeg2kcp.h"

FMJPEG2KCodecParameter::FMJPEG2KCodecParameter(
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


FMJPEG2KCodecParameter::FMJPEG2KCodecParameter(const FMJPEG2KCodecParameter& arg)
: DcmCodecParameter(arg)
, fragmentSize(arg.fragmentSize)
, createOffsetTable(arg.createOffsetTable)
, convertToSC(arg.convertToSC)
, createInstanceUID(arg.createInstanceUID)
, verboseMode(arg.verboseMode)
{
}

FMJPEG2KCodecParameter::~FMJPEG2KCodecParameter()
{
}
  
DcmCodecParameter *FMJPEG2KCodecParameter::clone() const
{
  return new FMJPEG2KCodecParameter(*this);
} 

const char *FMJPEG2KCodecParameter::className() const
{
  return "FMJPEG2KCodecParameter";
}


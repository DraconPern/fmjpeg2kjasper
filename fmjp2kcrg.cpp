
#include "dcmtk/config/osconfig.h"
#include "fmjpeg2kcrg.h"

#include "dcmtk/dcmdata/dccodec.h"
#include "fmjpeg2kcd.h" 

// initialization of static members
OFBool FMJPEG2KCodecRegistration::registered = OFFalse;
FMJPEG2KCodecParameter *FMJPEG2KCodecRegistration::cp = NULL;
FMJPEG2KCodec *FMJPEG2KCodecRegistration::codec = NULL;

void FMJPEG2KCodecRegistration::registerCodecs(
	OFBool pCreateSOPInstanceUID,
	OFBool pVerbose)
{
	if (! registered)
	{
		cp = new FMJPEG2KCodecParameter(
			pVerbose,
			pCreateSOPInstanceUID,
			0, OFTrue, OFFalse);

		if (cp)
		{
			codec = new FMJPEG2KCodec();
			if (codec) DcmCodecList::registerCodec(codec, NULL, cp);
			registered = OFTrue;
		}
	}
}

void FMJPEG2KCodecRegistration::cleanup()
{
	if (registered)
	{
		DcmCodecList::deregisterCodec(codec);
		delete codec;
		delete cp;
		registered = OFFalse;
#ifdef DEBUG
		// not needed but useful for debugging purposes
		codec  = NULL;
		cp     = NULL;
#endif
	}
}


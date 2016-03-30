
#include "dcmtk/config/osconfig.h"
#include "fmjp2kcrg.h"

#include "dcmtk/dcmdata/dccodec.h"
#include "fmjp2kcd.h" 

// initialization of static members
OFBool FMJP2KCodecRegistration::registered = OFFalse;
FMJP2KCodecParameter *FMJP2KCodecRegistration::cp = NULL;
FMJP2KCodec *FMJP2KCodecRegistration::codec = NULL;

void FMJP2KCodecRegistration::registerCodecs(
	OFBool pCreateSOPInstanceUID,
	OFBool pVerbose)
{
	if (! registered)
	{
		cp = new FMJP2KCodecParameter(
			pVerbose,
			pCreateSOPInstanceUID,
			0, OFTrue, OFFalse);

		if (cp)
		{
			codec = new FMJP2KCodec();
			if (codec) DcmCodecList::registerCodec(codec, NULL, cp);
			registered = OFTrue;
		}
	}
}

void FMJP2KCodecRegistration::cleanup()
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


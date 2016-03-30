
#ifndef FMJPEG2000CRG_H
#define FMJPEG2000CRG_H

#include "dcmtk/config/osconfig.h"
#include "dcmtk/ofstd/oftypes.h"  /* for OFBool */

#include "fmjp2kcp.h"
class FMJP2KCodec;

class FMJP2KCodecRegistration 
{
public: 
  /** registers FMJPEG2KCodec.
   *  If already registered, call is ignored unless cleanup() has
   *  been performed before.
   *  @param pCreateSOPInstanceUID flag indicating whether or not
   *    a new SOP Instance UID should be assigned upon decompression.
   *  @param pVerbose verbose mode flag
   */   
  static void registerCodecs(
    OFBool pCreateSOPInstanceUID = OFFalse,
    OFBool pVerbose = OFFalse);

  /** deregisters decoder.
   *  Attention: Must not be called while other threads might still use
   *  the registered codecs, e.g. because they are currently decoding
   *  DICOM data sets through dcmdata.
   */  
  static void cleanup();

private:

  /// private undefined copy constructor
  FMJP2KCodecRegistration(const FMJP2KCodecRegistration&);
  
  /// private undefined copy assignment operator
  FMJP2KCodecRegistration& operator=(const FMJP2KCodecRegistration&);

  /// flag indicating whether the decoder is already registered.
  static OFBool registered;

  /// pointer to codec parameter
  static FMJP2KCodecParameter *cp;
  
  /// pointer to the decoder
  static FMJP2KCodec *codec;
};

#endif

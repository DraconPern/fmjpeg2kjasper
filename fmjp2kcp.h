#ifndef JPEG2000CP_H
#define JPEG2000CP_H

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dccodec.h" /* for DcmCodecParameter */

class FMJPEG2KCodecParameter: public DcmCodecParameter
{
public:

  /** constructor.
   *  @param pVerbose verbose mode flag
   *  @param pCreateSOPInstanceUID true if a new SOP instance UID should be assigned
   *    upon compression/decompression
   *  @param pFragmentSize maximum fragment size (in kbytes) for compression, 0 for unlimited.
   *  @param pCreateOffsetTable create offset table during image compression?
   *  @param pConvertToSC flag indicating whether image should be converted to 
   *    Secondary Capture upon compression
   *  @param pReverseDecompressionByteOrder flag indicating whether the byte order should
   *    be reversed upon decompression. Needed to correctly decode some incorrectly encoded
   *    images with more than one byte per sample.
   */
  FMJPEG2KCodecParameter(
    OFBool pVerbose = OFFalse,
    OFBool pCreateSOPInstanceUID = OFFalse,
    Uint32 pFragmentSize = 0,
    OFBool pCreateOffsetTable = OFTrue,
    OFBool pConvertToSC = OFFalse);

  /// copy constructor
  FMJPEG2KCodecParameter(const FMJPEG2KCodecParameter& arg);

  /// destructor
  virtual ~FMJPEG2KCodecParameter();

  /** this methods creates a copy of type DcmCodecParameter *
   *  it must be overweritten in every subclass.
   *  @return copy of this object
   */
  virtual DcmCodecParameter *clone() const;

  /** returns the class name as string.
   *  can be used as poor man's RTTI replacement.
   */
  virtual const char *className() const;

  /** returns maximum fragment size (in kbytes) for compression, 0 for unlimited.
   *  @returnmaximum fragment size for compression
   */
  Uint32 getFragmentSize() const
  {
    return fragmentSize;
  }

  /** returns offset table creation flag
   *  @return offset table creation flag
   */
  OFBool getCreateOffsetTable() const
  {
    return createOffsetTable;
  }

  /** returns secondary capture conversion flag
   *  @return secondary capture conversion flag
   */
  OFBool getConvertToSC() const
  {
    return convertToSC;
  }

  /** returns mode for SOP Instance UID creation
   *  @return mode for SOP Instance UID creation
   */
  OFBool getUIDCreation() const
  {
    return createInstanceUID;
  }

  /** returns verbose mode flag
   *  @return verbose mode flag
   */
  OFBool isVerbose() const
  {
    return verboseMode;
  }


private:

  /// private undefined copy assignment operator
  FMJPEG2KCodecParameter& operator=(const FMJPEG2KCodecParameter&);

  /// maximum fragment size (in kbytes) for compression, 0 for unlimited.
  Uint32 fragmentSize;

  /// create offset table during image compression
  OFBool createOffsetTable;

  /// flag indicating whether image should be converted to Secondary Capture upon compression
  OFBool convertToSC;

  /// create new Instance UID during compression/decompression?
  OFBool createInstanceUID;

  /// verbose mode flag. If true, warning messages are printed to console
  OFBool verboseMode;
};


#endif

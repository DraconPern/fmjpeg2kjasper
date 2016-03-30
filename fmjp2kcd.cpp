#include "dcmtk/config/osconfig.h"
#include "fmjp2kcd.h"
#include <jasper/jasper.h>

// dcmdata includes
#include "fmjp2kcp.h"
#include "dcmtk/dcmdata/dcdatset.h"  /* for class DcmDataset */
#include "dcmtk/dcmdata/dcdeftag.h"  /* for tag constants */
#include "dcmtk/dcmdata/dcpixseq.h"  /* for class DcmPixelSequence */
#include "dcmtk/dcmdata/dcpxitem.h"  /* for class DcmPixelItem */
#include "dcmtk/dcmdata/dcvrpobw.h"  /* for class DcmPolymorphOBOW */
#include "dcmtk/dcmdata/dcswap.h"    /* for swapIfNecessary() */
#include "dcmtk/dcmdata/dcuid.h"     /* for dcmGenerateUniqueIdentifer()*/
#include "dcmtk/dcmimgle/diutils.h" /* for EP_Interpretation */
#include "dcmtk/dcmjpeg/djutils.h" /* for enums */
#include "dcmtk/ofstd/ofstd.h"

FMJP2KCodec::FMJP2KCodec()        
	: DcmCodec()
{
	jas_init();
}

FMJP2KCodec::~FMJP2KCodec()
{
	jas_cleanup();
}

OFBool FMJP2KCodec::canChangeCoding(
	const E_TransferSyntax oldRepType,
	const E_TransferSyntax newRepType) const
{
	E_TransferSyntax myXfer = EXS_JPEG2000LosslessOnly;
	DcmXfer newRep(newRepType);
	DcmXfer oldRep(oldRepType);
	if (newRep.isNotEncapsulated() && (oldRepType == myXfer)) return OFTrue; // decompress requested
	if (oldRep.isNotEncapsulated() && (newRepType == myXfer)) return OFTrue; // compress requested

	myXfer = EXS_JPEG2000;
	if (newRep.isNotEncapsulated() && (oldRepType == myXfer)) return OFTrue; // decompress requested
	// if (oldRep.isNotEncapsulated() && (newRepType == myXfer)) return OFTrue; // compress requested

	// we don't support transcoding
	return OFFalse;
}

OFCondition FMJP2KCodec::determineDecompressedColorModel(
	const DcmRepresentationParameter * fromParam,
	DcmPixelSequence * fromPixSeq,
	const DcmCodecParameter * cp,
	DcmItem *dataset,
	OFString &decompressedColorModel) const
{		
	OFCondition result = EC_IllegalParameter;
	if (dataset != NULL)
	{
		// retrieve color model from given dataset
		result = dataset->findAndGetOFString(DCM_PhotometricInterpretation, decompressedColorModel);
	}
	return result;
}

OFCondition FMJP2KCodec::decode(
	const DcmRepresentationParameter * /* fromRepParam */,
	DcmPixelSequence * pixSeq,
	DcmPolymorphOBOW& uncompressedPixelData,
	const DcmCodecParameter * cp,
	const DcmStack& objStack) const
{
	OFCondition result = EC_Normal;

	// assume we can cast the codec parameter to what we need
	const FMJP2KCodecParameter *djcp = OFstatic_cast(const FMJP2KCodecParameter *, cp);

	DcmStack localStack(objStack);
	(void)localStack.pop();             // pop pixel data element from stack
	DcmObject *dataset = localStack.pop(); // this is the item in which the pixel data is located
	if ((!dataset) || ((dataset->ident() != EVR_dataset) && (dataset->ident() != EVR_item))) result = EC_InvalidTag;
	else
	{
		Uint16 imageSamplesPerPixel = 0;
		Uint16 imageRows = 0;
		Uint16 imageColumns = 0;
		Sint32 imageFrames = 1;
		Uint16 imageBitsAllocated = 0;
		Uint16 imageBytesAllocated = 0;
		Uint16 imagePlanarConfiguration = 0;
		OFBool createPlanarConfiguration = OFFalse;
		OFBool createPlanarConfigurationInitialized = OFFalse;
		EP_Interpretation colorModel = EPI_Unknown;
		DcmItem *ditem = OFstatic_cast(DcmItem *, dataset);

		if (result.good()) result = ditem->findAndGetUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
		if (result.good()) result = ditem->findAndGetUint16(DCM_Rows, imageRows);
		if (result.good()) result = ditem->findAndGetUint16(DCM_Columns, imageColumns);
		if (result.good()) result = ditem->findAndGetUint16(DCM_BitsAllocated, imageBitsAllocated);
		if (result.good())
		{
			imageBytesAllocated = OFstatic_cast(Uint16, imageBitsAllocated / 8);
			if ((imageBitsAllocated < 8) || (imageBitsAllocated % 8 != 0)) result = EC_CannotChangeRepresentation;
		}
		if (result.good() && (imageSamplesPerPixel > 1))
		{
			result = ditem->findAndGetUint16(DCM_PlanarConfiguration, imagePlanarConfiguration);

			// DICOM standard says it has to be 0
			if (imagePlanarConfiguration != 0)
				result = EC_CannotChangeRepresentation;
		}

		// number of frames is an optional attribute - we don't mind if it isn't present.
		if (result.good()) (void) ditem->findAndGetSint32(DCM_NumberOfFrames, imageFrames);

		EP_Interpretation dicomPI = DcmJpegHelper::getPhotometricInterpretation((DcmItem *)dataset);

		OFBool isYBR = OFFalse;
		if ((dicomPI == EPI_YBR_Full)||(dicomPI == EPI_YBR_Full_422)||(dicomPI == EPI_YBR_Partial_422)) isYBR = OFTrue;

		if (imageFrames >= OFstatic_cast(Sint32, pixSeq->card()))
			imageFrames = pixSeq->card() - 1; // limit number of frames to number of pixel items - 1
		if (imageFrames < 1)
			imageFrames = 1; // default in case the number of frames attribute contains garbage		

		if (result.good())
		{
			DcmPixelItem *pixItem = NULL;
			Uint8 * jpgData = NULL;

			{
				Uint32 frameSize = imageBytesAllocated * imageRows * imageColumns * imageSamplesPerPixel;
				Uint32 totalSize = frameSize * imageFrames;
				if (totalSize & 1) totalSize++; // align on 16-bit word boundary
				Uint16 *imageData16 = NULL;
				Sint32 currentFrame = 0;                
				Uint8 precision = 0;	// guess after first frame

				result = uncompressedPixelData.createUint16Array(totalSize / sizeof(Uint16), imageData16);
				if (result.good())
				{
					Uint8 *imageData8 = OFreinterpret_cast(Uint8 *, imageData16);

					while ((currentFrame < imageFrames) && result.good())
					{
						// find the total of all items
						int compressedsize = 0;

						Uint32 currentItem = 1;		// assume ignore offset table
						determineStartFragment(currentFrame, imageFrames, pixSeq, currentItem);

						// determine the corresponding item (last fragment) for next frame
						Uint32 currentItemLast = 2;		// guess..
						// get the starting fragment of the next frame, if not, it must be all of it.
						if(determineStartFragment(currentFrame + 1, imageFrames, pixSeq, currentItemLast) != EC_Normal)
							currentItemLast = pixSeq->card();

						int i = currentItem;						
						while(i < currentItemLast && result.good())
						{
							result = pixSeq->getItem(pixItem, i++);

							if (result.good())
								compressedsize += pixItem->getLength();							
						}												

						jpgData = new Uint8[compressedsize];
						Uint8 *jpgDataptr = jpgData;

						i = currentItem;
						while(i < currentItemLast && result.good())
						{
							result = pixSeq->getItem(pixItem, i++);

							if (result.good())
							{
								Uint8 *fragment = NULL;
								pixItem->getUint8Array(fragment);
								memcpy(jpgDataptr, fragment, pixItem->getLength());
								jpgDataptr += pixItem->getLength();							
							}
						}						

						if (result.good())
						{                            
							// pointers for buffer copy operations
							Uint8 *outputBuffer8 = NULL;                            

							jas_stream_t *in;
							in = jas_stream_memopen((char *)jpgData, compressedsize);
							jas_image_getfmt(in);
							//jas_image_decode can memleak if the image is corrupt
							jas_image_t *image = jas_image_decode(in, -1, 0);					
							jas_stream_close(in);														

							if (image != NULL)
							{
								outputBuffer8 = imageData8;								

								if (imageBytesAllocated == 1)		// 8bits per component, this should be the most common color images
								{									
									for (int row_index = 0; row_index < jas_image_height(image); row_index++)
									{
										for (int column_index = 0; column_index < jas_image_width(image); column_index++)
										{
											// channels means R, B, G
											for (register int channel_index = 0; channel_index < imageSamplesPerPixel; channel_index++)
											{
												*outputBuffer8 = (Uint8)jas_image_readcmptsample(image, channel_index, column_index, row_index);
												outputBuffer8++;
											}
										}
									}
								}
								else if (imageBytesAllocated == 2)	// 16 bits per component, this should be common for gray scale images
								{
									jas_matrix_t *arow = jas_matrix_create(1 , jas_image_width(image));

									for (register int channel_index = 0; channel_index < imageSamplesPerPixel; channel_index++)
									{
										for (register int row_index = 0; row_index < jas_image_height(image); row_index++)
										{									
											jas_image_readcmpt(image, channel_index, 0, row_index, jas_image_width(image), 1, arow);
											for (register int column_index = 0; column_index < jas_image_width(image); column_index++)
											{
												*((Uint16 *)outputBuffer8) = (Uint16) jas_matrix_getv(arow, column_index); // writes two bytes
												outputBuffer8 += 2;  
											}
										}
									}

									jas_matrix_destroy(arow);

									/*
									for (register int row_index = 0; row_index < jas_image_height(image); row_index++)
									{									
									for (register int column_index = 0; column_index < jas_image_width(image); column_index++)
									{
									for (register int channel_index = 0; channel_index < imageSamplesPerPixel; channel_index++)
									{
									*outputBuffer8 = (Uint16)jas_image_readcmptsample(image, channel_index, column_index, row_index);
									outputBuffer8 += 2;                                            
									}
									}
									}*/
								}
								else
								{
									result = EC_CannotChangeRepresentation;
								}

								if(jas_clrspc_fam(jas_image_clrspc(image)) == JAS_CLRSPC_FAM_RGB)
									colorModel = EPI_RGB;
								else if(jas_clrspc_fam(jas_image_clrspc(image)) == JAS_CLRSPC_FAM_GRAY)
									colorModel = EPI_Monochrome2;
								else if(jas_clrspc_fam(jas_image_clrspc(image)) == JAS_CLRSPC_FAM_YCBCR)
									colorModel = EPI_YBR_Full;

								precision = (imageBytesAllocated * 8);

								jas_image_destroy(image);
							}
							else
							{
								result = EC_CannotChangeRepresentation;
							}
						}

						delete [] jpgData;


						// advance by one frame
						if (result.good())
						{														
							currentFrame++;
							imageData8 += frameSize;
						}

					} /* while still frames to process */

					/*if (result.good())
					{
					// decompression is complete, finally adjust byte order if necessary
					if (jpeg->bytesPerSample() == 1) // we're writing bytes into words
					{
					result = swapIfNecessary(gLocalByteOrder, EBO_LittleEndian, imageData16,
					totalSize, sizeof(Uint16));
					}
					}*/

					if (result.good())
					{
						switch (colorModel)
						{
						case EPI_Monochrome2:
							result = ((DcmItem *)dataset)->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
							if (result.good())
							{
								imageSamplesPerPixel = 1;
								result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
							}
							break;
						case EPI_YBR_Full:
							result = ((DcmItem *)dataset)->putAndInsertString(DCM_PhotometricInterpretation, "YBR_FULL");
							if (result.good())
							{
								imageSamplesPerPixel = 3;
								result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
							}
							break;
						case EPI_RGB:
							result = ((DcmItem *)dataset)->putAndInsertString(DCM_PhotometricInterpretation, "RGB");
							if (result.good())
							{
								imageSamplesPerPixel = 3;
								result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
							}
							break;
						default:
							/* leave photometric interpretation untouched unless it is YBR_FULL_422
							* or YBR_PARTIAL_422. In this case, replace by YBR_FULL since decompression
							* eliminates the subsampling.
							*/
							if ((dicomPI == EPI_YBR_Full_422)||(dicomPI == EPI_YBR_Partial_422))
							{
								result = ((DcmItem *)dataset)->putAndInsertString(DCM_PhotometricInterpretation, "YBR_FULL");
							}
							break;
						}
					}

					// Bits Allocated is now either 8 or 16
					if (result.good())
					{
						if (precision > 8) result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_BitsAllocated, 16);
						else result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_BitsAllocated, 8);
					}

					// Planar Configuration depends on the createPlanarConfiguration flag
					if ((result.good()) && (imageSamplesPerPixel > 1))
					{
						result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_PlanarConfiguration, 0);
					}
					/*	
					// Bits Stored cannot be larger than precision
					if ((result.good()) && (imageBitsStored > precision))
					{
					result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_BitsStored, precision);
					}

					// High Bit cannot be larger than precision - 1
					if ((result.good()) && ((unsigned long)(imageHighBit+1) > (unsigned long)precision))
					{
					result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_HighBit, precision-1);
					}
					*/

				}
			}
		}

		// the following operations do not affect the Image Pixel Module
		// but other modules such as SOP Common.  We only perform these
		// changes if we're on the main level of the dataset,
		// which should always identify itself as dataset, not as item.
		if (dataset->ident() == EVR_dataset)
		{
			/*// create new SOP instance UID if codec parameters require so
			if (result.good() && djcp->getUIDCreation()) result =
			DcmCodec::newInstance(OFstatic_cast(DcmItem *, dataset), NULL, NULL, NULL);*/
		}
	}
	return result;
}


OFCondition FMJP2KCodec::decodeFrame(
	const DcmRepresentationParameter * fromParam,
	DcmPixelSequence * fromPixSeq,
	const DcmCodecParameter * cp,
	DcmItem *dataset,
	Uint32 frameNo,
	Uint32& startFragment,
	void *buffer,
	Uint32 bufSize,
	OFString& decompressedColorModel) const
{

	OFCondition result = EC_Normal;
	// assume we can cast the codec parameter to what we need
	const FMJP2KCodecParameter *djcp = (const FMJP2KCodecParameter *)cp;

	if ((!dataset)||((dataset->ident()!= EVR_dataset) && (dataset->ident()!= EVR_item))) result = EC_InvalidTag;
	else
	{
		Uint16 imageSamplesPerPixel = 0;
		Uint16 imageRows = 0;
		Uint16 imageColumns = 0;
		Sint32 imageFrames = 1;
		Uint16 imageBitsAllocated = 0;
		Uint16 imageBytesAllocated = 0;
		Uint16 imagePlanarConfiguration = 0;
		DcmItem *ditem = OFstatic_cast(DcmItem *, dataset);

		if (result.good()) result = ditem->findAndGetUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
		if (result.good()) result = ditem->findAndGetUint16(DCM_Rows, imageRows);
		if (result.good()) result = ditem->findAndGetUint16(DCM_Columns, imageColumns);
		if (result.good()) result = ditem->findAndGetUint16(DCM_BitsAllocated, imageBitsAllocated);
		if (result.good())
		{
			imageBytesAllocated = OFstatic_cast(Uint16, imageBitsAllocated / 8);
			if ((imageBitsAllocated < 8) || (imageBitsAllocated % 8 != 0)) result = EC_CannotChangeRepresentation;
		}
		if (result.good() && (imageSamplesPerPixel > 1))
		{
			result = ditem->findAndGetUint16(DCM_PlanarConfiguration, imagePlanarConfiguration);

			// DICOM standard says it has to be 0
			if (imagePlanarConfiguration != 0)
				result = EC_CannotChangeRepresentation;
		}

		// number of frames is an optional attribute - we don't mind if it isn't present.
		if (result.good()) (void) ditem->findAndGetSint32(DCM_NumberOfFrames, imageFrames);

		if (imageFrames < 1) imageFrames = 1; // default in case this attribute contains garbage

		// determine the corresponding item (first fragment) for this frame
		Uint32 currentItem = startFragment;

		// if the user has provided this information, we trust him.
		// If the user has passed a zero, try to find out ourselves.
		if (currentItem == 0 && result.good())
		{
			result = determineStartFragment(frameNo, imageFrames, fromPixSeq, currentItem);
		}

		// determine the corresponding item (last fragment) for next frame
		Uint32 currentItemLast = startFragment + 1;		// guess..
		// get the starting fragment of the next frame, if not, it must be all of it.
		if(determineStartFragment(frameNo + 1, imageFrames, fromPixSeq, currentItemLast) != EC_Normal)
			currentItemLast = fromPixSeq->card();

		// output the parameter
		startFragment = currentItemLast;

		// book-keeping needed to clean-up memory the end of this routine
		Uint32 firstFragmentUsed = currentItem;
		Uint32 pastLastFragmentUsed  = firstFragmentUsed;

		if (result.good())
		{
			DcmPixelItem *pixItem = NULL;
			Uint8 * jpgData = NULL;            

			{
				Uint32 frameSize = imageBytesAllocated * imageRows * imageColumns * imageSamplesPerPixel;                
				Uint16 *imageData16 = NULL;
				Sint32 currentFrame = frameNo;
				Uint32 fragmentLength = 0;

				imageData16 = OFreinterpret_cast(Uint16 *, buffer);
				if (result.good())
				{
					Uint8 *imageData8 = OFreinterpret_cast(Uint8 *, imageData16);

					if ((currentFrame < imageFrames) && result.good())
					{
						// find the total of all items
						int compressedsize = 0;
						int i = currentItem;
						while(i < currentItemLast && result.good())
						{
							result = fromPixSeq->getItem(pixItem, i++);

							if (result.good())
								compressedsize += pixItem->getLength();							
						}

						jpgData = new Uint8[compressedsize];
						Uint8 *jpgDataptr = jpgData;

						// now copy the data
						i = currentItem;		
						while(i < currentItemLast && result.good())
						{
							result = fromPixSeq->getItem(pixItem, i++);

							if (result.good())
							{
								Uint8 *fragment = NULL;
								pixItem->getUint8Array(fragment);
								memcpy(jpgDataptr, fragment, pixItem->getLength());
								jpgDataptr += pixItem->getLength();							
							}
						}				

						if (result.good())
						{                            

							// pointers for buffer copy operations
							Uint8 *outputBuffer8 = NULL;                            

							jas_stream_t *in;
							in = jas_stream_memopen((char *)jpgData, fragmentLength);
							//jas_image_decode can memleak if the image is corrupt
							jas_image_t *image = jas_image_decode(in, -1, 0);					
							jas_stream_close(in);

							if (image != NULL)
							{
								outputBuffer8 = imageData8;

								if (imageBytesAllocated == 1)		// 8bits per component, this should be the most common color images
								{
									for (int row_index = 0; row_index < jas_image_height(image); row_index++)
									{
										for (int column_index = 0; column_index < jas_image_width(image); column_index++)
										{
											// channels means R, B, G
											for (register int channel_index = 0; channel_index < imageSamplesPerPixel; channel_index++)
											{
												*outputBuffer8 = (Uint8)jas_image_readcmptsample(image, channel_index, column_index, row_index);
												outputBuffer8++;
											}
										}
									}
								}
								else if (imageBytesAllocated == 2)	// 16 bits per component, this should be common for gray scale images
								{
									jas_matrix_t *arow = jas_matrix_create(1 , jas_image_width(image));

									for (register int channel_index = 0; channel_index < imageSamplesPerPixel; channel_index++)
									{
										for (register int row_index = 0; row_index < jas_image_height(image); row_index++)
										{									
											jas_image_readcmpt(image, channel_index, 0, row_index, jas_image_width(image), 1, arow);
											for (register int column_index = 0; column_index < jas_image_width(image); column_index++)
											{
												*((Uint16 *)outputBuffer8) = (Uint16) jas_matrix_getv(arow, column_index); // writes two bytes
												outputBuffer8 += 2;  
											}
										}
									}

									jas_matrix_destroy(arow);
									/*
									for (register int row_index = 0; row_index < jas_image_height(image); row_index++)
									{									
									for (register int column_index = 0; column_index < jas_image_width(image); column_index++)
									{
									for (register int channel_index = 0; channel_index < imageSamplesPerPixel; channel_index++)
									{
									*outputBuffer8 = (Uint16)jas_image_readcmptsample(image, channel_index, column_index, row_index);
									outputBuffer8 += 2;                                            
									}
									}
									}*/
								}
								else
								{
									result = EC_CannotChangeRepresentation;
								}

								if(jas_clrspc_fam(jas_image_clrspc(image)) == JAS_CLRSPC_FAM_RGB)
								{
									result = ((DcmItem *)dataset)->putAndInsertString(DCM_PhotometricInterpretation, "RGB");
									if (result.good())
									{
										imageSamplesPerPixel = 3;
										result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
									}
								}
								else if(jas_clrspc_fam(jas_image_clrspc(image)) == JAS_CLRSPC_FAM_GRAY)
								{
									result = ((DcmItem *)dataset)->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
									if (result.good())
									{
										imageSamplesPerPixel = 1;
										result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
									}
								}
								else if(jas_clrspc_fam(jas_image_clrspc(image)) == JAS_CLRSPC_FAM_YCBCR)
								{
									result = ((DcmItem *)dataset)->putAndInsertString(DCM_PhotometricInterpretation, "YBR_FULL");
									if (result.good())
									{
										imageSamplesPerPixel = 3;
										result = ((DcmItem *)dataset)->putAndInsertUint16(DCM_SamplesPerPixel, imageSamplesPerPixel);
									}
								}

								jas_image_destroy(image);
							}
							else
							{
								result = EC_CannotChangeRepresentation;
							}
						}

						delete [] jpgData; 

					} 

					swapIfNecessary(gLocalByteOrder, EBO_LittleEndian, imageData16, bufSize, sizeof(Uint16));
				}
			}
		}
	}
	return result;
}

OFCondition FMJP2KCodec::encode(
	const Uint16 *pixelData,
	const Uint32 length,
	const DcmRepresentationParameter * /* toRepParam */ ,
	DcmPixelSequence * & pixSeq,
	const DcmCodecParameter *cp,
	DcmStack & objStack) const
{
	OFCondition result = EC_Normal;

	// assume we can cast the codec parameter to what we need
	const FMJP2KCodecParameter *djcp = OFstatic_cast(const FMJP2KCodecParameter *, cp);
	DcmStack localStack(objStack);
	(void)localStack.pop();             // pop pixel data element from stack
	DcmObject *dataset = localStack.pop(); // this is the item in which the pixel data is located
	Uint8 *pixelData8 = OFreinterpret_cast(Uint8 *, OFconst_cast(Uint16 *, pixelData));
	Uint8 *pixelPointer = NULL;
	DcmOffsetList offsetList;

	Uint32 i;
	OFBool byteSwapped = OFFalse;  // true if we have byte-swapped the original pixel data

	if ((!dataset) || ((dataset->ident() != EVR_dataset) && (dataset->ident() != EVR_item))) result = EC_InvalidTag;
	else
	{
		DcmItem *ditem = OFstatic_cast(DcmItem *, dataset);
		Uint16 bitsAllocated = 0;
		Uint16 bytesAllocated = 0;
		Uint16 samplesPerPixel = 0;
		Uint16 planarConfiguration = 0;
		Uint16 columns = 0;
		Uint16 rows = 0;
		Sint32 numberOfFrames = 1;
		Uint32 numberOfStripes = 0;
		Uint32 compressedSize = 0;

		result = ditem->findAndGetUint16(DCM_BitsAllocated, bitsAllocated);
		if (result.good()) result = ditem->findAndGetUint16(DCM_SamplesPerPixel, samplesPerPixel);
		if (result.good()) result = ditem->findAndGetUint16(DCM_Columns, columns);
		if (result.good()) result = ditem->findAndGetUint16(DCM_Rows, rows);
		if (result.good())
		{
			result = ditem->findAndGetSint32(DCM_NumberOfFrames, numberOfFrames);
			if (result.bad() || numberOfFrames < 1) numberOfFrames = 1;
			result = EC_Normal;
		}
		if (result.good() && (samplesPerPixel > 1))
		{
			result = ditem->findAndGetUint16(DCM_PlanarConfiguration, planarConfiguration);

			// we can't handle planar images
			if (planarConfiguration != 0)
				result = EC_CannotChangeRepresentation;
		}

		if (result.good())
		{
			// check if bitsAllocated is a multiple of 8 - we don't handle anything else
			bytesAllocated = OFstatic_cast(Uint16, bitsAllocated / 8);
			if ((bitsAllocated < 8) || (bitsAllocated % 8 != 0)) result = EC_CannotChangeRepresentation;

			// make sure that all the descriptive attributes have sensible values
			if ((columns < 1) || (rows < 1) || (samplesPerPixel < 1)) result = EC_CannotChangeRepresentation;

			// make sure that we have at least as many bytes of pixel data as we expect
			if (bytesAllocated * samplesPerPixel * columns * rows * numberOfFrames > length) result = EC_CannotChangeRepresentation;
		}

		DcmPixelSequence *pixelSequence = NULL;
		DcmPixelItem *offsetTable = NULL;

		// create initial pixel sequence
		if (result.good())
		{
			pixelSequence = new DcmPixelSequence(DcmTag(DCM_PixelData, EVR_OB));
			if (pixelSequence == NULL) result = EC_MemoryExhausted;
			else
			{
				// create empty offset table
				offsetTable = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
				if (offsetTable == NULL) result = EC_MemoryExhausted;
				else pixelSequence->insert(offsetTable);
			}
		}

		if (result.good())
		{
			const Uint32 frameSize = columns * rows * samplesPerPixel * bytesAllocated;
			Uint32 frameOffset = 0;
			Uint32 sampleOffset = 0;
			Uint32 offsetBetweenSamples = 0;
			Uint32 sample = 0;
			Uint32 byte = 0;


			// compute byte offset between samples
			if (planarConfiguration == 0)
				offsetBetweenSamples = samplesPerPixel * bytesAllocated;

			// loop through all frames of the image
			for (Uint32 currentFrame = 0; ((currentFrame < OFstatic_cast(Uint32, numberOfFrames)) && result.good()); currentFrame++)
			{
				// offset to start of frame, in bytes
				frameOffset = frameSize * currentFrame;

				jas_image_cmptparm_t component_info[4];
				for (i = 0; i < samplesPerPixel; i++)
				{
					component_info[i].tlx = 0;
					component_info[i].tly = 0;
					component_info[i].hstep = 1;
					component_info[i].vstep = 1;
					component_info[i].width = columns;
					component_info[i].height = rows;
					component_info[i].prec = bitsAllocated;
					component_info[i].sgnd = false;
				}

				jas_image_t *jp2_image = jas_image_create(samplesPerPixel, component_info, JAS_CLRSPC_UNKNOWN);

				if (samplesPerPixel == 1)
				{
					/*
					sRGB Grayscale.
					*/
					jas_image_setclrspc(jp2_image, JAS_CLRSPC_SGRAY);
					jas_image_setcmpttype(jp2_image, 0,
						JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_GRAY_Y));
				}
				else
				{
					/*
					sRGB.
					*/
					if (samplesPerPixel == 4 )
						jas_image_setcmpttype(jp2_image, 3, JAS_IMAGE_CT_OPACITY);
					jas_image_setclrspc(jp2_image, JAS_CLRSPC_SRGB);
					jas_image_setcmpttype(jp2_image, 0,
						JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_R));
					jas_image_setcmpttype(jp2_image, 1,
						JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_G));
					jas_image_setcmpttype(jp2_image, 2,
						JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_B));
				}

				Uint8 *pixelData8 = OFreinterpret_cast(Uint8 *, OFconst_cast(Uint16 *, pixelData));
				pixelData8 += frameOffset;

				if (bytesAllocated == 1)
				{
					for (int row_index = 0; row_index < jas_image_height(jp2_image); row_index++)
					{
						for (int column_index = 0; column_index < jas_image_width(jp2_image); column_index++)
						{
							// channels means R, B, G
							for (register int channel_index = 0; channel_index < samplesPerPixel; channel_index++)
							{
								jas_image_writecmptsample(jp2_image, channel_index, column_index, row_index, *pixelData8);
								pixelData8++;
							}
						}
					}
				}
				else if (bytesAllocated == 2)
				{
					Uint16 *pixelData16 = (Uint16 *) pixelData8;
					for (int row_index = 0; row_index < jas_image_height(jp2_image); row_index++)
					{
						for (int column_index = 0; column_index < jas_image_width(jp2_image); column_index++)
						{
							// channels means R, B, G
							for (register int channel_index = 0; channel_index < samplesPerPixel; channel_index++)
							{
								jas_image_writecmptsample(jp2_image, channel_index, column_index, row_index, *pixelData16);
								pixelData16++;
							}
						}
					}
				}

				jas_stream_t *out;
				out = jas_stream_memopen(NULL, 0);
				jas_image_encode(jp2_image, out, jas_image_strtofmt("jpc"), ""); // no optstr, by default lossless (integer mode)

				// magic time ;)  we know the internal structure of out, so cast it directly
				jas_stream_memobj_t *mo = (jas_stream_memobj_t *) out->obj_;
				pixelSequence->storeCompressedFrame(offsetList, mo->buf_, mo->len_, djcp->getFragmentSize());
				compressedSize += mo->len_;

				jas_stream_close(out);
				jas_image_destroy(jp2_image);

			}

			if ((result.good()) && (djcp->getCreateOffsetTable()))
			{
				// create offset table
				result = offsetTable->createOffsetTable(offsetList);
			}

			// store pixel sequence if everything went well.
			if (result.good()) pixSeq = pixelSequence;
			else
			{
				delete pixelSequence;
				pixSeq = NULL;
			}
		}


		// the following operations do not affect the Image Pixel Module
		// but other modules such as SOP Common.  We only perform these
		// changes if we're on the main level of the dataset,
		// which should always identify itself as dataset, not as item.
		if (dataset->ident() == EVR_dataset)
		{
			if (result.good())
			{
				// create new UID if mode is true or if we're converting to Secondary Capture
				if (djcp->getConvertToSC() || djcp->getUIDCreation())
				{
					result = DcmCodec::newInstance(OFstatic_cast(DcmItem *, dataset), "DCM", "121320", "Uncompressed predecessor");

					// set image type to DERIVED
					if (result.good()) result = updateImageType(OFstatic_cast(DcmItem *, dataset));

					// update derivation description
					if (result.good())
					{
						// compute original image size in bytes, ignoring any padding bits.
						double compressionRatio = 0.0;
						if (compressedSize > 0) compressionRatio = (OFstatic_cast(double, columns * rows * bitsAllocated * OFstatic_cast(Uint32, numberOfFrames) * samplesPerPixel) / 8.0) / compressedSize;
						result = updateDerivationDescription(OFstatic_cast(DcmItem *, dataset), compressionRatio);
					}
				}
			}

			// convert to Secondary Capture if requested by user.
			// This method creates a new SOP class UID, so it should be executed
			// after the call to newInstance() which creates a Source Image Sequence.
			if (result.good() && djcp->getConvertToSC()) result = DcmCodec::convertToSecondaryCapture(OFstatic_cast(DcmItem *, dataset));
		}
	}

	return result;
}


OFCondition FMJP2KCodec::encode(
	const E_TransferSyntax /* fromRepType */,
	const DcmRepresentationParameter * /* fromRepParam */,
	DcmPixelSequence * /* fromPixSeq */,
	const DcmRepresentationParameter * /* toRepParam */,
	DcmPixelSequence * & /* toPixSeq */,
	const DcmCodecParameter * /* cp */,
	DcmStack & /* objStack */) const
{
	// we don't support re-coding for now.
	return EC_IllegalCall;
}


OFCondition FMJP2KCodec::updateDerivationDescription(
	DcmItem *dataset,
	double ratio)
{
	char buf[32];

	// create new Derivation Description
	OFString derivationDescription = "Lossless JPEG2000 compression, compression ratio ";
	OFStandard::ftoa(buf, sizeof(buf), ratio, OFStandard::ftoa_uppercase, 0, 5);
	derivationDescription += buf;

	// append old Derivation Description, if any
	const char *oldDerivation = NULL;
	if ((dataset->findAndGetString(DCM_DerivationDescription, oldDerivation)).good() && oldDerivation)
	{
		derivationDescription += " [";
		derivationDescription += oldDerivation;
		derivationDescription += "]";
		if (derivationDescription.length() > 1024)
		{
			// ST is limited to 1024 characters, cut off tail
			derivationDescription.erase(1020);
			derivationDescription += "...]";
		}
	}

	return dataset->putAndInsertString(DCM_DerivationDescription, derivationDescription.c_str());
}

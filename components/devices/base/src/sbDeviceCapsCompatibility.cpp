/* vim: set sw=2 :miv */
/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2009 POTI, Inc.
 * http://www.songbirdnest.com
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the ``GPL'').
 *
 * Software distributed under the License is distributed
 * on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied. See the GPL for the specific language
 * governing rights and limitations.
 *
 * You should have received a copy of the GPL along with this
 * program. If not, go to http://www.gnu.org/licenses/gpl.html
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *=END SONGBIRD GPL
 */

#include "sbDeviceCapsCompatibility.h"

#include <nsArrayUtils.h>

#include <sbFraction.h>
#include <sbIDeviceCapabilities.h>
#include <sbIMediaInspector.h>
#include <sbMemoryUtils.h>

/**
 * To log this class, set the following environment variable in a debug build:
 *  NSPR_LOG_MODULES=sbDeviceCapsCompatibility:5 (or :3 for LOG messages only)
 */
#ifdef PR_LOGGING
static PRLogModuleInfo* gDeviceCapsCompatibility =
                          PR_NewLogModule("sbDeviceCapsCompatibility");
#define LOG(args) PR_LOG(gDeviceCapsCompatibility, PR_LOG_WARNING, args)
#define TRACE(args) PR_LOG(gDeviceCapsCompatibility, PR_LOG_DEBUG, args)
#else /* PR_LOGGING */
#define LOG(args)   /* nothing */
#define TRACE(args) /* nothing */
#endif /* PR_LOGGING */

NS_IMPL_THREADSAFE_ISUPPORTS1(sbDeviceCapsCompatibility,
                              sbIDeviceCapsCompatibility)

sbDeviceCapsCompatibility::sbDeviceCapsCompatibility()
  : mDeviceCapabilities(nsnull),
    mMediaFormat(nsnull),
    mContentType(0),
    mMediaVideoStream(nsnull),
    mMediaAudioStream(nsnull),
    mMediaVideoWidth(0),
    mMediaVideoHeight(0),
    mMediaVideoBitRate(0),
    mMediaVideoSampleRate(0),
    mMediaVideoPARNumerator(0),
    mMediaVideoPARDenominator(0),
    mMediaVideoFRNumerator(0),
    mMediaVideoFRDenominator(0),
    mMediaAudioBitRate(0),
    mMediaAudioSampleRate(0),
    mMediaAudioChannels(0)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
}

sbDeviceCapsCompatibility::~sbDeviceCapsCompatibility()
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  /* destructor code */
}

// Compare nsString to nsCString
static inline PRBool
StringEqualsToCString(nsAString& string1, nsACString& string2)
{
  return string1.Equals(NS_ConvertUTF8toUTF16(string2).BeginReading());
}

/* sbIDeviceCapsCompatibility interface implementation */

NS_IMETHODIMP
sbDeviceCapsCompatibility::Initialize(
                               sbIDeviceCapabilities* aDeviceCapabilities,
                               sbIMediaFormat* aMediaFormat,
                               PRUint32 aContentType)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aDeviceCapabilities);
  NS_ENSURE_ARG_POINTER(aMediaFormat);

  nsresult rv;

  mDeviceCapabilities = aDeviceCapabilities;
  mMediaFormat = aMediaFormat;
  mContentType = aContentType;

  // Get the audio and video streams
  rv = mMediaFormat->GetVideoStream(getter_AddRefs(mMediaVideoStream));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMediaFormat->GetAudioStream(getter_AddRefs(mMediaAudioStream));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapsCompatibility::Compare(PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aCompatible);
  NS_ENSURE_TRUE(mDeviceCapabilities, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mMediaFormat, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  *aCompatible = PR_FALSE;

  switch (mContentType) {
    case sbIDeviceCapabilities::CONTENT_AUDIO:
      rv = CompareAudioFormat(aCompatible);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case sbIDeviceCapabilities::CONTENT_VIDEO:
      rv = CompareVideoFormat(aCompatible);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case sbIDeviceCapabilities::CONTENT_IMAGE:
      rv = CompareImageFormat(aCompatible);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    default:
      NS_WARNING("sbDeviceCapsCompatibility::Compare: "
                 "unknown content type for comparison");
      break;
  }

  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareAudioFormat(PRBool* aCompatible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
sbDeviceCapsCompatibility::CompareImageFormat(PRBool* aCompatible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
sbDeviceCapsCompatibility::CompareVideoFormat(PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aCompatible);
  NS_ENSURE_TRUE(mDeviceCapabilities, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mMediaFormat, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mMediaVideoStream, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Retrive all the video properties
  // Get video container type
  nsCOMPtr<sbIMediaFormatContainer> mediaContainer;
  rv = mMediaFormat->GetContainer(getter_AddRefs(mediaContainer));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(mediaContainer, NS_OK);

  rv = mediaContainer->GetContainerType(mMediaContainerType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get video type
  rv = mMediaVideoStream->GetVideoType(mMediaVideoType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get video width and height
  rv = mMediaVideoStream->GetVideoWidth(&mMediaVideoWidth);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMediaVideoStream->GetVideoHeight(&mMediaVideoHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get video bit rate
  rv = mMediaVideoStream->GetBitRate(&mMediaVideoBitRate);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get video PAR (Pixel aspect ratio)
  rv = mMediaVideoStream->GetVideoPAR(&mMediaVideoPARNumerator,
                                      &mMediaVideoPARDenominator);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get video frame rate
  rv = mMediaVideoStream->GetVideoFrameRate(&mMediaVideoFRNumerator,
                                            &mMediaVideoFRDenominator);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Get additional Video properties

  // Retrive all the audio properties if available
  if (mMediaAudioStream) {
    // Get Audio type
    rv = mMediaAudioStream->GetAudioType(mMediaAudioType);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get Audio bit rate
    rv = mMediaAudioStream->GetBitRate(&mMediaAudioBitRate);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get Audio sample rate
    rv = mMediaAudioStream->GetSampleRate(&mMediaAudioSampleRate);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get Audio channels
    rv = mMediaAudioStream->GetChannels(&mMediaAudioChannels);
    NS_ENSURE_SUCCESS(rv, rv);

    // TODO: Get additional audio properties
  }

  // Get supported formats
  PRUint32 formatsLength;
  char **formats;
  rv = mDeviceCapabilities->GetSupportedFormats(mContentType,
                                                &formatsLength,
                                                &formats);

  if (NS_SUCCEEDED(rv) && formatsLength > 0) {
    sbAutoNSArray<char*> autoFormats(formats, formatsLength);
    for (PRUint32 formatIndex = 0;
         formatIndex < formatsLength;
         ++formatIndex) {
      NS_ConvertASCIItoUTF16 format(formats[formatIndex]);
    
      nsCOMPtr<sbIVideoFormatType> videoFormat;
      rv = mDeviceCapabilities->GetFormatType(format,
                                              getter_AddRefs(videoFormat));
      if (NS_SUCCEEDED(rv) && videoFormat) {
        // Compare container type
        nsCString deviceContainerType;
        rv = videoFormat->GetContainerType(deviceContainerType);
        NS_ENSURE_SUCCESS(rv, rv);

        // Container type not equal
        if (!StringEqualsToCString(mMediaContainerType, deviceContainerType))
          continue;

        // Get device video stream
        nsCOMPtr<sbIDevCapVideoStream> videoStream;
        rv = videoFormat->GetVideoStream(getter_AddRefs(videoStream));
        NS_ENSURE_SUCCESS(rv, rv);

        // Skip to the next format
        if (!videoStream) {
          LOG(("%s[%p] -- Empty video stream for supported video format",
               __FUNCTION__, this));
          continue;
        }

        rv = CompareVideoStream(videoStream, aCompatible);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!(*aCompatible))
          continue;

        if (mMediaAudioStream) {
          // Get device audio stream
          nsCOMPtr<sbIDevCapAudioStream> audioStream;
          rv = videoFormat->GetAudioStream(getter_AddRefs(audioStream));
          NS_ENSURE_SUCCESS(rv, rv);

          // Skip to the next format
          if (!audioStream) {
            LOG(("%s[%p] -- Empty audio stream for supported video format",
                 __FUNCTION__, this));
            continue;
          }

          rv = CompareAudioStream(audioStream, aCompatible);
          NS_ENSURE_SUCCESS(rv, rv);
          if (!(*aCompatible))
            continue;
        }

        // Come this far means we get a match. Compatible.
        break;
      }
    }
  }

  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareVideoStream(
                               sbIDevCapVideoStream* aVideoStream,
                               PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aVideoStream);
  NS_ENSURE_ARG_POINTER(aCompatible);
  NS_ENSURE_TRUE(mMediaVideoStream, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare video type
  nsCString deviceVideoType;
  rv = aVideoStream->GetType(deviceVideoType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Video type not equal. Not compatible.
  if (!StringEqualsToCString(mMediaVideoType, deviceVideoType))
    return NS_OK;

  rv = CompareVideoWidthAndHeight(aVideoStream, aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!(*aCompatible))
    return NS_OK;

  // Video bit rate could be unknown in the media file.
  if (mMediaVideoBitRate) {
    rv = CompareVideoBitRate(aVideoStream, aCompatible);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!(*aCompatible))
      return NS_OK;
  }

  rv = CompareVideoPAR(aVideoStream, aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!(*aCompatible))
    return NS_OK;

  rv = CompareVideoFrameRate(aVideoStream, aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!(*aCompatible))
    return NS_OK;

  // TODO:Insert the comparison for additional properties here

  // Compatible and return.
  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareAudioStream(
                               sbIDevCapAudioStream* aAudioStream,
                               PRBool *aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aAudioStream);
  NS_ENSURE_ARG_POINTER(aCompatible);
  NS_ENSURE_TRUE(mMediaAudioStream, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare audio type
  nsCString deviceAudioType;
  rv = aAudioStream->GetType(deviceAudioType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Audio type not equal. Not compatible.
  if (!StringEqualsToCString(mMediaAudioType, deviceAudioType))
    return NS_OK;

  // Audio bit rate could be unknown in the media file.
  if (mMediaAudioBitRate) {
    rv = CompareAudioBitRate(aAudioStream, aCompatible);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!(*aCompatible))
      return NS_OK;
  }

  rv = CompareAudioSampleRate(aAudioStream, aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!(*aCompatible))
    return NS_OK;

  rv = CompareAudioChannels(aAudioStream, aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!(*aCompatible))
    return NS_OK;

  // TODO:Insert the comparison for additional properties here

  // Compatible and return.
  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareVideoWidthAndHeight(
                               sbIDevCapVideoStream* aVideoStream,
                               PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aVideoStream);
  NS_ENSURE_ARG_POINTER(aCompatible);

  nsresult rv;

  // Get supported widths and heights from device capabilities
  nsCOMPtr<sbIDevCapRange> deviceSupportedWidths;
  rv = aVideoStream->GetSupportedWidths(
                         getter_AddRefs(deviceSupportedWidths));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIDevCapRange> deviceSupportedHeights;
  rv = aVideoStream->GetSupportedHeights(
                         getter_AddRefs(deviceSupportedHeights));
  NS_ENSURE_SUCCESS(rv, rv);

  // Compare width and height
  if (deviceSupportedWidths && deviceSupportedHeights) {
    PRBool inRange = PR_FALSE;
    rv = deviceSupportedWidths->IsValueInRange(mMediaVideoWidth, &inRange);
    NS_ENSURE_SUCCESS(rv, rv);

    // Video width not in range. Not compatible.
    if (!inRange)
      return NS_OK;

    rv = deviceSupportedHeights->IsValueInRange(mMediaVideoHeight, &inRange);
    NS_ENSURE_SUCCESS(rv, rv);

    *aCompatible = inRange;
  }
  // Fall back to the array of supported sizes.
  else {
    nsCOMPtr<nsIArray> deviceSupportedExplicitSizes;
    rv = aVideoStream->GetSupportedExplicitSizes(
                           getter_AddRefs(deviceSupportedExplicitSizes));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 length;
    rv = deviceSupportedExplicitSizes->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(length > 0, "supported sizes must not be empty!");

    if (length > 1) {
      PRBool match = PR_FALSE;
      for (PRUint32 index = 0; index < length; ++index) {
        nsCOMPtr<sbIImageSize> supportedSize =
          do_QueryElementAt(deviceSupportedExplicitSizes, index, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 width, height;
        rv = supportedSize->GetWidth(&width);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = supportedSize->GetHeight(&height);
        NS_ENSURE_SUCCESS(rv, rv);

        // Match. Compatible.
        if (mMediaVideoWidth == width && mMediaVideoHeight == height) {
          match = PR_TRUE;
          break;
        }
      }

      *aCompatible = match;
    }
    // Single entry means 'recommended' size and arbitrary dimensions
    // are supported.
    else if (length == 1)
      *aCompatible = PR_TRUE;
  }
  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareVideoBitRate(
                               sbIDevCapVideoStream* aVideoStream,
                               PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aVideoStream);
  NS_ENSURE_ARG_POINTER(aCompatible);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare video bit rate
  nsCOMPtr<sbIDevCapRange> deviceSupportedBitRates;
  rv = aVideoStream->GetSupportedBitRates(
                         getter_AddRefs(deviceSupportedBitRates));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = deviceSupportedBitRates->IsValueInRange(mMediaVideoBitRate, aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareVideoPAR(
                                sbIDevCapVideoStream* aVideoStream,
                                PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aVideoStream);
  NS_ENSURE_ARG_POINTER(aCompatible);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare video PAR
  char **deviceSupportedVideoPARs;
  PRUint32 deviceVideoPARsCount;
  rv = aVideoStream->GetSupportedVideoPARs(&deviceVideoPARsCount,
                                           &deviceSupportedVideoPARs);
  NS_ENSURE_SUCCESS(rv, rv);

  sbAutoNSArray<char*> autoVideoPARs(deviceSupportedVideoPARs,
                                     deviceVideoPARsCount);

  for (PRUint32 i = 0; i < deviceVideoPARsCount; ++i) {
    sbFraction fraction;
    rv = sbFractionFromString(
             NS_ConvertASCIItoUTF16(deviceSupportedVideoPARs[i]),
             fraction);
    NS_ENSURE_SUCCESS(rv, rv);

    // Match
    if (fraction == sbFraction(mMediaVideoPARNumerator,
                               mMediaVideoPARDenominator)) {
      *aCompatible = PR_TRUE;
      break;
    }
  }

  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareVideoFrameRate(
                               sbIDevCapVideoStream* aVideoStream,
                               PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aVideoStream);
  NS_ENSURE_ARG_POINTER(aCompatible);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare frame rate
  char **deviceSupportedFrameRates;
  PRUint32 deviceFrameRatesCount;
  rv = aVideoStream->GetSupportedFrameRates(&deviceFrameRatesCount,
                                            &deviceSupportedFrameRates);
  NS_ENSURE_SUCCESS(rv, rv);

  sbAutoNSArray<char*> autoFrameRates(deviceSupportedFrameRates,
                                      deviceFrameRatesCount);
  for (PRUint32 i = 0; i < deviceFrameRatesCount; ++i) {
    sbFraction fraction;
    rv = sbFractionFromString(
             NS_ConvertASCIItoUTF16(deviceSupportedFrameRates[i]),
             fraction);
    NS_ENSURE_SUCCESS(rv, rv);

    // Match
    if (fraction == sbFraction(mMediaVideoFRNumerator,
                               mMediaVideoFRDenominator)) {
      *aCompatible = PR_TRUE;
      break;
    }
  }

  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareAudioBitRate(
                               sbIDevCapAudioStream* aAudioStream,
                               PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aAudioStream);
  NS_ENSURE_ARG_POINTER(aCompatible);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare audio bit rate
  nsCOMPtr<sbIDevCapRange> deviceSupportedBitRates;
  rv = aAudioStream->GetSupportedBitRates(
                         getter_AddRefs(deviceSupportedBitRates));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = deviceSupportedBitRates->IsValueInRange(mMediaAudioBitRate, aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareAudioSampleRate(
                               sbIDevCapAudioStream* aAudioStream,
                               PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aAudioStream);
  NS_ENSURE_ARG_POINTER(aCompatible);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare audio sample bit
  nsCOMPtr<sbIDevCapRange> deviceSupportedSampleRates;
  rv = aAudioStream->GetSupportedSampleRates(
                         getter_AddRefs(deviceSupportedSampleRates));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = deviceSupportedSampleRates->IsValueInRange(mMediaAudioSampleRate,
                                                  aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbDeviceCapsCompatibility::CompareAudioChannels(
                               sbIDevCapAudioStream* aAudioStream,
                               PRBool* aCompatible)
{
  TRACE(("%s[%p]", __FUNCTION__, this));
  NS_ENSURE_ARG_POINTER(aAudioStream);
  NS_ENSURE_ARG_POINTER(aCompatible);

  nsresult rv;
  *aCompatible = PR_FALSE;

  // Compare audio channels
  nsCOMPtr<sbIDevCapRange> deviceSupportedChannels;
  rv = aAudioStream->GetSupportedChannels(
                         getter_AddRefs(deviceSupportedChannels));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = deviceSupportedChannels->IsValueInRange(mMediaAudioChannels,
                                               aCompatible);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
/*
//
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright 2006 Pioneers of the Inevitable LLC
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the GPL).
// 
// Software distributed under the License is distributed 
// on an AS IS basis, WITHOUT WARRANTY OF ANY KIND, either 
// express or implied. See the GPL for the specific language 
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this 
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc., 
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
// 
// END SONGBIRD GPL
//
*/

/** 
* \file  DeviceBase.cpp
* \brief Songbird DeviceBase Component Implementation.
*/

#include "DeviceBase.h"
#include "sbIDeviceBase.h"

#if defined(XP_WIN)
#include "objbase.h"
#endif

#include "nspr.h"

#include <xpcom/nsXPCOM.h>
#include <xpcom/nsComponentManagerUtils.h>
#include <xpcom/nsAutoLock.h>
#include <xpcom/nsMemory.h>

#include <xpcom/nsCRT.h>

#include "IDatabaseResult.h"
#include "IDatabaseQuery.h"

#include "IMediaLibrary.h"
#include "IPlaylist.h"

#define MSG_DEVICE_BASE             (0x2000) // Base message ID

#define MSG_DEVICE_TRANSFER         (MSG_DEVICE_BASE + 0)
#define MSG_DEVICE_UPLOAD           (MSG_DEVICE_BASE + 1)
#define MSG_DEVICE_DELETE           (MSG_DEVICE_BASE + 2)
#define MSG_DEVICE_UPDATE_METADATA  (MSG_DEVICE_BASE + 3)
#define MSG_DEVICE_EVENT            (MSG_DEVICE_BASE + 4)
#define MSG_DEVICE_INITIALIZE       (MSG_DEVICE_BASE + 5)
#define MSG_DEVICE_FINALIZE         (MSG_DEVICE_BASE + 6)
#define MSG_DEVICE_EJECT            (MSG_DEVICE_BASE + 7)

#define TRANSFER_TABLE_NAME         NS_LITERAL_STRING("Transfer")
#define URL_COLUMN_NAME             NS_LITERAL_STRING("url")
#define SOURCE_COLUMN_NAME          NS_LITERAL_STRING("source")
#define DESTINATION_COLUMN_NAME     NS_LITERAL_STRING("destination")
#define INDEX_COLUMN_NAME           NS_LITERAL_STRING("id")

NS_IMPL_THREADSAFE_ISUPPORTS1(sbDeviceThread, nsIRunnable)

//-----------------------------------------------------------------------------
sbDeviceBase::sbDeviceBase(PRBool usingThread)
: mDeviceQueueHasItem(PR_FALSE)
, mpCallbackListLock(PR_NewLock())
, mpDeviceThread(nsnull)
, mpDeviceThreadMonitor(nsAutoMonitor::NewMonitor("sbDeviceBase.mpDeviceThreadMonitor"))
, mDeviceThreadShouldShutdown(PR_FALSE)
{
  NS_ASSERTION(mpCallbackListLock, "sbDeviceBase.mpCallbackListLock failed");
  NS_ASSERTION(mpDeviceThreadMonitor, "sbDeviceBase.mpDeviceThreadMonitor failed");
  if (usingThread) {
    do {
      nsCOMPtr<nsIRunnable> pThread = new sbDeviceThread(this);
      NS_ASSERTION(pThread, "Unable to create sbDeviceThread");
      if (!pThread) 
        break;
      nsresult rv = NS_NewThread(getter_AddRefs(mpDeviceThread),
        pThread,
        0,
        PR_JOINABLE_THREAD);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to start sbDeviceThread");
      if (NS_FAILED(rv))
        mpDeviceThread = nsnull;
    } while (PR_FALSE); // Only do this once
  }
} //ctor

//-----------------------------------------------------------------------------
sbDeviceBase::~sbDeviceBase()
{
  // Shutdown the thread
  if (mpDeviceThread) {
    {
      nsAutoMonitor mon(mpDeviceThreadMonitor);
      mDeviceThreadShouldShutdown = PR_TRUE;
      mon.NotifyAll();
    }
    mpDeviceThread->Join();
    mpDeviceThread = nsnull;
  }

  {
    nsAutoLock listLock(mpCallbackListLock);
    if(mCallbackList.size()) {
      for(size_t nCurrent = 0; nCurrent < mCallbackList.size(); ++nCurrent) {
        if(mCallbackList[nCurrent])
          mCallbackList[nCurrent]->Release();
      }
      mCallbackList.clear();
    }
  }

  if (mpCallbackListLock)
    PR_DestroyLock(mpCallbackListLock);
  if (mpDeviceThreadMonitor)
    nsAutoMonitor::DestroyMonitor(mpDeviceThreadMonitor);
} //dtor

/* PRBool Initialize (); */
NS_IMETHODIMP sbDeviceBase::Initialize(PRBool *_retval)
{
  NS_NOTYETIMPLEMENTED("sbDeviceBase::Initialize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool Finalize (); */
NS_IMETHODIMP sbDeviceBase::Finalize(PRBool *_retval)
{
  NS_NOTYETIMPLEMENTED("sbDeviceBase::Finalize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool AddCallback (in sbIDeviceBaseCallback pCallback); */
NS_IMETHODIMP sbDeviceBase::AddCallback(sbIDeviceBaseCallback *pCallback, PRBool *_retval)
{
  *_retval = PR_TRUE;

  nsAutoLock lock(mpCallbackListLock);
  size_t nSize = mCallbackList.size();

  for(size_t nCurrent = 0; nCurrent < nSize; nCurrent++) {
    if(mCallbackList[nCurrent] == pCallback)
      return NS_OK;
  }

  pCallback->AddRef();
  mCallbackList.push_back(pCallback);

  return NS_OK;
}

/* PRBool RemoveCallback (in sbIDeviceBaseCallback pCallback); */
NS_IMETHODIMP sbDeviceBase::RemoveCallback(sbIDeviceBaseCallback *pCallback, PRBool *_retval)
{
  *_retval = PR_FALSE;

  nsAutoLock lock(mpCallbackListLock);
  std::vector<sbIDeviceBaseCallback *>::iterator itCallback = mCallbackList.begin();

  for(; itCallback != mCallbackList.end(); ++itCallback) {
    if((*itCallback) == pCallback) {
      (*itCallback)->Release();
      mCallbackList.erase(itCallback);

      *_retval = PR_TRUE;
      return NS_OK;
    }
  }

  return NS_OK;
}

/* static */
void PR_CALLBACK sbDeviceBase::DeviceProcess(sbDeviceBase* pDevice)
{
  NS_ASSERTION(pDevice, "Null Pointer!");

  ThreadMessage* pDeviceMessage;

  pDevice->OnThreadBegin();

  while (PR_TRUE) {

    pDeviceMessage = nsnull;

    { // Enter Monitor

      nsAutoMonitor mon(pDevice->mpDeviceThreadMonitor);
      while (!pDevice->mDeviceQueueHasItem && !pDevice->mDeviceThreadShouldShutdown)
        mon.Wait();

      if (pDevice->mDeviceThreadShouldShutdown) {
        pDevice->OnThreadEnd();
        return;
      }

      pDeviceMessage = pDevice->mDeviceMessageQueue.front();
      pDevice->mDeviceMessageQueue.pop_front();
      if (pDevice->mDeviceMessageQueue.empty())
        pDevice->mDeviceQueueHasItem = PR_FALSE;

    } // Exit Monitor

    NS_ASSERTION(pDeviceMessage, "No message!");

    switch(pDeviceMessage->message) {
      case MSG_DEVICE_TRANSFER:
        pDevice->TransferNextFile((PRInt32) pDeviceMessage->data1, (TransferData *) pDeviceMessage->data2);
        break;
      case MSG_DEVICE_INITIALIZE:
        pDevice->InitializeSync();
        break;
      case MSG_DEVICE_FINALIZE:
        pDevice->FinalizeSync();
        break;
      case MSG_DEVICE_EVENT:
        pDevice->DeviceEventSync((PRBool) pDeviceMessage->data2);
        break;
      default:
        NS_NOTREACHED("Bad DeviceMessage");
        break;
    }

    delete pDeviceMessage;
  } // while
}

PRBool sbDeviceBase::SubmitMessage(PRUint32 message, void* data1, void* data2)
{
  //NS_ENSURE_TRUE(mDeviceThread.GetThread(), PR_FALSE);

  ThreadMessage* pDeviceMessage = new ThreadMessage;
  NS_ENSURE_TRUE(pDeviceMessage, PR_FALSE);

  pDeviceMessage->message = message;
  pDeviceMessage->data1 = data1;
  pDeviceMessage->data2 = data2;

  {
    nsAutoMonitor mon(mpDeviceThreadMonitor);
    mDeviceMessageQueue.push_back(pDeviceMessage);
    mDeviceQueueHasItem = PR_TRUE;
    mon.Notify();
  }

  return PR_TRUE;
}

/* wstring GetDeviceCategory (); */
NS_IMETHODIMP sbDeviceBase::GetDeviceCategory(PRUnichar **_retval)
{
  NS_NOTYETIMPLEMENTED("sbDeviceBase::GetDeviceCategory");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring EnumDeviceString (in PRUint32 index); */
NS_IMETHODIMP sbDeviceBase::EnumDeviceString(PRUint32 index, PRUnichar **_retval)
{
  NS_NOTYETIMPLEMENTED("sbDeviceBase::EnumDeviceString");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring GetContext (in wstring deviceString); */
NS_IMETHODIMP sbDeviceBase::GetContext(const PRUnichar *deviceString, PRUnichar **_retval)
{
  NS_NOTYETIMPLEMENTED("sbDeviceBase::GetContext");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute wstring name; */
NS_IMETHODIMP sbDeviceBase::GetName(PRUnichar * *aName)
{
  return NS_OK;
}
NS_IMETHODIMP sbDeviceBase::SetName(const PRUnichar * aName)
{
  return NS_OK;
}

/* PRBool IsDownloadSupported (); */
NS_IMETHODIMP sbDeviceBase::IsDownloadSupported(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* PRUint32 GetSupportedFormats (); */
NS_IMETHODIMP sbDeviceBase::GetSupportedFormats(const PRUnichar *deviceString, PRUint32 *_retval)
{
  return NS_OK;
}

/* PRBool IsUploadSupported (); */
NS_IMETHODIMP sbDeviceBase::IsUploadSupported(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* PRBool IsTransfering (); */
NS_IMETHODIMP sbDeviceBase::IsTransfering(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = IsTransferInProgress(deviceString);
  return NS_OK;
}

/* PRBool IsDeleteSupported (); */
NS_IMETHODIMP sbDeviceBase::IsDeleteSupported(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* PRUint32 GetUsedSpace (); */
NS_IMETHODIMP sbDeviceBase::GetUsedSpace(const PRUnichar *deviceString, PRUint32 *_retval)
{
  return NS_OK;
}

/* PRUint32 GetFreeSpace (); */
NS_IMETHODIMP sbDeviceBase::GetAvailableSpace(const PRUnichar *deviceString, PRUint32 *_retval)
{
  return NS_OK;
}

/* PRBool GetTrackTable (out wstring dbContext, out wstring tableName); */
NS_IMETHODIMP sbDeviceBase::GetTrackTable(const PRUnichar *deviceString, PRUnichar **dbContext, PRUnichar **tableName, PRBool *_retval)
{
  return NS_OK;
}

/* PRBool TransferTrackTable (in wstring dbContext, in wstring tableName); */
PRBool sbDeviceBase::StartTransfer(const PRUnichar *deviceString, const PRUnichar *tableName)
{
  TransferData* data = new TransferData;
  NS_ENSURE_TRUE(data, PR_FALSE);

  data->deviceString = deviceString;
  PRUnichar* context = nsnull;
  GetContext(deviceString, &context);
  data->dbContext = context;
  PR_Free(context);
  data->dbTable = tableName;

  // Send a thread message if using a thread otherwise just call the Tranfer function.
  return TransferNextFile(-1, data);
  PRBool retval = true ?
    SubmitMessage(MSG_DEVICE_TRANSFER, (void*)-1, (void*)data) :
  TransferNextFile(-1, data);

  return retval;
}

/* PRBool AbortTransfer (); */
NS_IMETHODIMP sbDeviceBase::AbortTransfer(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = StopCurrentTransfer(deviceString);
  return NS_OK;
}

/* PRBool DeleteTrackTable (in wstring dbContext, in wstring tableName); */
NS_IMETHODIMP sbDeviceBase::DeleteTable(const PRUnichar *deviceString, const PRUnichar *dbContext, const PRUnichar *tableName, PRBool *_retval)
{
  return NS_OK;
}

/* PRBool IsUpdateSupported (); */
NS_IMETHODIMP sbDeviceBase::IsUpdateSupported(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* PRBool UpdateTable (in wstring dbContext, in wstring tableName); */
NS_IMETHODIMP sbDeviceBase::UpdateTable(const PRUnichar *deviceString, const PRUnichar *tableName, PRBool *_retval)
{
  return NS_OK;
}

/* PRBool IsEjectSupported (); */
NS_IMETHODIMP sbDeviceBase::IsEjectSupported(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* PRBool EjectDevice (); */
NS_IMETHODIMP sbDeviceBase::EjectDevice(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (IsEjectSupported(deviceString, _retval)) {
    SubmitMessage(MSG_DEVICE_EJECT, 0, 0);
    *_retval = PR_TRUE;
  }
  return NS_OK;
}

// Use the prevIndex to get the next row in the table which will have an "id" value greater
// that prevIndex.
PRBool sbDeviceBase::GetNextTransferFileEntry(PRInt32 prevIndex, const PRUnichar *deviceString, PRBool bDownloading, PRInt32& curIndex, nsString& recvSource, nsString& recvDestination)
{
  PRUnichar* dbContext;
  GetContext(deviceString, &dbContext);
  nsString transferTable = bDownloading?GetDeviceDownloadTable(deviceString):GetDeviceUploadTable(deviceString);

  sbIDatabaseResult* resultset;
  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );

  if (!query)
    return PR_FALSE;

  nsString query_str(NS_LITERAL_STRING("select * from "));
  query_str += transferTable;
  query_str += NS_LITERAL_STRING(" where id > ");
  query_str.AppendInt(prevIndex);
  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(dbContext);
  PR_Free(dbContext);
  query->AddQuery(query_str.get()); 
  PRInt32 ret;
  query->Execute(&ret);
  if (ret)
    return PR_FALSE;

  query->GetResultObject(&resultset);

  PRInt32 rowcount = 0;
  resultset->GetRowCount( &rowcount );

  if ( rowcount )
  {
    // We only want the first record
    PRUint32 rowNumber = 0;
    PRUnichar *id = NULL;
    resultset->GetRowCellByColumn(rowNumber, INDEX_COLUMN_NAME.get(), &id);
    PRInt32 conversionError = 0;
    curIndex = nsString(id).ToInteger(&conversionError);
    
    PRUnichar *source = NULL;
    resultset->GetRowCellByColumn(rowNumber, SOURCE_COLUMN_NAME.get(), &source);
    recvSource = source;

    PRUnichar *destination = NULL;
    resultset->GetRowCellByColumn(rowNumber, DESTINATION_COLUMN_NAME.get(), &destination);
    recvDestination = destination;

    PR_Free(id);
    PR_Free(source);
    PR_Free(destination);

    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool sbDeviceBase::TransferNextFile(PRInt32 prevTransferRowNumber, void *data)
{
  if (!data)
    return PR_FALSE;

  nsString dbContext = ((TransferData *)data)->dbContext;
  nsString tableName = ((TransferData *)data)->dbTable;
  PRUnichar* deviceString = (PRUnichar *) ((TransferData *)data)->deviceString.get();


  // Read the table for Transferring files
  sbIDatabaseResult* resultset;
  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );

  // This can happen when the app is shutting down
  if (!query)
    return PR_FALSE;

  nsString query_str(NS_LITERAL_STRING("select * from "));
  query_str += tableName;
  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(dbContext.get()); 
  query->AddQuery(query_str.get()); 

  PRInt32 ret;
  query->Execute(&ret);
  query->GetResultObject(&resultset);

  // These are the names of the columns in Transfer table
  // that will be used in obtaining the column indexes.
  const nsString sourcePathColumnName = SOURCE_COLUMN_NAME;
  const nsString destinationPathColumnName = DESTINATION_COLUMN_NAME;
  const nsString indexColumnName = INDEX_COLUMN_NAME;

  PRUnichar *sourcePath;
  PRUnichar *destinationPath;
  PRUnichar *index;

  PRInt32 sourcePathColumnIndex = -1;
  PRInt32 destinationPathColumnIndex = -1;
  PRUint32 indexColumnIndex = -1;
  PRInt32 colCount;
  PRBool transferComplete = PR_TRUE;

  // Find the column indexes with the Transfer info
  resultset->GetColumnCount(&colCount);
  for (PRInt32 colNumber = 0; colNumber < colCount; colNumber ++)
  {
    PRUnichar *columnName;
    resultset->GetColumnName(colNumber, &columnName);
    if (sourcePathColumnName == nsString(columnName))
    {
      sourcePathColumnIndex = colNumber;
    }
    else
      if (destinationPathColumnName == nsString(columnName))
      {
        destinationPathColumnIndex = colNumber;
      }
      else
        if (indexColumnName == nsString(columnName))
        {
          indexColumnIndex = colNumber;
        }
  }

  if ((sourcePathColumnIndex != -1) && (destinationPathColumnIndex != -1)) 
  {

    // Now iterate thru the resultset to get the next Transfer
    // information.
    PRInt32 rowcount;
    resultset->GetRowCount( &rowcount );

/*
    mig - This does not seem to be an error.

    if ((rowcount > 0) && (prevTransferRowNumber == -1))
    {
      NS_NOTREACHED("(rowcount > 0) && (prevTransferRowNumber == -1)");
    }
*/

    // Iterate thru the list of download track records to
    // find the first available track to download.
    PRInt32 rowNumber = 0;
    for ( ; rowNumber < rowcount; rowNumber++ )
    {
      // Get progress value
      PRUnichar *progressString = NULL;
      resultset->GetRowCellByColumn(rowNumber, NS_LITERAL_STRING("progress").get(), &progressString);
      PRInt32 errorCode;
      PRInt32 progress = nsString(progressString).ToInteger(&errorCode);
      PR_Free(progressString);
      if (progress == 100)
      {
        continue; // This track has already completed downloading
      }

      // Transfer this file
      resultset->GetRowCell(rowNumber, sourcePathColumnIndex, &sourcePath);
      resultset->GetRowCell(rowNumber, destinationPathColumnIndex, &destinationPath);
      resultset->GetRowCell(rowNumber, indexColumnIndex, &index);

      PRBool bRet = TransferFile(deviceString, sourcePath, destinationPath, (PRUnichar *) dbContext.get(), (PRUnichar *) tableName.get(), index, rowNumber);
      if(bRet)
      {
        DoTransferStartCallback(sourcePath, destinationPath);
      }

      PR_Free(index);
      PR_Free(sourcePath);
      PR_Free(destinationPath);

      break;

    }

    if (rowNumber != rowcount)
    {
      // Last Transfer is not done yet
      transferComplete = PR_FALSE;
    }
  }
  resultset->Release();

  if (transferComplete)
  {
    TransferComplete();
    DeviceIdle(deviceString);
  }

  delete data;
  data = nsnull;

  return PR_TRUE;
}

PRBool sbDeviceBase::UpdateIOProgress(PRUnichar* deviceString, PRUnichar* table, PRUnichar* index, PRUint32 percentComplete)
{
  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  nsString query_str;
  PRInt32 ret = -1;
  PRUnichar* dbContext;
  GetContext(deviceString, &dbContext);

  if ( query ) 
  {
    query->SetAsyncQuery( PR_FALSE ); // Would need callbacks
    query->SetDatabaseGUID(dbContext);

    // check if this record is removed, in which case
    // we should skip to transferring the next transfer table record.
    sbIDatabaseResult* resultset;

    query_str = NS_LITERAL_STRING("select * from ");
    query_str += table;
    query_str += NS_LITERAL_STRING(" WHERE id = ");
    query_str += index;
    query->AddQuery(query_str.get());
    query->Execute(&ret);
    query->GetResultObject(&resultset);
    PRInt32 numRows = 0;
    resultset->GetRowCount(&numRows);
    query->ResetQuery();
    if (numRows == 0)
    {
      // Yes, as we suspected ...
      StopCurrentTransfer(deviceString);
      // Start a new transfer as this would initiate a transfer
      // on the next transfer table record.
      StartTransfer(deviceString, table);
    }
    else
    {

      query_str = NS_LITERAL_STRING("UPDATE ");
      query_str += table;
      query_str += NS_LITERAL_STRING(" SET ");
      query_str += NS_LITERAL_STRING(" Progress = ");
      query_str.AppendInt( percentComplete ); // ahem.  this is not a string.
      query_str += NS_LITERAL_STRING(" WHERE id = ");
      query_str += index;

      query->AddQuery(query_str.get());

      if (percentComplete == 100) 
      {
        // Update the location
        query_str = NS_LITERAL_STRING("UPDATE ");
        query_str += table;
        query_str += NS_LITERAL_STRING(" SET ");
        query_str += NS_LITERAL_STRING(" url = destination");
        query_str += NS_LITERAL_STRING(" WHERE id = ");
        query_str += index;

        query->AddQuery(query_str.get());
      }
      query->Execute(&ret);
    }
  }

  PR_Free(dbContext);

  return (ret != 0);
}

PRBool sbDeviceBase::UpdateIOStatus(PRUnichar* deviceString, PRUnichar* table, PRUnichar* index, const PRUnichar* status)
{
  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  PRUnichar* dbContext;
  GetContext(deviceString, &dbContext);

  nsString query_str(NS_LITERAL_STRING("Update "));
  query_str += table;
  query_str += NS_LITERAL_STRING(" set ");
  query_str += NS_LITERAL_STRING(" Status=\"");
  query_str += status;
  query_str += NS_LITERAL_STRING("\" where id=");
  query_str += index;

  query->SetAsyncQuery( PR_FALSE ); // Would need callbacks
  query->SetDatabaseGUID(dbContext);
  query->AddQuery(query_str.get());
  PRInt32 ret;
  query->Execute(&ret);

  PR_Free(dbContext);

  return (ret != 0);
}

/* wstring GetNumDestinations (in wstring DeviceCategory, in wstring DeviceString); */
NS_IMETHODIMP sbDeviceBase::GetNumDestinations(const PRUnichar *DeviceString, PRUnichar **_retval)
{
  return NS_OK; 
}

/* PRBool MakeTransferTable (in wstring DeviceCategory, in wstring DeviceString, in wstring TableName); */
NS_IMETHODIMP sbDeviceBase::MakeTransferTable(const PRUnichar *DeviceString, const PRUnichar *ContextInput, const PRUnichar *TableName, const PRUnichar *FilterColumn, PRUint32 FilterCount, const PRUnichar **FilterValues, const PRUnichar *sourcePath, const PRUnichar *destPath, PRBool bDownloading, PRUnichar **TransferTableName, PRBool *_retval)
{
  *_retval = CreateTransferTable(DeviceString, 
                                ContextInput, 
                                TableName, 
                                FilterColumn, 
                                FilterCount, 
                                FilterValues, 
                                sourcePath, 
                                destPath, 
                                bDownloading, 
                                TransferTableName);
  return NS_OK;
}

/* PRBool DownloadTable (in wstring DeviceCategory, in wstring DeviceString, in wstring TableName); */
NS_IMETHODIMP sbDeviceBase::DownloadTable(const PRUnichar *DeviceString, const PRUnichar *TableName, PRBool *_retval)
{
  // Initiate a download assuming the caller has previously called MakeTransferTable() to create a download table
  // in Device's DB context.
  *_retval = PR_FALSE;

  if (IsDeviceIdle(DeviceString))
  {
    DeviceDownloading(DeviceString);
    *_retval = StartTransfer(DeviceString, TableName);
  }

  return NS_OK;
}

/* PRBool DownloadTable (in wstring DeviceCategory, in wstring DeviceString, in wstring TableName); */
NS_IMETHODIMP sbDeviceBase::UploadTable(const PRUnichar *DeviceString, const PRUnichar *TableName, PRBool *_retval)
{
  // Initiate a download assuming the caller has previously called MakeTransferTable() to create a download table
  // in Device's DB context.
  *_retval = PR_FALSE;

  if (IsDeviceIdle(DeviceString))
  {
    DeviceUploading(DeviceString);
    *_retval = StartTransfer(DeviceString, TableName);
  }

  return NS_OK;
}

/* PRUint32 GetNumDevices (in wstring DeviceCategory); */
NS_IMETHODIMP sbDeviceBase::GetNumDevices(PRUint32 *_retval)
{
  // Needs to change this to actual number of devices for this category
  *_retval = 1;

  return NS_OK;
}

/* PRBool SuspendTransfer (); */
NS_IMETHODIMP sbDeviceBase::SuspendTransfer(const PRUnichar *deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;

  if (!IsTransferInProgress(deviceString))
    return NS_OK;

  *_retval = SuspendCurrentTransfer(deviceString);
  if (*_retval)
  {
    if (IsDownloadInProgress(deviceString))
      DeviceDownloadPaused(deviceString);
    else if (IsUploadInProgress(deviceString))
      DeviceUploadPaused(deviceString);
  }

  return NS_OK;
}

PRBool sbDeviceBase::SuspendCurrentTransfer(const PRUnichar *deviceString)
{
  return PR_FALSE;
}

/* PRBool ResumeTransfer (); */
NS_IMETHODIMP sbDeviceBase::ResumeTransfer(const PRUnichar* deviceString, PRBool *_retval)
{
  *_retval = PR_FALSE;

  if (!IsTransferPaused(deviceString))
    return NS_OK;

  *_retval = ResumeTransfer(deviceString);
  if (*_retval) {
    // Correct download status
    if (IsDownloadPaused(deviceString))
      DeviceDownloading(deviceString);
    else
      DeviceUploading(deviceString);
  }

  return NS_OK;
}

// Should be overridden in the derived class
PRBool sbDeviceBase::ResumeTransfer(const PRUnichar* deviceString)
{
  return PR_FALSE;
}


PRBool sbDeviceBase::CreateTransferTable(const PRUnichar *DeviceString, const PRUnichar* ContextInput, const PRUnichar* TableName, const PRUnichar *FilterColumn, PRUint32 FilterCount, const PRUnichar **filterValues, const PRUnichar* sourcePath, const PRUnichar* destPath, PRBool isDownloading, PRUnichar **TransferTableName)
{
  PRUnichar* deviceContext = nsnull;
  GetContext(DeviceString, &deviceContext);

  // Obtain the schema for the passed table
  // for creating a similar table in device's database context.
  nsCOMPtr<sbIDatabaseResult> resultset;

  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(ContextInput);

  nsCOMPtr<sbIPlaylistManager> pPlaylistManager = do_CreateInstance("@songbird.org/Songbird/PlaylistManager;1");
  nsCOMPtr<sbISimplePlaylist> pPlaylistSource;
  nsCOMPtr<sbISimplePlaylist> pPlaylistDest;

  nsString destTable = GetTransferTable(DeviceString, isDownloading);

#if 0
  nsString text( FilterColumn );
  text += NS_LITERAL_STRING( " - " );
  text.AppendInt( FilterCount );
  text += NS_LITERAL_STRING( "\r\n" );
  for ( PRUint32 i = 0; i < FilterCount; i++ )
  {
    text += NS_LITERAL_STRING( "   " );
    text += filterValues[ i ];
    text += NS_LITERAL_STRING( "\r\n" );
  }
  ::MessageBoxW( nsnull, text.get(), text.get(), MB_OK );
#endif

  nsString transferTableDescription = isDownloading?GetDeviceDownloadTableDescription(DeviceString):GetDeviceUploadTableDescription(DeviceString);
  nsString transferTableReadable = isDownloading?GetDeviceDownloadTableType(DeviceString):GetDeviceUploadTableType(DeviceString);
  nsString transferTableType = isDownloading?GetDeviceDownloadReadable(DeviceString):GetDeviceUploadTableReadable(DeviceString);

  sbISimplePlaylist* t;
  query->SetDatabaseGUID(ContextInput);
  pPlaylistManager->GetSimplePlaylist(TableName, query.get(), &t);
  pPlaylistManager->CopySimplePlaylist(ContextInput, TableName, FilterColumn, FilterCount, filterValues, deviceContext, destTable.get(), transferTableDescription.get(), transferTableReadable.get(), transferTableType.get(), query.get(), getter_AddRefs(pPlaylistDest));

  if(!pPlaylistDest.get())
    return PR_FALSE;

  pPlaylistDest->AddColumn(NS_LITERAL_STRING("source").get(), NS_LITERAL_STRING("text").get());
  pPlaylistDest->AddColumn(NS_LITERAL_STRING("destination").get(), NS_LITERAL_STRING("text").get());
  pPlaylistDest->AddColumn(NS_LITERAL_STRING("progress").get(), NS_LITERAL_STRING("integer(0, 100)").get());
  pPlaylistDest->AddColumn(NS_LITERAL_STRING("status").get(), NS_LITERAL_STRING("text").get());

  pPlaylistDest->SetColumnInfo(NS_LITERAL_STRING("source").get(), NS_LITERAL_STRING("Source URL").get(), PR_TRUE, PR_TRUE, PR_FALSE, -10000, 70, PR_FALSE);
  pPlaylistDest->SetColumnInfo(NS_LITERAL_STRING("destination").get(), NS_LITERAL_STRING("Destination URL").get(), PR_TRUE, PR_TRUE, PR_FALSE, -9000, 70, PR_FALSE);
  pPlaylistDest->SetColumnInfo(NS_LITERAL_STRING("progress").get(), NS_LITERAL_STRING("Download Progress").get(), PR_TRUE, PR_TRUE, PR_FALSE, -8000, 20, PR_FALSE);
  pPlaylistDest->SetColumnInfo(NS_LITERAL_STRING("status").get(), NS_LITERAL_STRING("Status").get(), PR_TRUE, PR_TRUE, PR_FALSE, -7000, 20, PR_FALSE);

  PRInt32 nEntryCount = 0;
  pPlaylistDest->GetAllEntries(&nEntryCount);

  query->GetResultObjectOrphan(getter_AddRefs(resultset));

  PRBool isSourceGiven = nsString(sourcePath).Length()>0;
  PRBool isDestinationGiven = nsString(destPath).Length()>0;

  // Now copy the transfer data
  PRInt32 rowcount;
  resultset->GetRowCount( &rowcount );

  //Clean it up since we'll reuse it.
  query->ResetQuery();

  for ( PRInt32 row = 0; row < rowcount; row++ ) 
  {
    nsString updateDataQuery;
    nsString destinationPathFile;
    nsString sourcePathFile;

    updateDataQuery = NS_LITERAL_STRING("UPDATE \"") + destTable + NS_LITERAL_STRING("\"");
    updateDataQuery += NS_LITERAL_STRING(" SET source = ");

    PRUnichar *strCurSource = nsnull;
    resultset->GetRowCellByColumn(row, NS_LITERAL_STRING("source").get(), &strCurSource);

    PRUnichar *strCurDest = nsnull;
    resultset->GetRowCellByColumn(row, NS_LITERAL_STRING("destination").get(), &strCurDest);

    // Default to what's already there
    if(nsString(strCurSource).Length()) sourcePathFile = strCurSource;
    if(nsString(strCurDest).Length()) destinationPathFile = strCurDest;

    if (isSourceGiven) 
    {
      sourcePathFile = sourcePath;
      sourcePathFile += NS_LITERAL_STRING("\\");
    }

    if (isDestinationGiven) 
    {
      destinationPathFile = destPath;
      destinationPathFile += NS_LITERAL_STRING("\\");
    }

    PRUnichar* data;
    resultset->GetRowCellByColumn(row, NS_LITERAL_STRING("url").get(), &data);

    nsString fileName;
    nsString dataString(data);
    GetFileNameFromURL(NS_CONST_CAST(PRUnichar*, DeviceString),
                       dataString,
                       fileName);

    if (!isDestinationGiven)
      destinationPathFile = data;
    else 
    {
      fileName.ReplaceChar(FILE_ILLEGAL_CHARACTERS, '_');
      destinationPathFile += fileName;
    }

    if (!isSourceGiven)
      sourcePathFile = data;
    else
      sourcePathFile += fileName;

    // These are filled intelligently, well at least not a blind copy!
    AddQuotedString(updateDataQuery, sourcePathFile.get());
    updateDataQuery += NS_LITERAL_STRING(" destination = ");
    AddQuotedString(updateDataQuery, destinationPathFile.get(), PR_FALSE);
    updateDataQuery += NS_LITERAL_STRING(" WHERE url = ");
    AddQuotedString(updateDataQuery, data, PR_FALSE);

    query->AddQuery(updateDataQuery.get());

    PR_Free(data);
    PR_Free(strCurSource);
    PR_Free(strCurDest);
  }

  PRInt32 queryError = 0;
  query->Execute(&queryError);

  if(queryError)
  {
    NS_NOTREACHED("CreateTransferTable queryError");
    //There's a bad error probably... might want to look into it, later :)
  }

  // Now correct the indexes by matching index to rowid
  // This will create an unique index for each transfer record.
  nsString adjustIndexesQuery = NS_LITERAL_STRING("UPDATE "); 
  adjustIndexesQuery += destTable;
  adjustIndexesQuery += NS_LITERAL_STRING(" SET id = rowid");
  query->ResetQuery();
  query->AddQuery(adjustIndexesQuery.get());
  query->Execute(&queryError); 
  query->ResetQuery();

  //Return the name of transfer table
  PRUint32 nLen = destTable.Length() + 1;
  *TransferTableName = (PRUnichar *) nsMemory::Clone(destTable.get(), nLen * sizeof(PRUnichar));

  PR_Free(deviceContext);

  return PR_TRUE;
}


void sbDeviceBase::RemoveExistingTransferTableEntries(const PRUnichar* DeviceString, PRBool isDownloadTable, PRBool dropTable)
{	
  PRUnichar* deviceContext = nsnull;
  GetContext(DeviceString, &deviceContext);

  nsresult rv;
  nsCOMPtr<sbIDatabaseQuery> query =
    do_CreateInstance("@songbird.org/Songbird/DatabaseQuery;1" , &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create a DatabaseQuery object");
    return;
  }

  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(deviceContext);


  nsString deleteEntriesSQL;
  nsString transferTable = GetTransferTable(DeviceString, isDownloadTable);
  if (!dropTable)
  {
    // If DeviceString is not provided then delete everything
    if (DeviceString) 
    {
      deleteEntriesSQL = NS_LITERAL_STRING("delete from ");
      deleteEntriesSQL += transferTable;
    }
    query->AddQuery(deleteEntriesSQL.get()); 

    PRInt32 ret;
    query->Execute(&ret);
    query->ResetQuery();
  }
  else
  {
    nsCOMPtr<sbIPlaylistManager> pPlaylistManager = do_CreateInstance("@songbird.org/Songbird/PlaylistManager;1");
    PRInt32 retVal = 0;
    pPlaylistManager->DeleteSimplePlaylist(transferTable.get(), query.get(), &retVal);
  }


  PR_Free(deviceContext);
}

inline void sbDeviceBase::AddQuotedString(nsString &destinationString, const PRUnichar* sourceString, PRBool suffixWithComma)
{
  destinationString += NS_LITERAL_STRING("\"");
  destinationString += sourceString;
  destinationString += NS_LITERAL_STRING("\"");
  if (suffixWithComma)
    destinationString += NS_LITERAL_STRING(",");
}

PRBool sbDeviceBase::GetFileNameFromURL(const PRUnichar *DeviceString, nsString& url, nsString& fileName)
{
  // Extract file name from the url(path) string
  const PRUnichar backSlash = *(NS_LITERAL_STRING("\\").get());
  const PRUnichar frontSlash = *(NS_LITERAL_STRING("/").get());
  const PRUnichar dot = *(NS_LITERAL_STRING(".").get());

  PRInt32 foundPosition = url.RFindChar(frontSlash);
  if((foundPosition != -1) || ((foundPosition = url.RFindChar(backSlash))!=-1))
  {
    fileName = Substring(url, ++foundPosition);
    // Now add the extension if the device changes the format
    // of transferred file.
    PRUint32 fileFormat;
    GetDownloadFileType(DeviceString, &fileFormat);
    if (kSB_DEVICE_FILE_FORMAT_UNDEFINED != fileFormat)
    {
      nsString outputFileExtension;
      if (GetFileExtension(fileFormat, outputFileExtension))
      {
        foundPosition = fileName.RFindChar(dot);
        if (foundPosition)
        {
          fileName = Substring(fileName, 0, foundPosition + 1);
          fileName += outputFileExtension;
        }
      }
    }
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool sbDeviceBase::GetFileExtension(PRUint32 fileFormat, nsString& fileExtension)
{
  PRBool retval = true;

  switch (fileFormat)
  {
  case kSB_DEVICE_FILE_FORMAT_WAV:
    fileExtension = NS_LITERAL_STRING("wav");
    break;
  case kSB_DEVICE_FILE_FORMAT_MP3:
    fileExtension = NS_LITERAL_STRING("mp3");
    break;
  case kSB_DEVICE_FILE_FORMAT_WMA:
    fileExtension = NS_LITERAL_STRING("wma");
    break;
  default:
    retval = PR_FALSE;
    break;
  }

  return retval;
}

nsString sbDeviceBase::GetTransferTable(const PRUnichar* deviceString, PRBool getDownloadTable)
{
  nsString aDeviceTransferTable;
  
  if (getDownloadTable == PR_TRUE)
    aDeviceTransferTable = GetDeviceDownloadTable(deviceString);
  else
    aDeviceTransferTable = GetDeviceUploadTable(deviceString);

  return aDeviceTransferTable;
}

PRBool sbDeviceBase::GetUploadFileFormat(PRUint32& fileFormat)
{
  return PR_FALSE;
}

PRBool sbDeviceBase::GetDownloadFileFormat(PRUint32& fileFormat)
{
  return PR_FALSE;
}

PRBool sbDeviceBase::GetSourceAndDestinationURL(const PRUnichar* dbContext, const PRUnichar* table, const PRUnichar* index, nsString& sourceURL, nsString& destURL)
{
  PRBool bRet = PR_FALSE;
  PRInt32 nRet = 0;

  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  if (query == nsnull)
    return PR_FALSE;

  query->SetDatabaseGUID(dbContext);

  nsString strQuery = NS_LITERAL_STRING("SELECT source, destination FROM ");
  AddQuotedString(strQuery, table, PR_FALSE);
  strQuery += NS_LITERAL_STRING(" WHERE id = ");
  strQuery += index;

  query->AddQuery(strQuery.get());
  query->SetAsyncQuery(PR_FALSE);
  query->Execute(&nRet);

  nsCOMPtr<sbIDatabaseResult> result;
  query->GetResultObject(getter_AddRefs(result));

  if(result) {
    PRUnichar *strSourceURL = nsnull, *strDestURL = nsnull;

    result->GetRowCellByColumn(0, NS_LITERAL_STRING("source").get(), &strSourceURL);
    result->GetRowCellByColumn(0, NS_LITERAL_STRING("destination").get(), &strDestURL);

    sourceURL = strSourceURL;
    destURL = strDestURL;

    bRet = PR_TRUE;
  }

  return bRet;
}

void sbDeviceBase::DoTransferStartCallback(const PRUnichar* sourceURL, const PRUnichar* destinationURL)
{
  nsAutoLock lock(mpCallbackListLock);
  size_t nSize = mCallbackList.size();
  for(size_t nCurrent = 0; nCurrent < nSize; ++nCurrent)
  {
    if(mCallbackList[nCurrent] != nsnull)
    {
      try
      {
        sbIDeviceBaseCallback *pCallback = mCallbackList[nCurrent];
        pCallback->OnTransferStart(sourceURL, destinationURL);
      }
      catch(...)
      {
        //Oops. Someone is being really bad.
        NS_NOTREACHED("pCallback->OnTransferStart threw an exception");
      }
    }
  }
}

void sbDeviceBase::DoTransferCompleteCallback(const PRUnichar* sourceURL, const PRUnichar* destinationURL, PRInt32 nStatus)
{
  nsAutoLock lock(mpCallbackListLock);
  size_t nSize = mCallbackList.size();
  for(size_t nCurrent = 0; nCurrent < nSize; ++nCurrent)
  {
    if(mCallbackList[nCurrent] != nsnull)
    {
      try
      {
        sbIDeviceBaseCallback *pCallback = mCallbackList[nCurrent];
        pCallback->OnTransferComplete(sourceURL, destinationURL, nStatus);
      }
      catch(...)
      {
        //Oops. Someone is being really bad.
        NS_NOTREACHED("pCallback->OnTransferComplete threw an exception");
      }
    }
  }
}

void sbDeviceBase::DoDeviceConnectCallback(const PRUnichar* deviceString)
{
  nsAutoLock lock(mpCallbackListLock);
  size_t nSize = mCallbackList.size();
  for(size_t nCurrent = 0; nCurrent < nSize; ++nCurrent)
  {
    if(mCallbackList[nCurrent] != nsnull)
    {
      try
      {
        sbIDeviceBaseCallback *pCallback = mCallbackList[nCurrent];
        pCallback->OnDeviceConnect(deviceString);
      }
      catch(...)
      {
        //Oops. Someone is being really bad.
        NS_NOTREACHED("pCallback->OnDeviceConnect threw an exception");
      }
    }
  }
}

void sbDeviceBase::DoDeviceDisconnectCallback(const PRUnichar* deviceString)
{
  nsAutoLock lock(mpCallbackListLock);
  size_t nSize = mCallbackList.size();
  for(size_t nCurrent = 0; nCurrent < nSize; ++nCurrent)
  {
    if(mCallbackList[nCurrent] != nsnull)
    {
      try
      {
        sbIDeviceBaseCallback *pCallback = mCallbackList[nCurrent];
        pCallback->OnDeviceDisconnect(deviceString);
      }
      catch(...)
      {
        //Oops. Someone is being really bad.
        NS_NOTREACHED("pCallback->OnDeviceDisconnect threw an exception");
      }
    }
  }
}

/* PRUint32 GetDeviceState (); */
NS_IMETHODIMP sbDeviceBase::GetDeviceState(const PRUnichar *deviceString, PRUint32 *_retval)
{
  *_retval = kSB_DEVICE_STATE_IDLE;
  return NS_OK;
}

/* PRBool RemoveTranferTracks (in PRUint32 index); */
NS_IMETHODIMP sbDeviceBase::RemoveTranferTracks(const PRUnichar *deviceString, PRUint32 index, PRBool *_retval)
{
  // Remove this track from transfer table
  PRUnichar* deviceContext = nsnull;
  GetContext(deviceString, &deviceContext);

  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  nsString deleteEntriesSQL(NS_LITERAL_STRING("delete from "));
  deleteEntriesSQL += GetTransferTable(deviceString, PR_TRUE);

  deleteEntriesSQL += NS_LITERAL_STRING(" WHERE id = ");
  deleteEntriesSQL.AppendInt(index);

  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(deviceContext);
  query->AddQuery(deleteEntriesSQL.get()); 

  PRInt32 ret;
  query->Execute(&ret);

  *_retval = (ret == 0);

  PR_Free(deviceContext);

  *_retval = PR_FALSE;
  return NS_OK;
}

PRBool sbDeviceBase::StopCurrentTransfer(const PRUnichar* deviceString)
{
  return PR_FALSE;
}

// ResumeAbortedTransfer() is different than ResumeTransfer()
// as ResumeAbortedTransfer() handles transfer re-initiation ic
// cases where app was closed or crashed in the middle of transfer.
void sbDeviceBase::ResumeAbortedTransfer(const PRUnichar* deviceString)
{
  ResumeAbortedDownload(deviceString);
  ResumeAbortedUpload(deviceString);
}

void sbDeviceBase::ResumeAbortedDownload(const PRUnichar* deviceString)
{
  nsString transferTable = GetDeviceDownloadTable(NS_LITERAL_STRING("").get());
  PRUnichar* context = nsnull;
  GetContext(deviceString, &context);
  nsString dbContext(context);
  PR_Free(context);

  // Read the table for Transferring files
  sbIDatabaseResult* resultset;
  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  nsString query_str(NS_LITERAL_STRING("select * from "));
  query_str += transferTable;
  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(dbContext.get());
  query->AddQuery(query_str.get()); 

  PRInt32 ret;
  query->Execute(&ret);
  query->GetResultObject(&resultset);

  // Now copy the transfer data
  PRInt32 rowcount;
  resultset->GetRowCount( &rowcount );

  for ( PRInt32 row = 0; row < rowcount; row++ ) {
    PRUnichar *progressString = nsnull;
    resultset->GetRowCellByColumn(row, NS_LITERAL_STRING("progress").get(), &progressString);
    PRInt32 errorCode;
    PRInt32 progress = nsString(progressString).ToInteger(&errorCode);
    PR_Free(progressString);
    if (progress < 100) {
      // Unfinished transfer,
      // re-initiate it.
      DeviceDownloading(deviceString);

      TransferData *data = new TransferData;
      data->dbContext = dbContext;
      data->dbTable = transferTable;
      TransferNextFile(row-1, data);
      break;
    }
  }
}

void sbDeviceBase::ResumeAbortedUpload(const PRUnichar* deviceString)
{
  nsString transferTable = GetDeviceUploadTable(NS_LITERAL_STRING("").get());
  PRUnichar* context = nsnull;
  GetContext((PRUnichar *)(""), &context);
  nsString dbContext(context);
  PR_Free(context);

  // Read the table for Transferring files
  sbIDatabaseResult* resultset;
  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  nsString query_str(NS_LITERAL_STRING("select * from "));
  query_str += transferTable;
  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(dbContext.get());
  query->AddQuery(query_str.get()); 

  PRInt32 ret;
  query->Execute(&ret);
  query->GetResultObject(&resultset);

  // Now copy the transfer data
  PRInt32 rowcount;
  resultset->GetRowCount( &rowcount );

  for ( PRInt32 row = 0; row < rowcount; row++ )
  {
    PRUnichar *progressString = nsnull;
    resultset->GetRowCellByColumn(row, NS_LITERAL_STRING("progress").get(), &progressString);
    PRInt32 errorCode;
    PRInt32 progress = nsString(progressString).ToInteger(&errorCode);
    PR_Free(progressString);
    if (progress < 100)
    {
      // Unfinished transfer,
      // re-initiate it.
      DeviceUploading(deviceString);

      TransferData *data = new TransferData;
      data->dbContext = dbContext;
      data->dbTable = transferTable;
      TransferNextFile(row-1, data);
      break;
    }
  }
}


/* PRBool AutoDownloadTable(in wstring DeviceString, in wstring ContextInput, in wstring TableName, in wstring FilterColumn, in PRUint32 FilterCount, [array, size_is(FilterCount)] in wstring FilterValues, in wstring sourcePath, in wstring destPath, out wstring TransferTable); */
NS_IMETHODIMP sbDeviceBase::AutoDownloadTable(const PRUnichar *DeviceString, const PRUnichar *ContextInput, const PRUnichar *TableName, const PRUnichar *FilterColumn, PRUint32 FilterCount, const PRUnichar **FilterValues, const PRUnichar *sourcePath, const PRUnichar *destPath, PRUnichar **TransferTableName, PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (IsDeviceIdle(DeviceString) || IsDownloadInProgress(DeviceString))
  {
    if (CreateTransferTable(DeviceString, ContextInput, TableName, FilterColumn, FilterCount, FilterValues, sourcePath, destPath, PR_TRUE, TransferTableName))
    {
      // If we are already downloading then these newly added requests
      // will be picked up automatically on finishing the previous 
      // requests, so check for idle state ...
      if (IsDeviceIdle(DeviceString))
      {
        DeviceDownloading(DeviceString);
        *_retval = StartTransfer(DeviceString, *TransferTableName);
      }
    }
    else
    {
      DeviceIdle(DeviceString);
    }
  }

  return NS_OK;
}

/* PRBool AutoUploadTable(in wstring DeviceString, in wstring ContextInput, in wstring TableName, in wstring FilterColumn, in PRUint32 FilterCount, [array, size_is(FilterCount)] in wstring FilterValues, in wstring sourcePath, in wstring destPath, out wstring TransferTable); */
NS_IMETHODIMP sbDeviceBase::AutoUploadTable(const PRUnichar *DeviceString, const PRUnichar *ContextInput, const PRUnichar *TableName, const PRUnichar *FilterColumn, PRUint32 FilterCount, const PRUnichar **FilterValues, const PRUnichar *sourcePath, const PRUnichar *destPath, PRUnichar **TransferTableName, PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (IsDeviceIdle(DeviceString) || IsUploadInProgress(DeviceString))
  {
    if (CreateTransferTable(DeviceString, ContextInput, TableName, FilterColumn, FilterCount, FilterValues, sourcePath, destPath, PR_TRUE, TransferTableName))
    {
      // If we are already uploading then these newly added requests
      // will be picked up automatically on finishing the previous 
      // requests, so check for idle state ...
      if (IsDeviceIdle(DeviceString))
      {
        DeviceUploading(DeviceString);
        *_retval = StartTransfer(DeviceString, *TransferTableName);
      }
    }
    else
    {
      DeviceIdle(DeviceString);
    }
  }

  return NS_OK;
}

/* wstring GetDownloadTable (in wstring deviceString); */
NS_IMETHODIMP sbDeviceBase::GetDownloadTable(const PRUnichar *deviceString, PRUnichar **_retval)
{
  nsString transferTable = GetDeviceDownloadTable(deviceString);

  size_t nLen = transferTable.Length() + 1;
  *_retval = (PRUnichar *) nsMemory::Clone(transferTable.get(), nLen * sizeof(PRUnichar));

  if(*_retval == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

/* wstring GetUploadTable (in wstring deviceString); */
NS_IMETHODIMP sbDeviceBase::GetUploadTable(const PRUnichar *deviceString, PRUnichar **_retval)
{
  nsString transferTable = GetDeviceUploadTable(deviceString);

  size_t nLen = transferTable.Length() + 1;
  *_retval = (PRUnichar *) nsMemory::Clone(transferTable.get(), nLen * sizeof(PRUnichar));

  if(*_retval == nsnull) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

// Creates a table with the usual columns for storing attributes for tracks
// NOTE: This function will drop the table if it already exists!.
PRBool sbDeviceBase::CreateTrackTable(nsString& deviceString, nsString& tableName)
{
  PRInt32 dbOpRetVal = 0;

  PRUnichar* dbContext;
  GetContext((PRUnichar *) deviceString.get(), &dbContext);

  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );

  // First get rid of any existing table by that name
  nsString dropTableQuery(NS_LITERAL_STRING("DROP TABLE "));
  dropTableQuery += tableName;
  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(dbContext);
  query->AddQuery(dropTableQuery.get()); 
  query->Execute(&dbOpRetVal);

  if (dbOpRetVal == 0)
  {
    nsString query_str(NS_LITERAL_STRING("CREATE TABLE "));
    query_str += tableName;
    // Add the columns for attributes
    query_str += NS_LITERAL_STRING(" (idx integer primary key autoincrement, url text, name text, tim text, artist text, album text, genre text, length integer)");

    query->SetAsyncQuery(PR_FALSE); 
    query->SetDatabaseGUID(dbContext);
    query->AddQuery(query_str.get()); 

    query->Execute(&dbOpRetVal);
  }

  PR_Free(dbContext);

  return (dbOpRetVal == 0);
}

PRBool sbDeviceBase::AddTrack(nsString& deviceString, 
                              nsString& tableName, 
                              nsString& url,
                              nsString& name,
                              nsString& tim,
                              nsString& artist,
                              nsString& album,
                              nsString& genre,
                              PRUint32 length)
{
  PRUnichar* dbContext;
  GetContext((PRUnichar *) deviceString.get(), &dbContext);

  nsCOMPtr<sbIDatabaseQuery> query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
  nsString addRecordQuery(NS_LITERAL_STRING("INSERT INTO "));
  addRecordQuery += tableName;
  addRecordQuery += NS_LITERAL_STRING("(url, name, tim, artist, album, genre, length) VALUES (");
  // Add all the column data here
  AddQuotedString(addRecordQuery, url.get());
  AddQuotedString(addRecordQuery, name.get());
  AddQuotedString(addRecordQuery, tim.get());
  AddQuotedString(addRecordQuery, artist.get());
  AddQuotedString(addRecordQuery, album.get());
  AddQuotedString(addRecordQuery, genre.get());

  addRecordQuery.AppendInt(length);

  addRecordQuery += NS_LITERAL_STRING(")");

  query->SetAsyncQuery(PR_FALSE); 
  query->SetDatabaseGUID(dbContext);
  query->AddQuery(addRecordQuery.get()); 

  PRInt32 dbOpRetVal = 0;
  query->Execute(&dbOpRetVal);

  PR_Free(dbContext);

  return (dbOpRetVal == 0);
}

PRBool sbDeviceBase::InitializeAsync()
{
  if (mpDeviceThread) {
    SubmitMessage(MSG_DEVICE_INITIALIZE, (void *) -1, (void *) nsnull);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool sbDeviceBase::FinalizeAsync()
{
  if (mpDeviceThread) {
    SubmitMessage(MSG_DEVICE_FINALIZE, (void *) -1, (void *) nsnull);
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool sbDeviceBase::DeviceEventAsync(PRBool mediaInserted)
{  
  if (mpDeviceThread) {
    DeviceEventSync( mediaInserted);
    return PR_TRUE;
  }
  return PR_FALSE;
}

void sbDeviceBase::DownloadDone(PRUnichar* deviceString, PRUnichar* table, PRUnichar* index)
{
  nsString sourceURL, destURL;
  PRUnichar* dbContext = NULL;
  GetContext(deviceString, &dbContext);
  if(sbDeviceBase::GetSourceAndDestinationURL(dbContext, table, index, sourceURL, destURL))
  {
    DoTransferCompleteCallback(sourceURL.get(), destURL.get(), 1);

    const static PRUnichar *aMetaKeys[] = {NS_LITERAL_STRING("title").get()};
    const static PRUint32 nMetaKeyCount = sizeof(aMetaKeys) / sizeof(aMetaKeys[0]);

    nsString strFile;
    GetFileNameFromURL(deviceString, destURL, strFile);

    PRUnichar *pGUID = NULL;
    PRUnichar** aMetaValues = (PRUnichar **) nsMemory::Alloc(nMetaKeyCount * sizeof(PRUnichar *));
    aMetaValues[0] = (PRUnichar *) nsMemory::Clone(strFile.get(), (strFile.Length() + 1) * sizeof(PRUnichar));
    nsCOMPtr<sbIDatabaseQuery> pQuery = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
    pQuery->SetAsyncQuery(PR_FALSE);
    pQuery->SetDatabaseGUID(NS_LITERAL_STRING("songbird").get());

    nsCOMPtr<sbIMediaLibrary> pLibrary = do_CreateInstance( "@songbird.org/Songbird/MediaLibrary;1" );
    pLibrary->SetQueryObject(pQuery.get());

    //Make sure the filename is unique when download an item from a remote source.
    pLibrary->AddMedia(destURL.get(), nMetaKeyCount, aMetaKeys, nMetaKeyCount, const_cast<const PRUnichar **>(aMetaValues), PR_TRUE, PR_FALSE, &pGUID);

    PRBool bRet = PR_FALSE;
    pLibrary->SetValueByGUID(pGUID, NS_LITERAL_STRING("url").get(), destURL.get(), PR_FALSE, &bRet);
    pLibrary->SetValueByGUID(pGUID, NS_LITERAL_STRING("title").get(), strFile.get(), PR_FALSE, &bRet);
    pLibrary->SetValueByGUID(pGUID, NS_LITERAL_STRING("length").get(), NS_LITERAL_STRING("0").get(), PR_FALSE, &bRet);

    PR_Free(aMetaValues[0]);
    PR_Free(aMetaValues);
    PR_Free(pGUID);


  }

  TransferData* data = new TransferData;
  data->dbContext = dbContext;
  data->dbTable = table;
  data->deviceString = deviceString;
  TransferNextFile(GetCurrentTransferRowNumber(deviceString), data);

  PR_Free(dbContext);
}


// The following functions need to be overridden in the derived
// classes to handle the calls synchronously.
PRBool sbDeviceBase::InitializeSync()
{
  return PR_FALSE;
}

PRBool sbDeviceBase::FinalizeSync()
{
  return PR_FALSE;
}

PRBool sbDeviceBase::DeviceEventSync(PRBool mediaInserted)
{
  return PR_FALSE;
}

/* PRBool SetDownloadFileType (in PRUint32 fileType); */
NS_IMETHODIMP sbDeviceBase::SetDownloadFileType(const PRUnichar *deviceString, PRUint32 fileType, PRBool *_retval)
{
  // Derived class needs to implement this
  *_retval = false;
  return NS_OK;
}

/* PRBool SetUploadFileType (in PRUint32 fileType); */
NS_IMETHODIMP sbDeviceBase::SetUploadFileType(const PRUnichar *deviceString, PRUint32 fileType, PRBool *_retval)
{
  // Derived class needs to implement this
  *_retval = false;
  return NS_OK;
}

/* PRUint32 GetDownloadFileType (in wstring deviceString); */
NS_IMETHODIMP sbDeviceBase::GetDownloadFileType(const PRUnichar *deviceString, PRUint32 *_retval)
{
  *_retval = kSB_DEVICE_FILE_FORMAT_UNDEFINED;
  return NS_OK;
}

/* PRUint32 GetUploadFileType (in wstring deviceString); */
NS_IMETHODIMP sbDeviceBase::GetUploadFileType(const PRUnichar *deviceString, PRUint32 *_retval)
{
  *_retval = kSB_DEVICE_FILE_FORMAT_UNDEFINED;
  return NS_OK;
}

void sbDeviceBase::TransferComplete()
{
}


/* End of implementation class template. */

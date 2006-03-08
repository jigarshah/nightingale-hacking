/*
//
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright� 2006 Pioneers of the Inevitable LLC
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the �GPL�).
// 
// Software distributed under the License is distributed 
// on an �AS IS� basis, WITHOUT WARRANTY OF ANY KIND, either 
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

#pragma once

class sbCDObjectManager
{
public:
  virtual PRUnichar* GetContext(const PRUnichar* deviceString) = 0;
  virtual PRUnichar* EnumDeviceString(PRUint32 index) = 0;
  virtual PRBool IsDownloadSupported(const PRUnichar*  deviceString) = 0;
  virtual PRUint32 GetSupportedFormats(const PRUnichar*  deviceString) = 0;
  virtual PRBool IsUploadSupported(const PRUnichar*  deviceString) = 0;
  virtual PRBool IsTransfering(const PRUnichar*  deviceString) = 0;
  virtual PRBool IsDeleteSupported(const PRUnichar*  deviceString) = 0;
  virtual PRUint32 GetUsedSpace(const PRUnichar*  deviceString) = 0;
  virtual PRUint32 GetAvailableSpace(const PRUnichar*  deviceString) = 0;
  virtual PRBool GetTrackTable(const PRUnichar*  deviceString, PRUnichar** dbContext, PRUnichar** tableName) = 0;
  virtual PRBool EjectDevice(const PRUnichar*  deviceString) = 0; 
  virtual PRBool IsUpdateSupported(const PRUnichar*  deviceString) = 0;
  virtual PRBool IsEjectSupported(const PRUnichar*  deviceString) = 0;
  virtual PRUint32 GetNumDevices() = 0;
  virtual PRUint32 GetDeviceState(const PRUnichar*  deviceString) = 0;
  virtual PRUnichar* GetNumDestinations (const PRUnichar*  DeviceString) = 0;
  virtual PRBool SuspendTransfer() = 0;
  virtual PRBool ResumeTransfer() = 0;

  virtual ~sbCDObjectManager(){}

  virtual void Initialize() = 0;
  virtual void Finalize() = 0;
};



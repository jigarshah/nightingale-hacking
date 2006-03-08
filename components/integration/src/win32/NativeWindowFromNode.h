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

/** 
* \file  NativeWindowFromNode.h
* \brief Finds the Native window handle associated with a DOM node - Prototypes.
*/

#pragma once

#ifdef WIN32
#include <windows.h>
#define NATIVEWINDOW HWND
#else
#define NATIVEWINDOW void*
#endif

// INCLUDES ===================================================================

class nsISupports;

// CLASSES ====================================================================
class NativeWindowFromNode 
{
public:
  static NATIVEWINDOW get(nsISupports *window);  

protected:
};


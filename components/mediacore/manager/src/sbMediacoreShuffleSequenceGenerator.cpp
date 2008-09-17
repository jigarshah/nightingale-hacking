/* vim: set sw=2 :miv */
/*
//
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the "GPL").
// 
// Software distributed under the License is distributed 
// on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either 
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

#include "sbMediacoreShuffleSequenceGenerator.h"

#include <nsIURI.h>
#include <nsIURL.h>

#include <nsAutoLock.h>
#include <nsArrayUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>
#include <nsStringGlue.h>
#include <nsTArray.h>

#include <sbTArrayStringEnumerator.h>

NS_IMPL_ISUPPORTS1(sbMediacoreShuffleSequenceGenerator, 
                   sbIMediacoreSequenceGenerator)

sbMediacoreShuffleSequenceGenerator::sbMediacoreShuffleSequenceGenerator()
{
}

sbMediacoreShuffleSequenceGenerator::~sbMediacoreShuffleSequenceGenerator()
{
}

nsresult 
sbMediacoreShuffleSequenceGenerator::Init()
{
  mMonitor = 
    nsAutoMonitor::NewMonitor("sbMediacoreShuffleSequenceGenerator::mMonitor");
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP 
sbMediacoreShuffleSequenceGenerator::OnGenerateSequence(sbIMediaListView *aView, 
                                                        nsIArray **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

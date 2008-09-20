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

function runTest() {

  var gst = Cc["@songbirdnest.com/Songbird/Playback/GStreamer/Service;1"]
              .getService(Ci.sbIGStreamerService);

  var list = [];

  var handler = {
    beginInspect: function() {
    },
    endInspect: function() {
    },
    beginPluginInfo: function(aName, aDescription, aFilename, aVersion, aLicense, aSource, aPackage, aOrigin) {
      list.push(aName);
    },
    endPluginInfo: function() {
    },
    beginFactoryInfo: function(aLongName, aClass, aDescription, aAuthor, aRankName, aRank) {
    },
    endFactoryInfo: function() {
    },
    beginPadTemplateInfo: function(aName, aDirection, aPresence, aCodecDescription) {
    },
    endPadTemplateInfo: function() {
    }
  };

  gst.inspect(handler);

  log(list.length + " plugins found");

  var platform = getPlatform();

  // XXX: will be there for all platforms - should have 'ogg' and 'vorbis' too
  assertContains(list, ["staticelements"]);

  if (platform != "Linux") {
    // XXX: Re-enable this once we have working linux binaries
    assertContains(list, ["ogg", "vorbis"]);
  }

  if (platform == "Windows NT") {
    assertContains(list, ["directsound", "directdraw"]);
  }

  if (platform == "Darwin") {
    assertContains(list, ["osxaudio", "osxvideo"]);
  }

  if (platform == "Linux") {
    // XXX: change back to assertContains once we have working linux binaries
    assertDoesNotContain(list, ["alsa", "ximagesink", "xvimagesink"]);

    // XXX: should be available for all platforms
    assertDoesNotContain(list, ["ogg", "vorbis"]);
  }
}

function assertContains(a, b) {
  for (var i = 0; i < b.length; i++) {
    assertTrue(a.indexOf(b[i]) >= 0, "Plugin not found: " + b[i]);
  }
}

function assertDoesNotContain(a, b) {
  for (var i = 0; i < b.length; i++) {
    assertFalse(a.indexOf(b[i]) >= 0, "Plugin unexpectedly found: "+b[i]);
  }
}


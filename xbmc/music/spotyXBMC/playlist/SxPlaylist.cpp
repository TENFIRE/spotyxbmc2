/*
 spotyxbmc2 - A project to integrate Spotify into XBMC
 Copyright (C) 2011  David Erenger

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 For contact with the author:
 david.erenger@gmail.com
 */

#include "SxPlaylist.h"
#include "../session/Session.h"
#include "../track/TrackStore.h"
#include "../thumb/ThumbStore.h"
#include "../track/SxTrack.h"
#include "../album/SxAlbum.h"
#include "../thumb/SxThumb.h"
#include "../XBMCUpdater.h"

namespace addon_music_spotify {

SxPlaylist::SxPlaylist(sp_playlist* spPlaylist, int index, bool isFolder) {
  m_spPlaylist = spPlaylist;
  m_isValid = true;
  m_isFolder = isFolder;
  m_index = index;
  m_thumb = NULL;

  if (!isFolder) {
    if (sp_playlist_is_loaded(spPlaylist))
      init();
    m_plCallbacks.description_changed = 0;
    m_plCallbacks.image_changed = 0;
    m_plCallbacks.playlist_metadata_updated = &cb_playlist_metadata_updated;
    m_plCallbacks.playlist_renamed = &cb_playlist_renamed;
    m_plCallbacks.playlist_state_changed = &cb_state_change;
    m_plCallbacks.playlist_update_in_progress = 0;
    m_plCallbacks.subscribers_changed = 0;
    m_plCallbacks.track_created_changed = 0;
    m_plCallbacks.track_message_changed = 0;
    m_plCallbacks.track_seen_changed = 0;
    m_plCallbacks.tracks_added = &cb_tracks_added;
    m_plCallbacks.tracks_moved = &cb_tracks_moved;
    m_plCallbacks.tracks_removed = &cb_tracks_removed;

    sp_playlist_add_callbacks(spPlaylist, &m_plCallbacks, this);
  }
}

void SxPlaylist::init() {
  Logger::printOut("Playlistinit");
  //TODO fix a thumb, why is it never returning any images?
  byte image[20];
  if (sp_playlist_get_image(m_spPlaylist, image)) {
    m_thumb = ThumbStore::getInstance()->getThumb(image);
  }

  //Logger::printOut(sp_playlist_name(m_spSxPlaylist));
  for (int index = 0; index < sp_playlist_num_tracks(m_spPlaylist); index++) {
    SxTrack* track = TrackStore::getInstance()->getTrack(sp_playlist_track(m_spPlaylist, index));
    if (track) {
      m_tracks.push_back(track);
      //no thumb, lets pick one from the track list
      if (m_thumb == NULL)
        if (track->getThumb() != NULL)
          //no need to add ref to the thumb, when the track disappears the playlist will switch thumb
          m_thumb = track->getThumb();
    }
  }

}

SxPlaylist::~SxPlaylist() {
  if (!isFolder()) {
    while (!m_tracks.empty()) {
      TrackStore::getInstance()->removeTrack(m_tracks.back()->getSpTrack());
      m_tracks.pop_back();
    }
  }
  if (m_isValid)
    sp_playlist_release(m_spPlaylist);
}

const char* SxPlaylist::getName() {
  return sp_playlist_name(m_spPlaylist);
}

const char* SxPlaylist::getOwnerName() {
  sp_user* user = sp_playlist_owner(m_spPlaylist);
  if (user != sp_session_user(Session::getInstance()->getSpSession())) {
    return sp_user_display_name(user);
  }
  return NULL;
}

bool SxPlaylist::isLoaded() {
  if (!isFolder()) {
    for (int i = 0; i < m_tracks.size(); i++) {
      if (!m_tracks[i]->isLoaded())
        return false;
    }
  }
  return true;
}

SxTrack* SxPlaylist::getTrack(int index) {
  if (index < getNumberOfTracks() && m_isValid && isLoaded() && !isFolder())
    return m_tracks[index];
  return NULL;
}

void SxPlaylist::reLoad() {
  Logger::printOut("reload play");
  if (m_isValid && !isFolder()) {
    m_thumb = NULL;
    //TODO fix a thumb, why is it never returning any images?
    byte image[20];
    if (sp_playlist_get_image(m_spPlaylist, image)) {
      m_thumb = ThumbStore::getInstance()->getThumb(image);
    }

    vector<SxTrack*> newTracks;
    for (int index = 0; index < sp_playlist_num_tracks(m_spPlaylist); index++) {
      sp_track* spTrack = sp_playlist_track(m_spPlaylist, index);
      if (!sp_track_is_available(Session::getInstance()->getSpSession(), spTrack))
        continue;
      SxTrack* track = TrackStore::getInstance()->getTrack(spTrack);
      if (track) {
        newTracks.push_back(track);
        //no thumb, lets pick one from the track list
        if (m_thumb == NULL)
          if (track->getThumb() != NULL)
            //no need to add ref to the thumb, when the track disappears the playlist will switch thumb
            m_thumb = track->getThumb();
      }
    }
    while (!m_tracks.empty()) {
      TrackStore::getInstance()->removeTrack(m_tracks.back()->getSpTrack());
      m_tracks.pop_back();
    }
    Logger::printOut("reload play done");
    m_tracks = newTracks;
  }
  XBMCUpdater::updatePlaylist(m_index);
}

void SxPlaylist::cb_tracks_removed(sp_playlist *pl, const int *tracks, int num_tracks, void *userdata) {
  SxPlaylist* plist = (SxPlaylist*) userdata;
  if (plist->isLoaded())
    plist->reLoad();
}

void SxPlaylist::cb_playlist_renamed(sp_playlist *pl, void *userdata) {
  //set some kind of dirty flag to the store
}

void SxPlaylist::cb_playlist_metadata_updated(sp_playlist *pl, void *userdata) {
  SxPlaylist* plist = (SxPlaylist*) userdata;
  if (plist->isLoaded())
    plist->reLoad();
}

void SxPlaylist::cb_tracks_moved(sp_playlist *pl, const int *tracks, int num_tracks, int new_position, void *userdata) {
  SxPlaylist* plist = (SxPlaylist*) userdata;
  if (plist->isLoaded())
    plist->reLoad();
}

void SxPlaylist::cb_state_change(sp_playlist *pl, void *userdata) {
}

void SxPlaylist::cb_tracks_added(sp_playlist *pl, sp_track * const *tracks, int num_tracks, int position, void *userdata) {
  SxPlaylist* plist = (SxPlaylist*) userdata;
  if (plist->isLoaded())
    plist->reLoad();
}

} /* namespace addon_music_spotify */


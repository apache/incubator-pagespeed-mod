/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "net/instaweb/rewriter/public/critical_finder_support_util.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/critical_keys.pb.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/base64_util.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/util/nonce_generator.h"

namespace net_instaweb {

namespace {

typedef std::map<GoogleString, int> SupportMap;

// *dest += addend, but capping at kint32max
inline void SaturatingAddTo(int32 addend, int32* dest) {
  int64 result = *dest;
  result += addend;
  *dest = (result > kint32max) ? kint32max : static_cast<int32>(result);
}

bool LessBySupportMapValue(const SupportMap::value_type& pair1,
                           const SupportMap::value_type& pair2) {
    return (pair1.second < pair2.second);
}

SupportMap ConvertCriticalKeysProtoToSupportMap(
    const CriticalKeys& critical_keys) {
  SupportMap support_map;
  // Start by reading in the support data.
  for (int i = 0; i < critical_keys.key_evidence_size(); ++i) {
    const CriticalKeys::KeyEvidence& evidence = critical_keys.key_evidence(i);
    if (!evidence.key().empty()) {
      // We aggregate here just in case of a corrupt duplicate entry.
      SaturatingAddTo(evidence.support(), &support_map[evidence.key()]);
    }
  }
  return support_map;
}

void WriteSupportMapToCriticalKeysProto(const SupportMap& support_map,
                                        CriticalKeys* critical_keys) {
  // Clean out the existing evidence and inject the fresh evidence.
  critical_keys->clear_key_evidence();
  for (SupportMap::const_iterator entry = support_map.begin();
       entry != support_map.end(); ++entry) {
    CriticalKeys::KeyEvidence* evidence = critical_keys->add_key_evidence();
    evidence->set_key(entry->first);
    evidence->set_support(entry->second);
  }
}

// Decay a single support value by multiplying it by k / (k+1), rounding down.
// Fractional arithmetic done in int64 to avoid int overflow.
inline int Decay(int support_interval, int64 support_value) {
  support_value *= support_interval;
  support_value /= support_interval + 1;
  return support_value;
}

// Decay support, deleting elements whose support drops to 0.
void DecaySupportMap(int support_interval, SupportMap* support_map) {
  SupportMap::iterator next = support_map->begin(), end = support_map->end();
  while (next != end) {
    // We increment at the top of the loop so that we can erase curr
    // without invalidating next.  See:
    // http://stackoverflow.com/questions/2874441/deleting-elements-from-stl-set-while-iterating
    SupportMap::iterator curr = next++;
    // Multiply support_value by the fraction
    // support_interval / (support_interval + 1)
    // using int64 to avoid overflow.
    int support_value = Decay(support_interval, curr->second);
    if (support_value == 0 && curr->second > 0) {
      // Remove entry when its support falls to 0 (this will expire entries that
      // should not be candidates; if curr should still be a candidate, we will
      // re-insert it as part of beaconing).
      support_map->erase(curr);
    } else {
      curr->second = static_cast<int32>(support_value);
    }
  }
}

void ClearInvalidNonces(const int64 now_ms, CriticalKeys* critical_keys) {
  // Invalidate expired entries, and if no valid entries remain then delete all
  // outstanding entries.
  bool found_valid_nonce = false;
  for (int i = 0; i < critical_keys->pending_nonce_size(); ++i) {
    CriticalKeys::PendingNonce* entry = critical_keys->mutable_pending_nonce(i);
    if (!entry->has_nonce()) {
      // Entry unoccupied.  Fall through.
    } else if ((entry->timestamp_ms() + kBeaconTimeoutIntervalMs) < now_ms) {
      entry->clear_timestamp_ms();
      entry->clear_nonce();
      critical_keys->set_nonces_recently_expired(
          critical_keys->nonces_recently_expired() + 1);
    } else {
      found_valid_nonce = true;
    }
  }
  if (!found_valid_nonce) {
    critical_keys->clear_pending_nonce();
  }
}

// Generate a nonce and record the existence of a beacon with that nonce sent at
// timestamp_ms, updating *nonce with the new value.
void AddNonceToCriticalSelectors(const int64 timestamp_ms,
                                 NonceGenerator* nonce_generator,
                                 CriticalKeys* critical_keys,
                                 GoogleString* nonce) {
  CHECK(nonce_generator != NULL);
  uint64 nonce_value = nonce_generator->NewNonce();
  StringPiece nonce_piece(reinterpret_cast<char*>(&nonce_value),
                          sizeof(nonce_value));
  Web64Encode(nonce_piece, nonce);
  if (nonce->size() > 11) {
    // Only keep the first 66 bits of nonce since original value is 64 bits.
    nonce->resize(11);
  }
  ClearInvalidNonces(timestamp_ms, critical_keys);
  // Look for an invalid entry to reuse.
  CriticalKeys::PendingNonce* pending_nonce = NULL;
  for (int i = 0; i < critical_keys->pending_nonce_size(); ++i) {
    CriticalKeys::PendingNonce* entry = critical_keys->mutable_pending_nonce(i);
    if (!entry->has_nonce()) {
      pending_nonce = entry;
      break;
    }
  }
  if (pending_nonce == NULL) {
    // No entry to reuse; create new entry
    pending_nonce = critical_keys->add_pending_nonce();
  }
  pending_nonce->set_timestamp_ms(timestamp_ms);
  pending_nonce->set_nonce(*nonce);
}

}  // namespace

// Check whether the given nonce is valid, invalidating any expired nonce
// entries we might encounter.  To avoid the need to copy and clear the nonce
// list, we invalidate the entry used and any expired entries by clearing the
// nonce value and timestamp.  These entries will be reused by
// AddNonceToCriticalSelectors.
bool ValidateAndExpireNonce(int64 now_ms, StringPiece nonce,
                            CriticalKeys* critical_keys) {
  if (nonce.empty()) {
    // Someone sent us a clearly bogus beacon result.
    return false;
  }
  ClearInvalidNonces(now_ms, critical_keys);
  for (int i = 0; i < critical_keys->pending_nonce_size(); ++i) {
    CriticalKeys::PendingNonce* entry = critical_keys->mutable_pending_nonce(i);
    if (nonce == entry->nonce()) {
      // Matched.  Entry is valid.  Remove it.
      entry->clear_timestamp_ms();
      entry->clear_nonce();
      return true;
    }
  }
  return false;
}

void GetCriticalKeysFromProto(int64 support_percentage,
                              const CriticalKeys& critical_keys,
                              StringSet* keys) {
  int64 support_threshold =
      (support_percentage == 0) ?
      1 : (support_percentage * critical_keys.maximum_possible_support());
  // Collect supported beacon results
  for (int i = 0; i < critical_keys.key_evidence_size(); ++i) {
    const CriticalKeys::KeyEvidence& evidence = critical_keys.key_evidence(i);
    // Do percentage conversion on support value using int64 to avoid overflow.
    int64 support = evidence.support();
    if (support * 100 >= support_threshold && !evidence.key().empty()) {
      keys->insert(evidence.key());
    }
  }
}

// Merge the given set into the existing critical key proto by adding
// support for new_set to existing support.
void UpdateCriticalKeys(bool require_prior_support,
                        const StringSet& new_set, int support_value,
                        CriticalKeys* critical_keys) {
  DCHECK(critical_keys != NULL);
  SupportMap support_map = ConvertCriticalKeysProtoToSupportMap(*critical_keys);
  DecaySupportMap(support_value, &support_map);
  // Update maximum_possible_support.  Initial value must account for legacy
  // data.
  int maximum_support;
  if (critical_keys->has_maximum_possible_support()) {
    maximum_support =
        Decay(support_value, critical_keys->maximum_possible_support());
  } else if (support_map.empty()) {
    maximum_support = 0;
  } else {
    maximum_support = std::max_element(support_map.begin(), support_map.end(),
                                       LessBySupportMapValue)->second;
  }
  SaturatingAddTo(support_value, &maximum_support);
  critical_keys->set_maximum_possible_support(maximum_support);
  // Actually add the new_set to the support_map.
  if (require_prior_support) {
    for (StringSet::const_iterator s = new_set.begin();
         s != new_set.end(); ++s) {
      // Only add entries that are already in the support_map
      // (critical_css_beacon_filter initializes candidate entries to have
      // support 0).  This avoids a cache-fill DoS with spurious beacon data.
      SupportMap::iterator entry = support_map.find(*s);
      if (entry != support_map.end()) {
        SaturatingAddTo(support_value, &entry->second);
      }
    }
  } else {
    // Unconditionally add entries to support_map.
    for (StringSet::const_iterator s = new_set.begin();
         s != new_set.end(); ++s) {
      SaturatingAddTo(support_value, &support_map[*s]);
    }
  }
  critical_keys->set_valid_beacons_received(
      critical_keys->valid_beacons_received() + 1);
  critical_keys->set_nonces_recently_expired(0);
  WriteSupportMapToCriticalKeysProto(support_map, critical_keys);
}

void WriteCriticalKeysToPropertyCache(
    const StringSet& new_keys, StringPiece nonce, int support_interval,
    CriticalKeysWriteFlags flags,
    StringPiece property_name, const PropertyCache* cache,
    const PropertyCache::Cohort* cohort, AbstractPropertyPage* page,
    MessageHandler* message_handler, Timer* timer) {
  // We can't do anything here if page is NULL, so bail out early.
  if (page == NULL) {
    return;
  }
  scoped_ptr<CriticalKeys> critical_keys;
  // TODO(jud): Consider refactoring this into the subclasses as part of the
  // WriteCriticalSelectors refactoring that's ongoing.  Note that this may
  // break slamm's tests at the bottom of critical_selector_finder_test.cc
  // depending on how subclassing is done, so some care will be required.
  if (flags & kReplacePriorResult) {
    critical_keys.reset(new CriticalKeys);
  } else {
    // We first need to read the current critical keys in the property cache,
    // then update it with the new set if it exists, or create it if it doesn't.
    PropertyCacheDecodeResult decode_result;
    critical_keys.reset(DecodeFromPropertyCache<CriticalKeys>(
        cache, page, cohort, property_name, -1, &decode_result));
    switch (decode_result) {
      case kPropertyCacheDecodeOk:
        // We successfully decoded the property cache value, so use the returned
        // CriticalKeys.
        break;
      case kPropertyCacheDecodeNotFound:
        // We either got here because the property cache is not set up correctly
        // (the cohort doesn't exist), or we just don't have a value already.
        // For the former, bail out since there is no use trying to update the
        // property cache if it is not setup. For the later, create a new
        // CriticalKeys, since we just haven't written a value before.
        if (cohort == NULL) {
          return;
        }
        FALLTHROUGH_INTENDED;
      case kPropertyCacheDecodeExpired:
      case kPropertyCacheDecodeParseError:
        // We can proceed here, but we need to create a new CriticalKeys.
        critical_keys.reset(new CriticalKeys);
        break;
    }

    if (!(flags & kSkipNonceCheck) &&
        !ValidateAndExpireNonce(timer->NowMs(), nonce, critical_keys.get())) {
      return;
    }
  }
  UpdateCriticalKeys(flags & kRequirePriorSupport, new_keys, support_interval,
                     critical_keys.get());

  PropertyCacheUpdateResult result = UpdateInPropertyCache(
      *critical_keys, cohort, property_name, false /* write_cohort */, page);
  switch (result) {
    case kPropertyCacheUpdateNotFound:
      message_handler->MessageS(kWarning,
                                "Unable to get Critical keys set for update.");
      break;
    case kPropertyCacheUpdateEncodeError:
      message_handler->MessageS(kWarning, "Trouble marshaling CriticalKeys!?");
      break;
    case kPropertyCacheUpdateOk:
      // Nothing more to do.
      break;
  }
}

bool ShouldBeacon(int64 next_beacon_timestamp_ms, const RewriteDriver& driver) {
  // When downstream cache integration is enabled, and there is a rebeaconing
  // key already specified in the config, we should only rebeacon when there
  // is a matching key in the beacon requesting header.
  if (driver.options()->IsDownstreamCacheIntegrationEnabled() &&
      driver.options()->IsDownstreamCacheRebeaconingKeyConfigured()) {
    return driver.options()->MatchesDownstreamCacheRebeaconingKey(
        driver.request_headers()->Lookup1(kPsaShouldBeacon));
  }
  int64 now_ms = driver.timer()->NowMs();
  return now_ms >= next_beacon_timestamp_ms;
}

void PrepareForBeaconInsertionHelper(CriticalKeys* proto,
                                     NonceGenerator* nonce_generator,
                                     RewriteDriver* driver,
                                     bool using_candidate_key_detection,
                                     BeaconMetadata* result) {
  result->status = kDoNotBeacon;
  CHECK(proto);
  if (!ShouldBeacon(proto->next_beacon_timestamp_ms(), *driver)) {
    return;
  }

  if (driver->options()->IsDownstreamCacheIntegrationEnabled() &&
      !driver->options()->IsDownstreamCacheRebeaconingKeyConfigured()) {
    // Note that we do not put out this message on a per request basis, because
    // it will clutter up the logs. Instead we do it only once every beaconing
    // interval.
    driver->message_handler()->Message(
        kWarning,
        "You seem to have downstream caching configured on your server. "
        "DownstreamCacheRebeaconingKey should also be set for this to work "
        "correctly. Refer to "
        "https://modpagespeed.com/doc/"
        "downstream-caching#beaconing "
        "for more details.");
  }
  // We need to rebeacon so update the timestamp for the next time to
  // rebeacon. If we are using candidate key detection, then check how many
  // valid beacons we have received since the last time the candidate keys
  // changed to determine if we are doing high frequency vs low frequency
  // beaconing.
  // TODO(jmaessen): Add noise to inter-beacon interval.  How? Currently first
  // visit to page after next_beacon_timestamp_ms will beacon.
  int64 beacon_reinstrument_time_ms =
      driver->options()->beacon_reinstrument_time_sec() * Timer::kSecondMs;
  if ((proto->nonces_recently_expired() > kNonceExpirationLimit) ||
      (using_candidate_key_detection &&
       (proto->valid_beacons_received() >= kHighFreqBeaconCount))) {
    beacon_reinstrument_time_ms *= kLowFreqBeaconMult;
  }
  int64 now_ms = driver->timer()->NowMs();
  proto->set_next_beacon_timestamp_ms(now_ms + beacon_reinstrument_time_ms);

  AddNonceToCriticalSelectors(now_ms, nonce_generator, proto, &result->nonce);
  result->status = kBeaconWithNonce;
}

bool UpdateCandidateKeys(const StringSet& keys, CriticalKeys* proto,
                         bool clear_rebeacon_timestamp) {
  // Check to see if candidate keys are already known to pcache.  Insert
  // previously-unknown candidates with a support of 0, to indicate that beacon
  // results for those keys will be considered valid.  Other keys returned in a
  // beacon result will simply be ignored, avoiding DoSing the pcache.  New
  // candidate keys cause us to re-beacon.
  SupportMap support_map = ConvertCriticalKeysProtoToSupportMap(*proto);
  bool support_map_changed = false;
  for (StringSet::const_iterator i = keys.begin(), end = keys.end(); i != end;
       ++i) {
    if (support_map.insert(std::pair<GoogleString, int>(*i, 0)).second) {
      support_map_changed = true;
    }
  }
  if (support_map_changed) {
    // The candidate keys changed, so we need to go into high frequency
    // beaconing mode. Reset the number of beacons received to signal this.
    proto->set_valid_beacons_received(0);
    // Clear the rebeaconing timestamp to force rebeaconing if requested.
    if (clear_rebeacon_timestamp) {
      proto->clear_next_beacon_timestamp_ms();
    }
    // Update the proto value with the new set of keys. Note that we are not
    // changing the calculated set of critical keys, so we don't need to
    // update the state in the RewriteDriver.
    WriteSupportMapToCriticalKeysProto(support_map, proto);
    return true;
  }

  return false;
}

}  // namespace net_instaweb

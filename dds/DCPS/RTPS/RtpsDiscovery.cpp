/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "RtpsDiscovery.h"

#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/ConfigUtils.h"
#include "dds/DCPS/DomainParticipantImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/BuiltInTopicUtils.h"
#include "dds/DCPS/Registered_Data_Types.h"
#include "dds/DdsDcpsInfoUtilsC.h"

#include "ace/Reactor.h"
#include "ace/Select_Reactor.h"

#include <cstdlib>

namespace {
  u_short get_default_d0(u_short fallback)
  {
#if !defined ACE_LACKS_GETENV && !defined ACE_LACKS_ENV
    const char* from_env = std::getenv("OPENDDS_RTPS_DEFAULT_D0");
    if (from_env) {
      return static_cast<u_short>(std::atoi(from_env));
    }
#endif
    return fallback;
  }
}

OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL

namespace OpenDDS {
namespace RTPS {

using DCPS::TimeDuration;

RtpsDiscovery::RtpsDiscovery(const RepoKey& key)
  : DCPS::PeerDiscovery<Spdp>(key)
  , resend_period_(30 /*seconds*/) // see RTPS v2.1 9.6.1.4.2
  , pb_(7400) // see RTPS v2.1 9.6.1.3 for PB, DG, PG, D0, D1 defaults
  , dg_(250)
  , pg_(2)
  , d0_(get_default_d0(0))
  , d1_(10)
  , dx_(2)
  , ttl_(1)
  , sedp_multicast_(true)
  , default_multicast_group_("239.255.0.1") /*RTPS v2.1 9.6.1.4.1*/
  , rtps_relay_only_(false)
  , use_ice_(false)
  , max_spdp_timer_period_(0, 10000)
  , max_auth_time_(300, 0)
  , auth_resend_period_(1, 0)
  , max_spdp_sequence_msg_reset_check_(3)
{
}

RtpsDiscovery::~RtpsDiscovery()
{
}

namespace {
  const ACE_TCHAR RTPS_SECTION_NAME[] = ACE_TEXT("rtps_discovery");
}

int
RtpsDiscovery::Config::discovery_config(ACE_Configuration_Heap& cf)
{
  const ACE_Configuration_Section_Key &root = cf.root_section();
  ACE_Configuration_Section_Key rtps_sect;

  if (cf.open_section(root, RTPS_SECTION_NAME, 0, rtps_sect) == 0) {

    // Ensure there are no properties in this section
    DCPS::ValueMap vm;
    if (DCPS::pullValues(cf, rtps_sect, vm) > 0) {
      // There are values inside [rtps_discovery]
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
                        ACE_TEXT("rtps_discovery sections must have a subsection name\n")),
                       -1);
    }
    // Process the subsections of this section (the individual rtps_discovery/*)
    DCPS::KeyList keys;
    if (DCPS::processSections(cf, rtps_sect, keys) != 0) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
                        ACE_TEXT("too many nesting layers in the [rtps] section.\n")),
                       -1);
    }

    // Loop through the [rtps_discovery/*] sections
    for (DCPS::KeyList::const_iterator it = keys.begin();
         it != keys.end(); ++it) {
      const OPENDDS_STRING& rtps_name = it->first;

      RtpsDiscovery_rch discovery = OpenDDS::DCPS::make_rch<RtpsDiscovery>(rtps_name);

      // spdpaddr defaults to DCPSDefaultAddress if set
      if (!TheServiceParticipant->default_address().empty()) {
        discovery->spdp_local_address(TheServiceParticipant->default_address().c_str());
      }

      DCPS::ValueMap values;
      DCPS::pullValues(cf, it->second, values);
      for (DCPS::ValueMap::const_iterator it = values.begin();
           it != values.end(); ++it) {
        const OPENDDS_STRING& name = it->first;
        if (name == "ResendPeriod") {
          const OPENDDS_STRING& value = it->second;
          int resend;
          if (!DCPS::convertToInteger(value, resend)) {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for ResendPeriod in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->resend_period(TimeDuration(resend));
        } else if (name == "PB") {
          const OPENDDS_STRING& value = it->second;
          u_short pb;
          if (!DCPS::convertToInteger(value, pb)) {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for PB in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->pb(pb);
        } else if (name == "DG") {
          const OPENDDS_STRING& value = it->second;
          u_short dg;
          if (!DCPS::convertToInteger(value, dg)) {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for DG in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->dg(dg);
        } else if (name == "PG") {
          const OPENDDS_STRING& value = it->second;
          u_short pg;
          if (!DCPS::convertToInteger(value, pg)) {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for PG in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->pg(pg);
        } else if (name == "D0") {
          const OPENDDS_STRING& value = it->second;
          u_short d0;
          if (!DCPS::convertToInteger(value, d0)) {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for D0 in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->d0(d0);
        } else if (name == "D1") {
          const OPENDDS_STRING& value = it->second;
          u_short d1;
          if (!DCPS::convertToInteger(value, d1)) {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for D1 in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->d1(d1);
        } else if (name == "DX") {
          const OPENDDS_STRING& value = it->second;
          u_short dx;
          if (!DCPS::convertToInteger(value, dx)) {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for DX in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->dx(dx);
        } else if (name == "TTL") {
          const OPENDDS_STRING& value = it->second;
          unsigned short ttl_us;
          if (!DCPS::convertToInteger(value, ttl_us) || ttl_us > UCHAR_MAX) {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for TTL in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->ttl(static_cast<unsigned char>(ttl_us));
        } else if (name == "SedpMulticast") {
          const OPENDDS_STRING& value = it->second;
          int smInt;
          if (!DCPS::convertToInteger(value, smInt)) {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config ")
               ACE_TEXT("Invalid entry (%C) for SedpMulticast in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->sedp_multicast(bool(smInt));
        } else if (name == "MulticastInterface") {
          discovery->multicast_interface(it->second);
        } else if (name == "SedpLocalAddress") {
          discovery->sedp_local_address(it->second);
        } else if (name == "SpdpLocalAddress") {
          discovery->spdp_local_address(it->second);
        } else if (name == "GuidInterface") {
          discovery->guid_interface(it->second);
        } else if (name == "InteropMulticastOverride") {
          /// FUTURE: handle > 1 group.
          discovery->default_multicast_group(it->second);
        } else if (name == "SpdpSendAddrs") {
          AddrVec spdp_send_addrs;
          const OPENDDS_STRING& value = it->second;
          size_t i = 0;
          do {
            i = value.find_first_not_of(' ', i); // skip spaces
            const size_t n = value.find_first_of(", ", i);
            spdp_send_addrs.push_back(value.substr(i, (n == OPENDDS_STRING::npos) ? n : n - i));
            i = value.find(',', i);
          } while (i++ != OPENDDS_STRING::npos); // skip past comma if there is one
          discovery->spdp_send_addrs().swap(spdp_send_addrs);
        } else if (name == "SpdpRtpsRelayAddress") {
          discovery->spdp_rtps_relay_address(ACE_INET_Addr(it->second.c_str()));
        } else if (name == "SedpRtpsRelayAddress") {
          discovery->sedp_rtps_relay_address(ACE_INET_Addr(it->second.c_str()));
        } else if (name == "RtpsRelayOnly") {
          const OPENDDS_STRING& value = it->second;
          int smInt;
          if (!DCPS::convertToInteger(value, smInt)) {
            ACE_ERROR_RETURN((LM_ERROR,
                              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config ")
                              ACE_TEXT("Invalid entry (%C) for RtpsRelayOnly in ")
                              ACE_TEXT("[rtps_discovery/%C] section.\n"),
                              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->rtps_relay_only(bool(smInt));
#ifdef OPENDDS_SECURITY
        } else if (name == "SedpStunServerAddress") {
          discovery->sedp_stun_server_address(ACE_INET_Addr(it->second.c_str()));
        } else if (name == "UseIce") {
          const OPENDDS_STRING& value = it->second;
          int smInt;
          if (!DCPS::convertToInteger(value, smInt)) {
            ACE_ERROR_RETURN((LM_ERROR,
                              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config ")
                              ACE_TEXT("Invalid entry (%C) for UseIce in ")
                              ACE_TEXT("[rtps_discovery/%C] section.\n"),
                              value.c_str(), rtps_name.c_str()), -1);
          }
          discovery->use_ice(bool(smInt));
        } else if (name == "IceTa") {
          // In milliseconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().T_a(TimeDuration::from_msec(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceTa in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceConnectivityCheckTTL") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().connectivity_check_ttl(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceConnectivityCheckTTL in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceChecklistPeriod") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().checklist_period(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceChecklistPeriod in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceIndicationPeriod") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().indication_period(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceIndicationPeriod in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceNominatedTTL") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().nominated_ttl(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceNominatedTTL in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceServerReflexiveAddressPeriod") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().server_reflexive_address_period(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceServerReflexiveAddressPeriod in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceServerReflexiveIndicationCount") {
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().server_reflexive_indication_count(int_value);
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceServerReflexiveIndicationCount in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceDeferredTriggeredCheckTTL") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().deferred_triggered_check_ttl(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceDeferredTriggeredCheckTTL in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "IceChangePasswordPeriod") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            ICE::Agent::instance()->get_configuration().change_password_period(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
               ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
               ACE_TEXT("Invalid entry (%C) for IceChangePasswordPeriod in ")
               ACE_TEXT("[rtps_discovery/%C] section.\n"),
               string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "MaxAuthTime") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            discovery->max_auth_time(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
                              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
                              ACE_TEXT("Invalid entry (%C) for MaxAuthTime in ")
                              ACE_TEXT("[rtps_discovery/%C] section.\n"),
                              string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "AuthResendPeriod") {
          // In seconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            discovery->auth_resend_period(TimeDuration(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
                              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
                              ACE_TEXT("Invalid entry (%C) for AuthResendPeriod in ")
                              ACE_TEXT("[rtps_discovery/%C] section.\n"),
                              string_value.c_str(), rtps_name.c_str()), -1);
          }
#endif /* OPENDDS_SECURITY */
        } else if (name == "MaxSpdpTimerPeriod") {
          // In milliseconds.
          const OPENDDS_STRING& string_value = it->second;
          int int_value;
          if (DCPS::convertToInteger(string_value, int_value)) {
            discovery->max_spdp_timer_period(TimeDuration::from_msec(int_value));
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for MaxSpdpTimerPeriod in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else if (name == "MaxSpdpSequenceMsgResetChecks") {
          const OPENDDS_STRING& string_value = it->second;
          u_short value;
          if (DCPS::convertToInteger(string_value, value)) {
            discovery->max_spdp_sequence_msg_reset_check(value);
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
              ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
              ACE_TEXT("Invalid entry (%C) for MaxSpdpSequenceMsgResetChecks in ")
              ACE_TEXT("[rtps_discovery/%C] section.\n"),
              string_value.c_str(), rtps_name.c_str()), -1);
          }
        } else {
          ACE_ERROR_RETURN((LM_ERROR,
            ACE_TEXT("(%P|%t) RtpsDiscovery::Config::discovery_config(): ")
            ACE_TEXT("Unexpected entry (%C) in [rtps_discovery/%C] section.\n"),
            name.c_str(), rtps_name.c_str()), -1);
        }
      }

      TheServiceParticipant->add_discovery(discovery);
    }
  }

  // If the default RTPS discovery object has not been configured,
  // instantiate it now.
  const DCPS::Service_Participant::RepoKeyDiscoveryMap& discoveryMap = TheServiceParticipant->discoveryMap();
  if (discoveryMap.find(Discovery::DEFAULT_RTPS) == discoveryMap.end()) {
    TheServiceParticipant->add_discovery(OpenDDS::DCPS::make_rch<RtpsDiscovery>(Discovery::DEFAULT_RTPS));
  }

  return 0;
}

// Participant operations:

OpenDDS::DCPS::RepoId
RtpsDiscovery::generate_participant_guid() {
  OpenDDS::DCPS::RepoId id = GUID_UNKNOWN;
  ACE_GUARD_RETURN(ACE_Thread_Mutex, g, lock_, id);
  if (!guid_interface_.empty()) {
    if (guid_gen_.interfaceName(guid_interface_.c_str()) != 0) {
      if (DCPS::DCPS_debug_level) {
        ACE_DEBUG((LM_WARNING, "(%P|%t) RtpsDiscovery::add_domain_participant()"
                   " - attempt to use specific network interface's MAC addr for"
                   " GUID generation failed.\n"));
      }
    }
  }
  guid_gen_.populate(id);
  id.entityId = ENTITYID_PARTICIPANT;
  return id;
}

DCPS::AddDomainStatus
RtpsDiscovery::add_domain_participant(DDS::DomainId_t domain,
                                      const DDS::DomainParticipantQos& qos)
{
  DCPS::AddDomainStatus ads = {OpenDDS::DCPS::RepoId(), false /*federated*/};
  ACE_GUARD_RETURN(ACE_Thread_Mutex, g, lock_, ads);
  if (!guid_interface_.empty()) {
    if (guid_gen_.interfaceName(guid_interface_.c_str()) != 0) {
      if (DCPS::DCPS_debug_level) {
        ACE_DEBUG((LM_WARNING, "(%P|%t) RtpsDiscovery::add_domain_participant()"
                   " - attempt to use specific network interface's MAC addr for"
                   " GUID generation failed.\n"));
      }
    }
  }
  guid_gen_.populate(ads.id);
  ads.id.entityId = ENTITYID_PARTICIPANT;
  try {
    const DCPS::RcHandle<Spdp> spdp (DCPS::make_rch<Spdp>(domain, ref(ads.id), qos, this));
    // ads.id may change during Spdp constructor
    participants_[domain][ads.id] = spdp;
  } catch (const std::exception& e) {
    ads.id = GUID_UNKNOWN;
    ACE_ERROR((LM_ERROR, "(%P|%t) RtpsDiscovery::add_domain_participant() - "
      "failed to initialize RTPS Simple Participant Discovery Protocol: %C\n",
      e.what()));
  }
  return ads;
}

#if defined(OPENDDS_SECURITY)
DCPS::AddDomainStatus
RtpsDiscovery::add_domain_participant_secure(DDS::DomainId_t domain,
                                      const DDS::DomainParticipantQos& qos,
                                      const OpenDDS::DCPS::RepoId& guid,
                                      DDS::Security::IdentityHandle id,
                                      DDS::Security::PermissionsHandle perm,
                                      DDS::Security::ParticipantCryptoHandle part_crypto)
{
  DCPS::AddDomainStatus ads = {guid, false /*federated*/};
  ads.id.entityId = ENTITYID_PARTICIPANT;
  try {
    const DCPS::RcHandle<Spdp> spdp (DCPS::make_rch<Spdp>(domain, ads.id, qos, this, id, perm, part_crypto));
    participants_[domain][ads.id] = spdp;
  } catch (const std::exception& e) {
    ads.id = GUID_UNKNOWN;
    ACE_ERROR((LM_WARNING, "(%P|%t) RtpsDiscovery::add_domain_participant_secure() - "
      "failed to initialize RTPS Simple Participant Discovery Protocol: %C\n",
      e.what()));
  }
  return ads;
}
#endif

void
RtpsDiscovery::signal_liveliness(const DDS::DomainId_t domain_id,
                                 const OpenDDS::DCPS::RepoId& part_id,
                                 DDS::LivelinessQosPolicyKind kind)
{
  get_part(domain_id, part_id)->signal_liveliness(kind);
}

#ifdef OPENDDS_SECURITY
DDS::Security::ParticipantCryptoHandle
RtpsDiscovery::get_crypto_handle(DDS::DomainId_t domain,
                                 const DCPS::RepoId& local_participant,
                                 const DCPS::RepoId& remote_participant) const
{
  ParticipantHandle p = get_part(domain, local_participant);
  if (p) {
    if (remote_participant == GUID_UNKNOWN || remote_participant == local_participant) {
      return p->crypto_handle();
    } else {
      return p->remote_crypto_handle(remote_participant);
    }
  }

  return DDS::HANDLE_NIL;
}

#endif

RtpsDiscovery::StaticInitializer::StaticInitializer()
{
  TheServiceParticipant->register_discovery_type("rtps_discovery", new Config);
}

u_short
RtpsDiscovery::get_spdp_port(DDS::DomainId_t domain,
                             const DCPS::RepoId& local_participant) const
{
  ParticipantHandle p = get_part(domain, local_participant);
  if (p) {
    return p->get_spdp_port();
  }

  return 0;
}

u_short
RtpsDiscovery::get_sedp_port(DDS::DomainId_t domain,
                             const DCPS::RepoId& local_participant) const
{
  ParticipantHandle p = get_part(domain, local_participant);
  if (p) {
    return p->get_sedp_port();
  }

  return 0;
}

void
RtpsDiscovery::schedule_send(DDS::DomainId_t domain,
                             const DCPS::RepoId& local_participant,
                             const DCPS::TimeDuration& delay) const
{
  ParticipantHandle p = get_part(domain, local_participant);
  if (p) {
    p->schedule_send(delay);
  }
}

} // namespace DCPS
} // namespace OpenDDS

OPENDDS_END_VERSIONED_NAMESPACE_DECL

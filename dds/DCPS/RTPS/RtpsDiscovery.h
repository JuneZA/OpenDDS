/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_RTPS_RTPSDISCOVERY_H
#define OPENDDS_RTPS_RTPSDISCOVERY_H


#include "dds/DCPS/DiscoveryBase.h"
#include "dds/DCPS/RTPS/GuidGenerator.h"
#include "dds/DCPS/RTPS/Spdp.h"
#include "rtps_export.h"

#include "ace/Configuration.h"

#include "dds/DCPS/PoolAllocator.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

class DDS_TEST;

OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL

namespace OpenDDS {
namespace RTPS {

const char RTPS_DISCOVERY_ENDPOINT_ANNOUNCEMENTS[] = "OpenDDS.RtpsDiscovery.EndpointAnnouncements";

/**
 * @class RtpsDiscovery
 *
 * @brief Discovery Strategy class that implements RTPS discovery
 *
 * This class implements the Discovery interface for Rtps-based
 * discovery.
 *
 */
class OpenDDS_Rtps_Export RtpsDiscovery : public OpenDDS::DCPS::PeerDiscovery<Spdp> {
public:
  explicit RtpsDiscovery(const RepoKey& key);
  ~RtpsDiscovery();

  virtual OpenDDS::DCPS::RepoId generate_participant_guid();

  virtual OpenDDS::DCPS::AddDomainStatus add_domain_participant(
    DDS::DomainId_t domain,
    const DDS::DomainParticipantQos& qos);

#if defined(OPENDDS_SECURITY)
  virtual OpenDDS::DCPS::AddDomainStatus add_domain_participant_secure(
    DDS::DomainId_t domain,
    const DDS::DomainParticipantQos& qos,
    const OpenDDS::DCPS::RepoId& guid,
    DDS::Security::IdentityHandle id,
    DDS::Security::PermissionsHandle perm,
    DDS::Security::ParticipantCryptoHandle part_crypto);
#endif

  virtual bool supports_liveliness() const { return true; }

  virtual void signal_liveliness(const DDS::DomainId_t domain_id,
                                 const OpenDDS::DCPS::RepoId& part_id,
                                 DDS::LivelinessQosPolicyKind kind);

  // configuration parameters:

  DCPS::TimeDuration resend_period() const { return resend_period_; }
  void resend_period(const DCPS::TimeDuration& period) {
    resend_period_ = period;
  }

  u_short pb() const { return pb_; }
  void pb(u_short port_base) {
    pb_ = port_base;
  }

  u_short dg() const { return dg_; }
  void dg(u_short domain_gain) {
    dg_ = domain_gain;
  }

  u_short pg() const { return pg_; }
  void pg(u_short participant_gain) {
    pg_ = participant_gain;
  }

  u_short d0() const { return d0_; }
  void d0(u_short offset_zero) {
    d0_ = offset_zero;
  }

  u_short d1() const { return d1_; }
  void d1(u_short offset_one) {
    d1_ = offset_one;
  }

  u_short dx() const { return dx_; }
  void dx(u_short offset_two) {
    dx_ = offset_two;
  }

  unsigned char ttl() const { return ttl_; }
  void ttl(unsigned char time_to_live) {
    ttl_ = time_to_live;
  }

  OPENDDS_STRING sedp_local_address() const { return sedp_local_address_; }
  void sedp_local_address(const OPENDDS_STRING& mi) {
    sedp_local_address_ = mi;
  }

  OPENDDS_STRING spdp_local_address() const { return spdp_local_address_; }
  void spdp_local_address(const OPENDDS_STRING& mi) {
    spdp_local_address_ = mi;
  }

  bool sedp_multicast() const { return sedp_multicast_; }
  void sedp_multicast(bool sm) {
    sedp_multicast_ = sm;
  }

  OPENDDS_STRING multicast_interface() const { return multicast_interface_; }
  void multicast_interface(const OPENDDS_STRING& mi) {
    multicast_interface_ = mi;
  }

  OPENDDS_STRING default_multicast_group() const { return default_multicast_group_; }
  void default_multicast_group(const OPENDDS_STRING& group) {
    default_multicast_group_ = group;
  }

  typedef OPENDDS_VECTOR(OPENDDS_STRING) AddrVec;
  const AddrVec& spdp_send_addrs() const { return spdp_send_addrs_; }
  AddrVec& spdp_send_addrs() { return spdp_send_addrs_; }

  OPENDDS_STRING guid_interface() const { return guid_interface_; }
  void guid_interface(const OPENDDS_STRING& gi) {
    guid_interface_ = gi;
  }

  const ACE_INET_Addr& spdp_rtps_relay_address() const { return spdp_rtps_relay_address_; }
  void spdp_rtps_relay_address(const ACE_INET_Addr& address) {
    spdp_rtps_relay_address_ = address;
  }

  const ACE_INET_Addr& sedp_rtps_relay_address() const { return sedp_rtps_relay_address_; }
  void sedp_rtps_relay_address(const ACE_INET_Addr& address) {
    sedp_rtps_relay_address_ = address;
  }

  bool rtps_relay_only() const { return rtps_relay_only_; }
  void rtps_relay_only(bool f) { rtps_relay_only_ = f; }

  const ACE_INET_Addr& sedp_stun_server_address() const { return sedp_stun_server_address_; }
  void sedp_stun_server_address(const ACE_INET_Addr& address) {
    sedp_stun_server_address_ = address;
  }

  bool use_ice() const { return use_ice_; }
  void use_ice(bool ui) {
    use_ice_ = ui;
  }

  const DCPS::TimeDuration& max_spdp_timer_period() const { return max_spdp_timer_period_; }
  void max_spdp_timer_period(const DCPS::TimeDuration& x) { max_spdp_timer_period_ = x; }

  const DCPS::TimeDuration& max_auth_time() const { return max_auth_time_; }
  void max_auth_time(const DCPS::TimeDuration& x) { max_auth_time_ = x; }

  const DCPS::TimeDuration& auth_resend_period() const { return auth_resend_period_; }
  void auth_resend_period(const DCPS::TimeDuration& x) { auth_resend_period_ = x; }

#ifdef OPENDDS_SECURITY
  DDS::Security::ParticipantCryptoHandle get_crypto_handle(DDS::DomainId_t domain,
                                                           const DCPS::RepoId& local_participant,
                                                           const DCPS::RepoId& remote_participant = GUID_UNKNOWN) const;
#endif

  u_short max_spdp_sequence_msg_reset_check() const { return max_spdp_sequence_msg_reset_check_; }
  void max_spdp_sequence_msg_reset_check(u_short reset_value) {
    max_spdp_sequence_msg_reset_check_ = reset_value;
  }

  u_short get_spdp_port(DDS::DomainId_t domain,
                        const DCPS::RepoId& local_participant) const;
  u_short get_sedp_port(DDS::DomainId_t domain,
                        const DCPS::RepoId& local_participant) const;
  void schedule_send(DDS::DomainId_t domain,
                     const DCPS::RepoId& local_participant,
                     const DCPS::TimeDuration& delay) const;

private:
  DCPS::TimeDuration resend_period_;
  u_short pb_, dg_, pg_, d0_, d1_, dx_;
  unsigned char ttl_;
  bool sedp_multicast_;
  OPENDDS_STRING multicast_interface_, sedp_local_address_, spdp_local_address_;
  OPENDDS_STRING default_multicast_group_;  /// FUTURE: handle > 1 group.
  OPENDDS_STRING guid_interface_;
  AddrVec spdp_send_addrs_;
  ACE_INET_Addr spdp_rtps_relay_address_;
  ACE_INET_Addr sedp_rtps_relay_address_;
  bool rtps_relay_only_;
  ACE_INET_Addr sedp_stun_server_address_;
  bool use_ice_;
  DCPS::TimeDuration max_spdp_timer_period_;
  DCPS::TimeDuration max_auth_time_;
  DCPS::TimeDuration auth_resend_period_;
  u_short max_spdp_sequence_msg_reset_check_;


  /// Guids will be unique within this RTPS configuration
  GuidGenerator guid_gen_;

public:
  class Config : public Discovery::Config {
  public:
    int discovery_config(ACE_Configuration_Heap& cf);
  };

  class OpenDDS_Rtps_Export StaticInitializer {
  public:
    StaticInitializer();
  };

private:
  friend class ::DDS_TEST;
};

static RtpsDiscovery::StaticInitializer initialize_rtps;

typedef OpenDDS::DCPS::RcHandle<RtpsDiscovery> RtpsDiscovery_rch;

} // namespace RTPS
} // namespace OpenDDS

OPENDDS_END_VERSIONED_NAMESPACE_DECL

#endif /* OPENDDS_RTPS_RTPSDISCOVERY_H  */

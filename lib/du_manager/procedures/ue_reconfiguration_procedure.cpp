/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "ue_reconfiguration_procedure.h"
#include "../../ran/gnb_format.h"
#include "srsgnb/mac/mac_ue_configurator.h"

using namespace srsgnb;
using namespace srsgnb::srs_du;

ue_reconfiguration_procedure::ue_reconfiguration_procedure(const f1ap_ue_config_update_request& request_,
                                                           ue_manager_ctrl_configurator&        ue_mng_,
                                                           mac_ue_configurator&                 mac_ue_mng_) :
  request(request_), ue_mng(ue_mng_), mac_ue_mng(mac_ue_mng_), ue(ue_mng.find_ue(request.ue_index))
{
  srsgnb_assert(ue != nullptr, "ueId={} not found", request.ue_index);
}

void ue_reconfiguration_procedure::operator()(coro_context<async_task<void>>& ctx)
{
  CORO_BEGIN(ctx);

  log_proc_started(logger, request.ue_index, ue->rnti, "UE Modification");

  add_bearers_in_ue_context();

  CORO_AWAIT(update_mac_bearers());

  log_proc_completed(logger, request.ue_index, ue->rnti, "UE Modification");

  CORO_RETURN();
}

void ue_reconfiguration_procedure::add_bearers_in_ue_context()
{
  for (srb_id_t srbid : request.srbs_to_addmod) {
    lcid_t lcid = (lcid_t)srbid;
    srsgnb_assert(not ue->bearers.contains(lcid), "Reconfigurations of bearers not supported");
    ue->bearers.emplace(lcid);
    du_logical_channel_context& srb = ue->bearers[lcid];
    srb.lcid                        = lcid;
    srb.drbid                       = drb_id_t::invalid;
  }

  for (const drb_to_addmod& drbtoadd : request.drbs_to_addmod) {
    srsgnb_assert(not ue->bearers.contains(drbtoadd.lcid), "Reconfigurations of bearers not supported");
    ue->bearers.emplace(drbtoadd.lcid);
    du_logical_channel_context& drb = ue->bearers[drbtoadd.lcid];
    drb.lcid                        = drbtoadd.lcid;
    drb.drbid                       = drbtoadd.drbid;
  }
}

async_task<mac_ue_reconfiguration_response_message> ue_reconfiguration_procedure::update_mac_bearers()
{
  mac_ue_reconfiguration_request_message mac_ue_reconf_req;
  mac_ue_reconf_req.ue_index    = request.ue_index;
  mac_ue_reconf_req.pcell_index = ue->pcell_index;

  for (srb_id_t srbid : request.srbs_to_addmod) {
    du_logical_channel_context& bearer = ue->bearers[srb_id_to_uint(srbid)];
    mac_ue_reconf_req.bearers_to_addmod.emplace_back();
    mac_logical_channel_addmod& lc_ch = mac_ue_reconf_req.bearers_to_addmod.back();

    lc_ch.lcid      = bearer.lcid;
    lc_ch.ul_bearer = bearer.mac_rx_notifier.get();
    lc_ch.dl_bearer = bearer.mac_tx_notifier.get();
    mac_ue_reconf_req.bearers_to_addmod.push_back(std::move(lc_ch));
  }

  for (const drb_to_addmod& drb : request.drbs_to_addmod) {
    du_logical_channel_context& bearer = ue->bearers[drb.lcid];
    mac_ue_reconf_req.bearers_to_addmod.emplace_back();
    mac_logical_channel_addmod& lc_ch = mac_ue_reconf_req.bearers_to_addmod.back();

    lc_ch.lcid      = bearer.lcid;
    lc_ch.ul_bearer = bearer.mac_rx_notifier.get();
    lc_ch.dl_bearer = bearer.mac_tx_notifier.get();
    mac_ue_reconf_req.bearers_to_addmod.push_back(std::move(lc_ch));
  }

  return mac_ue_mng.handle_ue_reconfiguration_request(mac_ue_reconf_req);
}

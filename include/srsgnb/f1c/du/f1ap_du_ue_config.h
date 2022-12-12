/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsgnb/adt/optional.h"
#include "srsgnb/asn1/f1ap/f1ap.h"
#include "srsgnb/f1c/du/f1c_bearer.h"
#include "srsgnb/f1c/du/f1c_rx_sdu_notifier.h"
#include "srsgnb/f1u/du/f1u_bearer.h"
#include "srsgnb/f1u/du/f1u_rx_sdu_notifier.h"
#include "srsgnb/ran/du_types.h"
#include "srsgnb/ran/lcid.h"
#include "srsgnb/ran/rnti.h"
#include <vector>

namespace srsgnb {
namespace srs_du {

/// \brief F1-C bearer to Add or Modify in UE F1 context.
struct f1c_bearer_to_addmod {
  srb_id_t             srb_id;
  f1c_rx_sdu_notifier* rx_sdu_notifier;
};

/// \brief F1-U bearer to Add or Modify in UE F1 context.
struct f1u_bearer_to_addmod {
  drb_id_t             drb_id;
  f1u_rx_sdu_notifier* rx_sdu_notifier;
};

/// \brief F1c bearer Added or Modified in UE F1 context.
struct f1c_bearer_addmodded {
  srb_id_t    srb_id;
  f1c_bearer* bearer;
};

/// \brief F1u bearer Added or Modified in UE F1 context.
struct f1u_bearer_addmodded {
  drb_id_t    drb_id;
  f1u_bearer* bearer;
};

/// \brief Request sent to the DU F1AP to create a new UE F1AP context.
struct f1ap_ue_creation_request {
  du_ue_index_t                     ue_index;
  du_cell_index_t                   pcell_index;
  rnti_t                            c_rnti;
  byte_buffer                       du_cu_rrc_container;
  std::vector<f1c_bearer_to_addmod> f1c_bearers_to_add;
};

/// \brief Response from the DU F1AP to the request to create a new UE.
struct f1ap_ue_creation_response {
  bool                     result;
  std::vector<f1c_bearer*> f1c_bearers_added;
};

/// \brief Request sent to the DU F1AP to update the configuration of an existing UE F1AP context.
struct f1ap_ue_configuration_request {
  du_ue_index_t                     ue_index;
  std::vector<f1c_bearer_to_addmod> f1c_bearers_to_add;
  std::vector<f1u_bearer_to_addmod> f1u_bearers_to_add;
};

/// \brief Response from the DU F1AP to the request to update the configuration of an existing UE.
struct f1ap_ue_configuration_response {
  std::vector<f1c_bearer_addmodded> f1c_bearers_added;
  std::vector<f1u_bearer_addmodded> f1u_bearers_added;
};

/// \brief DRB that the DU F1AP requests the DU manager to setup/modify.
struct drb_to_setup {
  drb_id_t               drb_id;
  optional<lcid_t>       lcid;
  asn1::f1ap::rlc_mode_e rlc_mode;
};

/// \brief Request from DU F1AP to DU manager to modify existing UE configuration.
struct f1ap_ue_context_update_request {
  du_ue_index_t             ue_index;
  std::vector<srb_id_t>     srbs_to_setup;
  std::vector<drb_to_setup> drbs_to_setup;
};

/// \brief Response from DU manager to DU F1AP with the result of the UE context update.
struct f1ap_ue_context_update_response {
  bool        result;
  byte_buffer du_to_cu_rrc_container;
};

} // namespace srs_du
} // namespace srsgnb

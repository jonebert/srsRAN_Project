/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "srsran/scheduler/config/csi_helper.h"

using namespace srsran;
using namespace csi_helper;

/// Get CRBs across which a CSI resource spans as per TS 38.331, "CSI-FrequencyOccupation".
///
/// \param nof_crbs Number of CRBs used for the CSI-RS.
/// \param bwp_nof_rbs Width of the BWP where the CSI-RS is used, in RBs.
/// \return A valid number of RBs for the CSI-RS.
static crb_interval get_csi_freq_occupation_rbs(unsigned nof_crbs, unsigned bwp_nof_rbs)
{
  // - Only multiples of 4 are allowed.
  // - The smallest configurable number is the MIN(24, BWP_width).
  // Note: If the configured value is larger than the width of the corresponding BWP, the UE shall assume that the
  // actual CSI-RS bandwidth is equal to the width of the BWP.
  const unsigned nof_rbs_in_multiples_of_4 = 4 * divide_ceil(nof_crbs, 4);

  return {0, std::max(nof_rbs_in_multiples_of_4, std::min(24U, bwp_nof_rbs))};
}

/// \brief Compute default CSI-RS signalling period to use, while constrained by TS38.214, 5.1.6.1.1.
csi_resource_periodicity srsran::csi_helper::get_max_csi_rs_period(subcarrier_spacing pdsch_scs)
{
  const csi_resource_periodicity max_csi_period =
      static_cast<csi_resource_periodicity>(std::min(80U * get_nof_slots_per_subframe(pdsch_scs), 640U));
  return max_csi_period;
}

static zp_csi_rs_resource make_default_zp_csi_rs_resource(const csi_builder_params& params)
{
  zp_csi_rs_resource res{};
  res.id                    = static_cast<zp_csi_rs_res_id_t>(0);
  res.res_mapping.nof_ports = params.nof_ports;
  if (params.nof_ports == 1) {
    res.res_mapping.fd_alloc.resize(12);
    res.res_mapping.fd_alloc.set(8, true);
    res.res_mapping.cdm = csi_rs_cdm_type::no_CDM;
  } else {
    res.res_mapping.fd_alloc.resize(6);
    res.res_mapping.fd_alloc.set(4, true);
    res.res_mapping.cdm = csi_rs_cdm_type::fd_CDM2;
  }
  res.res_mapping.first_ofdm_symbol_in_td = 8;
  res.res_mapping.freq_density            = csi_rs_freq_density_type::one;
  res.res_mapping.freq_band_rbs           = get_csi_freq_occupation_rbs(params.nof_rbs, params.nof_rbs);
  res.period                              = params.csi_rs_period;
  res.offset                              = 2;

  return res;
}

std::vector<zp_csi_rs_resource>
srsran::csi_helper::make_periodic_zp_csi_rs_resource_list(const csi_builder_params& params)
{
  std::vector<zp_csi_rs_resource> list;

  if (params.nof_ports == 1) {
    // 4 zp-CSI-RS resources at different frequency locations.
    list.resize(4, make_default_zp_csi_rs_resource(params));

    for (unsigned res_id = 0; res_id != list.size(); ++res_id) {
      list[res_id].id = static_cast<zp_csi_rs_res_id_t>(res_id);
      list[res_id].res_mapping.fd_alloc.reset();
      list[res_id].res_mapping.fd_alloc.set(8 + res_id, true);
    }

  } else if (params.nof_ports == 2) {
    // 2 zp-CSI-RS resources at different frequency locations.
    list.resize(2, make_default_zp_csi_rs_resource(params));

    for (unsigned res_id = 0; res_id != list.size(); ++res_id) {
      list[res_id].id = static_cast<zp_csi_rs_res_id_t>(res_id);
      list[res_id].res_mapping.fd_alloc.reset();
      list[res_id].res_mapping.fd_alloc.set(4 + res_id, true);
    }

  } else {
    report_fatal_error("Unsupported number of antenna ports");
  }

  return list;
}

zp_csi_rs_resource_set srsran::csi_helper::make_periodic_zp_csi_rs_resource_set(const csi_builder_params& params)
{
  zp_csi_rs_resource_set zp_set{};

  zp_set.id = static_cast<zp_csi_rs_res_set_id_t>(0);

  if (params.nof_ports == 1) {
    zp_set.zp_csi_rs_res_list = {static_cast<zp_csi_rs_res_set_id_t>(0),
                                 static_cast<zp_csi_rs_res_set_id_t>(1),
                                 static_cast<zp_csi_rs_res_set_id_t>(2),
                                 static_cast<zp_csi_rs_res_set_id_t>(3)};
  } else if (params.nof_ports == 2) {
    zp_set.zp_csi_rs_res_list = {static_cast<zp_csi_rs_res_set_id_t>(0), static_cast<zp_csi_rs_res_set_id_t>(1)};
  } else if (params.nof_ports == 4) {
    zp_set.zp_csi_rs_res_list = {static_cast<zp_csi_rs_res_set_id_t>(0)};
  } else {
    report_fatal_error("Unsupported number of antenna ports {}", params.nof_ports);
  }

  return zp_set;
}

/// \brief Converts a CSI resource periodicity to a suitable CSI report periodicity. Report peridiocity should be
/// larger than the resource periodicity.
static csi_report_periodicity convert_csi_resource_period_to_report_period(csi_resource_periodicity res_period)
{
  switch (res_period) {
    case csi_resource_periodicity::slots4:
      return csi_report_periodicity::slots4;
    case csi_resource_periodicity::slots5:
      return csi_report_periodicity::slots5;
    case csi_resource_periodicity::slots8:
      return csi_report_periodicity::slots8;
    case csi_resource_periodicity::slots10:
      return csi_report_periodicity::slots10;
    case csi_resource_periodicity::slots16:
      return csi_report_periodicity::slots16;
    case csi_resource_periodicity::slots20:
      return csi_report_periodicity::slots20;
    case csi_resource_periodicity::slots32:
      // fallthrough
    case csi_resource_periodicity::slots40:
      return csi_report_periodicity::slots40;
    case csi_resource_periodicity::slots64:
      // fallthrough
    case csi_resource_periodicity::slots80:
      return csi_report_periodicity::slots80;
    case csi_resource_periodicity::slots160:
      return csi_report_periodicity::slots160;
    case csi_resource_periodicity::slots320:
      // fallthrough
    case csi_resource_periodicity::slots640:
      return csi_report_periodicity::slots320;
  }
  return srsran::csi_report_periodicity::slots320;
}

// Fills the values that are common to all CSI-RS resources.
static nzp_csi_rs_resource make_common_nzp_csi_rs_resource(const csi_builder_params& params)
{
  nzp_csi_rs_resource res{};

  // Fill csi_rs_resource_mapping.
  res.res_mapping.freq_density  = csi_rs_freq_density_type::three;
  res.res_mapping.freq_band_rbs = get_csi_freq_occupation_rbs(params.nof_rbs, params.nof_rbs);

  res.pwr_ctrl_offset       = 0;
  res.pwr_ctrl_offset_ss_db = 0;
  res.scrambling_id         = params.pci;

  res.csi_res_period = params.csi_rs_period;

  res.qcl_info_periodic_csi_rs = static_cast<tci_state_id_t>(0);

  return res;
}

static nzp_csi_rs_resource make_channel_measurement_nzp_csi_rs_resource(const csi_builder_params& params)
{
  srsran_assert(params.meas_csi_slot_offset < csi_resource_periodicity_to_uint(params.csi_rs_period),
                "Invalid CSI slot offset {} >= {}",
                params.meas_csi_slot_offset,
                csi_resource_periodicity_to_uint(params.csi_rs_period));
  nzp_csi_rs_resource res = make_common_nzp_csi_rs_resource(params);

  res.res_id                              = static_cast<nzp_csi_rs_res_id_t>(0);
  res.csi_res_offset                      = params.meas_csi_slot_offset;
  res.res_mapping.first_ofdm_symbol_in_td = 4;
  res.res_mapping.nof_ports               = params.nof_ports;
  res.res_mapping.freq_density            = csi_rs_freq_density_type::one;

  if (params.nof_ports == 1) {
    // Freq Alloc -> Row2 (size 12) = 000000000001
    res.res_mapping.fd_alloc.resize(12);
    res.res_mapping.fd_alloc.set(11, true);
    res.res_mapping.cdm = csi_rs_cdm_type::no_CDM;
  } else if (params.nof_ports == 2) {
    // Freq Alloc -> Row Other (size 6) = 000001
    res.res_mapping.fd_alloc.resize(6);
    res.res_mapping.fd_alloc.set(5, true);
    res.res_mapping.cdm = csi_rs_cdm_type::fd_CDM2;
  } else if (params.nof_ports == 4) {
    report_fatal_error("Number of ports {} not supported", params.nof_ports);
  }

  return res;
}

/// \brief Generate Tracking NZP-CSI-RS resource.
static void
fill_tracking_nzp_csi_rs_resource(span<nzp_csi_rs_resource> tracking_csi_rs,
                                  const csi_builder_params& params,
                                  nzp_csi_rs_res_id_t       first_csi_res_id = static_cast<nzp_csi_rs_res_id_t>(1))
{
  static size_t NOF_TRACKING_RESOURCES = 4;

  srsran_assert(tracking_csi_rs.size() == NOF_TRACKING_RESOURCES, "Invalid tracking CSI-RS resource list size");
  srsran_assert(params.tracking_csi_slot_offset + 1 < csi_resource_periodicity_to_uint(params.csi_rs_period),
                "Invalid CSI slot offset");
  nzp_csi_rs_resource res = make_common_nzp_csi_rs_resource(params);

  res.res_mapping.nof_ports    = 1;
  res.res_mapping.fd_alloc     = {true, false, false, false};
  res.res_mapping.cdm          = csi_rs_cdm_type::no_CDM;
  res.res_mapping.freq_density = csi_rs_freq_density_type::three;

  static constexpr unsigned symbol_offsets[]  = {4, 8, 4, 8};
  static constexpr unsigned rel_slot_offset[] = {0, 0, 1, 1};
  for (unsigned i = 0; i != NOF_TRACKING_RESOURCES; ++i) {
    res.res_id                              = static_cast<nzp_csi_rs_res_id_t>(first_csi_res_id + i);
    res.res_mapping.first_ofdm_symbol_in_td = symbol_offsets[i];
    res.csi_res_offset                      = params.tracking_csi_slot_offset + rel_slot_offset[i];
    tracking_csi_rs[i]                      = res;
  }
}

std::vector<nzp_csi_rs_resource> srsran::csi_helper::make_nzp_csi_rs_resource_list(const csi_builder_params& params)
{
  std::vector<nzp_csi_rs_resource> list(5);

  // Channel Measurement - Resource 0.
  list[0]        = make_channel_measurement_nzp_csi_rs_resource(params);
  list[0].res_id = static_cast<nzp_csi_rs_res_id_t>(0);

  // Tracking - Resources 1-4.
  fill_tracking_nzp_csi_rs_resource(span<nzp_csi_rs_resource>(list).subspan(1, 4), params);

  return list;
}

static std::vector<nzp_csi_rs_resource_set> make_nzp_csi_rs_resource_sets()
{
  std::vector<nzp_csi_rs_resource_set> sets(2);

  // Resource Set 0 - Measurement.
  sets[0].res_set_id          = static_cast<nzp_csi_rs_res_set_id_t>(0);
  sets[0].nzp_csi_rs_res      = {static_cast<nzp_csi_rs_res_id_t>(0)};
  sets[0].is_trs_info_present = false;

  // Resource Set 1 - Tracking.
  sets[1].res_set_id          = static_cast<nzp_csi_rs_res_set_id_t>(1);
  sets[1].nzp_csi_rs_res      = {static_cast<nzp_csi_rs_res_id_t>(1),
                                 static_cast<nzp_csi_rs_res_id_t>(2),
                                 static_cast<nzp_csi_rs_res_id_t>(3),
                                 static_cast<nzp_csi_rs_res_id_t>(4)};
  sets[1].is_trs_info_present = true;

  return sets;
}

static std::vector<csi_im_resource> make_csi_im_resources(const csi_builder_params& params)
{
  std::vector<csi_im_resource> res(1);

  // Make CSI-IM resource match in REs, symbols, slots with zp-CSI-RS>
  std::vector<zp_csi_rs_resource> zp_csi_rs_list = make_periodic_zp_csi_rs_resource_list(params);
  const zp_csi_rs_resource&       zp0            = zp_csi_rs_list[0];

  res[0].res_id = static_cast<csi_im_res_id_t>(0);
  res[0].csi_im_res_element_pattern.emplace();
  res[0].csi_im_res_element_pattern->pattern_type = csi_im_resource::csi_im_resource_element_pattern_type::pattern1;
  res[0].csi_im_res_element_pattern->subcarrier_location = zp0.res_mapping.fd_alloc.find_lowest() * params.nof_ports;
  res[0].csi_im_res_element_pattern->symbol_location     = zp0.res_mapping.first_ofdm_symbol_in_td;
  res[0].freq_band_rbs                                   = zp0.res_mapping.freq_band_rbs;
  res[0].csi_res_period                                  = *zp0.period;
  res[0].csi_res_offset                                  = *zp0.offset;

  return res;
}

static std::vector<csi_im_resource_set> make_csi_im_resource_sets()
{
  std::vector<csi_im_resource_set> res_set(1);

  res_set[0].res_set_id        = static_cast<csi_im_res_set_id_t>(0);
  res_set[0].csi_ims_resources = {static_cast<csi_im_res_id_t>(0)};

  return res_set;
}

static std::vector<csi_resource_config> make_csi_resource_configs()
{
  std::vector<csi_resource_config> res_cfgs(3);

  // Resource 0 - Measurement.
  res_cfgs[0].res_cfg_id = static_cast<csi_res_config_id_t>(0);
  res_cfgs[0].csi_rs_res_set_list =
      csi_resource_config::nzp_csi_rs_ssb{.nzp_csi_rs_res_set_list = {static_cast<nzp_csi_rs_res_set_id_t>(0)}};
  res_cfgs[0].bwp_id   = to_bwp_id(0);
  res_cfgs[0].res_type = csi_resource_config::resource_type::periodic;

  // Resource 1 - Interference.
  res_cfgs[1]                     = res_cfgs[0];
  res_cfgs[1].res_cfg_id          = static_cast<csi_res_config_id_t>(1);
  res_cfgs[1].csi_rs_res_set_list = csi_resource_config::csi_im_resource_set_list{static_cast<csi_im_res_set_id_t>(0)};

  // Resource 2 - Tracking.
  res_cfgs[2]            = res_cfgs[0];
  res_cfgs[2].res_cfg_id = static_cast<csi_res_config_id_t>(2);
  res_cfgs[2].csi_rs_res_set_list =
      csi_resource_config::nzp_csi_rs_ssb{.nzp_csi_rs_res_set_list = {static_cast<nzp_csi_rs_res_set_id_t>(1)}};

  return res_cfgs;
}

static std::vector<csi_report_config> make_csi_report_configs(const csi_builder_params& params)
{
  std::vector<csi_report_config> reps(1);

  reps[0].report_cfg_id               = static_cast<csi_report_config_id_t>(0);
  reps[0].res_for_channel_meas        = static_cast<csi_res_config_id_t>(0);
  reps[0].csi_im_res_for_interference = static_cast<csi_res_config_id_t>(1);

  // Set Report Config.
  csi_report_config::periodic_or_semi_persistent_report_on_pucch report_cfg_type{};
  report_cfg_type.report_type = csi_report_config::periodic_or_semi_persistent_report_on_pucch::report_type_t::periodic;
  report_cfg_type.report_slot_period = convert_csi_resource_period_to_report_period(params.csi_rs_period);
  report_cfg_type.report_slot_offset = params.csi_report_slot_offset;
  report_cfg_type.pucch_csi_res_list = {
      csi_report_config::pucch_csi_resource{.ul_bwp = to_bwp_id(0), .pucch_res_id = 9}};
  reps[0].report_cfg_type = report_cfg_type;

  reps[0].report_qty_type = csi_report_config::report_quantity_type_t::cri_ri_pmi_cqi;
  reps[0].non_pmi_port_indication.push_back(csi_report_config::port_index_for_8_ranks{
      .port_index_type = srsran::csi_report_config::port_index_for_8_ranks::port_index_type_t::port_index_1});

  reps[0].report_freq_cfg.emplace();
  reps[0].report_freq_cfg->cqi_format_ind = csi_report_config::cqi_format_indicator::wideband_cqi;
  reps[0].report_freq_cfg->pmi_format_ind = csi_report_config::pmi_format_indicator::wideband_pmi;

  if (params.nof_ports == 2) {
    reps[0].codebook_cfg.emplace();
    codebook_config::type1                                                                     type1{};
    codebook_config::type1::single_panel                                                       single_panel{};
    codebook_config::type1::single_panel::two_antenna_ports_two_tx_codebook_subset_restriction bitmap(6);
    bitmap.fill(0, 6, true);
    single_panel.nof_antenna_ports = bitmap;
    single_panel.typei_single_panel_ri_restriction.resize(8);
    single_panel.typei_single_panel_ri_restriction.from_uint64(0b11);
    type1.sub_type                      = single_panel;
    type1.codebook_mode                 = 1;
    reps[0].codebook_cfg->codebook_type = type1;
  } else if (params.nof_ports > 2) {
    report_fatal_error("Unsupported number of antenna ports");
  }

  reps[0].is_group_based_beam_reporting_enabled = false;
  reps[0].cqi_table                             = csi_report_config::cqi_table_t::table1;
  reps[0].subband_size                          = csi_report_config::subband_size_t::value1;

  return reps;
}

csi_meas_config srsran::csi_helper::make_csi_meas_config(const csi_builder_params& params)
{
  csi_meas_config csi_meas{};

  // NZP-CSI-RS-Resources.
  csi_meas.nzp_csi_rs_res_list = make_nzp_csi_rs_resource_list(params);

  // NZP-CSI-RS-ResourceSets.
  csi_meas.nzp_csi_rs_res_set_list = make_nzp_csi_rs_resource_sets();

  // csi-IM-Resources.
  csi_meas.csi_im_res_list = make_csi_im_resources(params);

  // csi-IM-ResourceSets.
  csi_meas.csi_im_res_set_list = make_csi_im_resource_sets();

  // CSI-ResourceConfig.
  csi_meas.csi_res_cfg_list = make_csi_resource_configs();

  // CSI-ReportConfig.
  csi_meas.csi_report_cfg_list = make_csi_report_configs(params);

  return csi_meas;
}


#ifndef SRSGNB_MAC_UL_PDU_HANDLER_H
#define SRSGNB_MAC_UL_PDU_HANDLER_H

#include "../../ran/gnb_format.h"
#include "../mac_config.h"
#include "../mac_sch_pdu.h"
#include "mac_ul_ue_manager.h"
#include "srsgnb/mac/mac.h"
#include "srsgnb/ran/du_types.h"
#include "srsgnb/ran/slot_point.h"
#include "ul_bsr.h"

namespace srsgnb {

class mac_ul_pdu_handler
{
public:
  mac_ul_pdu_handler(mac_common_config_t& cfg_, sched_ue_feedback& sched_, mac_ul_sdu_notifier& sdu_notifier_) :
    cfg(cfg_), logger(cfg.logger), sched(sched_), sdu_notifier(sdu_notifier_)
  {}

  bool handle_ul_pdu(slot_point sl_rx, du_cell_index_t cell_index, mac_rx_pdu& pdu)
  {
    // Decode PDU
    pdu_ul.init_rx(true);
    if (pdu_ul.unpack(pdu.pdu.data(), pdu.pdu.size()) != 0) {
      return false;
    }

    // Log Rx PDU
    if (logger.info.enabled()) {
      fmt::memory_buffer str_buffer;
      pdu_ul.to_string(str_buffer);
      log_ul_pdu(logger, MAX_NOF_UES, pdu.rnti, cell_index, "PUSCH", to_c_str(str_buffer));
    }

    // Process MAC CRNTI CE first, if it exists
    uint32_t crnti_ce_pos = pdu_ul.get_num_subpdus();
    for (unsigned n = pdu_ul.get_num_subpdus(); n > 0; --n) {
      mac_sch_subpdu& subpdu = pdu_ul.get_subpdu(n - 1);
      if (subpdu.get_lcid() == mac_sch_subpdu::lcid_sch_t::CRNTI) {
        if (not process_ce_subpdu(pdu.rnti, cell_index, subpdu)) {
          continue;
        }
        crnti_ce_pos = n - 1;
      }
    }

    // Process SDUs and remaining MAC CEs
    for (unsigned n = 0; n < pdu_ul.get_num_subpdus(); ++n) {
      mac_sch_subpdu& subpdu = pdu_ul.get_subpdu(n);
      if (subpdu.is_sdu()) {
        sdu_notifier.on_ul_sdu(mac_ul_sdu{pdu.rnti, subpdu.get_lcid(), {}}); // TODO
      } else if (n != crnti_ce_pos) {
        if (not process_ce_subpdu(pdu.rnti, cell_index, subpdu)) {
          continue;
        }
      }
    }

    return true;
  }

private:
  bool process_ce_subpdu(rnti_t rnti, du_cell_index_t cell_index, mac_sch_subpdu& subpdu)
  {
    using lcid_sch_t = mac_sch_subpdu::lcid_sch_t;

    // Handle MAC CEs
    switch (subpdu.get_lcid()) {
      case lcid_sch_t::CCCH_SIZE_48:
      case lcid_sch_t::CCCH_SIZE_64: {
        //        mac_sch_subpdu& ccch_subpdu = const_cast<mac_sch_subpdu&>(subpdu);
        sdu_notifier.on_ul_sdu(mac_ul_sdu{rnti, 0, {}}); // TODO
        // store content for ConRes CE
        //        mac.store_msg3(rnti, make_byte_buffer(ccch_subpdu.get_sdu(), ccch_subpdu.get_sdu_length(),
        //        __FUNCTION__)); // TODO
      } break;
      case lcid_sch_t::CRNTI: {
        rnti_t ce_crnti = subpdu.get_c_rnti();
        //        rnti_t prev_rnti = rnti;
        rnti = ce_crnti;
        sched.ul_sr_info(rnti); // provide UL grant regardless of other BSR content for UE to complete RA
      } break;
      case lcid_sch_t::SHORT_BSR:
      case lcid_sch_t::SHORT_TRUNC_BSR: {
        mac_sch_subpdu::lcg_bsr_t sbsr              = subpdu.get_sbsr();
        uint32_t                  buffer_size_bytes = buff_size_field_to_bytes(sbsr.buffer_size, bsr_format::SHORT);

        ul_bsr_indication_message ul_bsr_ind{};
        ul_bsr_ind.cell_index = cell_index;
        ul_bsr_ind.rnti       = rnti;
        ul_bsr_ind.type = subpdu.get_lcid() == lcid_sch_t::SHORT_BSR ? bsr_type::SHORT_BSR : bsr_type::SHORT_TRUNC_BSR;
        if (buffer_size_bytes == 0) {
          // Assume all LCGs are 0 if reported SBSR is 0
          for (uint32_t j = 0; j <= MAX_LOGICAL_CHANNEL_GROUP; j++) {
            ul_bsr_ind.reported_lcgs.push_back(ul_lcg_report{j, 0});
          }
        } else {
          ul_bsr_ind.reported_lcgs.push_back(ul_lcg_report{sbsr.lcg_id, buffer_size_bytes});
        }
        sched.ul_bsr(ul_bsr_ind);
      } break;
      case lcid_sch_t::LONG_BSR:
      case lcid_sch_t::LONG_TRUNC_BSR: {
        mac_sch_subpdu::lbsr_t lbsr = subpdu.get_lbsr();

        ul_bsr_indication_message ul_bsr_ind{};
        ul_bsr_ind.cell_index = cell_index;
        ul_bsr_ind.rnti       = rnti;
        ul_bsr_ind.type = subpdu.get_lcid() == lcid_sch_t::SHORT_BSR ? bsr_type::SHORT_BSR : bsr_type::SHORT_TRUNC_BSR;
        for (auto& lb : lbsr.list) {
          ul_bsr_ind.reported_lcgs.push_back(
              ul_lcg_report{lb.lcg_id, buff_size_field_to_bytes(lb.buffer_size, bsr_format::LONG)});
        }
        sched.ul_bsr(ul_bsr_ind);
      } break;
      case lcid_sch_t::PADDING:
        break;
      default:
        logger.warning("Unhandled subPDU with LCID={}", subpdu.get_lcid());
    }

    return true;
  }

  /** Converts the buffer size field of a BSR (5 or 8-bit Buffer Size field) into Bytes
   * @param buff_size_field The buffer size field contained in the MAC PDU
   * @param format          The BSR format that determines the buffer size field length
   * @return uint32_t       The actual buffer size level in Bytes
   */
  static uint32_t buff_size_field_to_bytes(size_t buff_size_index, const bsr_format& format)
  {
    static const uint32_t max_offset = 1; // make the reported value bigger than the 2nd biggest

    // early exit
    if (buff_size_index == 0) {
      return 0;
    }

    switch (format) {
      case bsr_format::SHORT:
      case bsr_format::SHORT_TRUNC: {
        const size_t   idx    = std::min(buff_size_index, buffer_size_levels_5bit.size() - 1);
        const uint32_t offset = buff_size_index >= buffer_size_levels_5bit.size() - 1 ? max_offset : 0;
        return buffer_size_levels_5bit[idx] + offset;
      } break;
      case bsr_format::LONG:
      case bsr_format::LONG_TRUNC: {
        const size_t   idx    = std::min(buff_size_index, buffer_size_levels_8bit.size() - 1);
        const uint32_t offset = buff_size_index >= buffer_size_levels_8bit.size() - 1 ? max_offset : 0;
        return buffer_size_levels_8bit[idx] + offset;
      } break;
      default:
        break;
    }
    return 0;
  }

  mac_common_config_t&  cfg;
  srslog::basic_logger& logger;
  sched_ue_feedback&    sched;
  mac_ul_sdu_notifier&  sdu_notifier;

  mac_sch_pdu pdu_ul;
};

} // namespace srsgnb

#endif // SRSGNB_MAC_UL_PDU_HANDLER_H

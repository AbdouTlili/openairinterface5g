#ifndef _E2SM_MET_ENCODER_H
#define _E2SM_MET_ENCODER_H

#include "cmake_targets/ran_build/build/CMakeFiles/E2SM-MET/per_support.h"

ssize_t e2sm_met_encode(const struct asn_TYPE_descriptor_s *td,
		    const asn_per_constraints_t *constraints,void *sptr,
		    uint8_t **buf)
  __attribute__ ((warn_unused_result));

asn_dec_rval_t e2sm_met_decode(const struct asn_TYPE_descriptor_s *td,void *mrl,
		    const uint8_t * const buf,const uint32_t len);


void enc_dec(void);

#endif
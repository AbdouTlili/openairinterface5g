#include "assertions.h"
#include "conversions.h"
#include "intertask_interface.h"
#include "E2SM_MET_MeasurementRecordItemList.h"

#include "E2SM_MET_MeasurementRecordItem.h"


#include "cmake_targets/ran_build/build/CMakeFiles/E2SM-MET/asn_application.h"
#include "cmake_targets/ran_build/build/CMakeFiles/E2SM-MET/per_encoder.h"

#include "cmake_targets/ran_build/build/CMakeFiles/E2SM-MET/per_decoder.h"


ssize_t e2sm_met_encode(const struct asn_TYPE_descriptor_s *td,
        const asn_per_constraints_t *constraints,
        void *sptr,
        uint8_t **buf)
{
    ssize_t encoded;

    DevAssert(td != NULL);
    DevAssert(buf != NULL);

    xer_fprint(stdout, td, sptr);

    encoded = aper_encode_to_new_buffer(td, constraints, sptr, (void **)buf);
    if (encoded < 0) {
        return -1;
    }

    // ASN_STRUCT_FREE_CONTENTS_ONLY((*td), sptr);

    return encoded;
}



asn_dec_rval_t e2sm_met_decode(const struct asn_TYPE_descriptor_s *td,void *sptr,
		    const uint8_t * const buf,const uint32_t len)
{
//   DevAssert(sptr != NULL);
//   DevAssert(buf != NULL);
        fprintf(stderr,"*-*-*-*-*-*-*-*-*-*\n\n");

    for (int i = 0; i < 5; i++)
    {
        fprintf(stderr, "0x%02x,", buf[i]);
    }
        fprintf(stderr,"*-*-*-*-*-*-*-*-*-*\n\n");


  return aper_decode_complete(NULL,td,sptr,buf,len);
}


void enc_dec(void){

    // compiled with : asn1c -gen-PER -no-gen-OER -fcompound-names -no-gen-example -findirect-choice -fno-include-deps -D ./c/ met.asn1
    // cc -DPDU -DASN_DISABLE_OER_SUPPORT -o prog -I. *.c
    // creating MRI
    E2SM_MET_MeasurementRecordItem_t *m1 = (E2SM_MET_MeasurementRecordItem_t *) calloc(1,sizeof(E2SM_MET_MeasurementRecordItem_t *));
    *m1 =  44;
    E2SM_MET_MeasurementRecordItem_t *m0 = (E2SM_MET_MeasurementRecordItem_t *) calloc(1,sizeof(E2SM_MET_MeasurementRecordItem_t *));
    *m0 =  5;

    // creating mril 
    E2SM_MET_MeasurementRecordItemList_t *mril = (E2SM_MET_MeasurementRecordItemList_t *)calloc(1,sizeof(E2SM_MET_MeasurementRecordItemList_t));
    ASN_SEQUENCE_ADD(&mril->list, m1);
    ASN_SEQUENCE_ADD(&mril->list, m0);


    // adding the mri to mril 


    // printing 
    uint8_t *buffer;

    xer_fprint(stdout,&asn_DEF_E2SM_MET_MeasurementRecordItemList,mril);

    //encoding 

    ssize_t encoded = aper_encode_to_new_buffer(
    &asn_DEF_E2SM_MET_MeasurementRecordItemList,
    NULL,
    mril,
    (void **)&buffer);

    // DevAssert(encoded > 0);

    fprintf(stderr, "0------------------ ret_enc : %ld \n ,", encoded);

    for (int i = 0; i < encoded; i++)
    {
        fprintf(stderr, "0x%02x,", buffer[i]);
    }
    fprintf(stderr,"\n\n");


    // //decoding
    E2SM_MET_MeasurementRecordItem_t *m2 = 0;

    E2SM_MET_MeasurementRecordItemList_t *mril2 = 0  ;
    asn_dec_rval_t decode_result;
    decode_result = aper_decode(NULL, &asn_DEF_E2SM_MET_MeasurementRecordItemList,
                                               (void **)&mril2, buffer, encoded,0,0);
    // // DevAssert(decode_result.code == RC_OK);

    // // asn_dec_rval_t decode_result = e2sm_met_decode(&asn_DEF_E2SM_MET_E2SM_MET_MeasurementRecordItemList,(void **) &mr2, buffer, encoded  );
    // // // DevAssert(decode_result.code == RC_OK);


    // //printing 


    if (decode_result.code != RC_OK) {
        fprintf(stderr, "\tERROR decoding - consumed : %ld - code %d\n ",decode_result.consumed,decode_result.code);
        xer_fprint(stderr, &asn_DEF_E2SM_MET_MeasurementRecordItemList ,mril2);

    }else{
        fprintf(stderr,"\t  -------    decoded message ------------\n");
        xer_fprint(stderr, &asn_DEF_E2SM_MET_MeasurementRecordItemList ,mril2);
    }


}



// ssize_t e2sm_met_encode_pdu(E2AP_E2AP_PDU_t *pdu, uint8_t **buf, uint32_t *len)
// {
//     ssize_t encoded;

//     DevAssert(pdu != NULL);
//     DevAssert(buf != NULL);
//     DevAssert(len != NULL);

//     encoded = e2sm_met_encode(&asn_DEF_E2AP_E2AP_PDU,0,pdu,buf);
//     if (encoded < 0) {
//         return -1;
//     }

//     *len = encoded;

//     return encoded;
// }

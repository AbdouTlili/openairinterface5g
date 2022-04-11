
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include "common/utils/assertions.h"
#include "f1ap_common.h"
#include "ric_agent.h"
#include "e2ap_encoder.h"
#include "e2sm_kpm.h"
#include "e2ap_generate_messages.h"

//HERE
#include "E2SM_MET_SubscriptionID.h"
#include "E2SM_MET_MeasurementData.h"
#include "E2SM_MET_MeasurementRecord.h"
#include "E2SM_MET_MeasurementRecordItem.h"
#include "E2SM_MET_GranularityPeriod.h"
#include "E2SM_MET_E2SM-MET-IndicationMessage.h"
#include "E2SM_MET_E2SM-MET-IndicationHeader.h"
#include "E2SM_MET_E2SM-MET-IndicationHeader-Format1.h"
#include "E2SM_MET_MeasurementInfoList.h"
#include "E2SM_MET_MeasurementInfoItem.h"
#include "E2SM_MET_E2SM-MET-IndicationMessage-Format1.h"
#include "E2SM_MET_GlobalMETnode-ID.h"

#include "E2AP_Cause.h"


extern f1ap_cudu_inst_t f1ap_cu_inst[MAX_eNB];
extern int global_e2_node_id(ranid_t ranid, E2AP_GlobalE2node_ID_t* node_id);
extern RAN_CONTEXT_t RC;
extern eNB_RRC_KPI_STATS    rrc_kpi_stats;

/**
 ** The main thing with this abstraction is that we need per-SM modules
 ** to handle the details of the function, event trigger, action, etc
 ** definitions... and to actually do all the logic and implement the
 ** inner parts of the message encoding.  generic e2ap handles the rest.
 **/

//Functions of the SM 

#define MAX_RECORD_ITEM 6
#define MAX_UE 5
#define MAX_MET_MEAS    5

E2SM_MET_MeasurementRecordItem_t *g_indMsgMeasRecItemArr[MAX_KPM_MEAS];


static int e2sm_met_subscription_add(ric_agent_info_t *ric, ric_subscription_t *sub);
static int e2sm_met_subscription_del(ric_agent_info_t *ric, ric_subscription_t *sub, int force,long *cause,long *cause_detail);
static int e2sm_met_control(ric_agent_info_t *ric,ric_control_t *control);
static char *time_stamp(void);
static int e2sm_met_ricInd_timer_expiry(
        ric_agent_info_t *ric,
        long timer_id,
        ric_ran_function_id_t function_id,
        long request_id,
        long instance_id,
        long action_id,
        uint8_t **outbuf,
        uint32_t *outlen);

//TODONOW set the subID and granularity period somewhere 
E2SM_MET_SubscriptionID_t    g_subscriptionID;

E2SM_MET_GranularityPeriod_t     *g_granulPeriod;



/// actual func defs

static int e2sm_met_subscription_add(ric_agent_info_t *ric, ric_subscription_t *sub)
{
  /* XXX: process E2SM content. */
  if (LIST_EMPTY(&ric->subscription_list)) {
    LIST_INSERT_HEAD(&ric->subscription_list,sub,subscriptions);
  }
  else {
    LIST_INSERT_BEFORE(LIST_FIRST(&ric->subscription_list),sub,subscriptions);
  }
  return 0;
}

static int e2sm_met_subscription_del(ric_agent_info_t *ric, ric_subscription_t *sub, int force,long *cause,long *cause_detail)
{
    timer_remove(ric->e2sm_met_timer_id);
    LIST_REMOVE(sub, subscriptions);
    ric_free_subscription(sub);
    return 0;
}

static int e2sm_met_control(ric_agent_info_t *ric,ric_control_t *control)
{
    return 0;
}

static char *time_stamp(void)
{
    char *timestamp = (char *)malloc(sizeof(char) * 128);
    time_t ltime;
    ltime=time(NULL);
    struct tm *tm;
    tm=localtime(&ltime);

    sprintf(timestamp,"%d/%d/%d | %d:%d:%d", tm->tm_year+1900, tm->tm_mon,
            tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    return timestamp;
}


static int e2sm_met_ricInd_timer_expiry(
        ric_agent_info_t *ric,
        long timer_id,
        ric_ran_function_id_t function_id,
        long request_id,
        long instance_id,
        long action_id,
        uint8_t **outbuf,
        uint32_t *outlen)
{
    E2SM_MET_E2SM_MET_IndicationMessage_t* indicationmessage;
    ric_subscription_t *rs;

    DevAssert(timer_id == ric->e2sm_met_timer_id);

    char *time = time_stamp();
    RIC_AGENT_INFO("[%s] ----  Sending MET RIC Indication, timer_id %ld function_id %ld---------\n", 
                   time, timer_id, function_id);
    free(time);

    /* Fetch the RIC Subscription */
    rs = ric_agent_lookup_subscription(ric,request_id,instance_id,function_id);
    if (!rs) {
        RIC_AGENT_ERROR("failed to find subscription %ld/%ld/%ld\n", request_id,instance_id,function_id);
    }

    //REVIEW update the name and the params if necessar
    indicationmessage = encode_kpm_Indication_Msg(ric, rs);

    {
        char *error_buf = (char*)calloc(300, sizeof(char));
        size_t errlen;
        asn_check_constraints(&asn_DEF_E2SM_MET_E2SM_MET_IndicationMessage, indicationmessage, error_buf, &errlen);
        //fprintf(stderr,"KPM IND error length %zu\n", errlen);
        //fprintf(stderr,"KPM IND error buf %s\n", error_buf);
        free(error_buf);
        //xer_fprint(stderr, &asn_DEF_E2SM_KPM_E2SM_KPMv2_IndicationMessage, indicationmessage);
    }
    g_granularityIndx = 0; // Resetting

    //xer_fprint(stderr, &asn_DEF_E2SM_KPM_E2SM_KPMv2_IndicationMessage, indicationmessage);
    uint8_t e2smbuffer[8192];
    size_t e2smbuffer_size = 8192;

    asn_enc_rval_t er = asn_encode_to_buffer(NULL,
            ATS_ALIGNED_BASIC_PER,
            &asn_DEF_E2SM_MET_E2SM_MET_IndicationMessage,
            indicationmessage, e2smbuffer, e2smbuffer_size);

    //fprintf(stderr, "er encded is %zu\n", er.encoded);
    //fprintf(stderr, "after encoding KPM IND message\n");

    E2AP_E2AP_PDU_t *e2ap_pdu = (E2AP_E2AP_PDU_t*)calloc(1, sizeof(E2AP_E2AP_PDU_t));

    E2SM_MET_E2SM_MET_IndicationHeader_t* ind_header_style1 =
        (E2SM_MET_E2SM_MET_IndicationHeader_t*)calloc(1,sizeof(E2SM_MET_E2SM_MET_IndicationHeader_t));
    //REVIEW update the name and the params if necessar
    encode_e2sm_met_indication_header(ric->ranid, ind_header_style1);

    uint8_t e2sm_header_buf_style1[8192];
    size_t e2sm_header_buf_size_style1 = 8192;
    asn_enc_rval_t er_header_style1 = asn_encode_to_buffer(
            NULL,
            ATS_ALIGNED_BASIC_PER,
            &asn_DEF_E2SM_MET_E2SM_MET_IndicationHeader,
            ind_header_style1,
            e2sm_header_buf_style1,
            e2sm_header_buf_size_style1);

    if (er_header_style1.encoded < 0) {
        fprintf(stderr, "ERROR encoding indication header, name=%s, tag=%s", er_header_style1.failed_type->name, er_header_style1.failed_type->xml_tag);
    }

    DevAssert(er_header_style1.encoded >= 0);

    // TODO - remove hardcoded values
    generate_e2apv1_indication_request_parameterized(
            e2ap_pdu, request_id, instance_id, function_id, action_id,
            0, e2sm_header_buf_style1, er_header_style1.encoded,
            e2smbuffer, er.encoded);

    *outlen = e2ap_asn1c_encode_pdu(e2ap_pdu, outbuf);

    return 0;
}


// TODO this is a requirements (aka used by handle time expircy function) so it is imported here to be later modified to meet MET SM defs 
// REVIEW this contains the dummy data, it is not real ones + it is duplicated aka RecordItems are creaated once and duplicated in all Records in measData
    // the thing that made this the the use of a table aka array instead of a matrix for troring the measurments : g_indMsgMeasRecItemArr
static E2SM_MET_E2SM_MET_IndicationMessage_t*
encode_met_Indication_Msg(ric_agent_info_t* ric, ric_subscription_t *rs)
{
    int ret;
    uint8_t i,k;

    E2SM_MET_MeasurementData_t* meas_data
    E2SM_MET_MeasurementRecord_t* meas_rec[MAX_UE];
    E2SM_MET_MeasurementRecordItem_t* meas_data_item[MAX_RECORD_ITEM];

    

    if (action_def_missing == TRUE)
    { 
        for (i = 0; i < MAX_MET_MEAS; i++)
        {
            g_indMsgMeasRecItemArr[i] = (E2SM_MET_MeasurementRecordItem_t *)calloc(1,sizeof(E2SM_MET_MeasurementRecordItem_t));
            g_indMsgMeasRecItemArr[i]->present = E2SM_MET_MeasurementRecordItem_PR_integer;

            switch(i)
            {
                case 0:/*RRC.ConnEstabAtt.sum*/
                    g_indMsgMeasRecItemArr[i]->choice.integer = 1;
                    break;
                case 1:/*RRC.ConnEstabSucc.sum*/
                    g_indMsgMeasRecItemArr[i]->choice.integer = 2; 
                    break;
                case 2:/*RRC.ConnReEstabAtt.sum*/
                    g_indMsgMeasRecItemArr[i]->choice.integer = 3;
                    break;
                case 3:/*RRC.ConnMean*/
                    g_indMsgMeasRecItemArr[i]->choice.integer = 4;
                    break;
                case 4:/*RRC.ConnMax*/
                    g_indMsgMeasRecItemArr[i]->choice.integer = 5;
                    break;

                default:
                    break;
            }
        }
        g_granularityIndx = 1;
    } 

    //RIC_AGENT_INFO("Granularity Idx=:%d\n",g_granularityIndx);

    /*
     * measData->measurementRecord (List)
     */
    meas_data = (E2SM_MET_MeasurementData_t*)calloc(1, sizeof(E2SM_MET_MeasurementData_t));
    DevAssert(meas_data!=NULL);
    
    for (k=0; k < MAX_UE; k++)
    {
        /*
         * Measurement Record->MeasurementRecordItem (List)
         */
        meas_rec[k] = (E2SM_MET_MeasurementRecord_t *)calloc(1, sizeof(E2SM_MET_MeasurementRecord_t));
        meas_rec[k]->ueID = k;
        meas_rec[k]->ueTag = "ABC";
        for(i=0; i < MAX_RECORD_ITEM; i++)
        { 
            /* Meas Records meas_rec[]  have to be prepared for each Meas data item */
            ret = ASN_SEQUENCE_ADD(&meas_rec[k]->list, g_indMsgMeasRecItemArr[i]);
            DevAssert(ret == 0);
        }
        
        //this section is commmented because unlike the original KPM asn1 def the MET does not use DataItem but the structure is 
            // directly Data -> Record -> RecordItem

        // /* MeasDataItem*/
        // meas_data_item[k] = (E2SM_KPM_MeasurementDataItem_KPMv2_t*)calloc(1, sizeof(E2SM_KPM_MeasurementDataItem_KPMv2_t));
        // meas_data_item[k]->measRecord = *meas_rec[k];

        /* Enqueue Meas data items */
        ret = ASN_SEQUENCE_ADD(&meas_data->list, meas_rec[k]);
        DevAssert(ret == 0);
    }

   /*
    * measInfoList
    */
   
    E2SM_MET_MeasurementInfoList_t* meas_info_list = (E2SM_MET_MeasurementInfoList_t*)calloc(1, sizeof(E2SM_MET_MeasurementInfoList_t));
    for(i=0; i < MAX_RECORD_ITEM; i++)
    {
        E2SM_MET_MeasurementInfoItem_t* meas_info_item = (E2SM_MET_MeasurementInfoItem_t*)calloc(1, sizeof(E2SM_MET_MeasurementInfoItem_t));
        meas_info_item->measType.buf = (uint8_t *)strdup("measPH");
        meas_info_item->measType.size = strlen("measPH");
        ret = ASN_SEQUENCE_ADD(&meas_info_list->list, meas_info_item);
        DevAssert(ret == 0);
    }

    /*
     * IndicationMessage_Format1 -> measInfoList
     * IndicationMessage_Format1 -> measData
     */

    
    E2SM_MET_E2SM_MET_IndicationMessage_Format1_t* format = 
                        (E2SM_MET_E2SM_MET_IndicationMessage_Format1_t*)calloc(1, sizeof(E2SM_MET_E2SM_MET_IndicationMessage_Format1_t));
    ASN_STRUCT_RESET(asn_DEF_E2SM_MET_E2SM_MET_IndicationMessage_Format1, format);
    //format->subscriptID.size = g_subscriptionID.size;
    //format->subscriptID.buf = g_subscriptionID.buf;
    format->subscriptID = g_subscriptionID;
	format->measInfoList = meas_info_list;
    format->measData = *meas_data;
    format->granulPeriod = g_granulPeriod;

    /*
     * IndicationMessage -> IndicationMessage_Format1
     */

    
    E2SM_MET_E2SM_MET_IndicationMessage_t* indicationmessage = 
                                (E2SM_MET_E2SM_MET_IndicationMessage_t*)calloc(1, sizeof(E2SM_MET_E2SM_MET_IndicationMessage_t));
    indicationmessage->indicationMessage_formats.present = 
                                    E2SM_MET_E2SM_MET_IndicationMessage__indicationMessage_formats_PR_indicationMessage_Format1;
    indicationmessage->indicationMessage_formats.choice.indicationMessage_Format1 = *format;
    
    return indicationmessage;
}
//REVIEW AT the end of the function what known isses : 
    //  the granurality period and subId are not set by nay other function 


//TODO  this is also a requirement of the function e2sm_kpm_ricInd_timer_expiry imported here to be set to hundle MET SM
//NOW 
void encode_e2sm_met_indication_header(ranid_t ranid, E2SM_MET_E2SM_MET_IndicationHeader_t *ihead) 
{
    e2node_type_t node_type;
    ihead->indicationHeader_formats.present = E2SM_MET_E2SM_MET_IndicationHeader__indicationHeader_formats_PR_indicationHeader_Format1;
//HERE 
    E2SM_MET_E2SM_MET_IndicationHeader_Format1_t* ind_header = &ihead->indicationHeader_formats.choice.indicationHeader_Format1;

    /* MET Node ID */
    ind_header->metNodeID = (E2SM_MET_GlobalMETnode_ID_t *)calloc(1,sizeof(E2SM_MET_GlobalMETnode_ID_t));
    //REVIEW is the init of an int done correctly
    *(ind_header->metNodeID) = 10; //REVIEW  // hack 


    /* Collect Start Time Stamp */
    /* Encoded in the same format as the first four octets of the 64-bit timestamp format as defined in section 6 of IETF RFC 5905 */
    ind_header->colletStartTime.buf = (uint8_t *)calloc(1, 4);
    ind_header->colletStartTime.size = 4;
    *((uint32_t *)(ind_header->colletStartTime.buf)) = htonl((uint32_t)time(NULL));
    //xer_fprint(stderr, &asn_DEF_E2SM_KPM_E2SM_KPMv2_IndicationHeader, ihead);
}
//REVIEW AT the end of the function what known isses : 
    //  the ranid arg is not used and we are not sure yet whether the IE affectations are correct or not 
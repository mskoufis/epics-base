/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLiSoftCallback.c */
/*
 *      Author:  Marty Kraimer
 *      Date:    23APR2008
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "callback.h"
#include "cantProceed.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbNotify.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "longinRecord.h"
#include "epicsExport.h"

/* Create the dset for devLiSoftCallback */
static long init_record();
static long read_li();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_li;
	DEVSUPFUN	special_linconv;
}devLiSoftCallback={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_li,
	NULL};
epicsExportAddress(dset,devLiSoftCallback);

typedef struct notifyInfo {
    processNotify *ppn;
    CALLBACK *pcallback;
    long value;
    int    status;
}notifyInfo;

static void getCallback(processNotify *ppn,notifyGetType type)
{
    struct longinRecord *prec = (struct longinRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)prec->dpvt;
    int status = 0;
    long no_elements = 1;
    long options = 0;

    if(ppn->status==notifyCanceled) {
        printf("dbtpn:getCallback notifyCanceled\n");
        return;
    }
    switch(type) {
    case getFieldType:
        status = dbGetField(ppn->paddr,DBR_LONG,&pnotifyInfo->value,
                            &options,&no_elements,0);
        break;
    case getType:
        status = dbGet(ppn->paddr,DBR_LONG,&pnotifyInfo->value,
                       &options,&no_elements,0);
        break;
    }
    pnotifyInfo->status = status;
}

static void doneCallback(processNotify *ppn)
{
    struct longinRecord *prec = (struct longinRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)prec->dpvt;

    callbackRequestProcessCallback(pnotifyInfo->pcallback,prec->prio,prec);
}


static long init_record(struct longinRecord *prec)
{
    DBLINK *plink = &prec->inp;
    struct instio *pinstio;
    char  *pvname;
    DBADDR *pdbaddr=NULL;
    long  status;
    notifyInfo *pnotifyInfo;
    CALLBACK *pcallback;
    processNotify  *ppn=NULL;

    if(plink->type!=INST_IO) {
        recGblRecordError(S_db_badField,(void *)prec,
            "devLiSoftCallback (init_record) Illegal INP field");
        prec->pact=TRUE;
        return(S_db_badField);
    }
    pinstio=(struct instio*)&(plink->value);
    pvname = pinstio->string;
    pdbaddr = callocMustSucceed(1, sizeof(*pdbaddr),
        "devLiSoftCallback::init_record");
    status = dbNameToAddr(pvname,pdbaddr);
    if(status) {
        recGblRecordError(status,(void *)prec,
            "devLiSoftCallback (init_record) linked record not found");
        prec->pact=TRUE;
        return(status);
    }
    pnotifyInfo = callocMustSucceed(1, sizeof(*pnotifyInfo),
        "devLiSoftCallback::init_record");
    pcallback = callocMustSucceed(1, sizeof(*pcallback),
        "devLiSoftCallback::init_record");
    ppn = callocMustSucceed(1, sizeof(*ppn),
        "devLiSoftCallback::init_record");
    pnotifyInfo->ppn = ppn;
    pnotifyInfo->pcallback = pcallback;
    ppn->usrPvt = prec;
    ppn->paddr = pdbaddr;
    ppn->getCallback = getCallback;
    ppn->doneCallback = doneCallback;
    ppn->requestType = processGetRequest;
    prec->dpvt = pnotifyInfo;
    return 0;
}

static long read_li(longinRecord *prec)
{
    notifyInfo *pnotifyInfo = (notifyInfo *)prec->dpvt;

    if(prec->pact) {
        if(pnotifyInfo->status) {
            recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
            return(pnotifyInfo->status);
        }
        prec->val = pnotifyInfo->value;
        prec->udf = FALSE;
        return(0);
    }
    dbProcessNotify(pnotifyInfo->ppn);
    prec->pact = TRUE;
    return(0);
}

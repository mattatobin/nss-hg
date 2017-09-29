/* -*- Mode: C; tab-width: 8 -*-*/
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef _CMMFT_H_
#define _CMMFT_H_

#include "secasn1.h"

/*
 * These are the enumerations used to distinguish between the different
 * choices available for the CMMFCertOrEncCert structure.
 */
typedef enum {
    cmmfNoCertOrEncCert,
    cmmfCertificate,
    cmmfEncryptedCert
} CMMFCertOrEncCertChoice;

/*
 * This is the enumeration and the corresponding values used to 
 * represent the CMMF type PKIStatus
 */
typedef enum {
    cmmfGranted,
    cmmfGrantedWithMods,
    cmmfRejection,
    cmmfWaiting,
    cmmfRevocationWarning,
    cmmfRevocationNotification,
    cmmfKeyUpdateWarning,
    cmmfNumPKIStatus,
    cmmfNoPKIStatus
} CMMFPKIStatus;

/*
 * These enumerations are used to represent the corresponding values
 * in PKIFailureInfo defined in CMMF.
 */
typedef enum {
    cmmfBadAlg,
    cmmfBadMessageCheck,
    cmmfBadRequest,
    cmmfBadTime,
    cmmfBadCertId,
    cmmfBadDataFormat,
    cmmfWrongAuthority,
    cmmfIncorrectData,
    cmmfMissingTimeStamp,
    cmmfNoFailureInfo
} CMMFPKIFailureInfo;

typedef struct CMMFPKIStatusInfoStr          CMMFPKIStatusInfo;
typedef struct CMMFCertOrEncCertStr          CMMFCertOrEncCert;
typedef struct CMMFCertifiedKeyPairStr       CMMFCertifiedKeyPair;
typedef struct CMMFCertResponseStr           CMMFCertResponse;
typedef struct CMMFCertResponseSeqStr        CMMFCertResponseSeq;
typedef struct CMMFPOPODecKeyChallContentStr CMMFPOPODecKeyChallContent;
typedef struct CMMFChallengeStr              CMMFChallenge;
typedef struct CMMFRandStr                   CMMFRand;
typedef struct CMMFPOPODecKeyRespContentStr  CMMFPOPODecKeyRespContent;
typedef struct CMMFKeyRecRepContentStr       CMMFKeyRecRepContent;
typedef struct CMMFCertRepContentStr         CMMFCertRepContent;

/* Export this so people can call SEC_ASN1EncodeItem instead of having to 
 * write callbacks that are passed in to the high level encode function
 * for CMMFCertRepContent.
 */
extern const SEC_ASN1Template CMMFCertRepContentTemplate[];
extern const SEC_ASN1Template CMMFPOPODecKeyChallContentTemplate[];

#endif /*_CMMFT_H_*/
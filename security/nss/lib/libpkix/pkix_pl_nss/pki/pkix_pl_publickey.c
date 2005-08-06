/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * pkix_pl_publickey.c
 *
 * Certificate Object Functions
 *
 */

#include "pkix_pl_publickey.h"

/* --Private-Cert-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_PublicKey_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the PublicKey
 *  pointed to by "pkixPubKey" and stores it at "pString".
 *
 * PARAMETERS
 *  "pkixPubKey"
 *      Address of PublicKey whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a PublicKey Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_PublicKey_ToString_Helper(
        PKIX_PL_PublicKey *pkixPubKey,
        PKIX_PL_String **pString,
        void *plContext)
{
        SECAlgorithmID algorithm;
        SECOidTag pubKeyTag;
        char *asciiOID = NULL;
        PKIX_Boolean freeAsciiOID = PKIX_FALSE;
        SECItem oidBytes;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_ToString_Helper");
        PKIX_NULLCHECK_THREE(pkixPubKey, pkixPubKey->nssSPKI, pString);

        /*
         * XXX for now, we print out public key algorithm's
         * description - add params and bytes later
         */

        /*
         * If the algorithm OID is known to NSS,
         * we print out the ASCII description that is
         * registered with NSS. Otherwise, if unknown,
         * we print out the OID numbers (eg. "1.2.840.3")
         */

        algorithm = pkixPubKey->nssSPKI->algorithm;

        PKIX_PUBLICKEY_DEBUG("\t\tCalling SECOID_GetAlgorithmTag).\n");
        pubKeyTag = SECOID_GetAlgorithmTag(&algorithm);
        if (pubKeyTag != SEC_OID_UNKNOWN){
                PKIX_PUBLICKEY_DEBUG
                        ("\t\tCalling SECOID_FindOIDTagDescription).\n");
                asciiOID = (char *)SECOID_FindOIDTagDescription(pubKeyTag);
                if (!asciiOID){
                        PKIX_ERROR("SECOID_FindOIDTag Description failed");
                }
        } else { /* pubKeyTag == SEC_OID_UNKNOWN */
                oidBytes = algorithm.algorithm;
                PKIX_CHECK(pkix_pl_oidBytes2Ascii
                            (&oidBytes, &asciiOID, plContext),
                            "pkix_pl_oidBytes2Ascii failed");
                freeAsciiOID = PKIX_TRUE;
        }

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII, (void *)asciiOID, 0, pString, plContext),
                "Unable to create pString");

cleanup:

        /*
         * we only free asciiOID if it was malloc'ed by pkix_pl_oidBytes2Ascii
         */
        if (freeAsciiOID){
                PKIX_FREE(asciiOID);
        }

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_DestroySPKI
 * DESCRIPTION:
 *  Frees all memory associated with the CERTSubjectPublicKeyInfo pointed to
 *  by "nssSPKI".
 * PARAMETERS
 *  "nssSPKI"
 *      Address of CERTSubjectPublicKeyInfo. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an Object Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_DestroySPKI(
        CERTSubjectPublicKeyInfo *nssSPKI,
        void *plContext)
{
        PKIX_ENTER(PUBLICKEY, "pkix_pl_DestroySPKI");

        PKIX_NULLCHECK_ONE(nssSPKI);

        PKIX_PUBLICKEY_DEBUG("\t\tCalling SECOID_DestroyAlgorithmID).\n");
        SECOID_DestroyAlgorithmID(&nssSPKI->algorithm, PKIX_FALSE);

        PKIX_PUBLICKEY_DEBUG("\t\tCalling SECITEM_FreeItem).\n");
        SECITEM_FreeItem(&nssSPKI->subjectPublicKey, PKIX_FALSE);

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_PublicKey *pubKey = NULL;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_Destroy");

        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_PUBLICKEY_TYPE, plContext),
                    "Object is not a PublicKey");

        pubKey = (PKIX_PL_PublicKey *)object;

        PKIX_NULLCHECK_ONE(pubKey->nssSPKI);

        PKIX_CHECK(pkix_pl_DestroySPKI(pubKey->nssSPKI, plContext),
                    "pkix_pl_DestroySPKI failed");

        PKIX_FREE(pubKey->nssSPKI);

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_PublicKey *pkixPubKey = NULL;
        PKIX_PL_String *pubKeyString = NULL;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_toString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_PUBLICKEY_TYPE, plContext),
                    "Object is not a PublicKey");

        pkixPubKey = (PKIX_PL_PublicKey *)object;

        PKIX_CHECK(pkix_pl_PublicKey_ToString_Helper
                    (pkixPubKey, &pubKeyString, plContext),
                    "pkix_pl_PublicKey_ToString_Helper failed");

        *pString = pubKeyString;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_PublicKey *pkixPubKey = NULL;
        SECItem algOID;
        SECItem algParams;
        SECItem nssPubKey;
        PKIX_UInt32 algOIDHash;
        PKIX_UInt32 algParamsHash;
        PKIX_UInt32 pubKeyHash;
        PKIX_UInt32 fullHash;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_PUBLICKEY_TYPE, plContext),
                    "Object is not a PublicKey");

        pkixPubKey = (PKIX_PL_PublicKey *)object;

        PKIX_NULLCHECK_ONE(pkixPubKey->nssSPKI);

        algOID = pkixPubKey->nssSPKI->algorithm.algorithm;
        algParams = pkixPubKey->nssSPKI->algorithm.parameters;
        nssPubKey = pkixPubKey->nssSPKI->subjectPublicKey;

        PKIX_CHECK(pkix_hash
                    (algOID.data, algOID.len, &algOIDHash, plContext),
                    "pkix_hash failed");

        PKIX_CHECK(pkix_hash
                    (algParams.data, algParams.len, &algParamsHash, plContext),
                    "pkix_hash failed");

        PKIX_CHECK(pkix_hash
                    (nssPubKey.data, nssPubKey.len, &pubKeyHash, plContext),
                    "pkix_hash failed");

        fullHash = algOIDHash + algParamsHash + pubKeyHash;

        *pHashcode = pubKeyHash;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}


/*
 * FUNCTION: pkix_pl_PublicKey_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_PublicKey_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_PublicKey *firstPKIXPubKey = NULL;
        PKIX_PL_PublicKey *secondPKIXPubKey = NULL;
        CERTSubjectPublicKeyInfo *firstSPKI = NULL;
        CERTSubjectPublicKeyInfo *secondSPKI = NULL;
        SECComparison cmpResult;
        PKIX_UInt32 secondType;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a PublicKey */
        PKIX_CHECK(pkix_CheckType(firstObject, PKIX_PUBLICKEY_TYPE, plContext),
                    "FirstObject argument is not a PublicKey");

        /*
         * Since we know firstObject is a PublicKey, if both references are
         * identical, they must be equal
         */
        if (firstObject == secondObject){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a PublicKey, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    "Could not get type of second argument");
        if (secondType != PKIX_PUBLICKEY_TYPE) goto cleanup;

        firstPKIXPubKey = ((PKIX_PL_PublicKey *)firstObject);
        secondPKIXPubKey = (PKIX_PL_PublicKey *)secondObject;

        firstSPKI = firstPKIXPubKey->nssSPKI;
        secondSPKI = secondPKIXPubKey->nssSPKI;

        PKIX_NULLCHECK_TWO(firstSPKI, secondSPKI);

        PKIX_PL_NSSCALLRV(PUBLICKEY, cmpResult, SECOID_CompareAlgorithmID,
                        (&firstSPKI->algorithm, &secondSPKI->algorithm));

        if (cmpResult == SECEqual){
                PKIX_PUBLICKEY_DEBUG("\t\tCalling SECITEM_CompareItem).\n");
                cmpResult = SECITEM_CompareItem
                                (&firstSPKI->subjectPublicKey,
                                &secondSPKI->subjectPublicKey);
        }

        *pResult = (cmpResult == SECEqual)?PKIX_TRUE:PKIX_FALSE;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: pkix_pl_PublicKey_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_PUBLICKEY_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_PublicKey_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(PUBLICKEY, "pkix_pl_PublicKey_RegisterSelf");

        entry.description = "PublicKey";
        entry.destructor = pkix_pl_PublicKey_Destroy;
        entry.equalsFunction = pkix_pl_PublicKey_Equals;
        entry.hashcodeFunction = pkix_pl_PublicKey_Hashcode;
        entry.toStringFunction = pkix_pl_PublicKey_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;
        systemClasses[PKIX_PUBLICKEY_TYPE] = entry;

        PKIX_RETURN(PUBLICKEY);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_PublicKey_NeedsDSAParameters
 *              (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_PublicKey_NeedsDSAParameters(
        PKIX_PL_PublicKey *pubKey,
        PKIX_Boolean *pNeedsParams,
        void *plContext)
{
        CERTSubjectPublicKeyInfo *nssSPKI = NULL;
        KeyType pubKeyType;
        PKIX_Boolean needsParams = PKIX_FALSE;

        PKIX_ENTER(PUBLICKEY, "PKIX_PL_PublicKey_NeedsDSAParameters");
        PKIX_NULLCHECK_TWO(pubKey, pNeedsParams);

        nssSPKI = pubKey->nssSPKI;

        PKIX_PUBLICKEY_DEBUG("\t\tCalling CERT_GetCertKeyType).\n");
        pubKeyType = CERT_GetCertKeyType(nssSPKI);
        if (!pubKeyType){
                PKIX_ERROR("pubKeyType is nullKey");
        }

        if ((pubKeyType == dsaKey) &&
            (nssSPKI->algorithm.parameters.len == 0)){
                needsParams = PKIX_TRUE;
        }

        *pNeedsParams = needsParams;

cleanup:

        PKIX_RETURN(PUBLICKEY);
}

/*
 * FUNCTION: PKIX_PL_PublicKey_MakeInheritedDSAPublicKey
 *              (see comments in pkix_pl_pki.h)
 */
PKIX_Error *
PKIX_PL_PublicKey_MakeInheritedDSAPublicKey(
        PKIX_PL_PublicKey *firstKey,
        PKIX_PL_PublicKey *secondKey,
        PKIX_PL_PublicKey **pResultKey,
        void *plContext)
{
        CERTSubjectPublicKeyInfo *firstSPKI = NULL;
        CERTSubjectPublicKeyInfo *secondSPKI = NULL;
        CERTSubjectPublicKeyInfo *thirdSPKI = NULL;
        PKIX_PL_PublicKey *resultKey = NULL;
        KeyType firstPubKeyType;
        KeyType secondPubKeyType;
        SECStatus rv;

        PKIX_ENTER(PUBLICKEY, "PKIX_PL_PublicKey_MakeInheritedDSAPublicKey");
        PKIX_NULLCHECK_THREE(firstKey, secondKey, pResultKey);
        PKIX_NULLCHECK_TWO(firstKey->nssSPKI, secondKey->nssSPKI);

        firstSPKI = firstKey->nssSPKI;
        secondSPKI = secondKey->nssSPKI;

        PKIX_PUBLICKEY_DEBUG("\t\tCalling CERT_GetCertKeyType).\n");
        firstPubKeyType = CERT_GetCertKeyType(firstSPKI);
        if (!firstPubKeyType){
                PKIX_ERROR("firstPubKeyType is nullKey");
        }

        PKIX_PUBLICKEY_DEBUG("\t\tCalling CERT_GetCertKeyType).\n");
        secondPubKeyType = CERT_GetCertKeyType(secondSPKI);
        if (!secondPubKeyType){
                PKIX_ERROR("secondPubKeyType is nullKey");
        }

        if ((firstPubKeyType == dsaKey) &&
            (firstSPKI->algorithm.parameters.len == 0)){
                if (secondPubKeyType != dsaKey) {
                        PKIX_ERROR("Second key is not a DSA public key");
                } else if (secondSPKI->algorithm.parameters.len == 0) {
                        PKIX_ERROR
                                ("Second key is a DSA public key"
                                "but has null parameters");
                } else {
                        PKIX_CHECK(PKIX_PL_Calloc
                                    (1,
                                    sizeof (CERTSubjectPublicKeyInfo),
                                    (void **)&thirdSPKI,
                                    plContext),
                                    "PKIX_PL_Calloc failed");

                        PKIX_PUBLICKEY_DEBUG
                                ("\t\tCalling"
                                "SECKEY_CopySubjectPublicKeyInfo).\n");
                        rv = SECKEY_CopySubjectPublicKeyInfo
                                (NULL, thirdSPKI, firstSPKI);
                        if (rv != SECSuccess) {
                            PKIX_ERROR
                                    ("SECKEY_CopySubjectPublicKeyInfo failed");
                        }

                        PKIX_PUBLICKEY_DEBUG
                                ("\t\tCalling SECITEM_CopyItem).\n");
                        rv = SECITEM_CopyItem(NULL,
                                            &thirdSPKI->algorithm.parameters,
                                            &secondSPKI->algorithm.parameters);

                        if (rv != SECSuccess) {
                                PKIX_ERROR("SECITEM_CopyItem failed");
                        }

                        /* create a PKIX_PL_PublicKey object */
                        PKIX_CHECK(PKIX_PL_Object_Alloc
                                    (PKIX_PUBLICKEY_TYPE,
                                    sizeof (PKIX_PL_PublicKey),
                                    (PKIX_PL_Object **)&resultKey,
                                    plContext),
                                    "Could not create object");

                        /* populate the SPKI field */
                        resultKey->nssSPKI = thirdSPKI;
                        *pResultKey = resultKey;
                }
        } else {
                *pResultKey = NULL;
        }

cleanup:

        if (thirdSPKI && PKIX_ERROR_RECEIVED){
                PKIX_CHECK(pkix_pl_DestroySPKI(thirdSPKI, plContext),
                            "pkix_pl_DestroySPKI failed");
                PKIX_FREE(thirdSPKI);
        }

        PKIX_RETURN(PUBLICKEY);
}

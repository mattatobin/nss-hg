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
 * pkix_pl_bigint.c
 *
 * BigInt Object Functions
 *
 */

#include "pkix_pl_bigint.h"

/* --Private-Big-Int-Functions------------------------------------ */

/*
 * FUNCTION: pkix_pl_BigInt_Comparator
 * (see comments for PKIX_PL_ComparatorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_BigInt_Comparator(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pResult,
        void *plContext)
{
        PKIX_PL_BigInt *firstBigInt = NULL;
        PKIX_PL_BigInt *secondBigInt = NULL;
        char *firstPtr = NULL;
        char *secondPtr = NULL;
        PKIX_UInt32 firstLen, secondLen;

        PKIX_ENTER(BIGINT, "pkix_pl_BigInt_Comparator");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        PKIX_CHECK(pkix_CheckTypes
                    (firstObject, secondObject, PKIX_BIGINT_TYPE, plContext),
                    "Arguments are not BigInts");

        /* It's safe to cast */
        firstBigInt = (PKIX_PL_BigInt*)firstObject;
        secondBigInt = (PKIX_PL_BigInt*)secondObject;

        *pResult = 0;
        firstPtr = firstBigInt->dataRep;
        secondPtr = secondBigInt->dataRep;
        firstLen = firstBigInt->length;
        secondLen = secondBigInt->length;

        if (firstLen < secondLen) {
                *pResult = -1;
        } else if (firstLen > secondLen) {
                *pResult = 1;
        } else if (firstLen == secondLen) {
                PKIX_BIGINT_DEBUG("\t\tCalling PORT_Memcmp).\n");
                *pResult = PORT_Memcmp(firstPtr, secondPtr, firstLen);
        }

cleanup:

        PKIX_RETURN(BIGINT);
}

/*
 * FUNCTION: pkix_pl_BigInt_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_BigInt_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_BigInt *bigInt = NULL;

        PKIX_ENTER(BIGINT, "pkix_pl_BigInt_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BIGINT_TYPE, plContext),
                    "Object is not a BigInt");

        bigInt = (PKIX_PL_BigInt*)object;

        PKIX_FREE(bigInt->dataRep);
        bigInt->dataRep = NULL;
        bigInt->length = 0;

cleanup:

        PKIX_RETURN(BIGINT);
}


/*
 * FUNCTION: pkix_pl_BigInt_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_BigInt_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_BigInt *bigInt = NULL;
        char *outputText = NULL;
        PKIX_UInt32 i, j, lengthChars;

        PKIX_ENTER(BIGINT, "pkix_pl_BigInt_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BIGINT_TYPE, plContext),
                    "Object is not a BigInt");

        bigInt = (PKIX_PL_BigInt*)object;

        /* number of chars = 2 * (number of bytes) + null terminator */
        lengthChars = (bigInt->length * 2) + 1;

        PKIX_CHECK(PKIX_PL_Malloc
                    (lengthChars, (void **)&outputText, plContext),
                    "PKIX_PL_Malloc failed");

        for (i = 0, j = 0; i < bigInt->length; i += 1, j += 2){
                outputText[j] = pkix_i2hex((*(bigInt->dataRep+i) & 0xf0) >> 4);
                outputText[j+1] = pkix_i2hex(*(bigInt->dataRep+i) & 0x0f);
        }

        outputText[lengthChars-1] = '\0';

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII,
                    outputText,
                    0,
                    pString,
                    plContext),
                    "PKIX_PL_String_Create failed");

cleanup:

        PKIX_FREE(outputText);

        PKIX_RETURN(BIGINT);
}


/*
 * FUNCTION: pkix_pl_BigInt_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_BigInt_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_BigInt *bigInt = NULL;

        PKIX_ENTER(BIGINT, "pkix_pl_BigInt_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_BIGINT_TYPE, plContext),
                    "Object is not a BigInt");

        bigInt = (PKIX_PL_BigInt*)object;

        PKIX_CHECK(pkix_hash
                    ((void *)bigInt->dataRep,
                    bigInt->length,
                    pHashcode,
                    plContext),
                    "pkix_hash failed");

cleanup:

        PKIX_RETURN(BIGINT);
}

/*
 * FUNCTION: pkix_pl_BigInt_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_BigInt_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Int32 cmpResult = 0;

        PKIX_ENTER(BIGINT, "pkix_pl_BigInt_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        PKIX_CHECK(pkix_CheckType(first, PKIX_BIGINT_TYPE, plContext),
                    "First Argument is not a BigInt");

        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    "Could not get type of second argument");

        *pResult = PKIX_FALSE;

        if (secondType != PKIX_BIGINT_TYPE) goto cleanup;

        PKIX_CHECK(pkix_pl_BigInt_Comparator
                    (first, second, &cmpResult, plContext),
                    "pkix_pl_BigInt_Comparator failed");

        *pResult = (cmpResult == 0);

cleanup:

        PKIX_RETURN(BIGINT);
}

/*
 * FUNCTION: pkix_pl_BigInt_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_BIGINT_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_BigInt_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(BIGINT, "pkix_pl_BigInt_RegisterSelf");

        entry.description = "BigInt";
        entry.destructor = pkix_pl_BigInt_Destroy;
        entry.equalsFunction = pkix_pl_BigInt_Equals;
        entry.hashcodeFunction = pkix_pl_BigInt_Hashcode;
        entry.toStringFunction = pkix_pl_BigInt_ToString;
        entry.comparator = pkix_pl_BigInt_Comparator;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_BIGINT_TYPE] = entry;

        PKIX_RETURN(BIGINT);
}

/*
 * FUNCTION: pkix_pl_BigInt_CreateWithBytes
 * DESCRIPTION:
 *
 *  Creates a new BigInt of size "length" representing the array of bytes
 *  pointed to by "bytes" and stores it at "pBigInt". The caller should make
 *  sure that the first byte is not 0x00 (unless it is the the only byte).
 *  This function does not do that checking.
 *
 *  Once created, a PKIX_PL_BigInt object is immutable.
 *
 * PARAMETERS:
 *  "bytes"
 *      Address of array of bytes. Must be non-NULL.
 *  "length"
 *      Length of the array. Must be non-zero.
 *  "pBigInt"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext" - Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_BigInt_CreateWithBytes(
        char *bytes,
        PKIX_UInt32 length,
        PKIX_PL_BigInt **pBigInt,
        void *plContext)
{
        PKIX_PL_BigInt *bigInt = NULL;

        PKIX_ENTER(BIGINT, "pkix_pl_BigInt_CreateWithBytes");
        PKIX_NULLCHECK_TWO(pBigInt, bytes);

        if (length == 0) {
                PKIX_ERROR("BigInt length 0 is invalid")
        }

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_BIGINT_TYPE,
                sizeof (PKIX_PL_BigInt),
                (PKIX_PL_Object **)&bigInt,
                plContext),
                "Could not create object");

        PKIX_CHECK(PKIX_PL_Malloc
                    (length, (void **)&(bigInt->dataRep), plContext),
                    "PKIX_PL_Malloc failed");

        PKIX_BIGINT_DEBUG("\t\tCalling PORT_Memcpy).\n");
        (void) PORT_Memcpy(bigInt->dataRep, bytes, length);

        bigInt->length = length;

        *pBigInt = bigInt;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(bigInt);
        }

        PKIX_RETURN(BIGINT);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: PKIX_PL_BigInt_Create (see comments in pkix_pl_system.h)
 */
PKIX_Error *
PKIX_PL_BigInt_Create(
        PKIX_PL_String *stringRep,
        PKIX_PL_BigInt **pBigInt,
        void *plContext)
{
        PKIX_PL_BigInt *bigInt = NULL;
        char *asciiString = NULL;
        PKIX_UInt32 lengthBytes;
        PKIX_UInt32 lengthString;
        PKIX_UInt32 i;
        char currChar;

        PKIX_ENTER(BIGINT, "PKIX_PL_BigInt_Create");
        PKIX_NULLCHECK_TWO(pBigInt, stringRep);

        PKIX_CHECK(PKIX_PL_String_GetEncoded
                (stringRep,
                PKIX_ESCASCII,
                (void **)&asciiString,
                &lengthString,
                plContext),
                "PKIX_PL_String_GetEncoded failed");

        if ((lengthString == 0) || ((lengthString % 2) != 0)){
                PKIX_ERROR("Source string has invalid length");
        }

        if (lengthString != 2){
                if ((asciiString[0] == '0') && (asciiString[1] == '0')){
                        PKIX_ERROR("First DoubleHex MUST NOT be 00");
                }
        }

        for (i = 0; i < lengthString; i++) {
                currChar = asciiString[i];
                if (!PKIX_ISXDIGIT(currChar)){
                        PKIX_ERROR("Invalid character in BigInt");
                }
        }

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_BIGINT_TYPE,
                sizeof (PKIX_PL_BigInt),
                (PKIX_PL_Object **)&bigInt,
                plContext),
                "Could not create object");

        /* number of bytes = 0.5 * (number of chars) */
        lengthBytes = lengthString/2;

        PKIX_CHECK(PKIX_PL_Malloc
                    (lengthBytes, (void **)&(bigInt->dataRep), plContext),
                    "PKIX_PL_Malloc failed");

        for (i = 0; i < lengthString; i += 2){
                (bigInt->dataRep)[i/2] =
                        (pkix_hex2i(asciiString[i])<<4) |
                        pkix_hex2i(asciiString[i+1]);
        }

        bigInt->length = lengthBytes;

        *pBigInt = bigInt;

cleanup:

        PKIX_FREE(asciiString);

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(bigInt);
        }

        PKIX_RETURN(BIGINT);
}

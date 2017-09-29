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
/*
** secutil.c - various functions used by security stuff
**
*/

#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"
#include "prerror.h"
#include "prprf.h"
#include "plgetopt.h"

#include "secutil.h"
#include "secpkcs7.h"
#include <sys/stat.h>
#include <stdarg.h>

#ifdef XP_UNIX
#include <unistd.h>
#endif

/* for SEC_TraverseNames */
#include "cert.h"
#include "certt.h"
#include "certdb.h"
#include "cdbhdl.h"

#include "secmod.h"
#include "pk11func.h"
#include "secoid.h"
#include "blapi.h"	/* for RNG_RNGInit */

static char consoleName[] =  {
#ifdef XP_UNIX
    "/dev/tty"
#else
    "CON:"
#endif
};

char *
SECU_GetString(int16 error_number)
{

    static char errString[80];
    sprintf(errString, "Unknown error string (%d)", error_number);
    return errString;
}

void 
SECU_PrintError(char *progName, char *msg, ...)
{
    va_list args;
    PRErrorCode err = PORT_GetError();
    const char * errString = SECU_Strerror(err);

    va_start(args, msg);

    fprintf(stderr, "%s: ", progName);
    vfprintf(stderr, msg, args);
    if (errString != NULL && PORT_Strlen(errString) > 0)
	fprintf(stderr, ": %s\n", errString);
    else
	fprintf(stderr, "\n");

    va_end(args);
}

void
SECU_PrintSystemError(char *progName, char *msg, ...)
{
    va_list args;

    va_start(args, msg);
    fprintf(stderr, "%s: ", progName);
    vfprintf(stderr, msg, args);
    fprintf(stderr, ": %s\n", strerror(errno));
    va_end(args);
}

static void
secu_ClearPassword(char *p)
{
    if (p) {
	PORT_Memset(p, 0, PORT_Strlen(p));
	PORT_Free(p);
    }
}

static SECItem *
secu_GetZeroLengthPassword(SECKEYKeyDBHandle *handle)
{
    SECItem *pwitem;
    SECStatus rv;

    /* hash the empty string as a password */
    pwitem = SECKEY_DeriveKeyDBPassword(handle, "");
    if (pwitem == NULL) {
	return NULL;
    }

    /* check to see if this is the right password */
    rv = SECKEY_CheckKeyDBPassword(handle, pwitem);
    if (rv == SECFailure) {
	return NULL;
    }

    return pwitem;
}

char *
SECU_GetPasswordString(void *arg, char *prompt)
{
#ifndef _WINDOWS
    char *p = NULL;
    FILE *input, *output;

    /* open terminal */
    input = fopen(consoleName, "r");
    if (input == NULL) {
	fprintf(stderr, "Error opening input terminal for read\n");
	return NULL;
    }

    output = fopen(consoleName, "w");
    if (output == NULL) {
	fprintf(stderr, "Error opening output terminal for write\n");
	return NULL;
    }

    p = SEC_GetPassword (input, output, prompt, SEC_BlindCheckPassword);
        

    fclose(input);
    fclose(output);

    return p;

#else
    /* Win32 version of above. opening the console may fail
       on windows95, and certainly isn't necessary.. */

    char *p = NULL;

    p = SEC_GetPassword (stdin, stdout, prompt, SEC_BlindCheckPassword);
    return p;

#endif
}

SECItem *
SECU_GetPassword(void *arg, SECKEYKeyDBHandle *handle)
{
    char *p = NULL;
    SECStatus rv;
    SECItem *pwitem;

    /* Check to see if zero length password or not */
    pwitem = secu_GetZeroLengthPassword(handle);
    if (pwitem) {
	return pwitem;
    }

    p = SECU_GetPasswordString(arg,"Password: ");

    /* Check to see if zero length password or not */
    pwitem = secu_GetZeroLengthPassword(handle);
    if (pwitem) {
	return pwitem;
    }
    /* hash the password */
    pwitem = SECKEY_DeriveKeyDBPassword(handle, p);
    
    /* clear out the password strings */
    secu_ClearPassword(p);
    

    if ( pwitem == NULL ) {
	fprintf(stderr, "Error hashing password\n");
	return NULL;
    }

    /* confirm the password */
    rv = SECKEY_CheckKeyDBPassword(handle, pwitem);
    if (rv) {
	fprintf(stderr, "Sorry\n");
	SECITEM_ZfreeItem(pwitem, PR_TRUE);
	return NULL;
    }
    
    return pwitem;
}

/*
 *  p a s s w o r d _ h a r d c o d e 
 *
 *  A function to use the password passed in the -f(pwfile) argument
 *  of the command line.  
 *  After use once, null it out otherwise PKCS11 calls us forever.?
 *
 */
char *
SECU_FilePasswd(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    unsigned char phrase[200];
    PRFileDesc *fd;
    PRInt32 nb;
    char *pwFile = arg;
    int i;

    if (!pwFile)
	return 0;

    if (retry) {
	return 0;  /* no good retrying - the files contents will be the same */
    } 
 
    fd = PR_Open(pwFile, PR_RDONLY, 0);
    if (!fd) {
	fprintf(stderr, "No password file \"%s\" exists.\n", pwFile);
	return NULL;
    }

    nb = PR_Read(fd, phrase, sizeof(phrase));
  
    PR_Close(fd);
    /* handle the Windows EOL case */
    i = 0;
    while (phrase[i] != '\r' && phrase[i] != '\n' && i < nb) i++;
    phrase[i] = '\0';
    if (nb == 0) {
	fprintf(stderr,"password file contains no data\n");
	return NULL;
    } else {
	return (char*) PORT_Strdup((char*)phrase);
    }
    return (char*) PORT_Strdup((char*)phrase);
}

char *
SECU_GetModulePassword(PK11SlotInfo *slot, PRBool retry, void *arg) 
{
    char prompt[255];
    secuPWData *pwdata = arg;
    secuPWData pwnull = { PW_NONE, 0 };

    if (arg == NULL)
	pwdata = &pwnull;

    if (retry && pwdata->source != PW_NONE) {
	PR_fprintf(PR_STDERR, "incorrect password entered at command line.\n");
    	return NULL;
    }

    sprintf(prompt, "Enter Password or Pin for \"%s\":",
	    PK11_GetTokenName(slot));

    switch (pwdata->source) {
    case PW_NONE:
	return SECU_GetPasswordString(NULL, prompt);
    case PW_FROMFILE:
	return SECU_FilePasswd(slot, retry, pwdata->data);
    case PW_PLAINTEXT:
	return PL_strdup(arg);
    default:
	break;
    }

    return NULL;
}

char *
secu_InitSlotPassword(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    char *p0 = NULL;
    char *p1 = NULL;
    FILE *input, *output;
    secuPWData *pwdata = arg;

    if (pwdata->source == PW_NONE) {
	/* open terminal */
#ifdef _WINDOWS
	input = stdin;
#else
	input = fopen(consoleName, "r");
#endif
	if (input == NULL) {
	    PR_fprintf(PR_STDERR, "Error opening input terminal for read\n");
	    return NULL;
	}

	/* we have no password, so initialize database with one */
	PR_fprintf(PR_STDERR, 
                   "In order to finish creating your database, you\n");
	PR_fprintf(PR_STDERR, "must enter a password which will be used to\n");
	PR_fprintf(PR_STDERR, "encrypt this key and any future keys.\n\n");
	PR_fprintf(PR_STDERR, 
	           "The password must be at least 8 characters long,\n");
	PR_fprintf(PR_STDERR, "and must contain at least one non-alphabetic ");
	PR_fprintf(PR_STDERR, "character.\n\n");
    } else if (pwdata->source == PW_FROMFILE) {
	input = fopen(pwdata->data, "r");
	if (input == NULL) {
	    PR_fprintf(PR_STDERR, "Error opening \"%s\" for read\n",
	                           pwdata->data);
	    return NULL;
	}
    } else {
	p0 = PL_strdup(pwdata->data);
    }

    output = fopen(consoleName, "w");
    if (output == NULL) {
	PR_fprintf(PR_STDERR, "Error opening output terminal for write\n");
	return NULL;
    }


    for (;;) {
	if (!p0) {
	    p0 = SEC_GetPassword(input, output, "Enter new password: ",
			         SEC_BlindCheckPassword);
	}
	if (pwdata->source == PW_NONE) {
	    p1 = SEC_GetPassword(input, output, "Re-enter password: ",
				 SEC_BlindCheckPassword);
	}
	if (pwdata->source != PW_NONE || (PORT_Strcmp(p0, p1) == 0)) {
	    break;
	}
	PR_fprintf(PR_STDERR, "Passwords do not match. Try again.\n");
    }
        
    /* clear out the duplicate password string */
    secu_ClearPassword(p1);
    
    fclose(input);
    fclose(output);

    return p0;
}

SECStatus
SECU_ChangePW(PK11SlotInfo *slot, char *passwd, char *pwFile)
{
    SECStatus rv;
    secuPWData pwdata, newpwdata;
    char *oldpw = NULL, *newpw = NULL;

    if (passwd) {
	pwdata.source = PW_PLAINTEXT;
	pwdata.data = passwd;
    } else if (pwFile) {
	pwdata.source = PW_FROMFILE;
	pwdata.data = pwFile;
    } else {
	pwdata.source = PW_NONE;
	pwdata.data = NULL;
    }

    if (PK11_NeedUserInit(slot)) {
	newpw = secu_InitSlotPassword(slot, PR_FALSE, &pwdata);
	rv = PK11_InitPin(slot, (char*)NULL, newpw);
	goto done;
    }

    for (;;) {
	oldpw = SECU_GetModulePassword(slot, PR_FALSE, &pwdata);

	if (PK11_CheckUserPassword(slot, oldpw) != SECSuccess) {
	    if (pwdata.source == PW_NONE) {
		PR_fprintf(PR_STDERR, "Invalid password.  Try again.\n");
	    } else {
		PR_fprintf(PR_STDERR, "Invalid password.\n");
		PORT_Memset(oldpw, 0, PL_strlen(oldpw));
		PORT_Free(oldpw);
		return SECFailure;
	    }
	} else
	    break;

	PORT_Free(oldpw);
    }

    newpwdata.source = PW_NONE;
    newpwdata.data = NULL;

    newpw = secu_InitSlotPassword(slot, PR_FALSE, &newpwdata);

    if (PK11_ChangePW(slot, oldpw, newpw) != SECSuccess) {
	PR_fprintf(PR_STDERR, "Failed to change password.\n");
	return SECFailure;
    }

    PORT_Memset(oldpw, 0, PL_strlen(oldpw));
    PORT_Free(oldpw);

    PR_fprintf(PR_STDOUT, "Password changed successfully.\n");

done:
    PORT_Memset(newpw, 0, PL_strlen(newpw));
    PORT_Free(newpw);
    return SECSuccess;
}

struct matchobj {
    SECItem index;
    char *nname;
    PRBool found;
};


static SECStatus
secu_match_nickname(DBT *k, DBT *d, void *pdata)
{
    struct matchobj *match;
    unsigned char *buf;
    char *nname;
    int nnlen;

    match = (struct matchobj *)pdata;
    buf = (unsigned char *)d->data;

    if (match->found == PR_TRUE)
	return SECSuccess;

    nnlen = buf[2];
    nname = (char *)(buf + 3 + buf[1]);
    if (PORT_Strncmp(match->nname, nname, nnlen) == 0) {
	match->index.len = k->size;
	match->index.data = PORT_ZAlloc(k->size + 1);
	PORT_Memcpy(match->index.data, k->data, k->size);
	match->found = PR_TRUE;
    }
    return SECSuccess;
}

SECItem *
SECU_GetKeyIDFromNickname(char *name)
{
    struct matchobj match;
    SECKEYKeyDBHandle *handle;
    SECItem *keyid;

    match.nname = name;
    match.found = PR_FALSE;

    handle = SECKEY_GetDefaultKeyDB();

    SECKEY_TraverseKeys(handle, secu_match_nickname, &match);

    if (match.found == PR_FALSE)
	return NULL;

    keyid = SECITEM_DupItem(&match.index);
    return keyid;
}

PRBool
SECU_CheckKeyNameExists(SECKEYKeyDBHandle *handle, char *nickname)
{
    SECItem *keyid;

    keyid = SECU_GetKeyIDFromNickname(nickname);
    if(keyid == NULL)
	return PR_FALSE;
    SECITEM_FreeItem(keyid, PR_TRUE);
    return PR_TRUE;
}

SECKEYPrivateKey *
SECU_FindPrivateKeyFromNickname(char *name)
{
    SECItem *         keyid;
    SECKEYPrivateKey *key;
    PK11SlotInfo *    slot = PK11_GetInternalKeySlot();
    SECStatus         rv;

    keyid = SECU_GetKeyIDFromNickname(name);
    if (keyid == NULL)
	return NULL;

    PK11_SetPasswordFunc(SECU_GetModulePassword);
    if(PK11_NeedLogin(slot) && !PK11_IsLoggedIn(slot,NULL)) {
	rv = PK11_DoPassword(slot, PR_TRUE, NULL);
	if (rv != SECSuccess)
	    return NULL;
    }

    key = PK11_FindKeyByKeyID(slot, keyid, NULL);
    SECITEM_FreeItem(keyid, PR_TRUE);
    return key;
}

SECKEYLowPrivateKey *
SECU_FindLowPrivateKeyFromNickname(char *name)
{
    SECItem *keyID;
    SECKEYLowPrivateKey *key;

    keyID = SECU_GetKeyIDFromNickname(name);
    if (keyID == NULL)
	return NULL;

    key = SECKEY_FindKeyByPublicKey(SECKEY_GetDefaultKeyDB(), keyID,
				    SECU_GetPassword, NULL);
    SECITEM_FreeItem(keyID, PR_TRUE);
    return key;
}

SECStatus
SECU_DeleteKeyByName(SECKEYKeyDBHandle *handle, char *nickname)
{
    SECItem *keyID = NULL;
    SECStatus rv;

    keyID = SECU_GetKeyIDFromNickname(nickname);
    if (keyID == NULL)
	return SECFailure;

    rv = SECKEY_DeleteKey(handle, keyID);
    SECITEM_FreeItem(keyID, PR_TRUE);

    return rv;
}

SECKEYLowPrivateKey *
SECU_GetPrivateKey(SECKEYKeyDBHandle *handle, char *nickname)
{
    return SECU_FindLowPrivateKeyFromNickname(nickname);
}

SECStatus
SECU_ChangeKeyDBPassword(SECKEYKeyDBHandle *handle)
{
    static SECItem *newpwitem, *oldpwitem;
    char *p0 = 0;
    char *p1 = 0;
    int isTTY;
    SECStatus rv;
    int failed = 0;
    FILE *input, *output;
    PRBool newdb = PR_FALSE;

    if (SECKEY_HasKeyDBPassword(handle) == SECFailure) {
	fprintf(stderr, "Database not initialized.  Setting password.\n");
	newdb = PR_TRUE;
    }
    
    /* check for password file */
    /*
    if (newdb && pwFile != NULL) {
	p0 = SECU_FilePasswd(NULL, 0, NULL);
	goto pwfinish;
    }
    */

    /* check if old password is empty string */
    oldpwitem = secu_GetZeroLengthPassword(handle);

    /* open terminal */
#ifdef _WINDOWS
    input = stdin;
#else
    input = fopen(consoleName, "r");
    if (input == NULL) {
	fprintf(stderr, "Error opening input terminal\n");
	return SECFailure;
    }
#endif

    output = fopen(consoleName, "w");
    if (output == NULL) {
	fprintf(stderr, "Error opening output terminal\n");
	return SECFailure;
    }

    /* if old password is not zero length, ask for new password */
    if ((newdb == PR_FALSE) && (oldpwitem == NULL)) {
	p0 = SEC_GetPassword(input, output, "Old Password: ",
			   SEC_BlindCheckPassword);

	oldpwitem = SECKEY_DeriveKeyDBPassword(handle, p0);
	secu_ClearPassword(p0);

	if (oldpwitem == NULL) {
	    fprintf(stderr, "Error hashing password\n");
	    fclose(input);
	    fclose(output);
	    return SECFailure;
	}

	rv = SECKEY_CheckKeyDBPassword(handle, oldpwitem);
	if (rv) {
	    fprintf(stderr, "Sorry\n");
	    SECITEM_ZfreeItem(oldpwitem, PR_TRUE);
	    fclose(input);
	    fclose(output);
	    return SECFailure;
	}
    }

    isTTY = isatty(0);
    for (;;) {
	p0 = SEC_GetPassword(input, output, "Enter new password: ",
				 SEC_BlindCheckPassword);
	if (isTTY) {
	    p1 = SEC_GetPassword(input, output, "Re-enter password: ",
				 SEC_BlindCheckPassword);
	}
	
	if (!isTTY || ( PORT_Strcmp(p0, p1) == 0) ) {
	    break;
	}
	fprintf(stderr, "Passwords do not match. Try again.\n");
    }
    
    newpwitem = SECKEY_DeriveKeyDBPassword(handle, p0);

    /*
    fclose(input);
    fclose(output);
    */
    
  pwfinish:
    
    secu_ClearPassword(p0);
    secu_ClearPassword(p1);

    if (newpwitem == NULL) {
	fprintf(stderr, "Error hashing new password\n");
	SECITEM_ZfreeItem(oldpwitem, PR_TRUE);
	fclose(input);
	fclose(output);
	return SECFailure;
    }

    if (newdb == PR_TRUE) {
	rv = SECKEY_SetKeyDBPassword(handle, newpwitem);
	if (rv) {
	    fprintf(stderr, "Error setting database password\n");
	    failed = 1;
	}
    } else {
	rv = SECKEY_ChangeKeyDBPassword(handle, oldpwitem, newpwitem);
	if (rv) {
	    fprintf(stderr, "Error changing database password\n");
	    failed = 1;
	}
    }

    SECITEM_ZfreeItem(newpwitem, PR_TRUE);
    SECITEM_ZfreeItem(oldpwitem, PR_TRUE);

    if (input != stdin) fclose(input);
    fclose(output);

    if (failed) {
	return SECFailure;
    }

    return SECSuccess;
}

#ifdef notdef
static SECItem *
secu_GetDonglePassword(void *arg, SECKEYKeyDBHandle *handle)
{
    SECItem *pwitem;
    char *p = NULL;
    char *pathname;
    SECStatus rv;
    int fd;
    
    pathname = (char *)arg;
    
    fd = open((char *)pathname, O_RDONLY);
    if (!fd) {
        fprintf(stderr, "Unable to open dongle file \"%s\".\n", (char *)arg);
    }
    
    p = SEC_ReadDongleFile(fd);
    if (!p) {
        fprintf(stderr, "Unable to obtain dongle password\n");
    }
    
    /* check if we need to update the key database */
    if ( handle->version < PRIVATE_KEY_DB_FILE_VERSION ) {
	SECKEY_UpdateKeyDB(handle, p);
    }

    /* hash the password */
    pwitem = SECKEY_DeriveKeyDBPassword(handle, p);

    /* clear out the password strings */
    secu_ClearPassword(p);

    if (pwitem == NULL) {
	fprintf(stderr, "Error hashing password\n");
	return NULL;
    }

    /* confirm the password */
    rv = SECKEY_CheckKeyDBPassword(handle, pwitem);
    if (rv) {
	fprintf(stderr, "Sorry, dongle password is invalid\n");
	SECITEM_ZfreeItem(pwitem, PR_TRUE);
	return NULL;
    }

    return pwitem;
}

SECKEYPrivateKey *
SECU_GetPrivateDongleKey(SECKEYKeyDBHandle *handle, char *nickname, 
			 char *pathname)
{
    SECKEYPrivateKey *key;
    char *fullpath;
    int rv;
    
    fullpath = SECU_AppendFilenameToDir(pathname, "dongle");
    
    /* If dongle file doesn't exist, prompt for password */
    rv = access(fullpath, R_OK);
    if (rv < 0) {
	return SECU_GetPrivateKey(handle, nickname);
    }
    
    /* try dongle file */
    key = SECKEY_FindKeyByName(handle, nickname, secu_GetDonglePassword,
			    fullpath);

    /* if no key, maybe dongle is broken, so prompt for password */
    if (key == NULL) {
	key = SECU_GetPrivateKey(handle, nickname);
    }

    return key;
}
#endif

char *
SECU_DefaultSSLDir(void)
{
    char *dir;
    static char sslDir[1000];

    dir = getenv("SSL_DIR");
    if (!dir)
	return NULL;

    sprintf(sslDir, "%s", dir);

    if (sslDir[strlen(sslDir)-1] == '/')
	sslDir[strlen(sslDir)-1] = 0;

    return sslDir;
}

char *
SECU_AppendFilenameToDir(char *dir, char *filename)
{
    static char path[1000];

    if (dir[strlen(dir)-1] == '/')
	sprintf(path, "%s%s", dir, filename);
    else
	sprintf(path, "%s/%s", dir, filename);
    return path;
}

char *
SECU_ConfigDirectory(const char* base)
{
    static PRBool initted = PR_FALSE;
    const char *dir = ".netscape";
    char *home;
    static char buf[1000];

    if (initted) return buf;
    

    if (base == NULL || *base == 0) {
	home = getenv("HOME");
	if (!home) home = "";

	if (*home && home[strlen(home) - 1] == '/')
	    sprintf (buf, "%.900s%s", home, dir);
	else
	    sprintf (buf, "%.900s/%s", home, dir);
    } else {
	sprintf(buf, "%.900s", base);
	if (buf[strlen(buf) - 1] == '/')
	    buf[strlen(buf) - 1] = 0;
    }


    initted = PR_TRUE;
    return buf;
}

char *
SECU_CertDBNameCallback(void *arg, int dbVersion)
{
    char *fnarg;
    char *dir;
    char *filename;
    
    dir = SECU_ConfigDirectory(NULL);
    
    switch ( dbVersion ) {
      case 7:
	fnarg = "7";
	break;
      case 6:
	fnarg = "6";
	break;
      case 5:
	fnarg = "5";
	break;
      case 4:
      default:
	fnarg = "";
	break;
    }
    filename = PR_smprintf("%s/cert%s.db", dir, fnarg);
    return(filename);
}

char *
SECU_KeyDBNameCallback(void *arg, int dbVersion)
{
    char *fnarg;
    char *dir;
    char *filename;
    struct stat fd;
    
    dir = SECU_ConfigDirectory(NULL);

    if (stat(dir, &fd) != 0) {
	fprintf(stderr, "No directory \"%s\" exists.\n", dir);
	return NULL;
    }
    
    
    switch ( dbVersion ) {
      case 3:
	fnarg = "3";
	break;
      case 2:
      default:
	fnarg = "";
	break;
    }
    filename = PR_smprintf("%s/key%s.db", dir, fnarg);
    return(filename);
}

char *
SECU_SECModDBName(void)
{
    char *dir;
    char *filename;
    
    dir = SECU_ConfigDirectory(NULL);
    
    filename = PR_smprintf("%s/secmod.db", dir);
    return(filename);
}

SECKEYKeyDBHandle *
SECU_OpenKeyDB(PRBool readOnly)
{
    SECKEYKeyDBHandle *handle;
    
    handle = SECKEY_OpenKeyDB(readOnly, SECU_KeyDBNameCallback, NULL);
    SECKEY_SetDefaultKeyDB(handle);
    
    return(handle);
}

CERTCertDBHandle *
SECU_OpenCertDB(PRBool readOnly)
  /* NOTE: This routine has been modified to allow the libsec/pcertdb.c
   * routines to automatically find and convert the old cert database
   * into the new v3.0 format (cert db version 5).
   */
{
    CERTCertDBHandle *certHandle;
    SECStatus rv;

    /* Allocate a handle to fill with CERT_OpenCertDB below */
    certHandle = (CERTCertDBHandle *)PORT_ZAlloc(sizeof(CERTCertDBHandle));
    if (!certHandle) {
	return NULL;
    }

    rv = CERT_OpenCertDB(certHandle, readOnly, SECU_CertDBNameCallback, NULL);

    if (rv) {
	if (certHandle) 
	    PORT_Free (certHandle);  
	    /* we don't want to leave anything behind... */
	return NULL;
    } else {
	CERT_SetDefaultCertDB(certHandle);
    }

    return certHandle;
}

/*Turn off SSL for now */
/* This gets called by SSL when server wants our cert & key */
int
SECU_GetClientAuthData(void *arg, PRFileDesc *fd,
		       struct CERTDistNamesStr *caNames,
                      struct CERTCertificateStr **pRetCert,
                      struct SECKEYPrivateKeyStr **pRetKey)
{
    SECKEYPrivateKey *key;
    CERTCertificate *cert;
    int errsave;

    if (arg == NULL) {
        fprintf(stderr, "no key/cert name specified for client auth\n");
        return -1;
    }
    cert = PK11_FindCertFromNickname(arg, NULL);
    errsave = PORT_GetError();
    if (!cert) {
        if (errsave == SEC_ERROR_BAD_PASSWORD)
            fprintf(stderr, "Bad password\n");
        else if (errsave > 0)
            fprintf(stderr, "Unable to read cert (error %d)\n", errsave);
        else if (errsave == SEC_ERROR_BAD_DATABASE)
            fprintf(stderr, "Unable to get cert from database (%d)\n", errsave);
        else
            fprintf(stderr, "SECKEY_FindKeyByName: internal error %d\n", errsave);
        return -1;
    }

    key = PK11_FindKeyByAnyCert(arg,NULL);
    if (!key) {
        fprintf(stderr, "Unable to get key (%d)\n", PORT_GetError());
        return -1;
    }


    *pRetCert = cert;
    *pRetKey = key;

    return 0;
}

SECStatus
secu_StdinToItem(SECItem *dst)
{
    unsigned char buf[1000];
    PRInt32 numBytes;
    PRBool notDone = PR_TRUE;

    dst->len = 0;
    dst->data = NULL;

    while (notDone) {
	numBytes = PR_Read(PR_STDIN, buf, sizeof(buf));

	if (numBytes < 0) {
	    PORT_SetError(PR_IO_ERROR);
	    return SECFailure;
	}

	if (numBytes == 0)
	    break;

	if (buf[numBytes-1] == '\n') {
	    buf[numBytes-1] = '\0';
	    notDone = PR_FALSE;
	}

	if (dst->data) {
	    dst->data = (unsigned char*)PORT_Realloc(dst->data, 
	                                             dst->len+numBytes);
	    PORT_Memcpy(dst->data+dst->len, buf, numBytes);
	} else {
	    dst->data = (unsigned char*)PORT_Alloc(numBytes);
	    PORT_Memcpy(dst->data, buf, numBytes);
	}
	dst->len += numBytes;
    }

    return SECSuccess;
}

SECStatus
SECU_FileToItem(SECItem *dst, PRFileDesc *src)
{
    PRFileInfo info;
    PRInt32 numBytes;
    PRStatus prStatus;

    if (src == PR_STDIN)
	return secu_StdinToItem(dst);

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus != PR_SUCCESS) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }

    /* XXX workaround for 3.1, not all utils zero dst before sending */
    dst->data = 0;
    if (!SECITEM_AllocItem(NULL, dst, info.size))
	goto loser;

    numBytes = PR_Read(src, dst->data, info.size);
    if (numBytes != info.size) {
	PORT_SetError(SEC_ERROR_IO);
	goto loser;
    }

    return SECSuccess;
loser:
    SECITEM_FreeItem(dst, PR_FALSE);
    return SECFailure;
}

SECStatus
SECU_TextFileToItem(SECItem *dst, PRFileDesc *src)
{
    PRFileInfo info;
    PRInt32 numBytes;
    PRStatus prStatus;
    unsigned char *buf;

    if (src == PR_STDIN)
	return secu_StdinToItem(dst);

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus != PR_SUCCESS) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }

    buf = (unsigned char*)PORT_Alloc(info.size);
    if (!buf)
	return SECFailure;

    numBytes = PR_Read(src, buf, info.size);
    if (numBytes != info.size) {
	PORT_SetError(SEC_ERROR_IO);
	goto loser;
    }

    if (buf[numBytes-1] == '\n') numBytes--;
#ifdef _WINDOWS
    if (buf[numBytes-1] == '\r') numBytes--;
#endif

    /* XXX workaround for 3.1, not all utils zero dst before sending */
    dst->data = 0;
    if (!SECITEM_AllocItem(NULL, dst, numBytes))
	goto loser;

    memcpy(dst->data, buf, numBytes);

    PORT_Free(buf);
    return SECSuccess;
loser:
    PORT_Free(buf);
    return SECFailure;
}

SECStatus
SECU_ReadDERFromFile(SECItem *der, PRFileDesc *inFile, PRBool ascii)
{
    SECStatus rv;
    char *asc, *body, *trailer;
    if (ascii) {
	/* First convert ascii to binary */
	SECItem filedata;

	/* Read in ascii data */
	rv = SECU_FileToItem(&filedata, inFile);
	asc = filedata.data;
	if (!asc) {
	    fprintf(stderr, "unable to read data from input file\n");
	    return SECFailure;
	}

	/* check for headers and trailers and remove them */
	if ((body = strstr(asc, "-----BEGIN")) != NULL) {
	    body = PORT_Strchr(body, '\n') + 1;
	    trailer = strstr(body, "-----END");
	    if (trailer != NULL) {
		*trailer = '\0';
	    } else {
		fprintf(stderr, "input has header but no trailer\n");
		return SECFailure;
	    }
	} else {
	    body = asc;
	}
     
	/* Convert to binary */
	rv = ATOB_ConvertAsciiToItem(der, body);
	if (rv) {
	    fprintf(stderr, "error converting ascii to binary (%s)\n",
		    SECU_Strerror(PORT_GetError()));
	    return SECFailure;
	}
	PORT_Free(asc);
    } else {
	/* Read in binary der */
	rv = SECU_FileToItem(der, inFile);
	if (rv) {
	    fprintf(stderr, "error converting der (%s)\n", 
		    SECU_Strerror(PORT_GetError()));
	    return SECFailure;
	}
    }
    return SECSuccess;
}

#define INDENT_MULT	4
void
SECU_Indent(FILE *out, int level)
{
    int i;
    for (i = 0; i < level; i++) {
	fprintf(out, "    ");
    }
}

static void secu_Newline(FILE *out)
{
    fprintf(out, "\n");
}

void
SECU_PrintAsHex(FILE *out, SECItem *data, char *m, int level)
{
    unsigned i;
    int column;

    if ( m ) {
	SECU_Indent(out, level); fprintf(out, "%s:\n", m);
	level++;
    }
    
    SECU_Indent(out, level); column = level*INDENT_MULT;
    for (i = 0; i < data->len; i++) {
	if (i != data->len - 1) {
	    fprintf(out, "%02x:", data->data[i]);
	    column += 4;
	} else {
	    fprintf(out, "%02x", data->data[i]);
	    column += 3;
	    break;
	}
	if (column > 76) {
	    secu_Newline(out);
	    SECU_Indent(out, level); column = level*INDENT_MULT;
	}
    }
    level--;
    if (column != level*INDENT_MULT) {
	secu_Newline(out);
    }
}

void
SECU_PrintInteger(FILE *out, SECItem *i, char *m, int level)
{
    int iv;

    if (i->len > 4) {
	SECU_PrintAsHex(out, i, m, level);
    } else {
	iv = DER_GetInteger(i);
	SECU_Indent(out, level); 
	if (m) {
	    fprintf(out, "%s: %d (0x%x)\n", m, iv, iv);
	} else {
	    fprintf(out, "%d (0x%x)\n", iv, iv);
	}
    }
}

void
SECU_PrintString(FILE *out, SECItem *i, char *m, int level)
{
    char *string;
    unsigned char *data = i->data;
    int len = i->len;
    int lenlen;
    int tag;

    string = PORT_ZAlloc(i->len+1);
   
    tag = *data++; len--;
    if (data[1] & 0x80) {
	    lenlen = data[1] & 0x1f;
    } else {
	    lenlen = 1;
    }
    data += lenlen; len -= lenlen;
    if (len <= 0) return;
    PORT_Memcpy(string,data,len);

    /* should check the validity of tag, and convert the string as necessary */
    SECU_Indent(out, level); 
    if (m) {
	fprintf(out, "%s: \"%s\"\n", m, string);
    } else {
	fprintf(out, "\"%s\"\n", string);
    }
}

static void
secu_PrintBoolean(FILE *out, SECItem *i, char *m, int level)
{
    int val = 0;
    
    if ( i->data ) {
	val = i->data[0];
    }

    if (m) {
    	SECU_Indent(out, level); fprintf(out, "%s:\n", m); level++;
    }
    if ( val ) {
	SECU_Indent(out, level); fprintf(out, "%s\n", "True");
    } else {
	SECU_Indent(out, level); fprintf(out, "%s\n", "False");
    }    
}

/*
 * Format and print "time".  If the tag message "m" is not NULL,
 * do indent formatting based on "level" and add a newline afterward;
 * otherwise just print the formatted time string only.
 */
static void
secu_PrintTime(FILE *out, int64 time, char *m, int level)
{
    PRExplodedTime printableTime; 
    char *timeString;

    /* Convert to local time */
    PR_ExplodeTime(time, PR_GMTParameters, &printableTime);

    timeString = PORT_Alloc(100);
    if (timeString == NULL)
	return;

    if (m != NULL) {
	SECU_Indent(out, level);
	fprintf(out, "%s: ", m);
    }

    PR_FormatTime(timeString, 100, "%a %b %d %H:%M:%S %Y", &printableTime);
    fprintf(out, timeString);

    if (m != NULL)
	fprintf(out, "\n");

    PORT_Free(timeString);
}

/*
 * Format and print the UTC Time "t".  If the tag message "m" is not NULL,
 * do indent formatting based on "level" and add a newline afterward;
 * otherwise just print the formatted time string only.
 */
void
SECU_PrintUTCTime(FILE *out, SECItem *t, char *m, int level)
{
    int64 time;
    SECStatus rv;

    rv = DER_UTCTimeToTime(&time, t);
    if (rv != SECSuccess)
	return;

    secu_PrintTime(out, time, m, level);
}

/*
 * Format and print the Generalized Time "t".  If the tag message "m"
 * is not NULL, * do indent formatting based on "level" and add a newline
 * afterward; otherwise just print the formatted time string only.
 */
void
SECU_PrintGeneralizedTime(FILE *out, SECItem *t, char *m, int level)
{
    int64 time;
    SECStatus rv;


    rv = DER_GeneralizedTimeToTime(&time, t);
    if (rv != SECSuccess)
	return;

    secu_PrintTime(out, time, m, level);
}

static void secu_PrintAny(FILE *out, SECItem *i, char *m, int level);

void
SECU_PrintSet(FILE *out, SECItem *t, char *m, int level)
{
    int type= t->data[0] & 0x1f;
    int start;
    unsigned char *bp;

    SECU_Indent(out, level);
    if (m) {
    	fprintf(out, "%s: ", m);
    }
    fprintf(out,"%s {\n", 
		type == SEC_ASN1_SET ? "Set" : "Sequence");

    start = 2;
    if (t->data[1] & 0x80) {
	start = (t->data[1] & 0x7f) +1;
    }
    for (bp=&t->data[start]; bp < &t->data[t->len]; ) {
	SECItem tmp;
	unsigned int i,len,lenlen;

        if (bp[1] & 0x80) {
	    lenlen = bp[1] & 0x1f;
	    len = 0;
	    for (i=0; i < lenlen; i++) {
		len = len * 255 + bp[2+i];
	    }
	} else {
	    lenlen = 1;
	    len = bp[1];
	}
	tmp.len = len+lenlen+1;
	tmp.data = bp;
	bp += tmp.len;
	secu_PrintAny(out,&tmp,NULL,level+1);
    }
    SECU_Indent(out, level); fprintf(out, "}\n");

}

static void
secu_PrintAny(FILE *out, SECItem *i, char *m, int level)
{
    if ( i->len ) {
	switch (i->data[0] & 0x1f) {
	  case SEC_ASN1_INTEGER:
	    SECU_PrintInteger(out, i, m, level);
	    break;
	  case SEC_ASN1_OBJECT_ID:
	    SECU_PrintObjectID(out, i, m, level);
	    break;
	  case SEC_ASN1_BOOLEAN:
	    secu_PrintBoolean(out, i, m, level);
	    break;
	  case SEC_ASN1_UTF8_STRING:
	  case SEC_ASN1_PRINTABLE_STRING:
	  case SEC_ASN1_VISIBLE_STRING:
	  case SEC_ASN1_BMP_STRING:
	  case SEC_ASN1_IA5_STRING:
	  case SEC_ASN1_T61_STRING:
	  case SEC_ASN1_UNIVERSAL_STRING:
	    SECU_PrintString(out, i, m, level);
	    break;
	  case SEC_ASN1_GENERALIZED_TIME:
	    SECU_PrintGeneralizedTime(out, i, m, level);
	    break;
	  case SEC_ASN1_UTC_TIME:
	    SECU_PrintUTCTime(out, i, m, level);
	    break;
	  case SEC_ASN1_NULL:
	    SECU_Indent(out, level); fprintf(out, "%s: NULL\n", m);
	    break;
          case SEC_ASN1_SET:
          case SEC_ASN1_SEQUENCE:
	    SECU_PrintSet(out, i, m, level);
	    break;
	    
	  default:
	    SECU_PrintAsHex(out, i, m, level);
	    break;
	}
    }
}

static int
secu_PrintValidity(FILE *out, CERTValidity *v, char *m, int level)
{
    SECU_Indent(out, level);  fprintf(out, "%s:\n", m);
    SECU_PrintUTCTime(out, &v->notBefore, "Not Before", level+1);
    SECU_PrintUTCTime(out, &v->notAfter, "Not After", level+1);
    return 0;
}

void
SECU_PrintObjectID(FILE *out, SECItem *oid, char *m, int level)
{
    char *name;
    SECOidData *oiddata;
    
    oiddata = SECOID_FindOID(oid);
    if (oiddata == NULL) {
	SECU_PrintAsHex(out, oid, m, level);
	return;
    }
    name = oiddata->desc;

    SECU_Indent(out, level);
    if (m != NULL)
	fprintf(out, "%s: ", m);
    fprintf(out, "%s\n", name);
}

void
SECU_PrintAlgorithmID(FILE *out, SECAlgorithmID *a, char *m, int level)
{
    SECU_PrintObjectID(out, &a->algorithm, m, level);

    if (a->parameters.len == 0
	|| (a->parameters.len == 2
	    && PORT_Memcmp(a->parameters.data, "\005\000", 2) == 0)) {
	/* No arguments or NULL argument */
    } else {
	/* Print args to algorithm */
	SECU_PrintAsHex(out, &a->parameters, "Args", level+1);
    }
}

static void
secu_PrintAttribute(FILE *out, SEC_PKCS7Attribute *attr, char *m, int level)
{
    SECItem *value;
    int i;
    char om[100];

    if (m) {
    	SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    }

    /*
     * Should make this smarter; look at the type field and then decode
     * and print the value(s) appropriately!
     */
    SECU_PrintObjectID(out, &(attr->type), "Type", level+1);
    if (attr->values != NULL) {
	i = 0;
	while ((value = attr->values[i++]) != NULL) {
	    sprintf(om, "Value (%d)%s", i, attr->encoded ? " (encoded)" : ""); 
	    if (attr->encoded || attr->typeTag == NULL) {
		SECU_PrintAsHex(out, value, om, level+1);
	    } else {
		switch (attr->typeTag->offset) {
		  default:
		    SECU_PrintAsHex(out, value, om, level+1);
		    break;
		  case SEC_OID_PKCS9_CONTENT_TYPE:
		    SECU_PrintObjectID(out, value, om, level+1);
		    break;
		  case SEC_OID_PKCS9_SIGNING_TIME:
		    SECU_PrintUTCTime(out, value, om, level+1);
		    break;
		}
	    }
	}
    }
}

static void
secu_PrintRSAPublicKey(FILE *out, SECKEYPublicKey *pk, char *m, int level)
{
#if 0	/*
	 * um, yeah, that might be nice, but if you look at the callers
	 * you will see that they do not *set* this, so this will not work!
	 * Instead, somebody needs to fix the callers to be smarter about
	 * public key stuff, if that is important.
	 */
    PORT_Assert(pk->keyType == rsaKey);
#endif

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &pk->u.rsa.modulus, "Modulus", level+1);
    SECU_PrintInteger(out, &pk->u.rsa.publicExponent, "Exponent", level+1);
}

static void
secu_PrintDSAPublicKey(FILE *out, SECKEYPublicKey *pk, char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &pk->u.dsa.params.prime, "Prime", level+1);
    SECU_PrintInteger(out, &pk->u.dsa.params.subPrime, "Subprime", level+1);
    SECU_PrintInteger(out, &pk->u.dsa.params.base, "Base", level+1);
    SECU_PrintInteger(out, &pk->u.dsa.publicValue, "PublicValue", level+1);
}

static int
secu_PrintSubjectPublicKeyInfo(FILE *out, PRArenaPool *arena,
		       CERTSubjectPublicKeyInfo *i,  char *msg, int level)
{
    SECKEYPublicKey *pk;
    int rv;

    SECU_Indent(out, level); fprintf(out, "%s:\n", msg);
    SECU_PrintAlgorithmID(out, &i->algorithm, "Public Key Algorithm", level+1);

    pk = (SECKEYPublicKey*) PORT_ZAlloc(sizeof(SECKEYPublicKey));
    if (!pk)
	return PORT_GetError();

    DER_ConvertBitString(&i->subjectPublicKey);
    switch(SECOID_FindOIDTag(&i->algorithm.algorithm)) {
      case SEC_OID_PKCS1_RSA_ENCRYPTION:
	rv = SEC_ASN1DecodeItem(arena, pk, SECKEY_RSAPublicKeyTemplate,
				&i->subjectPublicKey);
	if (rv)
	    return rv;
	secu_PrintRSAPublicKey(out, pk, "RSA Public Key", level +1);
	break;
      case SEC_OID_ANSIX9_DSA_SIGNATURE:
	rv = SEC_ASN1DecodeItem(arena, pk, SECKEY_DSAPublicKeyTemplate,
				&i->subjectPublicKey);
	if (rv)
	    return rv;
	secu_PrintDSAPublicKey(out, pk, "DSA Public Key", level +1);
	break;
      default:
	fprintf(out, "bad SPKI algorithm type\n");
	return 0;
    }

    return 0;
}

static SECStatus
secu_PrintX509InvalidDate(FILE *out, SECItem *value, char *msg, int level)
{
    SECItem decodedValue;
    SECStatus rv;
    int64 invalidTime;
    char *formattedTime = NULL;

    decodedValue.data = NULL;
    rv = SEC_ASN1DecodeItem (NULL, &decodedValue, SEC_GeneralizedTimeTemplate,
			    value);
    if (rv == SECSuccess) {
	rv = DER_GeneralizedTimeToTime(&invalidTime, &decodedValue);
	if (rv == SECSuccess) {
	    formattedTime = CERT_GenTime2FormattedAscii
			    (invalidTime, "%a %b %d %H:%M:%S %Y");
	    SECU_Indent(out, level +1);
	    fprintf (out, "%s: %s\n", msg, formattedTime);
	    PORT_Free (formattedTime);
	}
    }
    PORT_Free (decodedValue.data);
    return (rv);
}

static SECStatus
PrintExtKeyUsageExten  (FILE *out, SECItem *value, char *msg, int level)
{
  CERTOidSequence *os;
  SECItem **op;

  SECU_Indent(out, level); fprintf(out, "Extended Key Usage Extension:\n");

  os = CERT_DecodeOidSequence(value);
  if( (CERTOidSequence *)NULL == os ) {
    return SECFailure;
  }

  if( (SECItem **)NULL == op ) {
    return SECFailure;
  }

  for( op = os->oids; *op; op++ ) {
    SECOidData *od = SECOID_FindOID(*op);
    
    if( (SECOidData *)NULL == od ) {
      SECU_Indent(out, level+1);
      SECU_PrintAsHex(out, *op, "Unknown:", level+2);
      secu_Newline(out);
      continue;
    }

    SECU_Indent(out, level+1);
    if( od->desc ) fprintf(out, "%s", od->desc);
    else SECU_PrintAsHex(out, &od->oid, "", level+2);

    secu_Newline(out);
  }

  return SECSuccess;
}

char *
itemToString(SECItem *item)
{
    char *string;

    string = PORT_ZAlloc(item->len+1);
    if (string == NULL) return NULL;
    PORT_Memcpy(string,item->data,item->len);
    string[item->len] = 0;
    return string;
}

static SECStatus
secu_PrintPolicyQualifier(FILE *out,CERTPolicyQualifier *policyQualifier,char *msg,int level)
{
   CERTUserNotice *userNotice;
   SECItem **itemList = NULL;
   char *string;

   SECU_PrintObjectID(out, &policyQualifier->qualifierID , 
					"Policy Qualifier Name", level);

   switch (policyQualifier->oid) {
   case SEC_OID_PKIX_USER_NOTICE_QUALIFIER:
	userNotice = CERT_DecodeUserNotice(&policyQualifier->qualifierValue);
        if (userNotice) {
	    if (userNotice->noticeReference.organization.len != 0) {
		string=itemToString(&userNotice->noticeReference.organization);
		itemList = userNotice->noticeReference.noticeNumbers;
		while (*itemList) {
		    SECU_PrintInteger(out,*itemList,string,level+1);
		    itemList++;
		}
		PORT_Free(string);
	    }
	    if (userNotice->displayText.len != 0) {
		SECU_PrintString(out,&userNotice->displayText,
						"Display Text", level+1);
	    }
	    break;
	} 
        /* fall through on error */
   case SEC_OID_PKIX_CPS_POINTER_QUALIFIER:
   default:
	secu_PrintAny(out, &policyQualifier->qualifierValue, "Policy Qualifier Data", level+1);
	break;
   }
   
   return SECSuccess;

}

static SECStatus
secu_PrintPolicyInfo(FILE *out,CERTPolicyInfo *policyInfo,char *msg,int level)
{
   CERTPolicyQualifier **policyQualifiers;

   policyQualifiers = policyInfo->policyQualifiers;
   SECU_PrintObjectID(out, &policyInfo->policyID , "Policy Name", level);
   
   while (*policyQualifiers != NULL) {
	secu_PrintPolicyQualifier(out,*policyQualifiers,"",level+1);
	policyQualifiers++;
   }
   return SECSuccess;

}

static SECStatus
secu_PrintPolicy(FILE *out, SECItem *value, char *msg, int level)
{
   CERTCertificatePolicies *policies = NULL;
   CERTPolicyInfo **policyInfos;

   if (msg) {
	SECU_Indent(out, level);
	fprintf(out,"%s: \n",msg);
	level++;
   }
   policies = CERT_DecodeCertificatePoliciesExtension(value);
   if (policies == NULL) {
	SECU_PrintAsHex(out, value, "Invalid Policy Data", level);
	return SECFailure;
   }

   policyInfos = policies->policyInfos;
   while (*policyInfos != NULL) {
	secu_PrintPolicyInfo(out,*policyInfos,"",level);
	policyInfos++;
   }

   CERT_DestroyCertificatePoliciesExtension(policies);
   return SECSuccess;
}

char *nsTypeBits[] = {
"SSL Client","SSL Server","S/MIME","Object Signing","Reserved","SSL CA","S/MIME CA","ObjectSigning CA" };

static SECStatus
secu_PrintBasicConstraints(FILE *out, SECItem *value, char *msg, int level) {
    CERTBasicConstraints constraints;
    SECStatus rv;

    SECU_Indent(out, level);
    if (msg) {
	    fprintf(out,"%s: ",msg);
    } 
    rv = CERT_DecodeBasicConstraintValue(&constraints,value);
    if (rv == SECSuccess && constraints.isCA) {
	fprintf(out,"Is a CA with a maximum path length of %d.\n",
					constraints.pathLenConstraint);
    } else  {
	fprintf(out,"Is not a CA.\n");
    }
    return SECSuccess;
}

static SECStatus
secu_PrintNSCertType(FILE *out, SECItem *value, char *msg, int level) {
	char NS_Type=0;
	int len, i, found=0;

	if (value->data[1] & 0x80) {
	    len = 3;
	} else {
	    len = value->data[1];
	}
	if ((value->data[0] != SEC_ASN1_BIT_STRING) || (len < 2)) {
	    secu_PrintAny(out, value, "Data", level);
	    return SECSuccess;
	}
	NS_Type=value->data[3];
	

	if (msg) {
	    SECU_Indent(out, level);
	    fprintf(out,"%s: ",msg);
	} else {
	    SECU_Indent(out, level);
	    fprintf(out,"Netscape Certificate Type: ");
	}
	for (i=0; i < 8; i++) {
	    if ( (0x80 >> i) & NS_Type) {
		fprintf(out,"%c%s",found?',':'<',nsTypeBits[i]);
		found = 1;
	    }
	}
	if (found) { fprintf(out,">\n"); } else { fprintf(out,"none\n"); }
	return SECSuccess;
}

void
SECU_PrintExtensions(FILE *out, CERTCertExtension **extensions,
		     char *msg, int level)
{
    SECOidTag oidTag;
    
    if ( extensions ) {
	SECU_Indent(out, level); fprintf(out, "%s:\n", msg);
	
	while ( *extensions ) {
	    SECItem *tmpitem;
	    SECU_Indent(out, level+1); fprintf(out, "Name:\n");

	    tmpitem = &(*extensions)->id;
	    SECU_PrintObjectID(out, tmpitem, NULL, level+2);

	    tmpitem = &(*extensions)->critical;
	    if ( tmpitem->len ) {
		secu_PrintBoolean(out, tmpitem, "Critical", level+1);
	    }

	    oidTag = SECOID_FindOIDTag (&((*extensions)->id));
	    tmpitem = &((*extensions)->value);

	    switch (oidTag) {
	      	case SEC_OID_X509_INVALID_DATE:
		case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME:
		   secu_PrintX509InvalidDate(out, tmpitem, "Date", level + 1);
		   break;
		case SEC_OID_X509_CERTIFICATE_POLICIES:
		   secu_PrintPolicy(out, tmpitem, "Data", level +1);
		   break;
		case SEC_OID_NS_CERT_EXT_BASE_URL:
		case SEC_OID_NS_CERT_EXT_REVOCATION_URL:
		case SEC_OID_NS_CERT_EXT_CA_REVOCATION_URL:
		case SEC_OID_NS_CERT_EXT_CA_CRL_URL:
		case SEC_OID_NS_CERT_EXT_CA_CERT_URL:
		case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_URL:
		case SEC_OID_NS_CERT_EXT_CA_POLICY_URL:
		case SEC_OID_NS_CERT_EXT_HOMEPAGE_URL:
		case SEC_OID_NS_CERT_EXT_LOST_PASSWORD_URL:
		case SEC_OID_OCSP_RESPONDER:
		    SECU_PrintString(out,tmpitem, "URL", level+1);
		    break;
		case SEC_OID_NS_CERT_EXT_COMMENT:
		    SECU_PrintString(out,tmpitem, "Comment", level+1);
		    break;
		case SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME:
		    SECU_PrintString(out,tmpitem, "ServerName", level+1);
		    break;
		case SEC_OID_NS_CERT_EXT_CERT_TYPE:
		    secu_PrintNSCertType(out,tmpitem,"Data",level+1);
		    break;
		case SEC_OID_X509_BASIC_CONSTRAINTS:
		    secu_PrintBasicConstraints(out,tmpitem,"Data",level+1);
		    break;
		case SEC_OID_X509_SUBJECT_ALT_NAME:
		case SEC_OID_X509_ISSUER_ALT_NAME:
	      /*
	       * We should add at least some of the more interesting cases
	       * here, but need to have subroutines to back them up.
	       */
		case SEC_OID_NS_CERT_EXT_NETSCAPE_OK:
		case SEC_OID_NS_CERT_EXT_ISSUER_LOGO:
		case SEC_OID_NS_CERT_EXT_SUBJECT_LOGO:
		case SEC_OID_NS_CERT_EXT_ENTITY_LOGO:
		case SEC_OID_NS_CERT_EXT_USER_PICTURE:
		case SEC_OID_NS_KEY_USAGE_GOVT_APPROVED:

		/* x.509 v3 Extensions */
		case SEC_OID_X509_SUBJECT_DIRECTORY_ATTR:
		case SEC_OID_X509_SUBJECT_KEY_ID:
		case SEC_OID_X509_KEY_USAGE:
		case SEC_OID_X509_PRIVATE_KEY_USAGE_PERIOD:
		case SEC_OID_X509_NAME_CONSTRAINTS:
		case SEC_OID_X509_CRL_DIST_POINTS:
		case SEC_OID_X509_POLICY_MAPPINGS:
		case SEC_OID_X509_POLICY_CONSTRAINTS:
		case SEC_OID_X509_AUTH_KEY_ID:
            goto defualt;
		case SEC_OID_X509_EXT_KEY_USAGE:
            PrintExtKeyUsageExten(out, tmpitem, "", level+1);
            break;
		case SEC_OID_X509_AUTH_INFO_ACCESS:

		case SEC_OID_X509_CRL_NUMBER:
		case SEC_OID_X509_REASON_CODE:

		/* PKIX OIDs */
		case SEC_OID_PKIX_OCSP:
		case SEC_OID_PKIX_OCSP_BASIC_RESPONSE:
		case SEC_OID_PKIX_OCSP_NONCE:
		case SEC_OID_PKIX_OCSP_CRL:
		case SEC_OID_PKIX_OCSP_RESPONSE:
		case SEC_OID_PKIX_OCSP_NO_CHECK:
		case SEC_OID_PKIX_OCSP_ARCHIVE_CUTOFF:
		case SEC_OID_PKIX_OCSP_SERVICE_LOCATOR:
		case SEC_OID_PKIX_REGCTRL_REGTOKEN:
		case SEC_OID_PKIX_REGCTRL_AUTHENTICATOR:
		case SEC_OID_PKIX_REGCTRL_PKIPUBINFO:
		case SEC_OID_PKIX_REGCTRL_PKI_ARCH_OPTIONS:
		case SEC_OID_PKIX_REGCTRL_OLD_CERT_ID:
		case SEC_OID_PKIX_REGCTRL_PROTOCOL_ENC_KEY:
		case SEC_OID_PKIX_REGINFO_UTF8_PAIRS:
		case SEC_OID_PKIX_REGINFO_CERT_REQUEST:
		case SEC_OID_EXT_KEY_USAGE_SERVER_AUTH:
		case SEC_OID_EXT_KEY_USAGE_CLIENT_AUTH:
		case SEC_OID_EXT_KEY_USAGE_CODE_SIGN:
		case SEC_OID_EXT_KEY_USAGE_EMAIL_PROTECT:
		case SEC_OID_EXT_KEY_USAGE_TIME_STAMP:

	      default:
          defualt:
		/*SECU_PrintAsHex(out, tmpitem, "Data", level+1); */
		secu_PrintAny(out, tmpitem, "Data", level+1);
		break;
	    }
		    
	    secu_Newline(out);
	    extensions++;
	}
    }
}


void
SECU_PrintName(FILE *out, CERTName *name, char *msg, int level)
{
    char *str;
    
    SECU_Indent(out, level); fprintf(out, "%s: ", msg);

    str = CERT_NameToAscii(name);
    if (!str) 
    	str = "!Invalid AVA!";
    fprintf(out, str);
	
    secu_Newline(out);
}

static int keyindex;

static SECStatus
secu_PrintKeyNickname(DBT *k, DBT *d, void *data)
{
    FILE *out;
    char *name;
    unsigned char *buf;

    buf = (unsigned char *)d->data;
    out = (FILE *)data;

    name = (char *)PORT_Alloc(buf[2]);
    if (name == NULL) {
	return(SECFailure);
    }

    PORT_Memcpy(name, (buf + 3 + buf[1]), buf[2]);

    /* print everything but password-check entry */
    if (PORT_Strcmp(name, "password-check") != 0) {
	keyindex++;
	fprintf(out, "<%d> %s\n", keyindex, name);
    }
    PORT_Free(name);

    return (SECSuccess);
}

int
SECU_PrintKeyNames(SECKEYKeyDBHandle *handle, FILE *out)
{
    int rv;

    SECU_Indent(out, 0);
    fprintf(out, "Version %d database\n\n", SECKEY_GetKeyDBVersion(handle));
    fprintf(out, "<Key Index>  Key Name\n--------\n");
    keyindex = 0;
    rv = SECKEY_TraverseKeys(handle, secu_PrintKeyNickname, out);
    if (rv) {
	return -1;
    }

    return 0;
}

#if 0
struct indexedkey
{
    int index;
    SECKEY_LowPrivateKey *key;
};

static SECStatus
secu_GetKeyIndex(DBT *k, DBT *d, void *data)
{
    char *name;
    unsigned char *buf;
    struct indexedkey *idkey = (struct indexedkey*)data;
    /*SECKEYLowPrivateKey *key = *(SECKEYLowPrivateKey**)data;*/

    buf = (unsigned char *)d->data;

    name = (char *)PORT_Alloc(buf[2]);
    if (name == NULL) {
	return(SECFailure);
    }

    PORT_Memcpy(name, (buf + 3 + buf[1]), buf[2]);

    /* print everything but password-check entry */
    if (PORT_Strcmp(name, "password-check") != 0) {
	keyindex++;
	if (keyindex == idkey->index)
	    idkey->key = SECKEY_DecryptKey(k,idkey->slot->password,
	                                  SECKEY_GetDefaultKeyDB());
    }
    PORT_Free(name);

    return (SECSuccess);
}

SECKEYLowPrivateKey*
secu_GetPrivKeyFromIndex(int index, PK11Slot slot);
{
    /*SECKEYLowPrivateKey* key;*/
    struct indexedkey idkey = { index, NULL };

    keyindex = 0;
    rv = SECKEY_TraverseKeys(SECKEY_GetDefaultKeyDB(), 
                             secu_GetKeyIndex, &idkey);

    if (rv) {
	return NULL;
    }
    return key;
}
#endif


void
printflags(char *trusts, unsigned int flags)
{
    if (flags & CERTDB_VALID_CA)
	if (!(flags & CERTDB_TRUSTED_CA) &&
	    !(flags & CERTDB_TRUSTED_CLIENT_CA))
	    PORT_Strcat(trusts, "c");
    if (flags & CERTDB_VALID_PEER)
	if (!(flags & CERTDB_TRUSTED))
	    PORT_Strcat(trusts, "p");
    if (flags & CERTDB_TRUSTED_CA)
	PORT_Strcat(trusts, "C");
    if (flags & CERTDB_TRUSTED_CLIENT_CA)
	PORT_Strcat(trusts, "T");
    if (flags & CERTDB_TRUSTED)
	PORT_Strcat(trusts, "P");
    if (flags & CERTDB_USER)
	PORT_Strcat(trusts, "u");
    if (flags & CERTDB_SEND_WARN)
	PORT_Strcat(trusts, "w");
    if (flags & CERTDB_INVISIBLE_CA)
	PORT_Strcat(trusts, "I");
    if (flags & CERTDB_GOVT_APPROVED_CA)
	PORT_Strcat(trusts, "G");
    return;
}

/* callback for listing certs through pkcs11 */
SECStatus
SECU_PrintCertNickname(CERTCertificate *cert, void *data)
{
    CERTCertTrust *trust;
    FILE *out;
    char trusts[30];
    char *name;

    PORT_Memset (trusts, 0, sizeof (trusts));
    out = (FILE *)data;
    
    if ( cert->dbEntry ) {
	name = cert->dbEntry->nickname;
	if ( name == NULL ) {
	    name = cert->emailAddr;
	}
	
        trust = &cert->dbEntry->trust;
	printflags(trusts, trust->sslFlags);
	PORT_Strcat(trusts, ",");
	printflags(trusts, trust->emailFlags);
	PORT_Strcat(trusts, ",");
	printflags(trusts, trust->objectSigningFlags);
	fprintf(out, "%-60s %-5s\n", name, trusts);
    } else {
	name = cert->nickname;
	if ( name == NULL ) {
	    name = cert->emailAddr;
	}
	
        trust = cert->trust;
	if (trust) {
	    printflags(trusts, trust->sslFlags);
	    PORT_Strcat(trusts, ",");
	    printflags(trusts, trust->emailFlags);
	    PORT_Strcat(trusts, ",");
	    printflags(trusts, trust->objectSigningFlags);
	} else {
	    PORT_Memcpy(trusts,",,",3);
	}
	fprintf(out, "%-60s %-5s\n", name, trusts);
    }

    return (SECSuccess);
}

typedef struct {
    char *		name;
    CERTCertTrust	trust;
} certNameAndTrustEntry;

typedef struct {
    int numCerts;
    certNameAndTrustEntry *nameAndTrustEntries;
} certNameAndTrustList;

SECStatus
sec_CountCerts(CERTCertificate *cert, SECItem *unknown, void *arg)
{
    (*(int*)arg)++;
    return SECSuccess;
}

SECStatus
sec_CollectCertNamesAndTrust(CERTCertificate *cert, SECItem *unknown, void *arg)
{
    certNameAndTrustList *pCertNames = (certNameAndTrustList*)arg;
    char *name;
    int i;

    i = pCertNames->numCerts;
    name = cert->dbEntry->nickname ? cert->dbEntry->nickname : cert->emailAddr;

    if (name)
	pCertNames->nameAndTrustEntries[i].name = PORT_Strdup(name);
    else
	pCertNames->nameAndTrustEntries[i].name = PORT_Strdup("<unknown>");

    PORT_Memcpy(&pCertNames->nameAndTrustEntries[i].trust, cert->trust, sizeof(*cert->trust));

    pCertNames->numCerts++;

    return SECSuccess;
}

static int
sec_name_and_trust_compare_by_name(const void *p1, const void *p2)
{
    certNameAndTrustEntry *e1 = (certNameAndTrustEntry *)p1;
    certNameAndTrustEntry *e2 = (certNameAndTrustEntry *)p2;
    return PORT_Strcmp(e1->name, e2->name);
}

static int
sec_combine_trust_flags(CERTCertTrust *trust)
{
    if (trust == NULL)
	return NULL;
    return trust->sslFlags | trust->emailFlags | trust->objectSigningFlags;
}

static int
sec_name_and_trust_compare_by_trust(const void *p1, const void *p2)
{
    certNameAndTrustEntry *e1 = (certNameAndTrustEntry *)p1;
    certNameAndTrustEntry *e2 = (certNameAndTrustEntry *)p2;
    int e1_is_ca, e2_is_ca;
    int e1_is_user, e2_is_user;
    int rv;

    e1_is_ca = (sec_combine_trust_flags(&e1->trust) & CERTDB_VALID_CA) != 0;
    e2_is_ca = (sec_combine_trust_flags(&e2->trust) & CERTDB_VALID_CA) != 0;
    e1_is_user = (sec_combine_trust_flags(&e1->trust) & CERTDB_USER) != 0;
    e2_is_user = (sec_combine_trust_flags(&e2->trust) & CERTDB_USER) != 0;

    /* first, sort by user status, then CA status, */
    /*  then by actual comparison of CA flags, then by name */
    if ((rv = (e2_is_user - e1_is_user)) == 0 && (rv = (e1_is_ca - e2_is_ca)) == 0)
	if (e1_is_ca || (rv = memcmp(&e1->trust, &e2->trust, sizeof(CERTCertTrust))) == 0)
	    return PORT_Strcmp(e1->name, e2->name);
	else
	    return rv;
    else
	return rv;
}

SECStatus
SECU_PrintCertificateNames(CERTCertDBHandle *handle, PRFileDesc *out, 
                           PRBool sortByName, PRBool sortByTrust)
{
    certNameAndTrustList certNames = { 0, NULL };
    int numCerts, i;
    SECStatus rv;
    int (*comparefn)(const void *, const void *);
    char trusts[30];

    numCerts = 0;

    rv = SEC_TraversePermCerts(handle, sec_CountCerts, &numCerts);
    if (rv != SECSuccess)
	return SECFailure;

    certNames.nameAndTrustEntries = 
		(certNameAndTrustEntry *)PORT_Alloc(numCerts * sizeof(certNameAndTrustEntry));
    if (certNames.nameAndTrustEntries == NULL)
	return SECFailure;

    rv = SEC_TraversePermCerts(handle, sec_CollectCertNamesAndTrust, &certNames);
    if (rv != SECSuccess)
	return SECFailure;

    if (sortByName)
	comparefn = sec_name_and_trust_compare_by_name;
    else if (sortByTrust)
	comparefn = sec_name_and_trust_compare_by_trust;
    else
	comparefn = NULL;

    if (comparefn)
	qsort(certNames.nameAndTrustEntries, certNames.numCerts, 
			    sizeof(certNameAndTrustEntry), comparefn);

    PR_fprintf(out, "\n%-60s %-5s\n\n", "Certificate Name", "Trust Attributes");
    for (i = 0; i < certNames.numCerts; i++) {
	PORT_Memset (trusts, 0, sizeof(trusts));
	printflags(trusts, certNames.nameAndTrustEntries[i].trust.sslFlags);
	PORT_Strcat(trusts, ",");
	printflags(trusts, certNames.nameAndTrustEntries[i].trust.emailFlags);
	PORT_Strcat(trusts, ",");
	printflags(trusts, certNames.nameAndTrustEntries[i].trust.objectSigningFlags);
	PR_fprintf(out, "%-60s %-5s\n", 
	           certNames.nameAndTrustEntries[i].name, trusts);
    }
    PR_fprintf(out, "\n");
    PR_fprintf(out, "p    Valid peer\n");
    PR_fprintf(out, "P    Trusted peer (implies p)\n");
    PR_fprintf(out, "c    Valid CA\n");
    PR_fprintf(out, "T    Trusted CA to issue client certs (implies c)\n");
    PR_fprintf(out, "C    Trusted CA to certs(only server certs for ssl) (implies c)\n");
    PR_fprintf(out, "u    User cert\n");
    PR_fprintf(out, "w    Send warning\n");

    for (i = 0; i < certNames.numCerts; i++)
	PORT_Free(certNames.nameAndTrustEntries[i].name);
    PORT_Free(certNames.nameAndTrustEntries);

    return rv;
}

int
SECU_PrintCertificateRequest(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = NULL;
    CERTCertificateRequest *cr;
    int rv;

    /* Decode certificate request */
    cr = (CERTCertificateRequest*) PORT_ZAlloc(sizeof(CERTCertificateRequest));
    if (!cr)
	return PORT_GetError();

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
	return SEC_ERROR_NO_MEMORY;

    rv = SEC_ASN1DecodeItem(arena, cr, CERT_CertificateRequestTemplate, der);
    if (rv) {
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
    }

    /* Pretty print it out */
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &cr->version, "Version", level+1);
    SECU_PrintName(out, &cr->subject, "Subject", level+1);
    rv = secu_PrintSubjectPublicKeyInfo(out, arena, &cr->subjectPublicKeyInfo,
			      "Subject Public Key Info", level+1);
    if (rv) {
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
    }
    secu_PrintAny(out, cr->attributes[0], "Attributes", level+1);

    PORT_FreeArena(arena, PR_FALSE);
    return 0;
}

int
SECU_PrintCertificate(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = NULL;
    CERTCertificate *c;
    int rv;
    int iv;
    
    /* Decode certificate */
    c = (CERTCertificate*) PORT_ZAlloc(sizeof(CERTCertificate));
    if (!c)
	return PORT_GetError();

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
	return SEC_ERROR_NO_MEMORY;

    rv = SEC_ASN1DecodeItem(arena, c, CERT_CertificateTemplate, der);
    if (rv) {
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
    }

    /* Pretty print it out */
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    iv = DER_GetInteger(&c->version);
    SECU_Indent(out, level+1); fprintf(out, "%s: %d (0x%x)\n", "Version", iv + 1, iv);

    SECU_PrintInteger(out, &c->serialNumber, "Serial Number", level+1);
    SECU_PrintAlgorithmID(out, &c->signature, "Signature Algorithm", level+1);
    SECU_PrintName(out, &c->issuer, "Issuer", level+1);
    secu_PrintValidity(out, &c->validity, "Validity", level+1);
    SECU_PrintName(out, &c->subject, "Subject", level+1);
    rv = secu_PrintSubjectPublicKeyInfo(out, arena, &c->subjectPublicKeyInfo,
			      "Subject Public Key Info", level+1);
    if (rv) {
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
    }
    SECU_PrintExtensions(out, c->extensions, "Signed Extensions", level+1);

    SECU_PrintFingerprints(out, &c->derCert, "Fingerprint", level);
    
    PORT_FreeArena(arena, PR_FALSE);
    return 0;
}

int
SECU_PrintPublicKey(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = NULL;
    SECKEYPublicKey key;
    int rv;

    PORT_Memset(&key, 0, sizeof(key));
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
	return SEC_ERROR_NO_MEMORY;

    rv = SEC_ASN1DecodeItem(arena, &key, SECKEY_RSAPublicKeyTemplate, der);
    if (rv) {
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
    }

    /* Pretty print it out */
    secu_PrintRSAPublicKey(out, &key, m, level);

    PORT_FreeArena(arena, PR_FALSE);
    return 0;
}

int
SECU_PrintPrivateKey(FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = NULL;
    SECKEYEncryptedPrivateKeyInfo key;
    int rv;

    PORT_Memset(&key, 0, sizeof(key));
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
	return SEC_ERROR_NO_MEMORY;

    rv = SEC_ASN1DecodeItem(arena, &key, SECKEY_EncryptedPrivateKeyInfoTemplate,
			    der);
    if (rv) {
	PORT_FreeArena(arena, PR_TRUE);
	return rv;
    }

    /* Pretty print it out */
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintAlgorithmID(out, &key.algorithm, "Encryption Algorithm", 
			  level+1);
    SECU_PrintAsHex(out, &key.encryptedData, "Encrypted Data", level+1);

    PORT_FreeArena(arena, PR_TRUE);
    return 0;
}

int
SECU_PrintFingerprints(FILE *out, SECItem *derCert, char *m, int level)
{
    char fingerprint[20];
    char *fpStr = NULL;
    SECItem fpItem;
    /* print MD5 fingerprint */
    memset(fingerprint, 0, sizeof fingerprint);
    MD5_HashBuf(fingerprint, derCert->data, derCert->len);
    fpItem.data = fingerprint;
    fpItem.len = MD5_LENGTH;
    fpStr = CERT_Hexify(&fpItem, 1);
    SECU_Indent(out, level);  fprintf(out, "%s (MD5):\n", m);
    SECU_Indent(out, level+1); fprintf(out, "%s\n", fpStr);
    PORT_Free(fpStr);
    fpStr = NULL;
    /* print SHA1 fingerprint */
    memset(fingerprint, 0, sizeof fingerprint);
    SHA1_HashBuf(fingerprint, derCert->data, derCert->len);
    fpItem.data = fingerprint;
    fpItem.len = SHA1_LENGTH;
    fpStr = CERT_Hexify(&fpItem, 1);
    SECU_Indent(out, level);  fprintf(out, "%s (SHA1):\n", m);
    SECU_Indent(out, level+1); fprintf(out, "%s\n", fpStr);
    PORT_Free(fpStr);
	fprintf(out, "\n");
    return 0;
}

/*
** PKCS7 Support
*/

/* forward declaration */
static int
secu_PrintPKCS7ContentInfo(FILE *, SEC_PKCS7ContentInfo *, char *, int);

/*
** secu_PrintPKCS7EncContent
**   Prints a SEC_PKCS7EncryptedContentInfo (without decrypting it)
*/
static void
secu_PrintPKCS7EncContent(FILE *out, SEC_PKCS7EncryptedContentInfo *src, 
			  char *m, int level)
{
    if (src->contentTypeTag == NULL)
	src->contentTypeTag = SECOID_FindOID(&(src->contentType));

    SECU_Indent(out, level);
    fprintf(out, "%s:\n", m);
    SECU_Indent(out, level + 1); 
    fprintf(out, "Content Type: %s\n",
	    (src->contentTypeTag != NULL) ? src->contentTypeTag->desc
					  : "Unknown");
    SECU_PrintAlgorithmID(out, &(src->contentEncAlg),
			  "Content Encryption Algorithm", level+1);
    SECU_PrintAsHex(out, &(src->encContent), 
		    "Encrypted Content", level+1);
}

/*
** secu_PrintRecipientInfo
**   Prints a PKCS7RecipientInfo type
*/
static void
secu_PrintRecipientInfo(FILE *out, SEC_PKCS7RecipientInfo *info, char *m, 
			int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(info->version), "Version", level + 1);	

    SECU_PrintName(out, &(info->issuerAndSN->issuer), "Issuer", 
		 level + 1);
    SECU_PrintInteger(out, &(info->issuerAndSN->serialNumber), 
		      "Serial Number", level + 1);

    /* Parse and display encrypted key */
    SECU_PrintAlgorithmID(out, &(info->keyEncAlg), 
			"Key Encryption Algorithm", level + 1);
    SECU_PrintAsHex(out, &(info->encKey), "Encrypted Key", level + 1);
}

/* 
** secu_PrintSignerInfo
**   Prints a PKCS7SingerInfo type
*/
static void
secu_PrintSignerInfo(FILE *out, SEC_PKCS7SignerInfo *info, char *m, int level)
{
    SEC_PKCS7Attribute *attr;
    int iv;
    char om[100];
    
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(info->version), "Version", level + 1);	

    SECU_PrintName(out, &(info->issuerAndSN->issuer), "Issuer", 
		 level + 1);
    SECU_PrintInteger(out, &(info->issuerAndSN->serialNumber), 
		      "Serial Number", level + 1);
  
    SECU_PrintAlgorithmID(out, &(info->digestAlg), "Digest Algorithm",
			  level + 1);
    
    if (info->authAttr != NULL) {
	SECU_Indent(out, level + 1); 
	fprintf(out, "Authenticated Attributes:\n");
	iv = 0;
	while ((attr = info->authAttr[iv++]) != NULL) {
	    sprintf(om, "Attribute (%d)", iv); 
	    secu_PrintAttribute(out, attr, om, level + 2);
	}
    }
    
    /* Parse and display signature */
    SECU_PrintAlgorithmID(out, &(info->digestEncAlg), 
			"Digest Encryption Algorithm", level + 1);
    SECU_PrintAsHex(out, &(info->encDigest), "Encrypted Digest", level + 1);
    
    if (info->unAuthAttr != NULL) {
	SECU_Indent(out, level + 1); 
	fprintf(out, "Unauthenticated Attributes:\n");
	iv = 0;
	while ((attr = info->unAuthAttr[iv++]) != NULL) {
	    sprintf(om, "Attribute (%x)", iv); 
	    secu_PrintAttribute(out, attr, om, level + 2);
	}
    }
}

void
SECU_PrintCRLInfo(FILE *out, CERTCrl *crl, char *m, int level)
{
    CERTCrlEntry *entry;
    int iv;
    char om[100];
    
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintAlgorithmID(out, &(crl->signatureAlg), "Signature Algorithm",
			  level + 1);
    SECU_PrintName(out, &(crl->name), "Name", level + 1);
    SECU_PrintUTCTime(out, &(crl->lastUpdate), "Last Update", level + 1);
    SECU_PrintUTCTime(out, &(crl->nextUpdate), "Next Update", level + 1);
    
    if (crl->entries != NULL) {
	iv = 0;
	while ((entry = crl->entries[iv++]) != NULL) {
	    sprintf(om, "Entry (%x):\n", iv); 
	    SECU_Indent(out, level + 1); fprintf(out, om);
	    SECU_PrintInteger(out, &(entry->serialNumber), "Serial Number",
			      level + 2);
	    SECU_PrintUTCTime(out, &(entry->revocationDate), "Revocation Date",
			      level + 2);
	    SECU_PrintExtensions
		   (out, entry->extensions, "Signed CRL Entries Extensions", level + 1);
	}
    }
    SECU_PrintExtensions
	   (out, crl->extensions, "Signed CRL Extension", level + 1);
}

/*
** secu_PrintPKCS7Signed
**   Pretty print a PKCS7 signed data type (up to version 1).
*/
static int
secu_PrintPKCS7Signed(FILE *out, SEC_PKCS7SignedData *src, char *m, int level)
{
    SECAlgorithmID *digAlg;		/* digest algorithms */
    SECItem *aCert;			/* certificate */
    CERTSignedCrl *aCrl;		/* certificate revocation list */
    SEC_PKCS7SignerInfo *sigInfo;	/* signer information */
    int rv, iv;
    char om[100];

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    /* Parse and list digest algorithms (if any) */
    if (src->digestAlgorithms != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Digest Algorithm List:\n");
	iv = 0;
	while ((digAlg = src->digestAlgorithms[iv++]) != NULL) {
	    sprintf(om, "Digest Algorithm (%x)", iv);
	    SECU_PrintAlgorithmID(out, digAlg, om, level + 2);
	}
    }

    /* Now for the content */
    rv = secu_PrintPKCS7ContentInfo(out, &(src->contentInfo), 
				    "Content Information", level + 1);
    if (rv != 0)
	return rv;

    /* Parse and list certificates (if any) */
    if (src->rawCerts != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Certificate List:\n");
	iv = 0;
	while ((aCert = src->rawCerts[iv++]) != NULL) {
	    sprintf(om, "Certificate (%x)", iv);
	    rv = SECU_PrintSignedData(out, aCert, om, level + 2, 
				      SECU_PrintCertificate);
	    if (rv)
		return rv;
	}
    }

    /* Parse and list CRL's (if any) */
    if (src->crls != NULL) {
	SECU_Indent(out, level + 1);  
	fprintf(out, "Signed Revocation Lists:\n");
	iv = 0;
	while ((aCrl = src->crls[iv++]) != NULL) {
	    sprintf(om, "Signed Revocation List (%x)", iv);
	    SECU_Indent(out, level + 2);  fprintf(out, "%s:\n", om);
	    SECU_PrintAlgorithmID(out, &aCrl->signatureWrap.signatureAlgorithm, 
				  "Signature Algorithm", level+3);
	    DER_ConvertBitString(&aCrl->signatureWrap.signature);
	    SECU_PrintAsHex(out, &aCrl->signatureWrap.signature, "Signature",
			    level+3);
	    SECU_PrintCRLInfo(out, &aCrl->crl, "Certificate Revocation List", 
			  level + 3); 
	}
    }

    /* Parse and list signatures (if any) */
    if (src->signerInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Signer Information List:\n");
	iv = 0;
	while ((sigInfo = src->signerInfos[iv++]) != NULL) {
	    sprintf(om, "Signer Information (%x)", iv);
	    secu_PrintSignerInfo(out, sigInfo, om, level + 2);
	}
    }  

    return 0;
}

/*
** secu_PrintPKCS7Enveloped
**  Pretty print a PKCS7 enveloped data type (up to version 1).
*/
static void
secu_PrintPKCS7Enveloped(FILE *out, SEC_PKCS7EnvelopedData *src,
			 char *m, int level)
{
    SEC_PKCS7RecipientInfo *recInfo;   /* pointer for signer information */
    int iv;
    char om[100];

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    /* Parse and list recipients (this is not optional) */
    if (src->recipientInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Recipient Information List:\n");
	iv = 0;
	while ((recInfo = src->recipientInfos[iv++]) != NULL) {
	    sprintf(om, "Recipient Information (%x)", iv);
	    secu_PrintRecipientInfo(out, recInfo, om, level + 2);
	}
    }  

    secu_PrintPKCS7EncContent(out, &src->encContentInfo, 
			      "Encrypted Content Information", level + 1);
}

/*
** secu_PrintPKCS7SignedEnveloped
**   Pretty print a PKCS7 singed and enveloped data type (up to version 1).
*/
static int
secu_PrintPKCS7SignedAndEnveloped(FILE *out,
				  SEC_PKCS7SignedAndEnvelopedData *src,
				  char *m, int level)
{
    SECAlgorithmID *digAlg;  /* pointer for digest algorithms */
    SECItem *aCert;           /* pointer for certificate */
    CERTSignedCrl *aCrl;        /* pointer for certificate revocation list */
    SEC_PKCS7SignerInfo *sigInfo;   /* pointer for signer information */
    SEC_PKCS7RecipientInfo *recInfo; /* pointer for recipient information */
    int rv, iv;
    char om[100];

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    /* Parse and list recipients (this is not optional) */
    if (src->recipientInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Recipient Information List:\n");
	iv = 0;
	while ((recInfo = src->recipientInfos[iv++]) != NULL) {
	    sprintf(om, "Recipient Information (%x)", iv);
	    secu_PrintRecipientInfo(out, recInfo, om, level + 2);
	}
    }  

    /* Parse and list digest algorithms (if any) */
    if (src->digestAlgorithms != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Digest Algorithm List:\n");
	iv = 0;
	while ((digAlg = src->digestAlgorithms[iv++]) != NULL) {
	    sprintf(om, "Digest Algorithm (%x)", iv);
	    SECU_PrintAlgorithmID(out, digAlg, om, level + 2);
	}
    }

    secu_PrintPKCS7EncContent(out, &src->encContentInfo, 
			      "Encrypted Content Information", level + 1);

    /* Parse and list certificates (if any) */
    if (src->rawCerts != NULL) {
	SECU_Indent(out, level + 1);  fprintf(out, "Certificate List:\n");
	iv = 0;
	while ((aCert = src->rawCerts[iv++]) != NULL) {
	    sprintf(om, "Certificate (%x)", iv);
	    rv = SECU_PrintSignedData(out, aCert, om, level + 2, 
				      SECU_PrintCertificate);
	    if (rv)
		return rv;
	}
    }

    /* Parse and list CRL's (if any) */
    if (src->crls != NULL) {
	SECU_Indent(out, level + 1);  
	fprintf(out, "Signed Revocation Lists:\n");
	iv = 0;
	while ((aCrl = src->crls[iv++]) != NULL) {
	    sprintf(om, "Signed Revocation List (%x)", iv);
	    SECU_Indent(out, level + 2);  fprintf(out, "%s:\n", om);
	    SECU_PrintAlgorithmID(out, &aCrl->signatureWrap.signatureAlgorithm, 
				  "Signature Algorithm", level+3);
	    DER_ConvertBitString(&aCrl->signatureWrap.signature);
	    SECU_PrintAsHex(out, &aCrl->signatureWrap.signature, "Signature",
			    level+3);
	    SECU_PrintCRLInfo(out, &aCrl->crl, "Certificate Revocation List", 
			  level + 3); 
	}
    }

    /* Parse and list signatures (if any) */
    if (src->signerInfos != NULL) {
	SECU_Indent(out, level + 1);
	fprintf(out, "Signer Information List:\n");
	iv = 0;
	while ((sigInfo = src->signerInfos[iv++]) != NULL) {
	    sprintf(om, "Signer Information (%x)", iv);
	    secu_PrintSignerInfo(out, sigInfo, om, level + 2);
	}
    }  

    return 0;
}

int
SECU_PrintCrl (FILE *out, SECItem *der, char *m, int level)
{
    PRArenaPool *arena = NULL;
    CERTCrl *c = NULL;
    int rv;

    do {
	/* Decode CRL */
	c = (CERTCrl*) PORT_ZAlloc(sizeof(CERTCrl));
	if (!c) {
	    rv = PORT_GetError();
	    break;
	}

	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if (!arena) {
	    rv = SEC_ERROR_NO_MEMORY;
	    break;
	}

	rv = SEC_ASN1DecodeItem(arena, c, CERT_CrlTemplate, der);
	if (rv != SECSuccess)
	    break;
	SECU_PrintCRLInfo (out, c, m, level);
    } while (0);
    PORT_FreeArena (arena, PR_FALSE);
    PORT_Free (c);
    return (rv);
}


/*
** secu_PrintPKCS7Encrypted
**   Pretty print a PKCS7 encrypted data type (up to version 1).
*/
static void
secu_PrintPKCS7Encrypted(FILE *out, SEC_PKCS7EncryptedData *src,
			 char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);

    secu_PrintPKCS7EncContent(out, &src->encContentInfo, 
			      "Encrypted Content Information", level + 1);
}

/*
** secu_PrintPKCS7Digested
**   Pretty print a PKCS7 digested data type (up to version 1).
*/
static void
secu_PrintPKCS7Digested(FILE *out, SEC_PKCS7DigestedData *src,
			char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_PrintInteger(out, &(src->version), "Version", level + 1);
    
    SECU_PrintAlgorithmID(out, &src->digestAlg, "Digest Algorithm",
			  level + 1);
    secu_PrintPKCS7ContentInfo(out, &src->contentInfo, "Content Information",
			       level + 1);
    SECU_PrintAsHex(out, &src->digest, "Digest", level + 1);  
}

/*
** secu_PrintPKCS7ContentInfo
**   Takes a SEC_PKCS7ContentInfo type and sends the contents to the 
** appropriate function
*/
static int
secu_PrintPKCS7ContentInfo(FILE *out, SEC_PKCS7ContentInfo *src,
			   char *m, int level)
{
    char *desc;
    SECOidTag kind;
    int rv;

    SECU_Indent(out, level);  fprintf(out, "%s:\n", m);
    level++;

    if (src->contentTypeTag == NULL)
	src->contentTypeTag = SECOID_FindOID(&(src->contentType));

    if (src->contentTypeTag == NULL) {
	desc = "Unknown";
	kind = SEC_OID_PKCS7_DATA;
    } else {
	desc = src->contentTypeTag->desc;
	kind = src->contentTypeTag->offset;
    }

    if (src->content.data == NULL) {
	SECU_Indent(out, level); fprintf(out, "%s:\n", desc);
	level++;
	SECU_Indent(out, level); fprintf(out, "<no content>\n");
	return 0;
    }

    rv = 0;
    switch (kind) {
      case SEC_OID_PKCS7_SIGNED_DATA:  /* Signed Data */
	rv = secu_PrintPKCS7Signed(out, src->content.signedData, desc, level);
	break;

      case SEC_OID_PKCS7_ENVELOPED_DATA:  /* Enveloped Data */
        secu_PrintPKCS7Enveloped(out, src->content.envelopedData, desc, level);
	break;

      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:  /* Signed and Enveloped */
	rv = secu_PrintPKCS7SignedAndEnveloped(out,
					src->content.signedAndEnvelopedData,
					desc, level);
	break;

      case SEC_OID_PKCS7_DIGESTED_DATA:  /* Digested Data */
	secu_PrintPKCS7Digested(out, src->content.digestedData, desc, level);
	break;

      case SEC_OID_PKCS7_ENCRYPTED_DATA:  /* Encrypted Data */
	secu_PrintPKCS7Encrypted(out, src->content.encryptedData, desc, level);
	break;

      default:
	SECU_PrintAsHex(out, src->content.data, desc, level);
	break;
    }

    return rv;
}

/*
** SECU_PrintPKCS7ContentInfo
**   Decode and print any major PKCS7 data type (up to version 1).
*/
int
SECU_PrintPKCS7ContentInfo(FILE *out, SECItem *der, char *m, int level)
{
    SEC_PKCS7ContentInfo *cinfo;
    int rv;

    cinfo = SEC_PKCS7DecodeItem(der, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    if (cinfo != NULL) {
	/* Send it to recursive parsing and printing module */
	rv = secu_PrintPKCS7ContentInfo(out, cinfo, m, level);
	SEC_PKCS7DestroyContentInfo(cinfo);
    } else {
	rv = -1;
    }

    return rv;
}

/*
** End of PKCS7 functions
*/

void
printFlags(FILE *out, unsigned int flags, int level)
{
    if ( flags & CERTDB_VALID_PEER ) {
	SECU_Indent(out, level); fprintf(out, "Valid Peer\n");
    }
    if ( flags & CERTDB_TRUSTED ) {
	SECU_Indent(out, level); fprintf(out, "Trusted\n");
    }
    if ( flags & CERTDB_SEND_WARN ) {
	SECU_Indent(out, level); fprintf(out, "Warn When Sending\n");
    }
    if ( flags & CERTDB_VALID_CA ) {
	SECU_Indent(out, level); fprintf(out, "Valid CA\n");
    }
    if ( flags & CERTDB_TRUSTED_CA ) {
	SECU_Indent(out, level); fprintf(out, "Trusted CA\n");
    }
    if ( flags & CERTDB_NS_TRUSTED_CA ) {
	SECU_Indent(out, level); fprintf(out, "Netscape Trusted CA\n");
    }
    if ( flags & CERTDB_USER ) {
	SECU_Indent(out, level); fprintf(out, "User\n");
    }
    if ( flags & CERTDB_TRUSTED_CLIENT_CA ) {
	SECU_Indent(out, level); fprintf(out, "Trusted Client CA\n");
    }
#ifdef DEBUG
    if ( flags & CERTDB_GOVT_APPROVED_CA ) {
	SECU_Indent(out, level); fprintf(out, "Step-up\n");
    }
#endif /* DEBUG */
}

void
SECU_PrintTrustFlags(FILE *out, CERTCertTrust *trust, char *m, int level)
{
    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    SECU_Indent(out, level+1); fprintf(out, "SSL Flags:\n");
    printFlags(out, trust->sslFlags, level+2);
    SECU_Indent(out, level+1); fprintf(out, "Email Flags:\n");
    printFlags(out, trust->emailFlags, level+2);
    SECU_Indent(out, level+1); fprintf(out, "Object Signing Flags:\n");
    printFlags(out, trust->objectSigningFlags, level+2);
}

int SECU_PrintSignedData(FILE *out, SECItem *der, char *m,
			   int level, SECU_PPFunc inner)
{
    PRArenaPool *arena = NULL;
    CERTSignedData *sd;
    int rv;

    /* Strip off the signature */
    sd = (CERTSignedData*) PORT_ZAlloc(sizeof(CERTSignedData));
    if (!sd)
	return PORT_GetError();

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena)
	return SEC_ERROR_NO_MEMORY;

    rv = SEC_ASN1DecodeItem(arena, sd, CERT_SignedDataTemplate, der);
    if (rv) {
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
    }

    SECU_Indent(out, level); fprintf(out, "%s:\n", m);
    rv = (*inner)(out, &sd->data, "Data", level+1);
    if (rv) {
	PORT_FreeArena(arena, PR_FALSE);
	return rv;
    }

    SECU_PrintAlgorithmID(out, &sd->signatureAlgorithm, "Signature Algorithm",
			  level+1);
    DER_ConvertBitString(&sd->signature);
    SECU_PrintAsHex(out, &sd->signature, "Signature", level+1);

    PORT_FreeArena(arena, PR_FALSE);
    return 0;

}

SECStatus
SECU_PKCS11Init(PRBool readOnly) {
    static PRBool      isInit = PR_FALSE;
    SECKEYKeyDBHandle *kdb_handle;
    char *             secmodule;
    int                rv;

    if (isInit) 
    	return SECSuccess;

    isInit = PR_TRUE;

    /*
     * Initialize the private key database.
     */
    /* in the "ideal world"(tm), the Key database would be initialized in
     * the Software PKCS#11 module. Unfortunately there's not enough of
     * an interface to pass all the info to the PCKS#11 module, so go ahead
     * and initialize here...
     */
    RNG_RNGInit(); /* Guess what SECKEY_OpenKeyDB calls if there is
		    * no keyDB? You got it Get Random Data... just one more
		    * reason to want to move this call into pkcs11.c
		    */
    RNG_SystemInfoForRNG();

    kdb_handle = SECU_OpenKeyDB(readOnly);
    
    if (kdb_handle != NULL) {
	SECKEY_SetDefaultKeyDB(kdb_handle);
    }

    /*
     * set our default password function
     */
    PK11_SetPasswordFunc(SECU_GetModulePassword);
    /*
     * OK, now we initialize the PKCS11 subsystems
     */
    secmodule = SECU_SECModDBName();
    SECMOD_init(secmodule);
    return SECSuccess; /*  check the return codes?? */
}

SECKEYLowPublicKey *SECU_ConvHighToLow(SECKEYPublicKey *pubk) 
{
    SECKEYLowPublicKey *copyk;
    PRArenaPool *arena;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
	return NULL;
    }

    copyk = (SECKEYLowPublicKey *) 
			PORT_ArenaZAlloc (arena, sizeof (SECKEYLowPublicKey));
    if (copyk != NULL) {
	SECStatus rv = SECSuccess;

	copyk->arena = arena;
	copyk->keyType = pubk->keyType;
	switch (pubk->keyType) {
	  case rsaKey:
	    rv = SECITEM_CopyItem(arena, &copyk->u.rsa.modulus,
				  &pubk->u.rsa.modulus);
	    if (rv == SECSuccess) {
		rv = SECITEM_CopyItem (arena, &copyk->u.rsa.publicExponent,
				       &pubk->u.rsa.publicExponent);
		if (rv == SECSuccess)
		    return copyk;
	    }
	    break;
	  case nullKey:
	    return copyk;
	  default:
	    rv = SECFailure;
	    break;
	}
	if (rv == SECSuccess)
	    return copyk;

	SECKEY_LowDestroyPublicKey (copyk);
    } else {
	PORT_SetError (SEC_ERROR_NO_MEMORY);
    }

    PORT_FreeArena (arena, PR_FALSE);
    return NULL;
}


#ifdef AIX
int _OS_SELECT (int nfds, void *readfds, void *writefds,
               void *exceptfds, struct timeval *timeout) {
   return select (nfds,readfds,writefds,exceptfds,timeout);
}
#endif 

SECItem *
SECU_GetPBEPassword(void *arg)
{
    char *p = NULL;
    SECItem *pwitem = NULL;

    p = SECU_GetPasswordString(arg,"Password: ");

    /* NOTE: This function is obviously unfinished. */

    if ( pwitem == NULL ) {
	fprintf(stderr, "Error hashing password\n");
	return NULL;
    }
    
    return pwitem;
}

SECStatus
SECU_ParseCommandLine(int argc, char **argv, char *progName, secuCommand *cmd)
{
    PRBool found;
    PLOptState *optstate;
    PLOptStatus status;
    char *optstring;
    int i, j;

    optstring = (char *)malloc(cmd->numCommands + 2*cmd->numOptions);
    j = 0;

    for (i=0; i<cmd->numCommands; i++) {
	optstring[j++] = cmd->commands[i].flag;
    }
    for (i=0; i<cmd->numOptions; i++) {
	optstring[j++] = cmd->options[i].flag;
	if (cmd->options[i].needsArg)
	    optstring[j++] = ':';
    }
    optstring[j] = '\0';
    optstate = PL_CreateOptState(argc, argv, optstring);

    /* Parse command line arguments */
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {

	/*  Wasn't really an option, just standalone arg.  */
	if (optstate->option == '\0')
	    continue;

	found = PR_FALSE;

	for (i=0; i<cmd->numCommands; i++) {
	    if (cmd->commands[i].flag == optstate->option) {
		cmd->commands[i].activated = PR_TRUE;
		if (optstate->value) {
		    cmd->commands[i].arg = optstate->value;
		}
		found = PR_TRUE;
		break;
	    }
	}

	if (found)
	    continue;

	for (i=0; i<cmd->numOptions; i++) {
	    if (cmd->options[i].flag == optstate->option) {
		cmd->options[i].activated = PR_TRUE;
		if (optstate->value) {
		    cmd->options[i].arg = optstate->value;
		}
		found = PR_TRUE;
		break;
	    }
	}

	if (!found)
	    return SECFailure;
    }
    if (status == PL_OPT_BAD)
	return SECFailure;
    return SECSuccess;
}

char *
SECU_GetOptionArg(secuCommand *cmd, int optionNum)
{
	if (optionNum < 0 || optionNum >= cmd->numOptions)
		return NULL;
	if (cmd->options[optionNum].activated)
		return PL_strdup(cmd->options[optionNum].arg);
	else
		return NULL;
}

static char SECUErrorBuf[64];

char *
SECU_ErrorStringRaw(int16 err)
{
    if (err == 0)
	sprintf(SECUErrorBuf, "");
    else if (err == SEC_ERROR_BAD_DATA)
	sprintf(SECUErrorBuf, "Bad data");
    else if (err == SEC_ERROR_BAD_DATABASE)
	sprintf(SECUErrorBuf, "Problem with database");
    else if (err == SEC_ERROR_BAD_DER)
	sprintf(SECUErrorBuf, "Problem with DER");
    else if (err == SEC_ERROR_BAD_KEY)
	sprintf(SECUErrorBuf, "Problem with key");
    else if (err == SEC_ERROR_BAD_PASSWORD)
	sprintf(SECUErrorBuf, "Incorrect password");
    else if (err == SEC_ERROR_BAD_SIGNATURE)
	sprintf(SECUErrorBuf, "Bad signature");
    else if (err == SEC_ERROR_EXPIRED_CERTIFICATE)
	sprintf(SECUErrorBuf, "Expired certificate");
    else if (err == SEC_ERROR_EXTENSION_VALUE_INVALID)
	sprintf(SECUErrorBuf, "Invalid extension value");
    else if (err == SEC_ERROR_INPUT_LEN)
	sprintf(SECUErrorBuf, "Problem with input length");
    else if (err == SEC_ERROR_INVALID_ALGORITHM)
	sprintf(SECUErrorBuf, "Invalid algorithm");
    else if (err == SEC_ERROR_INVALID_ARGS)
	sprintf(SECUErrorBuf, "Invalid arguments");
    else if (err == SEC_ERROR_INVALID_AVA)
	sprintf(SECUErrorBuf, "Invalid AVA");
    else if (err == SEC_ERROR_INVALID_TIME)
	sprintf(SECUErrorBuf, "Invalid time");
    else if (err == SEC_ERROR_IO)
	sprintf(SECUErrorBuf, "Security I/O error");
    else if (err == SEC_ERROR_LIBRARY_FAILURE)
	sprintf(SECUErrorBuf, "Library failure");
    else if (err == SEC_ERROR_NO_MEMORY)
	sprintf(SECUErrorBuf, "Out of memory");
    else if (err == SEC_ERROR_OLD_CRL)
	sprintf(SECUErrorBuf, "CRL is older than the current one");
    else if (err == SEC_ERROR_OUTPUT_LEN)
	sprintf(SECUErrorBuf, "Problem with output length");
    else if (err == SEC_ERROR_UNKNOWN_ISSUER)
	sprintf(SECUErrorBuf, "Unknown issuer");
    else if (err == SEC_ERROR_UNTRUSTED_CERT)
	sprintf(SECUErrorBuf, "Untrusted certificate");
    else if (err == SEC_ERROR_UNTRUSTED_ISSUER)
	sprintf(SECUErrorBuf, "Untrusted issuer");
    else if (err == SSL_ERROR_BAD_CERTIFICATE)
	sprintf(SECUErrorBuf, "Bad certificate");
    else if (err == SSL_ERROR_BAD_CLIENT)
	sprintf(SECUErrorBuf, "Bad client");
    else if (err == SSL_ERROR_BAD_SERVER)
	sprintf(SECUErrorBuf, "Bad server");
    else if (err == SSL_ERROR_EXPORT_ONLY_SERVER)
	sprintf(SECUErrorBuf, "Export only server");
    else if (err == SSL_ERROR_NO_CERTIFICATE)
	sprintf(SECUErrorBuf, "No certificate");
    else if (err == SSL_ERROR_NO_CYPHER_OVERLAP)
	sprintf(SECUErrorBuf, "No cypher overlap");
    else if (err == SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE)
	sprintf(SECUErrorBuf, "Unsupported certificate type");
    else if (err == SSL_ERROR_UNSUPPORTED_VERSION)
	sprintf(SECUErrorBuf, "Unsupported version");
    else if (err == SSL_ERROR_US_ONLY_SERVER)
	sprintf(SECUErrorBuf, "U.S. only server");
    else if (err == PR_IO_ERROR)
	sprintf(SECUErrorBuf, "I/O error");

    else if (err == SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE)
        sprintf (SECUErrorBuf, "Expired Issuer Certificate");
    else if (err == SEC_ERROR_REVOKED_CERTIFICATE)
        sprintf (SECUErrorBuf, "Revoked certificate");
    else if (err == SEC_ERROR_NO_KEY)
        sprintf (SECUErrorBuf, "No private key in database for this cert");
    else if (err == SEC_ERROR_CERT_NOT_VALID)
        sprintf (SECUErrorBuf, "Certificate is not valid");
    else if (err == SEC_ERROR_EXTENSION_NOT_FOUND)
        sprintf (SECUErrorBuf, "Certificate extension was not found");
    else if (err == SEC_ERROR_EXTENSION_VALUE_INVALID)
        sprintf (SECUErrorBuf, "Certificate extension value invalid");
    else if (err == SEC_ERROR_CA_CERT_INVALID)
        sprintf (SECUErrorBuf, "Issuer certificate is invalid");
    else if (err == SEC_ERROR_CERT_USAGES_INVALID)
        sprintf (SECUErrorBuf, "Certificate usages is invalid");
    else if (err == SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION)
        sprintf (SECUErrorBuf, "Certificate has unknown critical extension");
    else if (err == SEC_ERROR_PKCS7_BAD_SIGNATURE)
        sprintf (SECUErrorBuf, "Bad PKCS7 signature");
    else if (err == SEC_ERROR_INADEQUATE_KEY_USAGE)
        sprintf (SECUErrorBuf, "Certificate not approved for this operation");
    else if (err == SEC_ERROR_INADEQUATE_CERT_TYPE)
        sprintf (SECUErrorBuf, "Certificate not approved for this operation");

    return SECUErrorBuf;
}

char *
SECU_ErrorString(int16 err)
{
    char *error_string;

    *SECUErrorBuf = 0;
    SECU_ErrorStringRaw (err);

    if (*SECUErrorBuf == 0) { 
	error_string = SECU_GetString(err);
	if (error_string == NULL || *error_string == '\0') 
	    sprintf(SECUErrorBuf, "No error string found for %d.",  err);
	else
	    return error_string;
    }

    return SECUErrorBuf;
}
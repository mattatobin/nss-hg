#! /bin/ksh  
#
# This is just a quick script so we can still run our testcases.
# Longer term we need a scriptable test environment..
#
. ../common/init.sh
CURDIR=`pwd`
PORT=${PORT-8443}

# Test case files
SSLCOV=${CURDIR}/sslcov.txt
SSLAUTH=${CURDIR}/sslauth.txt
SSLSTRESS=${CURDIR}/sslstress.txt
REQUEST_FILE=${CURDIR}/sslreq.txt

#temparary files
PWFILE=/tmp/tests.pw.$$
CERTSCRIPT=/tmp/tests.certs.$$
NOISE_FILE=/tmp/tests.noise.$$
SERVEROUTFILE=/tmp/tests.server.$$

TEMPFILES="${PWFILE} ${CERTSCRIPT} ${SERVEROUTFILE} ${NOISE_FILE}"

none=1
coverage=0
auth=0
stress=0
fileout=0

for i in $*
do
    case $i in
    [aA][lL]*)
	none=0; coverage=1; auth=1; stress=1;;
    [aA][uU]*)
	none=0; auth=1;;
    [Cc]*)
	none=0; coverage=1;;
    [Ss]*)
	none=0; stress=1;;
     f)
	fileout=1;
     esac
done

if [ $none -eq 1 ]; then
    coverage=1
    auth=1
    stress=1
fi
    

#
# should also try to kill any running server
#
trap "rm -f ${TEMPFILES};  exit"  2 3


# Generate noise for our CA cert.
#
# NOTE: these keys are only suitable for testing, as this whole thing bypasses
# the entropy gathering. Don't use this method to generate keys and certs for
# product use or deployment.
#
ps -efl > ${NOISE_FILE} 2>&1
ps aux >> ${NOISE_FILE} 2>&1
netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1

#
# build the TEMP CA used for testing purposes
# 
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>Certutil Tests</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}
CADIR=${HOSTDIR}/CA
echo "********************** Creating a CA Certificate **********************"
if [ ! -d ${CADIR} ]; then
   mkdir -p ${CADIR}
fi
cd ${CADIR}
echo nss > ${PWFILE}
echo "   certutil -N -d . -f ${PWFILE}"
certutil -N -d . -f ${PWFILE}

echo 5 > ${CERTSCRIPT}
echo 9 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo y >> ${CERTSCRIPT}
echo 3 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo 5 >> ${CERTSCRIPT}
echo 6 >> ${CERTSCRIPT}
echo 7 >> ${CERTSCRIPT}
echo 9 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo    "certutil -S -n \"TestCA\" -s \"CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US\" -t \"CTu,CTu,CTu\" -v 60 -x -d . -1 -2 -5 -f ${PWFILE} -z ${NOISE_FILE}"
certutil -S -n "TestCA" -s "CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US" -t "CTu,CTu,CTu" -v 60 -x -d . -1 -2 -5 -f ${PWFILE} -z ${NOISE_FILE} < ${CERTSCRIPT}

if [ $? -ne 0 ]; then
    echo "<TR><TD>Creating CA Cert</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating CA Cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "**************** Creating Client CA Issued Certificate ****************"
netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1
CLIENTDIR=${HOSTDIR}/client
if [ ! -d ${CLIENTDIR} ]; then
   mkdir -p ${CLIENTDIR}
fi
cd ${CLIENTDIR}
cp ${CADIR}/*.db .
echo  "certutil -S -n \"TestUser\" -s \"CN=Test User, O=BOGUS Netscape, L=Mountain View, ST=California, C=US\" -t \"u,u,u\" -c "TestCA" -m 3 -v 60 -d . -f ${PWFILE} -z ${NOISE_FILE}"
certutil -S -n "TestUser" -s "CN=Test User, O=BOGUS NSS, L=Mountain View, ST=California, C=US" -t "u,u,u" -c "TestCA" -m 3 -v 60 -d . -f ${PWFILE} -z ${NOISE_FILE}
if [ $? -ne 0 ]; then
    echo "<TR><TD>Creating client Cert</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating client Cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi

echo "***** Creating Server CA Issued Certificate for ${HOST}.${DOMSUF} *****"
netstat >> ${NOISE_FILE} 2>&1
date >> ${NOISE_FILE} 2>&1
SERVERDIR=${HOSTDIR}/server
if [ ! -d ${SERVERDIR} ]; then
   mkdir -p ${SERVERDIR}
fi
cd ${SERVERDIR}
cp ../CA/*.db .
echo "certutil -S -n \"${HOST}.${DOMSUF}\" -s \"CN=${HOST}.${DOMSUF}, O=BOGUS Netscape, L=Mountain View, ST=California, C=US\" -t \"Pu,Pu,Pu\" -c "TestCA" -v 60  -d . -f ${PWFILE} -z ${NOISE_FILE}"
certutil -S -n "${HOST}.${DOMSUF}" -s "CN=${HOST}.${DOMSUF}, O=BOGUS Netscape, L=Mountain View, ST=California, C=US" -t "Pu,Pu,Pu" -c "TestCA" -m 1 -v 60 -d . -f ${PWFILE} -z ${NOISE_FILE}
if [ $? -ne 0 ]; then
    echo "<TR><TD>Creating Server Cert</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
else
    echo "<TR><TD>Creating Server Cert</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
fi
echo "</TABLE><BR>" >> ${RESULTS}

rm -f ${TEMPFILES}


# OK now lets run the tests....
if [ $coverage -eq 1 ]; then
echo "********************* SSL Cipher Coverage  ****************************"
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>SSL Cipher Coverage</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}
cd ${CLIENTDIR}
 cat ${SSLCOV} | while read tls param testname
do
    if [ $tls != "#" ]; then
	echo "********************* $testname ****************************"
	TLS_FLAG=-T
	if [ $tls = "TLS" ]; then
	    TLS_FLAG=""
	fi
	sparam=""
	if [ ${param} = "i" ]; then
		sparam='-c i'
	fi
	if [ ${fileout} -eq 1 ]; then
	    selfserv -v -p ${PORT} -d ${SERVERDIR} -n ${HOST}.${DOMSUF} -w nss ${sparam} > ${SERVEROUTFILE} 2>&1 &
	else
	    selfserv -v -p ${PORT} -d ${SERVERDIR} -n ${HOST}.${DOMSUF} -w nss ${sparam} &
	fi
	SERVERPID=$!
	sleep 10

	tstclnt -p ${PORT} -h ${HOST} -c ${param} ${TLS_FLAG} -f -d . < ${REQUEST_FILE}
	if [ $? -ne 0 ]; then
	    echo "<TR><TD>"${testname}"</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
	else
	    echo "<TR><TD>"${testname}"</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
	fi
	${KILL} ${SERVERPID}
	wait ${SERVERPID}
	if [ ${fileout} -eq 1 ]; then
	   cat ${SERVEROUTFILE}
	fi
    fi
done 

echo "</TABLE><BR>" >> ${RESULTS}
fi

if [ $auth -eq 1 ]; then
echo "********************* SSL Client Auth  ****************************"
cd ${CLIENTDIR}
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>SSL Client Authentication</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}

cat ${SSLAUTH} | while read value sparam cparam testname
do
    if [ $value != "#" ]; then
	echo "***** $testname ****"
	sparam=`echo $sparam | sed -e 's;_; ;g'`
	cparam=`echo $cparam | sed -e 's;_; ;g'`
	echo "selfserv -v -p ${PORT} -d ${SERVERDIR} -n ${HOST}.${DOMSUF} -w nss ${sparam} &"
	if [ ${fileout} -eq 1 ]; then
	    selfserv -v -p ${PORT} -d ${SERVERDIR} -n ${HOST}.${DOMSUF} -w nss ${sparam} > ${SERVEROUTFILE} 2>&1 &
	else
	    selfserv -v -p ${PORT} -d ${SERVERDIR} -n ${HOST}.${DOMSUF} -w nss ${sparam} &
	fi
	SERVERPID=$!
	sleep 10
	pwd
	echo "tstclnt -p ${PORT} -h ${HOST} -f -d ${CLIENTDIR} ${cparam}"
	tstclnt -p ${PORT} -h ${HOST} -f -d ${CLIENTDIR} ${cparam} < ${REQUEST_FILE}
	if [ $? -ne $value ]; then
	    echo "<TR><TD>"${testname}"</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
	else
	    echo "<TR><TD>"${testname}"</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
	fi
	${KILL} ${SERVERPID}
	wait ${SERVERPID}
	if [ ${fileout} -eq 1 ]; then
	   cat ${SERVEROUTFILE}
	fi
     fi
done 

echo "</TABLE><BR>" >> ${RESULTS}
fi


if [ $stress -eq 1 ]; then
echo "********************* Stress Test  ****************************"
cd ${CLIENTDIR}
echo "<TABLE BORDER=1><TR><TH COLSPAN=3>SSL Stress Test</TH></TR>" >> ${RESULTS}
echo "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" >> ${RESULTS}

cat ${SSLSTRESS} | while read value sparam cparam testname
do
    if [ $value != "#" ]; then
	echo "********************* $testname ****************************"
	sparam=`echo $sparam | sed -e 's;_; ;g'`
	cparam=`echo $cparam | sed -e 's;_; ;g'`
	if [ ${fileout} -eq 1 ]; then
	    selfserv -p ${PORT} -d ${SERVERDIR}  -n ${HOST}.${DOMSUF} -w nss ${sparam} > ${SERVEROUTFILE} 2>&1 &
	else
	    selfserv -p ${PORT} -d ${SERVERDIR}  -n ${HOST}.${DOMSUF} -w nss ${sparam} &
	fi
	SERVERPID=$!
	sleep 10

	strsclnt -p ${PORT} ${HOST} -d . -w nss $cparam 
	if [ $? -ne $value ]; then
	    echo "<TR><TD>"${testname}"</TD><TD bgcolor=red>Failed</TD><TR>" >> ${RESULTS}
	else
	    echo "<TR><TD>"${testname}"</TD><TD bgcolor=lightGreen>Passed</TD><TR>" >> ${RESULTS}
	fi
	${KILL} ${SERVERPID}
	wait ${SERVERPID}
	if [ ${fileout} -eq 1 ]; then
	   cat ${SERVEROUTFILE}
	fi
     fi
done 

echo "</TABLE><BR>" >> ${RESULTS}
fi

rm -f ${TEMPFILES}

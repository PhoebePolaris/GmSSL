﻿/* 
 *   Copyright 2014-2021 The GmSSL Project Authors. All Rights Reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gmssl/pem.h>
#include <gmssl/x509.h>
#include <gmssl/pkcs8.h>
#include <gmssl/rand.h>
#include <gmssl/error.h>


#ifndef WIN32
#include <pwd.h>
#include <unistd.h>
#endif


/*
from RFC 2253

String  X.500 AttributeType
------------------------------
CN      commonName
L       localityName
ST      stateOrProvinceName
O       organizationName
OU      organizationalUnitName
C       countryName
STREET  streetAddress
DC      domainComponent
UID     userid
*/

void print_usage(const char *prog)
{
	printf("Usage: %s command [options] ...\n", prog);
	printf("\n");
	printf("Options:\n");
	printf("  -C <str>           country name\n");
	printf("  -O <str>           orgnization name\n");
	printf("  -OU <str>          orgnizational unit name\n");
	printf("  -CN <str>          common name\n");
	printf("  -L <str>           locality name\n");
	printf("  -ST <str>          state of province name\n");
	printf("  -days <num>        validity days\n");
	printf("  -key <file>        private key file\n");
	printf("  -pass pass         password\n");
}

int main(int argc, char **argv)
{
	int ret = -1;
	char *prog = argv[0];
	char *country = NULL;
	char *state = NULL;
	char *org = NULL;
	char *org_unit = NULL;
	char *common_name = NULL;
	char *keyfile = NULL;
	int days = 0;
	char *outfile = NULL;

	FILE *keyfp = NULL;
	FILE *outfp = stdout;

	X509_CERTIFICATE cert;

	char *pass = NULL;

	uint8_t serial[12];
	X509_NAME name;
	time_t not_before;
	SM2_KEY sm2_key; // 这个应该是从文件中读取的！
	uint8_t uniq_id[32];

	uint8_t buf[1024];
	const uint8_t *cp = buf;
	uint8_t *p = buf;
	size_t len = 0;



	int kp[] = {
		OID_kp_serverAuth,
		OID_kp_clientAuth,
		OID_kp_codeSigning,
		OID_kp_emailProtection,
		OID_kp_timeStamping,
		OID_kp_OCSPSigning,
	};

	argc--;
	argv++;

	while (argc > 0) {
		if (!strcmp(*argv, "-help")) {
			print_usage(prog);
			return 0;

		} else if (!strcmp(*argv, "-CN")) {
			if (--argc < 1) goto bad;
			common_name = *(++argv);

		} else if (!strcmp(*argv, "-O")) {
			if (--argc < 1) goto bad;
			org = *(++argv);

		} else if (!strcmp(*argv, "-OU")) {
			if (--argc < 1) goto bad;
			org_unit = *(++argv);

		} else if (!strcmp(*argv, "-C")) {
			if (--argc < 1) goto bad;
			country = *(++argv);

		} else if (!strcmp(*argv, "-ST")) {
			if (--argc < 1) goto bad;
			state = *(++argv);

		} else if (!strcmp(*argv, "-key")) {
			if (--argc < 1) goto bad;
			keyfile = *(++argv);

		} else if (!strcmp(*argv, "-days")) {
			if (--argc < 1) goto bad;
			days = atoi(*(++argv));

		} else if (!strcmp(*argv, "-pass")) {
			if (--argc < 1) goto bad;
			pass = *(++argv);

		} else if (!strcmp(*argv, "-out")) {
			if (--argc < 1) goto bad;
			outfile = *(++argv);


		} else {
			fprintf(stderr, "%s: illegal option '%s'\n", prog, *argv);
			print_usage(prog);
			return 0;
		}

		argc--;
		argv++;
	}

	if (days <= 0 || !keyfile) {
		error_print();
		goto bad;
	}

	if (!(keyfp = fopen(keyfile, "r"))) {
		error_print();
		goto bad;
	}

	if (!pass) {
#ifndef WIN32
		pass = getpass("Encryption Password : ");
#else
		fprintf(stderr, "%s: '-pass' option required\n", prog);
#endif
	}

	if (outfile) {
		if (!(outfp = fopen(outfile, "wb"))) {
			error_print();
			return -1;
		}
	}

	if (sm2_enced_private_key_info_from_pem(&sm2_key, pass, keyfp) != 1) {
		error_print();
		goto end;
	}


	rand_bytes(serial, sizeof(serial));



	memset(&name, 0, sizeof(name));



	if (country) {
		if (x509_name_set_country(&name, country) != 1) {
			error_print();
			goto end;
		}
	}
	if (state) {
		if (x509_name_set_state_or_province(&name, state) != 1) {
			error_print();
			goto end;
		}
	}
	if (org) {
		if (x509_name_set_organization(&name, org) != 1) {
			error_print();
			goto end;
		}
	}
	if (org_unit) {
		if (x509_name_set_organizational_unit(&name, org_unit) != 1) {
			error_print();
			goto end;
		}
	}
	if (!common_name) {
		error_print();
		goto end;
	} else {
		if (x509_name_set_common_name(&name, common_name) != 1) {
			error_print();
			goto end;
		}
	}

	time(&not_before);


	memset(&cert, 0, sizeof(cert));
	x509_certificate_set_version(&cert, X509_version_v3);
	x509_certificate_set_serial_number(&cert, serial, sizeof(serial));
	x509_certificate_set_signature_algor(&cert, OID_sm2sign_with_sm3);
	x509_certificate_set_issuer(&cert, &name);
	x509_certificate_set_subject(&cert, &name);
	x509_certificate_set_validity(&cert, not_before, days);
	x509_certificate_set_subject_public_key_info_sm2(&cert, &sm2_key);
	x509_certificate_set_issuer_unique_id_from_public_key(&cert, &sm2_key);
	x509_certificate_set_subject_unique_id_from_public_key(&cert, &sm2_key);

	x509_certificate_set_basic_constraints(&cert, ASN1_TRUE, ASN1_TRUE, 6);

	x509_certificate_set_ext_key_usage(&cert, ASN1_TRUE, kp, sizeof(kp)/sizeof(kp[0]));

	x509_certificate_generate_subject_key_identifier(&cert, ASN1_TRUE);

	x509_certificate_set_inhibit_any_policy(&cert, ASN1_TRUE, 20);



	x509_certificate_set_policy_constraints(&cert, ASN1_FALSE, 5, 5);




	x509_certificate_sign_sm2(&cert, &sm2_key);
	x509_certificate_to_pem(&cert, outfp);
	ret = 0;
	goto end;

bad:
	fprintf(stderr, "%s: commands should not be used together\n", prog);

end:
	return ret;
}
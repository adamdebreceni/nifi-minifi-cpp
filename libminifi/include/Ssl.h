#pragma once

#include <cstdio>
#include <ctime>

namespace org::apache::nifi::minifi::ssl {

struct EVP_PKEY;
struct X509;
struct stack_of_X509;
struct BIO;
struct PKCS12;
struct ASN1_TIME;
struct BIO_METHOD;

struct EVP_CIPHER;
struct EVP_CIPHER_CTX;
struct ENGINE;

struct ASN1_OBJECT;
struct EXTENDED_KEY_USAGE;

struct SSL;
struct SSL_CTX;
struct SSL_METHOD;

struct X509_STORE_CTX;

struct SHA512_CTX;

extern void(*EVP_PKEY_free)(EVP_PKEY*);
extern void(*X509_free)(X509*);
extern void(*BIO_free)(BIO*);
extern void(*PKCS12_free)(PKCS12*);
extern long(*ERR_peek_last_error)();
extern void(*ERR_error_string_n)(unsigned long, char*, size_t);
extern const ASN1_TIME*(*X509_get0_notAfter)(const X509*);
extern int(*ASN1_length)(const ASN1_TIME*);
extern const unsigned char*(*ASN1_data)(const ASN1_TIME*);
extern int(*ASN1_time_parse)(const char*, size_t, struct tm*, int);

extern const BIO_METHOD*(*BIO_s_file)();
extern BIO*(*BIO_new)(const BIO_METHOD*);

extern PKCS12*(*d2i_PKCS12_bio)(BIO*, PKCS12**);
extern int(*PKCS12_parse)(PKCS12*, const char*, EVP_PKEY**, X509**, stack_of_X509**);
extern void(*X509_pop_free)(stack_of_X509*, void(*)(X509*));
extern int(*X509_num)(stack_of_X509*);
extern X509*(*X509_pop)(stack_of_X509*);
extern X509*(*PEM_read_bio_X509)(BIO*, X509**, int(*)(char*, int, int, void*), void*);
extern X509*(*PEM_read_bio_X509_AUX)(BIO*, X509**, int(*)(char*, int, int, void*), void*);
extern int(*read_filename)(BIO*, const char*);

extern EVP_CIPHER_CTX*(*EVP_CIPHER_CTX_new)();
extern void(*EVP_CIPHER_CTX_free)(EVP_CIPHER_CTX*);
extern const EVP_CIPHER*(*EVP_aes_256_ecb)();
extern int(*EVP_EncryptInit_ex)(EVP_CIPHER_CTX*, const EVP_CIPHER*, ENGINE*, const unsigned char*, const unsigned char*);
extern int(*EVP_CIPHER_CTX_set_padding)(EVP_CIPHER_CTX*, int);
extern int(*EVP_EncryptUpdate)(EVP_CIPHER_CTX*, unsigned char*, int*, const unsigned char*, int);
extern int(*EVP_EncryptFinal_ex)(EVP_CIPHER_CTX*, unsigned char*, int*);

extern int(*EVP_DecryptInit_ex)(EVP_CIPHER_CTX*, const EVP_CIPHER*, ENGINE*, const unsigned char*, const unsigned char*);
extern int(*EVP_DecryptUpdate)(EVP_CIPHER_CTX*, unsigned char*, int*, const unsigned char*, int);
extern int(*EVP_DecryptFinal_ex)(EVP_CIPHER_CTX*, unsigned char*, int*);
extern int(*CRYPTO_memcmp)(const void*, const void*, size_t);


extern void(*EXTENDED_KEY_USAGE_free)(EXTENDED_KEY_USAGE*);
extern int(*ASN1_OBJECT_num)(const EXTENDED_KEY_USAGE*);
extern const ASN1_OBJECT*(*ASN1_OBJECT_value)(const EXTENDED_KEY_USAGE*, int);
extern int(*ASN1_OBJECT_length)(const ASN1_OBJECT*);
extern const unsigned char*(*ASN1_OBJECT_data)(const ASN1_OBJECT*);

extern void(*SSL_library_init)();
extern void(*add_all_algorithms)();
extern void(*SSL_load_error_strings)();

extern SSL*(*SSL_new)(SSL_CTX*);
extern void(*SSL_free)(SSL*);
extern int(*SSL_set_fd)(SSL*, int);
extern void(*set_tlsext_host_name)(SSL*, const char*);
extern int(*SSL_connect)(SSL*);
extern int(*SSL_get_error)(const SSL*, int);
extern int(*SSL_accept)(SSL*);
extern int(*SSL_pending)(const SSL*);
extern int(*SSL_read)(SSL*, void*, int);
extern int(*SSL_write)(SSL*, const void*, int);

extern SSL_CTX*(*SSL_CTX_new)(const SSL_METHOD*);
extern void(*SSL_CTX_free)(SSL_CTX*);

extern const SSL_METHOD*(*TLSv1_2_server_method)();
extern const SSL_METHOD*(*TLSv1_2_client_method)();

extern int(*SSL_CTX_check_private_key)(const SSL_CTX*);
extern void(*SSL_CTX_set_verify)(SSL_CTX*, int, int (*)(int, X509_STORE_CTX*));

extern int(*SSL_CTX_load_verify_locations)(SSL_CTX*, const char*, const char*);
extern int(*SSL_CTX_use_certificate)(SSL_CTX*, X509*);
extern int(*add_extra_chain_cert)(SSL_CTX*, X509*);
extern int(*SSL_CTX_use_PrivateKey)(SSL_CTX*, EVP_PKEY*);

extern int(*SSL_CTX_set_default_verify_paths)(SSL_CTX*);
extern int(*SSL_CTX_use_certificate_chain_file)(SSL_CTX*, const char*);
extern void(*SSL_CTX_set_default_passwd_cb_userdata)(SSL_CTX*, void*);
extern void(*SSL_CTX_set_default_passwd_cb)(SSL_CTX*, int(*)(char*, int, int, void*));
extern int(*SSL_CTX_use_PrivateKey_file)(SSL_CTX*, const char*, int);

extern void(*ERR_print_errors_fp)(FILE *fp);

extern int(*SHA512_Update)(SHA512_CTX*, const void*, size_t);
extern int(*SHA512_Init)(SHA512_CTX*);
extern int(*SHA512_Final)(unsigned char*, SHA512_CTX*);
extern SHA512_CTX*(*SHA512_new)();
extern void(*SHA512_free)(SHA512_CTX*);

extern EXTENDED_KEY_USAGE*(*d2i_EXTENDED_KEY_USAGE)(EXTENDED_KEY_USAGE*, const unsigned char**, long);

extern int VERIFY_PEER;
extern int FILETYPE_PEM;
extern int ERROR_WANT_WRITE;
extern int ERROR_WANT_READ;
extern int SSL_SHA512_DIGEST_LENGTH;

void initializeSsl();

}  // namespace org::apache::nifi::minifi::ssl

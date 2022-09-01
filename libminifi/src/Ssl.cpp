#include "Ssl.h"

#include <filesystem>
#include <memory>
#include <dlfcn.h>

// #include "core/extension/DynamicLibrary.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::ssl {

void(*EVP_PKEY_free)(EVP_PKEY*);
void(*X509_free)(X509*);
void(*BIO_free)(BIO*);
void(*PKCS12_free)(PKCS12*);
long(*ERR_peek_last_error)();
void(*ERR_error_string_n)(unsigned long, char*, size_t);
const ASN1_TIME*(*X509_get0_notAfter)(const X509*);
int(*ASN1_length)(const ASN1_TIME*);
const unsigned char*(*ASN1_data)(const ASN1_TIME*);
int(*ASN1_time_parse)(const char*, size_t, struct tm*, int);

const BIO_METHOD*(*BIO_s_file)();
BIO*(*BIO_new)(const BIO_METHOD*);

PKCS12*(*d2i_PKCS12_bio)(BIO*, PKCS12**);
int(*PKCS12_parse)(PKCS12*, const char*, EVP_PKEY**, X509**, stack_of_X509**);
void(*X509_pop_free)(stack_of_X509*, void(*)(X509*));
int(*X509_num)(stack_of_X509*);
X509*(*X509_pop)(stack_of_X509*);
X509*(*PEM_read_bio_X509)(BIO*, X509**, int(*)(char*, int, int, void*), void*);
X509*(*PEM_read_bio_X509_AUX)(BIO*, X509**, int(*)(char*, int, int, void*), void*);
int(*read_filename)(BIO*, const char*);

EVP_CIPHER_CTX*(*EVP_CIPHER_CTX_new)();
void(*EVP_CIPHER_CTX_free)(EVP_CIPHER_CTX*);
const EVP_CIPHER*(*EVP_aes_256_ecb)();
int(*EVP_EncryptInit_ex)(EVP_CIPHER_CTX*, const EVP_CIPHER*, ENGINE*, const unsigned char*, const unsigned char*);
int(*EVP_CIPHER_CTX_set_padding)(EVP_CIPHER_CTX*, int);
int(*EVP_EncryptUpdate)(EVP_CIPHER_CTX*, unsigned char*, int*, const unsigned char*, int);
int(*EVP_EncryptFinal_ex)(EVP_CIPHER_CTX*, unsigned char*, int*);

int(*EVP_DecryptInit_ex)(EVP_CIPHER_CTX*, const EVP_CIPHER*, ENGINE*, const unsigned char*, const unsigned char*);
int(*EVP_DecryptUpdate)(EVP_CIPHER_CTX*, unsigned char*, int*, const unsigned char*, int);
int(*EVP_DecryptFinal_ex)(EVP_CIPHER_CTX*, unsigned char*, int*);
int(*CRYPTO_memcmp)(const void*, const void*, size_t);


void(*EXTENDED_KEY_USAGE_free)(EXTENDED_KEY_USAGE*);
int(*ASN1_OBJECT_num)(const EXTENDED_KEY_USAGE*);
const ASN1_OBJECT*(*ASN1_OBJECT_value)(const EXTENDED_KEY_USAGE*, int);
int(*ASN1_OBJECT_length)(const ASN1_OBJECT*);
const unsigned char*(*ASN1_OBJECT_data)(const ASN1_OBJECT*);

void(*SSL_library_init)();
void(*add_all_algorithms)();
void(*SSL_load_error_strings)();

SSL*(*SSL_new)(SSL_CTX*);
void(*SSL_free)(SSL*);
int(*SSL_set_fd)(SSL*, int);
void(*set_tlsext_host_name)(SSL*, const char*);
int(*SSL_connect)(SSL*);
int(*SSL_get_error)(const SSL*, int);
int(*SSL_accept)(SSL*);
int(*SSL_pending)(const SSL*);
int(*SSL_read)(SSL*, void*, int);
int(*SSL_write)(SSL*, const void*, int);

SSL_CTX*(*SSL_CTX_new)(const SSL_METHOD*);
void(*SSL_CTX_free)(SSL_CTX*);

const SSL_METHOD*(*TLSv1_2_server_method)();
const SSL_METHOD*(*TLSv1_2_client_method)();

int(*SSL_CTX_check_private_key)(const SSL_CTX*);
void(*SSL_CTX_set_verify)(SSL_CTX*, int, int (*)(int, X509_STORE_CTX*));

int(*SSL_CTX_load_verify_locations)(SSL_CTX*, const char*, const char*);
int(*SSL_CTX_use_certificate)(SSL_CTX*, X509*);
int(*add_extra_chain_cert)(SSL_CTX*, X509*);
int(*SSL_CTX_use_PrivateKey)(SSL_CTX*, EVP_PKEY*);

int(*SSL_CTX_set_default_verify_paths)(SSL_CTX*);
int(*SSL_CTX_use_certificate_chain_file)(SSL_CTX*, const char*);
void(*SSL_CTX_set_default_passwd_cb_userdata)(SSL_CTX*, void*);
void(*SSL_CTX_set_default_passwd_cb)(SSL_CTX*, int(*)(char*, int, int, void*));
int(*SSL_CTX_use_PrivateKey_file)(SSL_CTX*, const char*, int);

void(*ERR_print_errors_fp)(FILE *fp);

int(*SHA512_Update)(SHA512_CTX*, const void*, size_t);
int(*SHA512_Init)(SHA512_CTX*);
int(*SHA512_Final)(unsigned char*, SHA512_CTX*);
SHA512_CTX*(*SHA512_new)();
void(*SHA512_free)(SHA512_CTX*);

EXTENDED_KEY_USAGE*(*d2i_EXTENDED_KEY_USAGE)(EXTENDED_KEY_USAGE*, const unsigned char**, long);

int VERIFY_PEER;
int FILETYPE_PEM;
int ERROR_WANT_WRITE;
int ERROR_WANT_READ;
int SSL_SHA512_DIGEST_LENGTH;

#define LOAD_FUN(name) (name = reinterpret_cast<decltype(name)>(dlsym(lib, #name)), assert(name))
#define LOAD_CONST(name) (name = *reinterpret_cast<decltype(&name)>(dlsym(lib, #name)))

void initializeSsl() {
#ifdef WIN32
  static constexpr const char* lib_name = "minifi-ssl.dll";
#elifdef APPLE
  static constexpr const char* lib_name = "libminifi-ssl.dylib";
#else
  static constexpr const char* lib_name = "libminifi-ssl.so";
#endif
  static void* lib = [] {
    auto lib_path = std::filesystem::path{utils::file::FileUtils::get_executable_dir()} / lib_name;
    void* lib = dlopen(lib_path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
      throw std::runtime_error("Failed to load minifi-ssl library");
    }
    LOAD_FUN(EVP_PKEY_free);
    LOAD_FUN(X509_free);
    LOAD_FUN(BIO_free);
    LOAD_FUN(PKCS12_free);
    LOAD_FUN(ERR_peek_last_error);
    LOAD_FUN(ERR_error_string_n);
    LOAD_FUN(X509_get0_notAfter);
    LOAD_FUN(ASN1_length);
    LOAD_FUN(ASN1_data);
    LOAD_FUN(ASN1_time_parse);

    LOAD_FUN(BIO_s_file);
    LOAD_FUN(BIO_new);

    LOAD_FUN(d2i_PKCS12_bio);
    LOAD_FUN(PKCS12_parse);
    LOAD_FUN(X509_pop_free);
    LOAD_FUN(X509_num);
    LOAD_FUN(X509_pop);
    LOAD_FUN(PEM_read_bio_X509);
    LOAD_FUN(PEM_read_bio_X509_AUX);
    LOAD_FUN(read_filename);

    LOAD_FUN(EVP_CIPHER_CTX_new);
    LOAD_FUN(EVP_CIPHER_CTX_free);
    LOAD_FUN(EVP_aes_256_ecb);
    LOAD_FUN(EVP_EncryptInit_ex);
    LOAD_FUN(EVP_CIPHER_CTX_set_padding);
    LOAD_FUN(EVP_EncryptUpdate);
    LOAD_FUN(EVP_EncryptFinal_ex);

    LOAD_FUN(EVP_DecryptInit_ex);
    LOAD_FUN(EVP_DecryptUpdate);
    LOAD_FUN(EVP_DecryptFinal_ex);
    LOAD_FUN(CRYPTO_memcmp);


    LOAD_FUN(EXTENDED_KEY_USAGE_free);
    LOAD_FUN(ASN1_OBJECT_num);
    LOAD_FUN(ASN1_OBJECT_value);
    LOAD_FUN(ASN1_OBJECT_length);
    LOAD_FUN(ASN1_OBJECT_data);

    LOAD_FUN(SSL_library_init);
    LOAD_FUN(add_all_algorithms);
    LOAD_FUN(SSL_load_error_strings);

    LOAD_FUN(SSL_new);
    LOAD_FUN(SSL_free);
    LOAD_FUN(SSL_set_fd);
    LOAD_FUN(set_tlsext_host_name);
    LOAD_FUN(SSL_connect);
    LOAD_FUN(SSL_get_error);
    LOAD_FUN(SSL_accept);
    LOAD_FUN(SSL_pending);
    LOAD_FUN(SSL_read);
    LOAD_FUN(SSL_write);

    LOAD_FUN(SSL_CTX_new);
    LOAD_FUN(SSL_CTX_free);

    LOAD_FUN(TLSv1_2_server_method);
    LOAD_FUN(TLSv1_2_client_method);

    LOAD_FUN(SSL_CTX_check_private_key);
    LOAD_FUN(SSL_CTX_set_verify);

    LOAD_FUN(SSL_CTX_load_verify_locations);
    LOAD_FUN(SSL_CTX_use_certificate);
    LOAD_FUN(add_extra_chain_cert);
    LOAD_FUN(SSL_CTX_use_PrivateKey);

    LOAD_FUN(SSL_CTX_set_default_verify_paths);
    LOAD_FUN(SSL_CTX_use_certificate_chain_file);
    LOAD_FUN(SSL_CTX_set_default_passwd_cb_userdata);
    LOAD_FUN(SSL_CTX_set_default_passwd_cb);
    LOAD_FUN(SSL_CTX_use_PrivateKey_file);

    LOAD_FUN(ERR_print_errors_fp);

    LOAD_FUN(SHA512_Update);
    LOAD_FUN(SHA512_Init);
    LOAD_FUN(SHA512_Final);
    LOAD_FUN(SHA512_new);
    LOAD_FUN(SHA512_free);

    LOAD_FUN(d2i_EXTENDED_KEY_USAGE);

    LOAD_CONST(VERIFY_PEER);
    LOAD_CONST(FILETYPE_PEM);
    LOAD_CONST(ERROR_WANT_WRITE);
    LOAD_CONST(ERROR_WANT_READ);
    LOAD_CONST(SSL_SHA512_DIGEST_LENGTH);

    return lib;
  }();
}

static int init_ssl = (initializeSsl(),0);

}  // namespace org::apache::nifi::minifi::ssl

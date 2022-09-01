#include <openssl/sha.h>
#include <openssl/objects.h>
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/x509v3.h>

extern "C" {

extern const int VERIFY_PEER = SSL_VERIFY_PEER;
extern const int FILETYPE_PEM = SSL_FILETYPE_PEM;
extern const int ERROR_WANT_WRITE = SSL_ERROR_WANT_WRITE;
extern const int ERROR_WANT_READ = SSL_ERROR_WANT_READ;
extern const int SSL_SHA512_DIGEST_LENGTH = SHA512_DIGEST_LENGTH;

SHA512_CTX* SHA512_new() {
  return new SHA512_CTX();
}
void SHA512_free(SHA512_CTX* ptr) {
  delete ptr;
}

int add_extra_chain_cert(SSL_CTX* ssl, X509* cert) {
  return SSL_CTX_add_extra_chain_cert(ssl, cert);
}

void add_all_algorithms() {
  OpenSSL_add_all_algorithms();
}

int ASN1_length(const ASN1_TIME* time) {
  return time->length;
}

const unsigned char* ASN1_data(const ASN1_TIME* time) {
  return time->data;
}

int ASN1_OBJECT_length(const ASN1_OBJECT* obj) {
  return obj->length;
}

const unsigned char* ASN1_OBJECT_data(const ASN1_OBJECT* obj) {
  return obj->data;
}

void set_tlsext_host_name(SSL* ssl, const char* name) {
  SSL_set_tlsext_host_name(ssl, name);
}

void X509_pop_free(STACK_OF(X509)* stack, void(*cb)(X509*)) {
  return sk_X509_pop_free(stack, cb);
}

int X509_num(STACK_OF(X509)* stack) {
  return sk_X509_num(stack);
}

X509* X509_pop(STACK_OF(X509)* stack) {
  return sk_X509_pop(stack);
}

int read_filename(BIO* bio, const char* name) {
  return BIO_read_filename(bio, name);
}

int ASN1_OBJECT_num(const EXTENDED_KEY_USAGE* usage) {
  return sk_ASN1_OBJECT_num(usage);
}

const ASN1_OBJECT* ASN1_OBJECT_value(const EXTENDED_KEY_USAGE* usage, int i) {
  return sk_ASN1_OBJECT_value(usage, i);
}

}

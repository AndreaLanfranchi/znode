/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "secure.hpp"

#include <random>

#include </zen/node/common/log.hpp>
#include <gsl/gsl_util>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

namespace zen::network {

EVP_PKEY* generate_random_rsa_key_pair(int bits) {
    EVP_PKEY* pkey{nullptr};
    BIGNUM* bn{BN_new()};
    if (bn == nullptr) {
        ZEN_ERROR << "generate_random_rsa_key_pair : Failed to create BIGNUM";
        return nullptr;
    }
    auto bn_free{gsl::finally([bn]() { BN_free(bn); })};

    if (!BN_set_word(bn, RSA_F4)) {
        ZEN_ERROR << "generate_random_rsa_key_pair : Failed to set BIGNUM";
        return nullptr;
    }

    RSA* rsa{RSA_new()};
    if (rsa == nullptr) {
        ZEN_ERROR << "generate_random_rsa_key_pair : Failed to create RSA";
        return nullptr;
    }
    auto rsa_free{gsl::finally([rsa]() { RSA_free(rsa); })};

    RAND_poll();
    RSA_generate_key_ex(rsa, bits, bn, nullptr);
    pkey = EVP_PKEY_new();
    if (pkey == nullptr) {
        ZEN_ERROR << "generate_random_rsa_key_pair : Failed to create EVP_PKEY";
        return nullptr;
    }
    if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
        ZEN_ERROR << "generate_random_rsa_key_pair : Failed to assign RSA to EVP_PKEY";
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    return pkey;
}

X509* network::generate_self_signed_certificate(EVP_PKEY* pkey, const std::string& password) {
    if (!pkey) {
        ZEN_ERROR << "generate_self_signed_certificate : Invalid EVP_PKEY";
        return nullptr;
    }

    X509* x509_certificate = X509_new();
    if (!x509_certificate) {
        ZEN_ERROR << "generate_self_signed_certificate : Failed to create X509 certificate";
        return nullptr;
    }

    // Generate random serial number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distribution;
    ASN1_INTEGER* serial_number = ASN1_INTEGER_new();
    ASN1_INTEGER_set(serial_number, distribution(gen));
    X509_set_serialNumber(x509_certificate, serial_number);
    ASN1_INTEGER_free(serial_number);

    // Set issuer, subject and validity
    X509_gmtime_adj(X509_get_notBefore(x509_certificate), 0);
    X509_gmtime_adj(X509_get_notAfter(x509_certificate), static_cast<long>(86400 * kCertificateValidityDays));

    X509_NAME* subject = X509_NAME_new();
    X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, (unsigned char*)"zend++.node", -1, -1, 0);
    X509_set_subject_name(x509_certificate, subject);
    X509_set_issuer_name(x509_certificate, subject);
    X509_NAME_free(subject);

    // Set public key
    if (!X509_set_pubkey(x509_certificate, pkey)) {
        ZEN_ERROR << "generate_self_signed_certificate : Failed to set public key";
        X509_free(x509_certificate);
        return nullptr;
    }

    // Sign certificate
    if (!X509_sign(x509_certificate, pkey, EVP_sha256())) {
        ZEN_ERROR << "generate_self_signed_certificate : Failed to sign certificate";
        X509_free(x509_certificate);
        return nullptr;
    }

    return x509_certificate;
}

bool store_rsa_key_pair(EVP_PKEY* pkey, const std::string& password, const std::filesystem::path& path) {
    if (!pkey) {
        ZEN_ERROR << "store_rsa_key_pair : Invalid EVP_PKEY";
        return false;
    }

    if (path.empty() || path.is_relative() || !std::filesystem::is_directory(path)) {
        ZEN_ERROR << "store_rsa_key_pair : Invalid path " << path.string();
        return false;
    }

    auto file_path = path / kPrivateKeyFileName;
    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "wb");
#else
    file = fopen(file_path.string().c_str(), "wb");
#endif

    if (!file) {
        ZEN_ERROR << "store_rsa_key_pair : Failed to open file " << file_path.string();
        return false;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    int err{0};
    if (!password.empty()) {
        err = PEM_write_PKCS8PrivateKey(file, pkey, EVP_aes_256_cbc(), nullptr, 0, nullptr, (void*)password.c_str());
    } else {
        err = PEM_write_PrivateKey(file, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    }

    if (err) {
        char buffer[256];
        ERR_error_string_n(err, buffer, sizeof(buffer));
        ZEN_ERROR << "store_rsa_key_pair : Failed to write private key to file " << file_path.string()
                  << " error: " << buffer;
        return false;
    }

    return true;
}
bool store_x509_certificate(X509* cert, const std::filesystem::path& path) {
    if (!cert) {
        ZEN_ERROR << "store_x509_certificate : Invalid X509 certificate";
        return false;
    }

    if (path.empty() || path.is_relative() || !std::filesystem::is_directory(path)) {
        ZEN_ERROR << "store_x509_certificate : Invalid path " << path.string();
        return false;
    }

    auto file_path = path / kCertificateFileName;
    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "wb");
#else
    file = fopen(file_path.string().c_str(), "wb");
#endif

    if (!file) {
        ZEN_ERROR << "store_x509_certificate : Failed to open file " << file_path.string();
        return false;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    int err{PEM_write_X509(file, cert)};
    if (err) {
        char buffer[256];
        ERR_error_string_n(err, buffer, sizeof(buffer));
        ZEN_ERROR << "store_x509_certificate : Failed to write certificate to file " << file_path.string()
                  << " error: " << buffer;
        return false;
    }

    return true;
}

EVP_PKEY* load_rsa_private_key(const std::filesystem::path& directory_path, const std::string& password) {
    if (!std::filesystem::exists(directory_path) || !std::filesystem::is_directory(directory_path)) {
        ZEN_ERROR << "load_rsa_private_key : Invalid or not existing container directory " << directory_path.string();
        return nullptr;
    }

    auto file_path = directory_path / kPrivateKeyFileName;
    if (std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        ZEN_ERROR << "load_rsa_private_key : Invalid or not existing file " << file_path.string();
        return nullptr;
    }

    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "rb");
#else
    file = fopen(file_path.string().c_str(), "rb");
#endif

    if (!file) {
        ZEN_ERROR << "load_rsa_private_key : Failed to open file " << file_path.string();
        return nullptr;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    EVP_PKEY* pkey{nullptr};
    if (!password.empty()) {
        pkey = PEM_read_PrivateKey(file, nullptr, nullptr, (void*)password.c_str());
    } else {
        pkey = PEM_read_PrivateKey(file, nullptr, nullptr, nullptr);
    }

    if (!pkey) {
        ZEN_ERROR << "load_rsa_private_key : Failed to read private key from file " << file_path.string();
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    return pkey;
}

X509* load_x509_certificate(const std::filesystem::path& directory_path) {

    if (!std::filesystem::exists(directory_path) || !std::filesystem::is_directory(directory_path)) {
        ZEN_ERROR << "load_x509_certificate : Invalid or not existing container directory " << directory_path.string();
        return nullptr;
    }

    auto file_path = directory_path / kCertificateFileName;
    if (std::filesystem::exists(file_path) ||
        !std::filesystem::is_regular_file(file_path)) {
        ZEN_ERROR << "load_x509_certificate : Invalid or not existing file " << file_path.string();
        return nullptr;
    }

    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "rb");
#else
    file = fopen(file_path.string().c_str(), "rb");
#endif

    if (!file) {
        ZEN_ERROR << "load_x509_certificate : Failed to open file " << file_path.string();
        return nullptr;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    X509* cert{PEM_read_X509(file, nullptr, nullptr, nullptr)};
    if (!cert) {
        ZEN_ERROR << "load_x509_certificate : Failed to read certificate from file " << file_path.string();
        X509_free(cert);
        return nullptr;
    }

    return cert;
}
bool validate_certificate(X509* cert, EVP_PKEY* pkey) {
    if (!cert) {
        ZEN_ERROR << "validate_certificate : Invalid X509 certificate";
        return false;
    }

    if (!pkey) {
        ZEN_ERROR << "validate_certificate : Invalid EVP_PKEY";
        return false;
    }

    if (X509_verify(cert, pkey) != 1) {
        ZEN_ERROR << "validate_certificate : Failed to verify certificate";
        return false;
    }

    return true;
}
SSL_CTX* create_tls_context(TLSContextType type, const std::filesystem::path& cert_path,
                            const std::filesystem::path& key_path, const std::string& password) {
    if (!std::filesystem::exists(cert_path) || !std::filesystem::is_regular_file(cert_path) ||
        !std::filesystem::exists(key_path) || !std::filesystem::is_regular_file(key_path)) {
        ZEN_ERROR << "create_tls_context : Invalid or non existent certificate or key file";
        return nullptr;
    }

    SSL_CTX* ctx{nullptr};
    switch (type) {
        case TLSContextType::kServer: {
            ctx = SSL_CTX_new(TLS_server_method());
            break;
        }
        case TLSContextType::kClient: {
            ctx = SSL_CTX_new(TLS_client_method());
            break;
        }
        default: {
            ZEN_ERROR << "create_tls_context : Invalid TLS context type";
            return nullptr;
        }
    }

    if (!ctx) {
        ZEN_ERROR << "create_tls_context : Failed to create SSL context";
        return nullptr;
    }

    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
    SSL_CTX_set_options(ctx, SSL_OP_NO_RENEGOTIATION);
    SSL_CTX_set_ecdh_auto(ctx, 1);

    if (type == TLSContextType::kServer) {
        auto x509_cert{load_x509_certificate(cert_path)};
        auto rsa_pkey{load_rsa_private_key(key_path, password)};
        if (!x509_cert || !rsa_pkey) {
            ZEN_ERROR << "create_tls_context : Failed to load certificate or private key";
            SSL_CTX_free(ctx);
            return nullptr;
        }
        if (!validate_certificate(x509_cert, rsa_pkey)) {
            ZEN_ERROR << "create_tls_context : Failed to validate certificate";
            SSL_CTX_free(ctx);
            return nullptr;
        }

        SSL_CTX_use_certificate(ctx, x509_cert);
        SSL_CTX_use_PrivateKey(ctx, rsa_pkey);
    }

    return ctx;
}
}  // namespace zen::network

/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "secure.hpp"

#include <iostream>
#include <random>

#include <gsl/gsl_util>
#include <openssl/pem.h>
#include <openssl/rand.h>

namespace zenpp::net {

void print_ssl_error(unsigned long err, const log::Level severity) {
    if (err == 0U) return;
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    log::BufferBase(severity, "SSL error", {"code", std::to_string(err), "reason", std::string(buf)});
}

EVP_PKEY* generate_random_rsa_key_pair(int bits) {
    EVP_PKEY* pkey{nullptr};

    EVP_PKEY_CTX* ctx{EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr)};
    if (ctx == nullptr) {
        LOGF_ERROR << "Failed to create EVP_PKEY_CTX";
        return nullptr;
    }
    auto ctx_free{gsl::finally([ctx]() { EVP_PKEY_CTX_free(ctx); })};

    EVP_PKEY_keygen_init(ctx);
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) {
        LOGF_ERROR << "Failed to set RSA keygen bits";
        return nullptr;
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        LOGF_ERROR << "Failed to generate RSA key pair";
        return nullptr;
    }

    return pkey;
}

X509* generate_self_signed_certificate(EVP_PKEY* pkey) {
    if (pkey == nullptr) {
        LOG_ERROR << "Invalid EVP_PKEY";
        return nullptr;
    }

    X509* x509_certificate = X509_new();
    if (x509_certificate == nullptr) {
        auto err{ERR_get_error()};
        print_ssl_error(err);
        LOG_ERROR << "Failed to create X509 certificate";
        return nullptr;
    }

    // Generate random serial number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distribution;
    long random_number{distribution(gen)};
    ASN1_INTEGER_set(X509_get_serialNumber(x509_certificate), random_number);

    // Set issuer, subject and validity
    X509_gmtime_adj(X509_get_notBefore(x509_certificate), 0);
    X509_gmtime_adj(X509_get_notAfter(x509_certificate), static_cast<long>(86400 * kCertificateValidityDays));

    X509_NAME* subject = X509_NAME_new();
    X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("zenpp.node"), -1,
                               -1, 0);
    X509_set_subject_name(x509_certificate, subject);
    X509_set_issuer_name(x509_certificate, subject);
    X509_NAME_free(subject);

    // Set public key
    if (X509_set_pubkey(x509_certificate, pkey) == 0) {
        auto err{ERR_get_error()};
        print_ssl_error(err);
        LOG_ERROR << "Failed to set public key";
        X509_free(x509_certificate);
        return nullptr;
    }

    // Sign certificate
    if (X509_sign(x509_certificate, pkey, EVP_sha256()) == 0) {
        auto err{ERR_get_error()};
        print_ssl_error(err);
        LOG_ERROR << "Failed to sign certificate";
        X509_free(x509_certificate);
        return nullptr;
    }

    return x509_certificate;
}

bool store_rsa_key_pair(EVP_PKEY* pkey, const std::string& password, const std::filesystem::path& directory_path) {
    if (pkey == nullptr) {
        LOG_ERROR << "Invalid EVP_PKEY";
        return false;
    }

    if (directory_path.empty() or directory_path.is_relative() or not std::filesystem::is_directory(directory_path)) {
        LOG_ERROR << "Invalid path " << directory_path.string();
        return false;
    }

    const auto file_path = directory_path / kPrivateKeyFileName;
    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "wb");
#else
    file = fopen(file_path.string().c_str(), "wb");
#endif

    if (file == nullptr) {
        LOG_ERROR << "Failed to open file " << file_path.string();
        return false;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    int result{0};
    if (not password.empty()) {
        result = PEM_write_PKCS8PrivateKey(file, pkey, EVP_aes_256_cbc(), const_cast<char*>(password.data()),
                                           static_cast<int>(password.size()), nullptr, nullptr);
    } else {
        result = PEM_write_PrivateKey(file, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    }

    if (result == 0) {
        auto err{ERR_get_error()};
        print_ssl_error(err);
        LOG_ERROR << "Failed to write private key to file " << file_path.string();
        return false;
    }

    return true;
}

bool store_x509_certificate(X509* cert, const std::filesystem::path& directory_path) {
    if (cert == nullptr) {
        LOG_ERROR << "Invalid X509 certificate";
        return false;
    }

    if (directory_path.empty() || directory_path.is_relative() || !std::filesystem::is_directory(directory_path)) {
        LOG_ERROR << "Invalid path " << directory_path.string();
        return false;
    }

    auto file_path = directory_path / kCertificateFileName;
    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "wb");
#else
    file = fopen(file_path.string().c_str(), "wb");
#endif

    if (file == nullptr) {
        LOG_ERROR << "Failed to open file " << file_path.string();
        return false;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    if (PEM_write_X509(file, cert) == 0) {
        auto err{ERR_get_error()};
        print_ssl_error(err);
        LOG_ERROR << "Failed to write certificate to file " << file_path.string();
        return false;
    }

    return true;
}

EVP_PKEY* load_rsa_private_key(const std::filesystem::path& directory_path, const std::string& password) {
    if (!std::filesystem::exists(directory_path) || !std::filesystem::is_directory(directory_path)) {
        LOG_ERROR << "Invalid or not existing container directory " << directory_path.string();
        return nullptr;
    }

    auto file_path = directory_path / kPrivateKeyFileName;
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        LOG_ERROR << "Invalid or not existing file " << file_path.string();
        return nullptr;
    }

    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "rb");
#else
    file = fopen(file_path.string().c_str(), "rb");
#endif

    if (file == nullptr) {
        LOG_ERROR << "Failed to open file " << file_path.string();
        return nullptr;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    EVP_PKEY* pkey{nullptr};
    if (!password.empty()) {
        pkey = PEM_read_PrivateKey(file, nullptr, nullptr,
                                   const_cast<void*>(reinterpret_cast<const void*>(password.data())));
    } else {
        pkey = PEM_read_PrivateKey(file, nullptr, nullptr, nullptr);
    }

    if (pkey == nullptr) {
        LOG_ERROR << "Failed to read private key from file " << file_path.string();
        EVP_PKEY_free(pkey);
        return nullptr;
    }

    return pkey;
}

X509* load_x509_certificate(const std::filesystem::path& directory_path) {
    if (not std::filesystem::exists(directory_path) or not std::filesystem::is_directory(directory_path)) {
        LOG_ERROR << "Invalid or not existing container directory " << directory_path.string();
        return nullptr;
    }

    const auto file_path = directory_path / kCertificateFileName;
    if (not std::filesystem::exists(file_path) or not std::filesystem::is_regular_file(file_path)) {
        LOG_ERROR << "Invalid or not existing file " << file_path.string();
        return nullptr;
    }

    FILE* file{nullptr};

#if defined(_MSC_VER)
    fopen_s(&file, file_path.string().c_str(), "rb");
#else
    file = fopen(file_path.string().c_str(), "rb");
#endif

    if (file == nullptr) {
        LOG_ERROR << "Failed to open file " << file_path.string();
        return nullptr;
    }
    auto file_close{gsl::finally([file]() { fclose(file); })};

    X509* cert{PEM_read_X509(file, nullptr, nullptr, nullptr)};
    if (cert == nullptr) {
        LOG_ERROR << "Failed to read certificate from file " << file_path.string();
        X509_free(cert);
        return nullptr;
    }

    return cert;
}

bool validate_server_certificate(X509* cert, EVP_PKEY* pkey) {
    if (cert == nullptr) {
        LOG_ERROR << "Invalid X509 certificate";
        return false;
    }

    if (pkey == nullptr) {
        LOG_ERROR << "Invalid EVP_PKEY";
        return false;
    }
    // Get the certificate's validity period
    ASN1_TIME* not_before = X509_getm_notBefore(cert);
    ASN1_TIME* not_after = X509_getm_notAfter(cert);

    const auto current_time = time(nullptr);
    if (ASN1_TIME_cmp_time_t(not_before, current_time) == 1 or ASN1_TIME_cmp_time_t(not_after, current_time) == -1) {
        LOG_ERROR << "Certificate is not valid or expired";
        return false;
    }

    if (X509_verify(cert, pkey) not_eq 1) {
        LOG_ERROR << "Failed to verify certificate";
        return false;
    }

    return true;
}

SSL_CTX* generate_tls_context(TLSContextType type, const std::filesystem::path& directory_path,
                              const std::string& key_password) {
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
            LOG_ERROR << "Invalid TLS context type";
            return nullptr;
        }
    }

    if (ctx == nullptr) {
        LOG_ERROR << "Failed to create SSL context";
        return nullptr;
    }

    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_RENEGOTIATION);

    if (type == TLSContextType::kServer) {
        SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
        auto* x509_cert{load_x509_certificate(directory_path)};
        auto* rsa_pkey{load_rsa_private_key(directory_path, key_password)};

        if (x509_cert == nullptr or rsa_pkey == nullptr) {
            LOG_ERROR << "Failed to load certificate or private key from " << directory_path.string();
            if (x509_cert not_eq nullptr) X509_free(x509_cert);
            if (rsa_pkey not_eq nullptr) EVP_PKEY_free(rsa_pkey);
            SSL_CTX_free(ctx);
            return nullptr;
        }

        if (not validate_server_certificate(x509_cert, rsa_pkey)) {
            LOG_ERROR << "Failed to validate certificate (mismatching private key)";
            X509_free(x509_cert);
            EVP_PKEY_free(rsa_pkey);
            SSL_CTX_free(ctx);
            return nullptr;
        }

        if (SSL_CTX_use_certificate(ctx, x509_cert) != 1) {
            auto err{ERR_get_error()};
            print_ssl_error(err);
            LOG_ERROR << "Failed to use certificate for SSL server context";
            X509_free(x509_cert);
            EVP_PKEY_free(rsa_pkey);
            SSL_CTX_free(ctx);
            return nullptr;
        }

        if (SSL_CTX_use_PrivateKey(ctx, rsa_pkey) != 1) {
            auto err{ERR_get_error()};
            print_ssl_error(err);
            LOG_ERROR << "Failed to use private key for SSL server context";
            X509_free(x509_cert);
            EVP_PKEY_free(rsa_pkey);
            SSL_CTX_free(ctx);
            return nullptr;
        }
    }

    return ctx;
}

bool validate_tls_requirements(const std::filesystem::path& directory_path, const std::string& key_password) {
    auto cert_path = directory_path / kCertificateFileName;
    auto key_path = directory_path / kPrivateKeyFileName;

    if (std::filesystem::exists(cert_path) and std::filesystem::is_regular_file(cert_path) and
        std::filesystem::exists(key_path) and std::filesystem::is_regular_file(key_path)) {
        auto* pkey{load_rsa_private_key(directory_path, key_password)};
        auto* x509_cert{load_x509_certificate(directory_path)};
        if (pkey not_eq nullptr and x509_cert not_eq nullptr and validate_server_certificate(x509_cert, pkey)) {
            EVP_PKEY_free(pkey);
            X509_free(x509_cert);
            return true;
        }

        LOG_ERROR << "Failed to load certificate or private key from " << directory_path.string();
        if (pkey not_eq nullptr) EVP_PKEY_free(pkey);
        if (x509_cert not_eq nullptr) X509_free(x509_cert);
    }

    std::cout << "\n============================================================================================\n"
              << "A certificate (cert.pem) and or a private key (key.pem) are missing or invalid from \n"
              << directory_path.string() << std::endl;
    if (not ask_user_confirmation("Do you want me to (re)generate a new certificate and key ?")) {
        return false;
    }

    std::filesystem::remove(cert_path);  // Ensure removed
    std::filesystem::remove(key_path);

    LOG_TRACE << "Generating new certificate and key";
    auto* pkey{generate_random_rsa_key_pair(static_cast<int>(kCertificateKeyLength))};
    if (pkey == nullptr) {
        LOG_ERROR << "Failed to generate RSA key pair";
        return false;
    }
    auto pkey_free{gsl::finally([pkey]() { EVP_PKEY_free(pkey); })};

    LOG_TRACE << "Generating self signed certificate";
    auto* cert{generate_self_signed_certificate(pkey)};
    if (cert == nullptr) {
        LOG_ERROR << "Failed to generate self signed certificate";
        return false;
    }
    auto cert_free{gsl::finally([cert]() { X509_free(cert); })};

    LOG_TRACE << "Validating certificate";
    if (not validate_server_certificate(cert, pkey)) {
        LOG_ERROR << "Failed to validate certificate (mismatching private key)";
        return false;
    }

    LOG_TRACE << "Saving certificate and private key to files";
    if (not store_x509_certificate(cert, directory_path)) {
        LOG_ERROR << "Failed to save certificate to file " << cert_path.string();
        return false;
    }
    LOG_TRACE << "Saving private key to file";
    if (not store_rsa_key_pair(pkey, key_password, directory_path)) {
        LOG_ERROR << "Failed to save private key to file " << key_path.string();
        return false;
    }

    return true;
}
}  // namespace zenpp::net

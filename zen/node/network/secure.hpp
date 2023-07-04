/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <filesystem>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include <zen/node/common/directories.hpp>

namespace zen::network {

static constexpr size_t kCertificateKeyLength{4096};
static constexpr size_t kCertificateValidityDays{3650};  // 10 years
static constexpr std::string_view kCertificateFileName{"cert.pem"};
static constexpr std::string_view kPrivateKeyFileName{"key.pem"};

enum class TLSContextType {
    kServer,
    kClient
};

EVP_PKEY* generate_random_rsa_key_pair(int bits);

X509* generate_self_signed_certificate(EVP_PKEY* pkey, const std::string& password);

bool store_rsa_key_pair(EVP_PKEY* pkey, const std::string& password, const std::filesystem::path& path);

bool store_x509_certificate(X509* cert, const std::filesystem::path& path);

EVP_PKEY* load_rsa_private_key(const std::filesystem::path& directory_path, const std::string& password);

X509* load_x509_certificate(const std::filesystem::path& directory_path);

bool validate_certificate(X509* cert, EVP_PKEY* pkey);

SSL_CTX* create_tls_context(TLSContextType type, const std::filesystem::path& cert_path,
                            const std::filesystem::path& key_path, const std::string& password);

}  // namespace zen::network

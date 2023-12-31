/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <filesystem>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#include <infra/common/log.hpp>
#include <infra/filesystem/directories.hpp>

namespace znode::net {

static constexpr size_t kCertificateKeyLength{4096};
static constexpr size_t kCertificateValidityDays{3650};  // 10 years
static constexpr std::string_view kCertificateFileName{"cert.pem"};
static constexpr std::string_view kPrivateKeyFileName{"key.pem"};

enum class TLSContextType {
    kServer,
    kClient
};

//! \brief Explicit deleter for SSL_CTXes
struct SSLCTXDeleter {
    constexpr SSLCTXDeleter() noexcept = default;
    void operator()(SSL_CTX* ptr) const noexcept {
        SSL_CTX_free(ptr);
        ptr = nullptr;
    }
};

void print_ssl_error(unsigned long error_code, log::Level severity = log::Level::kError);

//! \brief Generates a random RSA key pair
//! \return A pointer to the generated key pair or nullptr if an error occurred
//! \remarks The caller is responsible for freeing the returned pointer
EVP_PKEY* generate_random_rsa_key_pair(int bits);

//! \brief Generates self signed certificate
//! \return A pointer to the generated certificate or nullptr if an error occurred
//! \remarks The caller is responsible for freeing the returned pointer
X509* generate_self_signed_certificate(EVP_PKEY* pkey);

//! \brief Stores the provided RSA key pair in the provided directory using the provided password (if not empty)
//! \remarks The caller is responsible for freeing the pointer to the key pair
//! \return true if the key pair was successfully stored, false otherwise
bool store_rsa_key_pair(EVP_PKEY* pkey, const std::string& password, const std::filesystem::path& directory_path);

//! \brief Stores the provided X509 certificate in the provided directory using the provided password (if not empty)
//! \remarks The caller is responsible for freeing the pointer to the certificate
//! \return true if the certificate was successfully stored, false otherwise
bool store_x509_certificate(X509* cert, const std::filesystem::path& directory_path);

//! \brief Loads the RSA key pair from the provided directory using the provided password (if not empty)
//! \remarks The caller is responsible for freeing the pointer to the key once returned
//! \return An EVP* raw pointer or nullptr if an error occurred
EVP_PKEY* load_rsa_private_key(const std::filesystem::path& directory_path, const std::string& password);

//! \brief Loads the X509 certificate from the provided directory
//! \remarks The caller is responsible for freeing the pointer to the certificate once returned
//! \return An X509* raw pointer or nullptr if an error occurred
X509* load_x509_certificate(const std::filesystem::path& directory_path);

//! \brief Validates the provided certificate and private key do match
bool validate_server_certificate(X509* cert, EVP_PKEY* pkey);

//! \brief Creates a TLS context of the provided type (client or server)
//! \remarks In case the type is server, the certificate and private key are loaded from the provided directory
//! and the password (if not empty) is used to decrypt the private key
//! \return A pointer to the created context or nullptr if an error occurred
SSL_CTX* generate_tls_context(TLSContextType type, const std::filesystem::path& directory_path,
                              const std::string& key_password);

//! \brief Checks for the presence of a valid certificate and private key in the provided directory and, if user
//! agrees, generates them
bool validate_tls_requirements(const std::filesystem::path& directory_path, const std::string& key_password);

}  // namespace znode::net

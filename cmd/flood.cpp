//
// Created by Andrea on 15/06/2023.
//

#include <fstream>
#include <iostream>
#include <string>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

// Function to extract host from URL
std::string GetHostFromURL(const std::string& url) {
    std::string host;
    size_t startPos = url.find("://");
    if (startPos != std::string::npos) {
        startPos += 3;  // Skip the "://"
        size_t endPos = url.find('/', startPos);
        if (endPos == std::string::npos) {
            host = url.substr(startPos);
        } else {
            host = url.substr(startPos, endPos - startPos);
        }
    }
    return host;
}

// Function to download a file from a URL using HTTPS
bool DownloadFile(const std::string& url, const std::string& outputPath) {
    std::string host = GetHostFromURL(url);
    if (host.empty()) {
        std::cout << "Invalid URL: " << url << std::endl;
        return false;
    }

    // Initialize OpenSSL
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    // Create SSL context
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        std::cout << "Failed to create SSL context." << std::endl;
        return false;
    }

    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_RENEGOTIATION | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
    SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!eNULL:!NULL:kRSA:!PSK:!SRP:!MD5:!RC4");
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

    // Create SSL BIO
    BIO* bio = BIO_new_ssl_connect(ctx);
    if (!bio) {
        std::cout << "Failed to create SSL BIO." << std::endl;
        SSL_CTX_free(ctx);
        return false;
    }

    // Get SSL pointer and set the hostname (this is required by SNI)
    SSL* ssl{nullptr};
    BIO_get_ssl(bio, nullptr);
    SSL_set_tlsext_host_name(ssl, host.c_str());

    // Set SSL BIO options
    BIO_set_conn_hostname(bio, host.c_str());
    BIO_set_conn_port(bio, "https");

    BIO_set_nbio(bio, 1);

    // Attempt to connect
    if (BIO_do_connect(bio) <= 0) {
        ERR_print_errors_fp(stderr);
        std::cout << "Failed to connect to the host." << std::endl;
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    // Verify SSL certificate
    if (BIO_do_handshake(bio) <= 0) {
        std::cout << "Failed to perform SSL handshake." << std::endl;
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    // Open output file
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cout << "Failed to open output file." << std::endl;
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    // Download the file
    char buffer[4096];
    int bytesRead = 0;
    while ((bytesRead = BIO_read(bio, buffer, sizeof(buffer))) > 0) {
        outputFile.write(buffer, bytesRead);
    }

    // Cleanup
    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    outputFile.close();

    std::cout << "File downloaded successfully." << std::endl;
    return true;
}

int main() {
    std::string url = "https://downloads.horizen.io/file/TrustedSetup/sprout-verifying.key";
    std::string outputPath = "sprout-verifying.key";
    bool success = DownloadFile(url, outputPath);
    if (success) {
        std::cout << "Downloaded file saved as: " << outputPath << std::endl;
    }
    return 0;
}

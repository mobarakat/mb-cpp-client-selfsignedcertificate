#include "pch.h"

#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_client.h"

#include <chrono>
#include <windows.h>
#include <winhttp.h>


//Loaded Client Certificate for ssl communication
static PCCERT_CONTEXT _pCertContext;

void loadOrFindCertificate() {
  if (_pCertContext) return;

  HANDLE _certFileHandle = NULL;

  /*Open File*/
  _certFileHandle = CreateFile(L"client.p12", GENERIC_READ, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

  if (INVALID_HANDLE_VALUE == _certFileHandle)
    return;

  DWORD certEncodedSize = GetFileSize(_certFileHandle, NULL);

  /*Check if file size */
  if (!certEncodedSize) {
    CloseHandle(_certFileHandle);
    return;
  }

  BYTE* certEncoded = new BYTE[(int)certEncodedSize];

  /*Read File */
  auto result = ReadFile(_certFileHandle, certEncoded, certEncodedSize, &certEncodedSize, 0);

  if (!result) {
    CloseHandle(_certFileHandle);
    return;
  }

  CRYPT_DATA_BLOB data;
  data.cbData = certEncodedSize;
  data.pbData = certEncoded;

  // Convert key-pair data to the in-memory certificate store
  WCHAR pszPassword[] = L"12345";
  HCERTSTORE hCertStore = PFXImportCertStore(&data, pszPassword, 0);
  SecureZeroMemory(pszPassword, sizeof(pszPassword));

  if (!hCertStore) {
    CloseHandle(_certFileHandle);
    return;
  }

  //get handle of loaded certificate
  _pCertContext = CertFindCertificateInStore
  (hCertStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, NULL);

  CloseHandle(_certFileHandle);

}

int main()
{

  // Create http_client configuration.
  web::http::client::http_client_config config;
  config.set_timeout(std::chrono::seconds(2));

  config.set_validate_certificates(true);

  auto func = [&](web::http::client::native_handle handle) {

    loadOrFindCertificate();

    //Attach certificate with request
    if (_pCertContext)
      WinHttpSetOption(handle, WINHTTP_OPTION_CLIENT_CERT_CONTEXT,
      (LPVOID)_pCertContext, sizeof(CERT_CONTEXT));
  };

  config.set_nativehandle_options(func);



  // Create http_client to send the request.
  web::http::client::http_client client(U("https://localhost:4150"), config);

  // Build request URI and start the request.
  auto requestTask = client.request(web::http::methods::GET)
    // Handle response headers arriving.
  .then([=](web::http::http_response response)
  {
    auto status(response.status_code());
    printf("Received response status code:%u\n", status);

    /* Extract plain text only if status code signals success */
    if (status >= 200 && status < 300)
      return response.extract_string(true);
    else
      return Concurrency::task<utility::string_t>([=] { return utility::string_t(); });

  }).then([=](utility::string_t val) {
    printf("Received response message:%s\n", utility::conversions::to_utf8string(val).c_str());
  });

  // Wait for all the outstanding I/O to complete and handle any exceptions
  try
  {
    requestTask.wait();
  }
  catch (const std::exception &e)
  {
    printf("Error exception:%s\n", e.what());
  }

  return 0;
}


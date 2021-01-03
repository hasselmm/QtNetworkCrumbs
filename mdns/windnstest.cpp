#define UNICODE

#include <windows.h>
#include <windns.h>

#include <iostream>
#include <iomanip>

#define DNS_QUERY_REQUEST_VERSION1 (1)
#define DNS_QUERY_REQUEST_VERSION2 (2)
#define DNS_REQUEST_PENDING              9506L

extern "C" {

typedef struct _DNS_QUERY_RESULT {
  ULONG       Version;
  DNS_STATUS  QueryStatus;
  ULONG64     QueryOptions;
  PDNS_RECORD pQueryRecords;
  PVOID       Reserved;
} DNS_QUERY_RESULT, *PDNS_QUERY_RESULT;

typedef VOID WINAPI DNS_SERVICE_BROWSE_CALLBACK(DWORD Status, PVOID pQueryContext, PDNS_RECORD pDnsRecord);
typedef DNS_SERVICE_BROWSE_CALLBACK *PDNS_SERVICE_BROWSE_CALLBACK;

typedef VOID WINAPI DNS_QUERY_COMPLETION_ROUTINE(PVOID pQueryContext, PDNS_QUERY_RESULT pQueryResults);
typedef DNS_QUERY_COMPLETION_ROUTINE *PDNS_QUERY_COMPLETION_ROUTINE;

typedef struct _DNS_SERVICE_BROWSE_REQUEST {
  ULONG  Version;
  ULONG  InterfaceIndex;
  PCWSTR QueryName;
  union {
    PDNS_SERVICE_BROWSE_CALLBACK pBrowseCallback;
    DNS_QUERY_COMPLETION_ROUTINE *pBrowseCallbackV2;
  };
  PVOID  pQueryContext;
} DNS_SERVICE_BROWSE_REQUEST, *PDNS_SERVICE_BROWSE_REQUEST;

typedef struct _DNS_SERVICE_CANCEL {
  PVOID reserved;
} DNS_SERVICE_CANCEL, *PDNS_SERVICE_CANCEL;

DNS_STATUS DnsServiceBrowse(PDNS_SERVICE_BROWSE_REQUEST pRequest, PDNS_SERVICE_CANCEL pCancel);

}

void onBrowseComplete(DWORD status, PVOID pQueryContext, PDNS_RECORD pDnsRecord)
{
    std::cout << "browse complete:" << std::hex << status << std::dec << ", record:" << pDnsRecord << std::endl;

    if (ERROR_SUCCESS == status) {
        for (auto r = pDnsRecord; r; r = r->pNext) {
            switch (r->wType) {
            case DNS_TYPE_A:
                std::cout << "- A: "
                          << ((r->Data.A.IpAddress >> 0) & 255) << "."
                          << ((r->Data.A.IpAddress >> 8) & 255) << "."
                          << ((r->Data.A.IpAddress >> 16) & 255) << "."
                          << ((r->Data.A.IpAddress >> 24) & 255) << std::endl;
                break;
            case DNS_TYPE_AAAA:
                std::cout << "- AAAA: " << std::hex << std::setfill('0') << std::setw(4)
                          << r->Data.AAAA.Ip6Address.IP6Dword[0] << ':'
                          << r->Data.AAAA.Ip6Address.IP6Dword[1] << ':'
                          << r->Data.AAAA.Ip6Address.IP6Dword[2] << ':'
                          << r->Data.AAAA.Ip6Address.IP6Dword[3] << std::endl;
                break;
            case DNS_TYPE_TEXT:
                std::cout << "- TXT: " << r->Data.TXT.pStringArray << std::endl;
                break;
            case DNS_TYPE_PTR:
                std::wcout << "- PTR: " << r->Data.PTR.pNameHost << std::endl;
                break;
            case DNS_TYPE_SRV:
                std::wcout << "- SRV: " << r->Data.SRV.pNameTarget << ":" << r->Data.SRV.wPort <<  std::endl;
                break;
            default:
                std::cout << "type:" << r->wType << std::endl;
                break;
            }

//            std::cout << "name: '" << r->pName << "'" << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    DNS_SERVICE_BROWSE_REQUEST sbr;
    DNS_SERVICE_CANCEL sc;

//    const wchar_t *services[] = {
//        L"_http._tcp.local",
//        L"_xpresstrain._tcp.local",
//        L"_androidtvremote._tcp.local",
//        nullptr,
//    };

    memset(&sbr, 0, sizeof sbr);
    sbr.Version = DNS_QUERY_REQUEST_VERSION1;
    sbr.InterfaceIndex = 0;
    sbr.QueryName = L"_xpresstrain._tcp.local";
    sbr.pBrowseCallback = &onBrowseComplete;

    const auto status = DnsServiceBrowse(&sbr, &sc);
    std::cout << "status:" << status << "/" << std::hex << status << std::endl;

    if (status != DNS_REQUEST_PENDING) {
        std::cout << "failure" << std::endl;
        return EXIT_FAILURE;
    }

    for (;;) {
        MSG msg;

        const auto ret = GetMessage(&msg, nullptr, 0, 0);

        if (ret > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else if (ret < 0) {
            std::cout << "msg loop error:" << ret << std::endl;
            return EXIT_FAILURE;
        } else {
            break;
        }
    }

    return EXIT_SUCCESS;
}

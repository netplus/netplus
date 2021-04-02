#include <netp/os/api_wrapper.hpp>
#include <netp/l2/m6.hpp>

#ifdef _NETP_WIN
#include <iphlpapi.h>
#pragma comment(lib, "IPHlpApi.lib")
namespace netp { namespace os {

	netp::u32_t get_process_id() {
		return GetCurrentProcessId();
	}

	//refer to https://msdn.microsoft.com/en-us/library/windows/desktop/ms724295(v=vs.85).aspx
	int get_local_computer_name(std::string& name) {
		WCHAR infoBuf[512] = { 0 };
		DWORD bufSize = 512;
		BOOL rt = GetComputerNameW(infoBuf, &bufSize);

		NETP_RETURN_V_IF_NOT_MATCH(NETP_NEGATIVE(rt), rt != netp::OK);
		netp::wchartochar(infoBuf, bufSize, name);
		return netp::OK;
	}

	int get_local_dns_server_list(vector_ipv4_t& ips) {
		vector_adapter_t adapters;
		int rt = get_adapters(adapters, netp::os::adapter_filter_flag::adp_f_skip_noup|adp_f_skip_af_inet6|adp_f_skip_af_packet);
		if (rt != netp::OK) {
			NETP_WARN("[api_wrapper_win]get adapters failed: %d", rt);
			return rt;
		}
		for (auto& adp : adapters) {
			ips.insert(ips.end(), adp.dns.begin(), adp.dns.end() );
		}
		return netp::OK;
	}

	int get_adapters(vector_adapter_t& adapters, int filter ) {

		ULONG adapter_address_buffer_size = (1024*256);
		IP_ADAPTER_ADDRESSES* adapter_address(nullptr);
		IP_ADAPTER_ADDRESSES* original_address(nullptr);

		adapter_address = (IP_ADAPTER_ADDRESSES*) netp::allocator<byte_t>::malloc(adapter_address_buffer_size);
		NETP_ALLOC_CHECK(adapter_address, adapter_address_buffer_size);
		original_address = adapter_address; //for free

		ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX;
		DWORD error = ::GetAdaptersAddresses(AF_INET, flags, nullptr, adapter_address, &adapter_address_buffer_size);

		if (ERROR_SUCCESS != error) {
			netp::allocator<byte_t>::free((byte_t*)original_address);
			original_address = nullptr;
			NETP_WARN("[api_wrapper_win]get_adapters, call GetAdaptersAddresses() failed, error: %d", error);
			return netp_last_errno();
		}

		while (adapter_address) {

			if ( (filter&adp_f_skip_noup) && adapter_address->OperStatus != IfOperStatusUp) {
				adapter_address = adapter_address->Next;
				NETP_DEBUG("[api_wrapper_win]get_adapters, not in up status,skip");
				continue;
			}

			if ((filter & adp_f_skip_loopback) && adapter_address->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
				adapter_address = adapter_address->Next;
				NETP_DEBUG("[api_wrapper_win]get_adapters, loopback skip");
				continue;
			}

			netp::adapter adapter_;
			adapter_.idx = adapter_address->IfIndex;
			adapter_.status_up = adapter_address->OperStatus == IfOperStatusUp;
			adapter_.iftype = adapter_address->IfType;
			adapter_.mtu = adapter_address->Mtu;
			adapter_.flag = adapter_address->Flags;
			adapter_.name = string_t(adapter_address->AdapterName);
			netp::wchartochar(adapter_address->FriendlyName, wcslen(adapter_address->FriendlyName), adapter_.friendlyname);
			netp::wchartochar(adapter_address->Description, wcslen(adapter_address->Description), adapter_.description);

			adapter_.mac.B6.b1 = adapter_address->PhysicalAddress[0];
			adapter_.mac.B6.b2 = adapter_address->PhysicalAddress[1];
			adapter_.mac.B6.b3 = adapter_address->PhysicalAddress[2];
			adapter_.mac.B6.b4 = adapter_address->PhysicalAddress[3];
			adapter_.mac.B6.b5 = adapter_address->PhysicalAddress[4];
			adapter_.mac.B6.b6 = adapter_address->PhysicalAddress[5];

			IP_ADAPTER_DNS_SERVER_ADDRESS* pDnsServer = adapter_address->FirstDnsServerAddress;
			while (pDnsServer) {
				sockaddr_in* sa_in = (sockaddr_in*)pDnsServer->Address.lpSockaddr;
				adapter_.dns.push_back( nipv4toipv4(sa_in->sin_addr.s_addr) );
				pDnsServer = pDnsServer->Next;
			}

			PIP_ADAPTER_UNICAST_ADDRESS pUnicast = adapter_address->FirstUnicastAddress;
			while (pUnicast) {
				if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
					sockaddr_in* sa_in = (sockaddr_in*)(pUnicast->Address.lpSockaddr);
					ULONG mask;
					ConvertLengthToIpv4Mask(pUnicast->OnLinkPrefixLength, &mask);
					adapter_.unicast.push_back({ nipv4toipv4(sa_in->sin_addr.s_addr) , htonl(mask) });
				} else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
					NETP_ERR("[api_wrapper_win]get_adapters, unicast, ipv6, skip");
				} else {
					NETP_ERR("[api_wrapper_win]get_adapters, unicast, invalid sock family: %d, skip", pUnicast->Address.lpSockaddr->sa_family);
				}

				pUnicast = pUnicast->Next;
			}

			PIP_ADAPTER_ANYCAST_ADDRESS pANYAddress = adapter_address->FirstAnycastAddress;
			while (pANYAddress) {

				switch (pANYAddress->Address.lpSockaddr->sa_family) {
				case AF_INET:
				{
					sockaddr_in* sa_in = (sockaddr_in*)(pANYAddress->Address.lpSockaddr);
					adapter_.anycast.push_back(nipv4toipv4(sa_in->sin_addr.s_addr));
				}
				break;
				case AF_INET6:
				{
					NETP_WARN("todo: ipv6");
				}
				break;
				}
				pANYAddress = pANYAddress->Next;
			}

			PIP_ADAPTER_MULTICAST_ADDRESS pMULAddress = adapter_address->FirstMulticastAddress;
			while (pMULAddress) {
				switch (pMULAddress->Address.lpSockaddr->sa_family) {
				case AF_INET:
				{
					sockaddr_in* sa_in = (sockaddr_in*)(pMULAddress->Address.lpSockaddr);
					adapter_.multicast.push_back(nipv4toipv4(sa_in->sin_addr.s_addr));
				}
				break;
				case AF_INET6:
				{
					NETP_WARN("todo: ipv6");
				}
				break;
				}
				pMULAddress = pMULAddress->Next;
			}

			PIP_ADAPTER_GATEWAY_ADDRESS_LH pGWAddress = adapter_address->FirstGatewayAddress;
			while (pGWAddress) {
				switch (pGWAddress->Address.lpSockaddr->sa_family) {
				case AF_INET:
				{
					sockaddr_in* sa_in = (sockaddr_in*)(pGWAddress->Address.lpSockaddr);
					adapter_.gateway.push_back(nipv4toipv4(sa_in->sin_addr.s_addr));
				}
				break;
				case AF_INET6:
				{
					NETP_INFO("todo");
				}
				break;
				}

				pGWAddress = pGWAddress->Next;
			}

			adapters.push_back(adapter_);
			adapter_address = adapter_address->Next;
		}

		netp::allocator<byte_t>::free((byte_t*)original_address);
		return netp::OK;
	}
}}
#endif
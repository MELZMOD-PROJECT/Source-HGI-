#pragma once
// ============================================================
//  HGI SLOT MOD - Protection.h (FIXED)
//  Anti-detection & DNS bypass untuk Higgs Domino
//  Server: i.foqtx.com, dominotest.net
// ============================================================
#include <string>
#include <cstdint>

// Toggle DNS bypass
bool BypassDNS = true;

// ============================================================
//  DNS Hooks - FIXED SIGNATURES
//  IL2CPP/System.Net menggunakan managed string (void*), 
//  BUKAN std::string by value. Menggunakan std::string di sini
//  akan menyebabkan stack corruption dan crash silent.
// ============================================================

// Static constructor
bool (*oDns_cctor)(void* unk);
bool Dns_cctor(void* unk) {
    if (BypassDNS) return true;
    return oDns_cctor(unk);
}

// GetHostByName_internal
bool (*oDns_GetHostByName_internal)(void* unk, void* host, void* h_name, void* h_aliases, void* h_addr_list);
bool Dns_GetHostByName_internal(void* unk, void* host, void* h_name, void* h_aliases, void* h_addr_list) {
    if (BypassDNS) {
        // Pass original params but override host resolution logic internally
        // Atau return true untuk skip DNS entirely
        return true; 
    }
    return oDns_GetHostByName_internal(unk, host, h_name, h_aliases, h_addr_list);
}

// GetHostByAddr_internal
bool (*oDns_GetHostByAddr_internal)(void* unk, void* host, void* h_name, void* h_aliases, void* h_addr_list);
bool Dns_GetHostByAddr_internal(void* unk, void* host, void* h_name, void* h_aliases, void* h_addr_list) {
    if (BypassDNS) return true;
    return oDns_GetHostByAddr_internal(unk, host, h_name, h_aliases, h_addr_list);
}

// hostent_to_IPHostEntry
bool (*oDns_hostent_to_IPHostEntry)(void* unk, void* h_name, void* h_aliases, void* h_addrlist);
bool Dns_hostent_to_IPHostEntry(void* unk, void* h_name, void* h_aliases, void* h_addrlist) {
    if (BypassDNS) return true;
    return oDns_hostent_to_IPHostEntry(unk, h_name, h_aliases, h_addrlist);
}

// GetHostByAddressFromString
bool (*oDns_GetHostByAddressFromString)(void* unk, void* address);
bool Dns_GetHostByAddressFromString(void* unk, void* address) {
    if (BypassDNS) return true;
    return oDns_GetHostByAddressFromString(unk, address);
}

// GetHostEntry
bool (*oDns_GetHostEntry)(void* unk, void* hostNameOrAddress);
bool Dns_GetHostEntry(void* unk, void* hostNameOrAddress) {
    if (BypassDNS) return true;
    return oDns_GetHostEntry(unk, hostNameOrAddress);
}

// GetHostAddresses
bool (*oDns_GetHostAddresses)(void* unk, void* hostNameOrAddress);
bool Dns_GetHostAddresses(void* unk, void* hostNameOrAddress) {
    if (BypassDNS) return true;
    return oDns_GetHostAddresses(unk, hostNameOrAddress);
}

// GetHostByName
bool (*oDns_GetHostByName)(void* unk, void* hostNameOrAddress);
bool Dns_GetHostByName(void* unk, void* hostNameOrAddress) {
    if (BypassDNS) return true;
    return oDns_GetHostByName(unk, hostNameOrAddress);
}

// ============================================================
//  DisposeServerObject - FIXED: Stub valid instead of NULL ptr
// ============================================================
void (*oDisposeServerObject)(void* unk);

void DisposeServerObjectStub(void* unk) {
    // Empty stub - do nothing safely
    (void)unk;
    return;
}
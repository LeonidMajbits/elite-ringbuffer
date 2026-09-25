// C++20 calls the opaque C API. No std::atomic overlay of shared C11 objects.
#include "elite_api.h"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <unistd.h>
static void require(elite_result r)
{
    if (r.status != ELITE_OK)
        throw std::runtime_error(std::string(elite_status_string(r.status))+" outcome="+std::to_string(r.outcome));
}
int main()
{
    elite_authority *a=nullptr; elite_object *o=nullptr;
    elite_connection *p=nullptr,*c=nullptr;
    try {
        std::array<uint8_t,16> host{},authority{}; host[0]=1; authority[0]=2;
        const auto nonce=static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        std::memcpy(authority.data()+8,&nonce,8);
        const auto pid=static_cast<uint32_t>(getpid()); std::memcpy(authority.data()+1,&pid,4);
        require(elite_authority_create(host.data(),authority.data(),1048576,&a));
        elite_config cfg{ELITE_SPSC,0,0,64,32,1,1,0};
        elite_endpoint_definition defs[2]{};
        defs[0].endpoint_id[0]=1; defs[1].endpoint_id[0]=2;
        defs[0].process_incarnation_id[0]=defs[1].process_incarnation_id[0]=1;
        defs[0].role=ELITE_PRODUCER; defs[1].role=ELITE_CONSUMER;
        require(elite_create(a,&cfg,defs,&o));require(elite_object_activate(o));
        elite_grant pg{},cg{};
        require(elite_object_register_process(o,0,getpid()));require(elite_object_register_process(o,1,getpid()));
        require(elite_object_grant(o,0,&pg));require(elite_object_grant(o,1,&cg));
        require(elite_attach(&pg,&p));require(elite_attach(&cg,&c));
        elite_lease w{},r{};elite_write_span ws{};elite_read_span rs{};
        require(elite_write_reserve(p,&w,&ws));
        { // Every raw span/alias ends before ownership transfer.
            std::span<uint8_t> bytes(static_cast<uint8_t *>(ws.data),ws.capacity);
            bytes[0]='O';bytes[1]='K';
        }
        require(elite_write_commit(p,&w,2,1,42));
        require(elite_read_borrow(c,&r,&rs));
        {
            std::span<const uint8_t> bytes(static_cast<const uint8_t *>(rs.data),rs.length);
            if(bytes.size()!=2 || bytes[0]!='O' || bytes[1]!='K') throw std::runtime_error("payload mismatch");
        }
        require(elite_read_release(c,&r));
        elite_cleanup_receipt receipt{};
        require(elite_detach(&p,&receipt));require(elite_object_ack_cleanup(o,&receipt));
        require(elite_detach(&c,&receipt));require(elite_object_ack_cleanup(o,&receipt));
        require(elite_object_destroy(&o));require(elite_authority_destroy(&a));
        std::cout<<"C++ opaque API "<<elite_version_string()<<": OK\n";
        return 0;
    } catch(const std::exception &e) {
        // A failed example exits rather than guessing a token's disposition.
        // In production keep the authority ledger and resolve outstanding
        // leases/cleanup receipts; see docs/INTEGRATION.md.
        std::cerr<<e.what()<<'\n'; return 1;
    }
}

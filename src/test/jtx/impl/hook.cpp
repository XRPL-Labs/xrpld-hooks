//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/basics/contract.h>
#include <ripple/protocol/jss.h>
#include <stdexcept>
#include <test/jtx/hook.h>
#include <ripple/app/hook/Enum.h>

namespace ripple {
namespace test {
namespace jtx {

Json::Value
hook(Account const& account, std::optional<std::vector<Json::Value>> hooks, std::uint32_t flags)
{
    Json::Value jv;
    jv[jss::Account] = account.human();
    jv[jss::TransactionType] = jss::SetHook;
    jv[jss::Flags] = flags;
    
    if (hooks)
    {
        jv[jss::Hooks] =
            Json::Value{Json::arrayValue};
        for (uint64_t i = 0; i < hooks->size(); ++i)
            jv[jss::Hooks][i][jss::Hook] = (*hooks)[i];
    }
    return jv;
}


std::string uint64_hex(uint64_t x)
{
    auto const nibble = [](uint64_t n) -> char
    {
        n &= 0x0FU;
        return
            n <= 9 ? '0' + n :
            'A' + (n - 10);
    };
    return std::string("") +
        nibble(x >> 60U) + nibble(x >> 56U) +
        nibble(x >> 52U) + nibble(x >> 48U) +
        nibble(x >> 44U) + nibble(x >> 40U) +
        nibble(x >> 36U) + nibble(x >> 32U) +
        nibble(x >> 28U) + nibble(x >> 24U) +
        nibble(x >> 20U) + nibble(x >> 16U) +
        nibble(x >> 12U) + nibble(x >>  8U) +
        nibble(x >>  4U) + nibble(x >>  0U);
}

Json::Value
hso_delete(void (*f)(Json::Value& jv))
{
     Json::Value jv;
     jv[jss::CreateCode] = "";
     jv[jss::Flags] = hsfOVERRIDE;

    if (f) 
        f(jv);

    return jv;
}

Json::Value
hso(std::vector<uint8_t> const& wasmBytes, void (*f)(Json::Value& jv))
{

    if (wasmBytes.size() == 0)
        throw std::runtime_error("empty hook wasm passed to hso()");

    Json::Value jv;
    jv[jss::CreateCode] = strHex(wasmBytes);
    {
        jv[jss::HookOn] = uint64_hex(0);
        jv[jss::HookNamespace] = to_string(uint256{beast::zero});
        jv[jss::HookApiVersion] = Json::Value{0};
    }

    if (f)
        f(jv);

    return jv;

}

}  // namespace jtx
}  // namespace test
}  // namespace ripple

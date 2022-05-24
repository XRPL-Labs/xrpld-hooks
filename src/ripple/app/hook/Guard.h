#include <map>
#include <vector>
#include <string_view>
#include <utility>
#include <iostream>
#include <ostream>
#include <stack>
#include <string>
#include <functional>
#include "Enum.h"

using GuardLog = std::optional<std::reference_wrapper<std::basic_ostream<char>>>;

#define DEBUG_GUARD 0
#define GUARDLOG(logCode)\
        if (!guardLog)\
        {\
        }\
        else\
            (*guardLog).get() << "HookSet(" << logCode << ")[" << guardLogAccStr << "]: "

// web assembly contains a lot of run length encoding in LEB128 format
inline uint64_t
parseLeb128(
    std::vector<unsigned char> const& buf,
    int start_offset,
    int* end_offset)
{
    uint64_t val = 0, shift = 0, i = start_offset;
    while (i < buf.size())
    {
        uint64_t b = (uint64_t)(buf[i]);
        uint64_t last = val;
        val += (b & 0x7FU) << shift;
        if (val < last)
        {
            // overflow
            throw std::overflow_error { "leb128 overflow" };
        }
        ++i;
        if (b & 0x80U)
        {
            shift += 7;
            continue;
        }
        *end_offset = i;
        return val;
    }
    return 0;
}


// this macro will return temMALFORMED if i ever exceeds the end of the hook
#define CHECK_SHORT_HOOK()\
{\
    if (i >= hook.size())\
    {\
        \
        GUARDLOG(hook::log::SHORT_HOOK) \
            << "Malformed transaction: Hook truncated or otherwise invalid. "\
            << "SetHook.cpp:" << __LINE__ << "\n";\
        return {};\
    }\
}

// checks the WASM binary for the appropriate required _g guard calls and rejects it if they are not found
// start_offset is where the codesection or expr under analysis begins and end_offset is where it ends
// returns {worst case instruction count} if valid or {} if invalid
// may throw overflow_error
inline
std::optional<uint64_t>
check_guard(
    std::vector<uint8_t> const& hook,
    int codesec,
    int start_offset,
    int end_offset,
    int guard_func_idx,
    int last_import_idx,
    GuardLog guardLog,
    std::string guardLogAccStr)
{

    if (DEBUG_GUARD)
        printf("\ncheck_guard called with "
               "codesec=%d start_offset=%d end_offset=%d guard_func_idx=%d last_import_idx=%d\n",
               codesec, start_offset, end_offset, guard_func_idx, last_import_idx);

    if (end_offset <= 0) end_offset = hook.size();
    int block_depth = 0;
    int mode = 1; // controls the state machine for searching for guards
                  // 0 = looking for guard from a trigger point (loop or function start)
                  // 1 = looking for a new trigger point (loop);
                  // currently always starts at 1 no-top-of-func check, see above block comment

    std::stack<uint64_t> stack; // we track the stack in mode 0 to work out if constants end up in the guard function
    std::map<uint32_t, uint64_t> local_map; // map of local variables since the trigger point
    std::map<uint32_t, uint64_t> global_map; // map of global variables since the trigger point

    // block depth level -> { largest guard, rolling instruction count } 
    std::map<int, std::pair<uint32_t, uint64_t>> instruction_count;

                        // largest guard  // instr ccount
    instruction_count[0] = {1,                  0};

    if (DEBUG_GUARD)
        printf("\n\n\nstart of guard analysis for codesec %d\n", codesec);

    for (int i = start_offset; i < end_offset; )
    {

        if (DEBUG_GUARD)
        {
            printf("->");
            for (int z = i; z < 16 + i && z < end_offset; ++z)
                printf("%02X", hook[z]);
            printf("\n");
        }

        CHECK_SHORT_HOOK();
        int instr = hook[i++];

        instruction_count[block_depth].second++;

        if (instr == 0x10) // call instr
        {
            int callee_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (DEBUG_GUARD)
                printf("%d - call instruction at %d -- call funcid: %d\n", mode, i, callee_idx);

            // disallow calling of user defined functions inside a hook
            if (callee_idx > last_import_idx)
            {
                GUARDLOG(hook::log::CALL_ILLEGAL)
                    << "GuardCheck "
                    << "Hook calls a function outside of the whitelisted imports "
                    << "codesec: " << codesec << " hook byte offset: " << i << "\n";

                return {};
            }

            if (callee_idx == guard_func_idx)
            {
                // found!
                if (mode == 0)
                {

                    if (stack.size() < 2)
                    {
                        GUARDLOG(hook::log::GUARD_PARAMETERS)
                            << "GuardCheck "
                            << "_g() called but could not detect constant parameters "
                            << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                        return {};
                    }

                    uint64_t a = stack.top();
                    stack.pop();
                    uint64_t b = stack.top();
                    stack.pop();
                    if (DEBUG_GUARD)
                        printf("FOUND: GUARD(%lu, %lu), codesec: %d offset %d\n", a, b, codesec, i);

                    if (b <= 0)
                    {
                        // 0 has a special meaning, generally it's not a constant value
                        // < 0 is a constant but negative, either way this is a reject condition
                         GUARDLOG(hook::log::GUARD_PARAMETERS)
                            << "GuardCheck "
                            << "_g() called but could not detect constant parameters "
                            << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                        return {};
                    }

                    // update the instruction count for this block depth to the largest possible guard
                    if (instruction_count[block_depth].first < a)
                    {
                        instruction_count[block_depth].first = a;
                        if (DEBUG_GUARD)
                        {
                            std::cout
                                << "HookDebug GuardCheck "
                                << "Depth " << block_depth << " guard: " << a << "\n";
                        }
                    }

                    // clear stack and maps
                    while (stack.size() > 0)
                        stack.pop();
                    local_map.clear();
                    global_map.clear();
                    mode = 1;
                }
            }
            continue;
        }

        if (instr == 0x11) // call indirect [ we don't allow guard to be called this way ]
        {
             GUARDLOG(hook::log::CALL_INDIRECT) << "GuardCheck "
                << "Call indirect detected and is disallowed in hooks "
                << "codesec: " << codesec << " hook byte offset: " << i << "\n";
            return {};
            /*
            if (DEBUG_GUARD)
                printf("%d - call_indirect instruction at %d\n", mode, i);
            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            ++i; CHECK_SHORT_HOOK(); //absorb 0x00 trailing
            continue;
            */
        }

        // unreachable and nop instructions
        if (instr == 0x00 || instr == 0x01)
        {
            if (DEBUG_GUARD)
                printf("%d - unreachable/nop instruction at %d\n", mode, i);
            continue;
        }

        // branch loop block instructions
        if ((instr >= 0x02 && instr <= 0x0F) || instr == 0x11)
        {
            if (mode == 0 && instr >= 0x03)
            {
                 GUARDLOG(hook::log::GUARD_MISSING)
                    << "GuardCheck "
                    << "_g() did not occur at start of loop statement "
                    << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                return {};
            }

            // block instruction
            // RH NOTE: block instructions *are* allowed between a loop and a guard
            if (instr == 0x02)
            {
                if (DEBUG_GUARD)
                    printf("%d - block instruction at %d\n", mode, i);

                ++i; CHECK_SHORT_HOOK();
                block_depth++;
                instruction_count[block_depth] = {1, 0};
                continue;
            }
            
            // execution to here means we are in 'search mode' for loop instructions

            // loop instruction
            if (instr == 0x03)
            {
                if (DEBUG_GUARD)
                    printf("%d - loop instruction at %d\n", mode, i);

                ++i; CHECK_SHORT_HOOK();
                mode = 0; // we now search for a guard()
                block_depth++;
                instruction_count[block_depth] = {1, 0};
                continue;
            }

            // if instr
            if (instr == 0x04)
            {
                if (DEBUG_GUARD)
                    printf("%d - if instruction at %d\n", mode, i);
                ++i; CHECK_SHORT_HOOK();
                block_depth++;
                instruction_count[block_depth] = {1, 0};
                continue;
            }

            // else instr
            if (instr == 0x05)
            {
                if (DEBUG_GUARD)
                    printf("%d - else instruction at %d\n", mode, i);
                continue;
            }

            // branch instruction
            if (instr == 0x0C || instr == 0x0D)
            {
                if (DEBUG_GUARD)
                    printf("%d - br instruction at %d\n", mode, i);
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                continue;
            }

            // branch table instr
            if (instr == 0x0E)
            {
                if (DEBUG_GUARD)
                    printf("%d - br_table instruction at %d\n", mode, i);
                int vec_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                for (int v = 0; v < vec_count; ++v)
                {
                    parseLeb128(hook, i, &i);
                    CHECK_SHORT_HOOK();
                }
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                continue;
            }
        }


        // parametric instructions | no operands
        if (instr == 0x1A || instr == 0x1B)
        {
            if (DEBUG_GUARD)
                printf("%d - parametric  instruction at %d\n", mode, i);
            continue;
        }

        // variable instructions
        if (instr >= 0x20 && instr <= 0x24)
        {
            if (DEBUG_GUARD)
                printf("%d - variable local/global instruction at %d\n", mode, i);

            int idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();

            if (mode == 1)
                continue;

            // we need to do stack and map manipualtion to track any possible constants before guard call
            if (instr == 0x20 || instr == 0x23) // local.get idx or global.get idx
            {
                auto& map = ( instr == 0x20 ? local_map : global_map );
                if (map.find(idx) == map.end())
                    stack.push(0); // we always put a 0 in place of a local or global we don't know the value of
                else
                    stack.push(map[idx]);
                continue;
            }

            if (instr == 0x21 || instr == 0x22 || instr == 0x24) // local.set idx or global.set idx
            {
                auto& map = ( instr == 0x21 || instr == 0x22 ? local_map : global_map );

                uint64_t to_store = (stack.size() == 0 ? 0 : stack.top());
                map[idx] = to_store;
                if (instr != 0x22)
                    stack.pop();

                continue;
            }
        }

        // RH TODO support guard consts being passed through memory functions (maybe)

        //memory instructions
        if (instr >= 0x28 && instr <= 0x3E)
        {
            if (DEBUG_GUARD)
                printf("%d - variable memory instruction at %d\n", mode, i);

            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            continue;
        }

        // more memory instructions
        if (instr == 0x3F || instr == 0x40)
        {
            if (DEBUG_GUARD)
                printf("%d - memory instruction at %d\n", mode, i);

            ++i; CHECK_SHORT_HOOK();
            if (instr == 0x40) // disallow memory.grow
            {
                GUARDLOG(hook::log::MEMORY_GROW)
                    << "GuardCheck "
                    << "Memory.grow instruction not allowed at "
                    << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                return {};
            }
            continue;
        }

        // int const instrs
        // numeric instructions with immediates
        if (instr == 0x41 || instr == 0x42)
        {

            if (DEBUG_GUARD)
                printf("%d - const instruction at %d\n", mode, i);

            uint64_t immediate = parseLeb128(hook, i, &i);
            CHECK_SHORT_HOOK(); // RH TODO enforce i32 i64 size limit

            // in mode 0 we should be stacking our constants and tracking their movement in
            // and out of locals and globals
            stack.push(immediate);
            continue;
        }

        // const instr
        // more numerics with immediates
        if (instr == 0x43 || instr == 0x44)
        {

            if (DEBUG_GUARD)
                printf("%d - const float instruction at %d\n", mode, i);

            i += ( instr == 0x43 ? 4 : 8 );
            CHECK_SHORT_HOOK();
            continue;
        }

        // numerics no immediates
        if (instr >= 0x45 && instr <= 0xC4)
        {
            if (DEBUG_GUARD)
                printf("%d - numeric instruction at %d\n", mode, i);
            continue;
        }

        // truncation instructions
        if (instr == 0xFC)
        {
            if (DEBUG_GUARD)
                printf("%d - truncation instruction at %d\n", mode, i);
            i++; CHECK_SHORT_HOOK();
            parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            continue;
        }

        if (instr == 0x0B)
        {
            if (DEBUG_GUARD)
                printf("%d - block end instruction at %d\n", mode, i);

            // end of expression
            if (block_depth == 0)
            break;

            block_depth--;

            if (block_depth < 0)
            {
                
                    GUARDLOG(hook::log::BLOCK_ILLEGAL) << "GuardCheck "
                    << "Unexpected 0x0B instruction, malformed"
                    << "codesec: " << codesec << " hook byte offset: " << i << "\n";
                return {};
            }

            // perform the instruction count * guard accounting
            instruction_count[block_depth].second +=
                instruction_count[block_depth+1].second * instruction_count[block_depth+1].first;
            instruction_count.erase(block_depth+1);
        }
    }

    
    GUARDLOG(hook::log::INSTRUCTION_COUNT) << "GuardCheck "
        << "Total worse-case execution count: " << instruction_count[0].second << "\n";

    // RH TODO: don't hardcode this
    if (instruction_count[0].second > 0xFFFFF)
    {
        GUARDLOG(hook::log::INSTRUCTION_EXCESS) << "GuardCheck "
            << "Maximum possible instructions exceed 1048575, please make your hook smaller "
            << "or check your guards!" << "\n";
        return {};
    }

    // if we reach the end of the code looking for another trigger the guards are installed correctly
    if (mode == 1)
        return instruction_count[0].second;

    GUARDLOG(hook::log::GUARD_MISSING) << "GuardCheck "
        << "Guard did not occur before end of loop / function. "
        << "Codesec: " << codesec << "\n";
    return {};
}

// may throw overflow_error
inline
std::optional<  // unpopulated means invalid
std::pair<
    uint64_t,   // max instruction count for hook()
    uint64_t    // max instruction count for cbak()
>>
validateGuards(
    std::vector<uint8_t> const& hook,
    bool strict,
    GuardLog guardLog,
    std::string guardLogAccStr)
{
    uint64_t byteCount = hook.size();

    // RH TODO compute actual smallest possible hook and update this value
    if (byteCount < 10)
    {
        GUARDLOG(hook::log::WASM_TOO_SMALL)
            << "Malformed transaction: Hook was not valid webassembly binary. Too small." << "\n";
        return {};
    }

    // check header, magic number
    unsigned char header[8] = { 0x00U, 0x61U, 0x73U, 0x6DU, 0x01U, 0x00U, 0x00U, 0x00U };
    for (int i = 0; i < 8; ++i)
    {
        if (hook[i] != header[i])
        {
            GUARDLOG(hook::log::WASM_BAD_MAGIC)
                << "Malformed transaction: Hook was not valid webassembly binary. "
                << "Missing magic number or version." << "\n";
            return {};
        }
    }

    // these will store the function type indicies of hook and cbak if
    // hook and cbak are found in the export section
    std::optional<int> hook_func_idx;
    std::optional<int> cbak_func_idx;

    // this maps function ids to type ids, used for looking up the type of cbak and hook
    // as established inside the wasm binary.
    std::map<int, int> func_type_map;


    // now we check for guards... first check if _g is imported
    int guard_import_number = -1;
    int last_import_number = -1;
    int import_count = 0;
    for (int i = 8, j = 0; i < hook.size();)
    {

        if (j == i)
        {
            // if the loop iterates twice with the same value for i then
            // it's an infinite loop edge case
            GUARDLOG(hook::log::WASM_PARSE_LOOP) 
                << "Malformed transaction: Hook is invalid WASM binary." << "\n";
            return {};
        }

        j = i;

        // each web assembly section begins with a single byte section type followed by an leb128 length
        int section_type = hook[i++];
        int section_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
        //int section_start = i;

        if (DEBUG_GUARD)
            printf("WASM binary analysis -- upto %d: section %d with length %d\n",
                    i, section_type, section_length);

        int next_section = i + section_length;

        // we are interested in the import section... we need to know if _g is imported and which import# it is
        if (section_type == 2) // import section
        {
            import_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (import_count <= 0)
            {
                GUARDLOG(hook::log::IMPORTS_MISSING)
                    << "Malformed transaction. "
                    << "Hook did not import any functions... "
                    << "required at least guard(uint32_t, uint32_t) and accept or rollback" << "\n";
                return {};
            }

            // process each import one by one
            int func_upto = 0; // not all imports are functions so we need an indep counter for these
            for (int j = 0; j < import_count; ++j)
            {
                // first check module name
                int mod_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (mod_length < 1 || mod_length > (hook.size() - i))
                {
                    GUARDLOG(hook::log::IMPORT_MODULE_BAD)
                        << "Malformed transaction. "
                        << "Hook attempted to specify nil or invalid import module" << "\n";
                    return {};
                }

                if (std::string_view( (const char*)(hook.data() + i), (size_t)mod_length ) != "env")
                {
                    GUARDLOG(hook::log::IMPORT_MODULE_ENV)
                        << "Malformed transaction. "
                        << "Hook attempted to specify import module other than 'env'" << "\n";
                    return {};
                }

                i += mod_length; CHECK_SHORT_HOOK();

                // next get import name
                int name_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (name_length < 1 || name_length > (hook.size() - i))
                {
                    GUARDLOG(hook::log::IMPORT_NAME_BAD) 
                        << "Malformed transaction. "
                        << "Hook attempted to specify nil or invalid import name" << "\n";
                    return {};
                }

                std::string import_name { (const char*)(hook.data() + i), (size_t)name_length };

                i += name_length; CHECK_SHORT_HOOK();

                // next get import type
                if (hook[i] > 0x00)
                {
                    // not a function import
                    // RH TODO check these other imports for weird stuff
                    i++; CHECK_SHORT_HOOK();
                    parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    continue;
                }

                // execution to here means it's a function import
                i++; CHECK_SHORT_HOOK();
                /*int type_idx = */
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();

                // RH TODO: validate that the parameters of the imported functions are correct
                if (import_name == "_g")
                {
                    guard_import_number = func_upto;
                } else if (hook_api::import_whitelist.find(import_name) == hook_api::import_whitelist.end())
                {
                    GUARDLOG(hook::log::IMPORT_ILLEGAL)
                        << "Malformed transaction. "
                        << "Hook attempted to import a function that does not "
                        << "appear in the hook_api function set: `" << import_name << "`" << "\n";
                    return {};
                }
                func_upto++;
            }

            if (guard_import_number == -1)
            {
                GUARDLOG(hook::log::GUARD_IMPORT)
                    << "Malformed transaction. "
                    << "Hook did not import _g (guard) function" << "\n";
                return {};
            }

            last_import_number = func_upto - 1;

            // we have an imported guard function, so now we need to enforce the guard rule:
            // all loops must start with a guard call before any branching
            // to enforce these rules we must do a second pass of the wasm in case the function
            // section was placed in this wasm binary before the import section

        } else
        if (section_type == 7) // export section
        {
            int export_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (export_count <= 0)
            {
                GUARDLOG(hook::log::EXPORTS_MISSING)
                    << "Malformed transaction. "
                    << "Hook did not export any functions... "
                    << "required hook(int64_t), callback(int64_t)." << "\n";
                return {};
            }

            for (int j = 0; j < export_count; ++j)
            {
                int name_len = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (name_len == 4)
                {

                    if (hook[i] == 'h' && hook[i+1] == 'o' && hook[i+2] == 'o' && hook[i+3] == 'k')
                    {
                        i += name_len; CHECK_SHORT_HOOK();
                        if (hook[i] != 0)
                        {
                            GUARDLOG(hook::log::EXPORT_HOOK_FUNC)
                                << "Malformed transaction. "
                                << "Hook did not export: A valid int64_t hook(uint32_t)" << "\n";
                            return {};
                        }

                        i++; CHECK_SHORT_HOOK();
                        hook_func_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                        continue;
                    }
                    
                    if (hook[i] == 'c' && hook[i+1] == 'b' && hook[i+2] == 'a' && hook[i+3] == 'k')
                    {
                        i += name_len; CHECK_SHORT_HOOK();
                        if (hook[i] != 0)
                        {
                            GUARDLOG(hook::log::EXPORT_CBAK_FUNC)
                                << "Malformed transaction. "
                                << "Hook did not export: A valid int64_t cbak(uint32_t)" << "\n";
                            return {};
                        }
                        i++; CHECK_SHORT_HOOK();
                        cbak_func_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                        continue;
                    }
                }

                i += name_len + 1;
                parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            }

            // execution to here means export section was parsed
            if (!hook_func_idx)
            {
                GUARDLOG(hook::log::EXPORT_MISSING)
                    << "Malformed transaction. "
                    << "Hook did not export: "
                    << ( !hook_func_idx ? "int64_t hook(uint32_t); " : "" ) << "\n";
                return {};
            }
        }
        else if (section_type == 3) // function section
        {
            int function_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            if (function_count <= 0)
            {
                GUARDLOG(hook::log::FUNCS_MISSING)
                    << "Malformed transaction. "
                    << "Hook did not establish any functions... "
                    << "required hook(int64_t), callback(int64_t)." << "\n";
                return {};
            }

            for (int j = 0; j < function_count; ++j)
            {
                int type_idx = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                printf("Function map: func %d -> type %d\n", j, type_idx);
                func_type_map[j] = type_idx;
            }
        }

        i = next_section;
        continue;
    }

    // we must subtract import_count from the hook and cbak function in order to be able to 
    // look them up in the functions section. this is a rule of the webassembly spec
    // note that at this point in execution we are guarenteed these are populated
    *hook_func_idx -= import_count;

    if (cbak_func_idx)
        *cbak_func_idx -= import_count;

    if (func_type_map.find(*hook_func_idx) == func_type_map.end() ||
        (cbak_func_idx && func_type_map.find(*cbak_func_idx) == func_type_map.end()))
    {
        GUARDLOG(hook::log::FUNC_TYPELESS)
            << "Malformed transaction. "
            << "hook or cbak functions did not have a corresponding type in WASM binary." << "\n";
        return {};
    }

    int hook_type_idx = func_type_map[*hook_func_idx];

    // cbak function is optional so if it exists it has a type otherwise it is skipped in checks
    if (cbak_func_idx && func_type_map[*cbak_func_idx] != hook_type_idx)
    {
        GUARDLOG(hook::log::HOOK_CBAK_DIFF_TYPES)
            << "Malformed transaction. "
            << "Hook and cbak func must have the same type. int64_t (*)(uint32_t).\n";
        return {};
    }

    int64_t maxInstrCountHook = 0;
    int64_t maxInstrCountCbak = 0;

/*    printf( "hook_func_idx: %d\ncbak_func_idx: %d\n"
            "hook_type_idx: %d\ncbak_type_idx: %d\n", 
            *hook_func_idx,
            *cbak_func_idx,
            hook_type_idx, *cbak_type_idx);
*/

    // second pass... where we check all the guard function calls follow the guard rules
    // minimal other validation in this pass because first pass caught most of it
    for (int i = 8; i < hook.size();)
    {

        int section_type = hook[i++];
        int section_length = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
        //int section_start = i;
        int next_section = i + section_length;

        if (section_type == 1) // type section
        {
            int type_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
            for (int j = 0; j < type_count; ++j)
            { 
                if (hook[i++] != 0x60)
                {
                    GUARDLOG(hook::log::FUNC_TYPE_INVALID) 
                        << "Invalid function type. "
                        << "Codesec: " << section_type << " "
                        << "Local: " << j << " "
                        << "Offset: " << i << "\n";
                    return {};
                }
                CHECK_SHORT_HOOK();
                
                int param_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                if (j == hook_type_idx && param_count != 1)
                {
                    GUARDLOG(hook::log::PARAM_HOOK_CBAK)
                        << "Malformed transaction. "
                        << "hook and cbak function definition must have exactly one parameter (uint32_t)." << "\n";
                    return {};
                }

                for (int k = 0; k < param_count; ++k)
                {
                    int param_type = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    if (param_type == 0x7FU || param_type == 0x7EU ||
                        param_type == 0x7DU || param_type == 0x7CU)
                    {
                        // pass, this is fine
                    }
                    else
                    {
                        GUARDLOG(hook::log::FUNC_PARAM_INVALID)
                            << "Invalid parameter type in function type. "
                            << "Codesec: " << section_type << " "
                            << "Local: " << j << " "
                            << "Offset: " << i << "\n";
                        return {};
                    }

                    if (DEBUG_GUARD) 
                        printf("Function type idx: %d, hook_func_idx: %d, cbak_func_idx: %d "
                               "param_count: %d param_type: %x\n", 
                               j, *hook_func_idx, *cbak_func_idx, param_count, param_type);

                    // hook and cbak parameter check here
                    if (j == hook_type_idx && param_type != 0x7FU /* i32 */)
                    {
                        GUARDLOG(hook::log::PARAM_HOOK_CBAK)
                            << "Malformed transaction. "
                            << "hook and cbak function definition must have exactly one uint32_t parameter." << "\n";
                        return {};
                    }
                }

                int result_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                
                // RH TODO: enable this for production
                // this needs a reliable hook cleaner otherwise it will catch most compilers out
                if (strict && result_count != 1)
                {
                    GUARDLOG(hook::log::FUNC_RETURN_COUNT)
                        << "Malformed transaction. "
                        << "Hook declares a function type that returns fewer or more than one value. " << "\n";
                    return {};
                }

                // this can only ever be 1 in production, but in testing it may also be 0 or >1
                // so for completeness this loop is here but can be taken out in prod
                for (int k = 0; k < result_count; ++k)
                {
                    int result_type = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    if (result_type == 0x7F || result_type == 0x7E ||
                        result_type == 0x7D || result_type == 0x7C)
                    {
                        // pass, this is fine
                    }
                    else
                    {
                        GUARDLOG(hook::log::FUNC_RETURN_INVALID)
                            << "Invalid return type in function type. "
                            << "Codesec: " << section_type << " "
                            << "Local: " << j << " "
                            << "Offset: " << i << "\n";
                        return {};
                    }
                    
                    if (DEBUG_GUARD)
                        printf("Function type idx: %d, hook_func_idx: %d, cbak_func_idx: %d "
                               "result_count: %d result_type: %x\n", 
                               j, *hook_func_idx, *cbak_func_idx, result_count, result_type);
                        
                    // hook and cbak return type check here
                    if (j == hook_type_idx && (result_count != 1 || result_type != 0x7E /* i64 */))
                    {
                        GUARDLOG(hook::log::RETURN_HOOK_CBAK)
                            << "Malformed transaction. "
                            << (j == hook_type_idx ? "hook" : "cbak") << " j=" << j << " "
                            << " function definition must have exactly one int64_t return type. "
                            << "resultcount=" << result_count << ", resulttype=" << result_type << ", "
                            << "paramcount=" << param_count << "\n";
                        return {};
                    }
                }
            }
        }
        else
        if (section_type == 10) // code section
        {
            // RH TODO: parse anywhere else an expr is allowed in wasm and enforce rules there too
            // these are the functions
            int func_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();

            for (int j = 0; j < func_count; ++j)
            {
                // parse locals
                int code_size = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                int code_end = i + code_size;
                int local_count = parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                for (int k = 0; k < local_count; ++k)
                {
                    /*int array_size = */
                    parseLeb128(hook, i, &i); CHECK_SHORT_HOOK();
                    if (!(hook[i] >= 0x7C && hook[i] <= 0x7F))
                    {
                        GUARDLOG(hook::log::TYPE_INVALID)
                            << "Invalid local type. "
                            << "Codesec: " << j << " "
                            << "Local: " << k << " "
                            << "Offset: " << i << "\n";
                        return {};
                    }
                    i++; CHECK_SHORT_HOOK();
                }

                if (i == code_end)
                    continue; // allow empty functions

                // execution to here means we are up to the actual expr for the codesec/function

                auto valid =
                    check_guard(
                        hook,
                        j,
                        i,
                        code_end,
                        guard_import_number,
                        last_import_number,
                        guardLog,
                        guardLogAccStr);

                if (!valid)
                    return {};

                if (hook_func_idx && *hook_func_idx == j)
                    maxInstrCountHook = *valid;
                else if (cbak_func_idx && *cbak_func_idx == j)
                    maxInstrCountCbak = *valid;
                else
                {
                    printf("code section: %d not hook_func_idx: %d or cbak_func_idx: %d\n",
                            j, *hook_func_idx, (cbak_func_idx ? *cbak_func_idx : -1));
                    //   assert(false);
                }
                i = code_end;
            }
        }
        i = next_section;
    }

    // execution to here means guards are installed correctly

    return std::pair<uint64_t, uint64_t>{maxInstrCountHook, maxInstrCountCbak};

    /*
    GUARDLOG(hook::log::WASM_SMOKE_TEST)
        << "Trying to wasm instantiate proposed hook "
        << "size = " <<  hook.size() << "\n";

    std::optional<std::string> result = 
        hook::HookExecutor::validateWasm(hook.data(), (size_t)hook.size());

    if (result)
    {
        GUARDLOG(hook::log::WASM_TEST_FAILURE)
            << "Tried to set a hook with invalid code. VM error: "
            << *result << "\n";
        return {};
    }
    */

    return std::pair<uint64_t, uint64_t>{maxInstrCountHook, maxInstrCountCbak};
}

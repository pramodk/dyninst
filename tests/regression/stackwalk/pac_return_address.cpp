#include "procstate.h"
#include "registers/aarch64_regs.h"
#include <cstdio>

using namespace Dyninst;
using namespace Dyninst::Stackwalker;

// Supply synthetic PAC metadata without attaching to a process or requiring PAC hardware.
class MaskProcess : public ProcessState {
public:
    Architecture arch = Arch_aarch64;
    MachRegisterVal mask = 0;
    bool available = true, first_party = false, valid_request = true;
    unsigned reads = 0;
    bool getRegValue(MachRegister reg, THR_ID thread, MachRegisterVal &value) override {
        ++reads;
        valid_request = valid_request && reg == aarch64::pauth_cmask && thread == 1;
        value = mask;
        return available;
    }
    bool readMem(void *, Address, size_t) override { return false; }
    bool getThreadIds(std::vector<THR_ID> &) override { return false; }
    bool getDefaultThread(THR_ID &thread) override { thread = 1; return true; }
    unsigned getAddressWidth() override { return 8; }
    Architecture getArchitecture() override { return arch; }
    bool isFirstParty() override { return first_party; }
};

int main() {
    const Address address = 0x0055000012345678ULL;
    const MachRegisterVal mask = 0x007f000000000000ULL;
    const struct {
        const char *name;
        Address input, expected;
        MachRegisterVal mask;
        Architecture arch;
        THR_ID thread;
        bool available, first_party;
        unsigned reads;
    } cases[] = {
        {"signed", address, 0x12345678, mask, Arch_aarch64, 1, true, false, 1},
        {"unsigned", 0x12345678, 0x12345678, mask, Arch_aarch64, 1, true, false, 1},
        {"zero mask", address, address, 0, Arch_aarch64, 1, true, false, 1},
        {"different mask", address, 0x0050000012345678ULL, 0x000f000000000000ULL,
         Arch_aarch64, 1, true, false, 1},
        {"failed read", address, address, mask, Arch_aarch64, 1, false, false, 1},
        {"null address", 0, 0, mask, Arch_aarch64, 1, true, false, 0},
        {"unknown thread", address, address, mask, Arch_aarch64, NULL_THR_ID, true, false, 0},
        {"other architecture", address, address, mask, Arch_x86_64, 1, true, false, 0},
        {"first party", address, address, mask, Arch_aarch64, 1, true, true, 0},
    };
    bool ok = true;
    for (const auto &test : cases) {
        MaskProcess process;
        process.mask = test.mask;
        process.arch = test.arch;
        process.available = test.available;
        process.first_party = test.first_party;
        if (process.normalizeReturnAddress(test.input, test.thread) != test.expected ||
            process.reads != test.reads || !process.valid_request) {
            std::fprintf(stderr, "PAC normalization failed: %s\n", test.name);
            ok = false;
        }
    }
    return ok ? 0 : 1;
}

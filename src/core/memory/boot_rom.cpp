// Copyright (C) 2020-2021 Zach Collins <the_7thSamurai@protonmail.com>
//
// Azayaka is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Azayaka is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Azayaka. If not, see <https://www.gnu.org/licenses/>.

#include "core/memory/boot_rom.hpp"
#include "core/gameboy.hpp"
#include "core/memory/mmu.hpp"
#include "core/rom/rom.hpp"
#include "common/logger.hpp"
#include "common/string_utils.hpp"
#include "common/binary_file.hpp"

BootRom::BootRom(GameBoy *gb) : Component(gb) {
    data = nullptr;

    enabled = 0x00;
}

BootRom::~BootRom() {
    if (data != nullptr)
        delete[] data;
}

int BootRom::load(const std::string &file_path) {
    BinaryFile file(file_path, BinaryFile::Mode_Read);

    if (!file.is_open())
        return -1;

    unsigned int size = file.size();

    if (size != 0x100 && size != 0x900) {
        LOG_ERROR("BootRom::load Invalid size for BootRom: " + std::to_string(size));
        return -1;
    }

    data = new byte[size];
    file.read(data, size);

    return 0;
}

byte BootRom::read(word address) {
    if ((gb->gbc_mode && address < 0x900) || (address < 0x100)) {
        if (!enabled)
            return data[address];
        else {
            LOG_WARNING("BootRom::read can't access address 0x" + StringUtils::hex(address) + " when not enabled");
            return 0;
        }
    }

    else if (address == 0xFF50)
        return 0xFF;

    LOG_WARNING("BootRom::read can't access address 0x" + StringUtils::hex(address));

    return 0;
}

void BootRom::write(word address, byte value) {
    if (address == 0xFF50) {
        enabled = value;

        if (enabled) {
            gb->mmu->register_component(gb->rom, 0x0000, 0x00FF);

            if (gb->gbc_mode)
                gb->mmu->register_component(gb->rom, 0x0200, 0x08FF);
        }
        return;
    }

    LOG_WARNING("BootRom::write can't access address 0x" + StringUtils::hex(address));
}

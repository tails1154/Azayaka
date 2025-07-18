// Copyright (C) 2020-2022 Zach Collins <the_7thSamurai@protonmail.com>
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

#pragma once

#include <string>

class Joypad;
class Settings;

class Input
{
public:
    enum ButtonType {
        ButtonType_A,
        ButtonType_B,
        ButtonType_Up,
        ButtonType_Down,
        ButtonType_Left,
        ButtonType_Right,
        ButtonType_Start,
        ButtonType_Select
    };

    virtual ~Input() { }

    virtual std::string get_key_name(ButtonType button) const = 0;
    virtual void map_key(ButtonType button, const std::string &name) = 0;

    virtual void update() = 0;

    void bind_joypad(Joypad *joypad);

    void load_settings(Settings &settings, bool num);

protected:
    const static int num_of_buttons = 8;

    Joypad *joypad;
};

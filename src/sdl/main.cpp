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

#include <iostream>
#include <vector>
#include <SDL.h>

#include "sdl/window_sdl.hpp"
#include "sdl/input_sdl.hpp"
#include "sdl/audio_sdl.hpp"
#include "sdl/debugger_sdl.hpp"
#include "core/rewinder.hpp"

#include "sdl/options.hpp"
#include "core/gameboy.hpp"
#include "core/rom_list.hpp"
#include "common/string_utils.hpp"
#include "common/parser.hpp"
#include "common/logger.hpp"
#include "common/image.hpp"
#include "common/file_utils.hpp"
#include "core/settings.hpp"

#include "core/accessory/printer.hpp"
#include "core/serial/serial_device_network.hpp"


#ifdef __APPLE__
#define MODIFIER KMOD_GUI
#else
#define MODIFIER KMOD_CTRL
#endif

void print_usage(char *arg0, const std::vector <Option*> &options);

int load_rom_from_path(GameBoy &gb, const std::string &path, bool force_gb, bool force_gbc, bool dump_usage);
int load_rom_from_dir(GameBoy &gb, std::string &path, RomList &rom_list, bool force_gb, bool force_gbc, bool dump_usage);

int main(int argc, char **argv) {
    DebugOption debug_option;
    ScaleOption scale_option;
    //RateOption rate_option; // TODO: Add this in
    ForceGbOption force_gb_option;
    ForceGbcOption force_gbc_option;
    //LinkOption link_option;
    PrinterOption printer_option;
    DumpUsageOption dump_usage_option;
    VerboseOption verbose_option;
    ForceSDLOption force_sdl_option;

    std::vector <Option*> options;
    options.push_back(&debug_option);
    options.push_back(&scale_option);
    //options.push_back(&rate_option);
    options.push_back(&force_gb_option);
    options.push_back(&force_gbc_option);
    options.push_back(&link_option);
    options.push_back(&printer_option);
    options.push_back(&dump_usage_option);
    options.push_back(&verbose_option);
    options.push_back(&force_sdl_option);

    Parser parser;

    for (Option *option : options)
        parser.add_option(option);
    std::string link_mode = ""; // "server" or "client"
std::string link_ip = "";
int link_port = 0;
    std::string rom_path = "", error;

    for (int i = 1; i < argc; i++) {
        if (parser.parse(argc, argv, i) < 0) {
            if (rom_path.empty())
                rom_path = argv[i];
            else {
                print_usage(argv[0], options);
                return -1;
            }
        }
    }

    for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--link-server") == 0 && i+2 < argc) {
        link_mode = "server";
        link_ip = argv[i+1];
        link_port = atoi(argv[i+2]);
        i += 2;
    }
    else if (strcmp(argv[i], "--link-client") == 0 && i+2 < argc) {
        link_mode = "client";
        link_ip = argv[i+1];
        link_port = atoi(argv[i+2]);
        i += 2;
    }
    // ... (rest of regular option parsing)
}
    if (rom_path.empty()) {
        print_usage(argv[0], options);
        return -1;
    }



    bool force_gb   = force_gb_option.get_force_gb();
    bool force_gbc  = force_gbc_option.get_force_gbc();
    bool dump_usage = dump_usage_option.get_dump_usage();
    bool link       = link_option.get_link();

    RomList rom_list(rom_path);

    GameBoy gb, gb2; // Second GameBoy is for link cable
    Debugger_SDL debugger(&gb);

    if (rom_list.is_valid()) {
        rom_list.update();

        if (load_rom_from_dir(gb, rom_path, rom_list, force_gb, force_gbc, dump_usage) < 0)
            return -1;

        if (link) {
            if (load_rom_from_dir(gb2, rom_path, rom_list, force_gb, force_gbc, dump_usage) < 0)
                return -1;
        }
    }

    else {
        if (load_rom_from_path(gb, rom_path, force_gb, force_gbc, dump_usage) < 0)
            return -1;

        if (link) {
            if (load_rom_from_path(gb2, rom_path, force_gb, force_gbc, dump_usage) < 0)
                return -1;
        }
    }

    Printer printer;
    if (printer_option.get_printer()) {
        printer.set_rom_path(rom_path);
        gb.bind_serial_device(&printer);
    }

    Logger::get_instance().enable_verbose(verbose_option.get_verbose());

    WindowSDL window;
    if (window.create("Azayaka", scale_option.get_scale(), link, force_sdl_option.get_force_sdl()) < 0)
        return -1;

    AudioSDL audio_driver;
    InputSDL input, input2;

    gb.bind_input(input);

    if (link)
        gb2.bind_input(input2);

    gb.bind_audio_driver(&audio_driver);

    Uint64 frame_start, frame_end;
    double elapsed_time = 0.0;
    int frame_counter = 0;

    bool running = 1, pause = 0, rewinding = 0;
    bool fullscreen = 0;

    unsigned int seconds = 0;

    SDL_Event event;
    Rewinder rewinder;

    // Is it acceptable to use "Azayaka" as the Organization?
    char *settings_path = SDL_GetPrefPath("Azayaka", "Azayaka");

    LOG_NOTICE("Looking for INI file in \"" + std::string(settings_path) + "\"");

    Settings settings;
    settings.load(std::string(settings_path) + "azayaka.ini");
    settings.configure(gb, window.get_display(), audio_driver, input, input2, rewinder);
    settings.save(std::string(settings_path) + "azayaka.ini");

    SDL_free(settings_path);

    gb.init();

    if (link) {
        gb2.init();
        gb.connect_gameboy_link(gb2);
    }

    audio_driver.pause(0);

    if (debug_option.get_debug()) {
        debugger.set_activated(1);
        window.set_status_text("Debug-Mode", -1);
        window.update(gb.get_screen_buffer());
    }

    if (!link_mode.empty()) {
    SerialDevice* serdev = new SerialDeviceNetwork(link_ip, link_port, link_mode == "server");
    gb.serial->set_serial_device(serdev);
}

    while (running) {
        frame_start = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;

            else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED)
                window.resize(event.window.data1, event.window.data2, link ? 1 : 0);

            else if (event.type == SDL_DROPFILE) {
                // TODO
            }

            else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_f: // Fullscreen
                        if (event.key.keysym.mod & MODIFIER) {
                            fullscreen = !fullscreen;
                            window.set_fullscreen(fullscreen);
                        }
                        break;

                    case SDLK_p: // Pause
                        if (event.key.keysym.mod & MODIFIER) {
                            pause = !pause;

                            if (pause) {
                                audio_driver.pause(1);
                                audio_driver.set_mode(AudioDriver::Mode_Normal);

                                window.set_status_text("Paused", -1);
                                LOG_DEBUG("Paused emulation");
                            }
                            else {
                                audio_driver.pause(0);
                                window.clear_status_text();
                                LOG_DEBUG("Unpaused emulation");
                            }
                        }
                        break;

                    case SDLK_c: // Debugger Keyboard-Interrupt
                        if (event.key.keysym.mod & MODIFIER && debug_option.get_debug()) {
                            debugger.set_activated(1);
                            window.set_status_text("Debug-Mode", -1);
                            window.update(gb.get_screen_buffer());
                        }
                        break;

                    case SDLK_r: // Reset
                        if (event.key.keysym.mod) {
                            gb.reset();
                            gb.bind_input(input);
                            gb.bind_audio_driver(&audio_driver);

                            audio_driver.reset();

                            window.set_status_text("Reseting", 2);
                            LOG_DEBUG("Reset emulation");
                        }
                        break;

                    case SDLK_LSHIFT: // Turbo
                    case SDLK_RSHIFT:
                        if (!pause) {
                            audio_driver.set_mode(AudioDriver::Mode_Turbo);
                            window.set_status_text("Turbo", -1);
                            LOG_DEBUG("Started running in turbo mode");
                        }
                        break;

                    case SDLK_LALT: // Slow-Motion
                    case SDLK_RALT:
                        if (!pause) {
                            audio_driver.set_mode(AudioDriver::Mode_SlowMotion);
                            window.set_status_text("Slow-Motion", -1);
                            LOG_DEBUG("Started running in slow-motion mode");
                        }
                        break;

                    case SDLK_BACKSPACE: // Rewind
                        if (!pause && !rewinding && !link) {
                            audio_driver.set_mode(AudioDriver::Mode_Turbo);

                            rewinding = 1;
                            window.set_status_text("Rewinding", -1);
                            LOG_DEBUG("Started rewinding");
                        }
                        break;

                    case SDLK_EQUALS: // Increase Volume
                    case SDLK_KP_PLUS:
                        audio_driver.add_to_volume(5);
                        window.set_status_text("Volume set to " + std::to_string(audio_driver.get_volume()) + "%", 2);
                        break;

                    case SDLK_MINUS: // Decrease Volume
                    case SDLK_KP_MINUS:
                        audio_driver.add_to_volume(-5);
                        window.set_status_text("Volume set to " + std::to_string(audio_driver.get_volume()) + "%", 2);
                        break;

                    case SDLK_ESCAPE: // Quit
                        running = 0;
                        break;

                    default:
                        // Save-States
                        if (event.key.keysym.sym >= SDLK_F1 && event.key.keysym.sym <= SDLK_F10) {
                            std::string num = std::to_string(event.key.keysym.sym - SDLK_F1 + 1);
                            if (event.key.keysym.sym < SDLK_F10)
                                num = " #0" + num;
                            else
                                num = " #" + num;

                            std::string path = File::remove_extension(rom_path) + num + ".state";

                            if (event.key.keysym.mod & MODIFIER) { // Save
                                if (gb.save_state(path) != -1)
                                    window.set_status_text("Saved State"+num, 2);
                                else
                                    window.set_status_text("Can't save State"+num, 2);
                            }

                            else { // Load
                                if (gb.load_state(path) != -1)
                                    window.set_status_text("Loaded State"+num, 2);
                                else
                                    window.set_status_text("Can't load State"+num, 2);

                                // Clear all the old rewind data, or you will have some really weird rewinds! :)
                                rewinder.clear();
                            }
                        }
                        break;
                }
            }

            else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_LSHIFT: // Turbo
                    case SDLK_RSHIFT:
                        if (!pause) {
                            audio_driver.set_mode(AudioDriver::Mode_Normal);
                            window.clear_status_text();
                            LOG_DEBUG("Stopped turbo mode");
                        }
                        break;

                    case SDLK_LALT: // Slow-Motion
                    case SDLK_RALT:
                        if (!pause) {
                            audio_driver.set_mode(AudioDriver::Mode_Normal);
                            window.clear_status_text();
                            LOG_DEBUG("Stopped slow-motion mode");
                        }
                        break;

                    case SDLK_BACKSPACE: // Rewind
                        if (!pause && !link) {
                            audio_driver.set_mode(AudioDriver::Mode_Normal);

                            rewinding = 0;
                            window.clear_status_text();
                            LOG_DEBUG("Stopped rewinding");

                            input.reset();
                        }
                        break;

                    case SDLK_F11: // Screenshot
                        if (Common::save_next_image(rom_path, gb.get_screen_buffer(), 160, 144) < 0)
                            window.set_status_text("Cant save Screenshot", 2);
                        else
                            window.set_status_text("Saved Screenshot", 2);
                        break;

                    case SDLK_F12: // Dump audio
                        if (audio_driver.is_dumping_audio()) {
                            audio_driver.stop_dumping_audio();
                            window.set_status_text("Stopped saving audio", 2);
                        }
                        else {
                            if (audio_driver.start_dumping_audio(File::get_next_path(File::remove_extension(rom_path)+".wav")) < 0)
                                window.set_status_text("Unable to save audio", 2);
                            else
                                window.set_status_text("Started saving audio", 2);
                        }
                        break;
                }
            }
        }

        if (pause) {
            window.update(gb.get_screen_buffer());
            SDL_Delay(17);
            continue;
        }

        if (!rewinding) {
            input.update();

            if (link)
                input2.update();
        }

        if (debug_option.get_debug()) {
            if (debugger.is_activated()) {
                std::cout << "-> ";

                std::string command;
                std::getline(std::cin, command);

                debugger.run_command(command);
                debugger.update();

                if (!debugger.is_activated())
                    window.clear_status_text();
            }

            else {
                while (!debugger.update());

                if (debugger.is_activated())
                    window.set_status_text("Debug-Mode", -1);
            }
        }

        else if (rewinding) {
            rewinder.pop(gb);

            if (rewinder.has_frames_left())
                gb.run_frame();
        }

        else {
            if (link)
                gb.run_link_frame(gb2);

            else {
                gb.run_frame();
                rewinder.push(gb);
            }
        }

        if (link) {
            window.clear();
            window.update(gb.get_screen_buffer(), 0);

            // Let the 2nd GameBoy's frame finished rendering
            gb.run_link_frame2(gb2);

            window.update(gb2.get_screen_buffer(), 1);
            window.show();
        }
        else
            window.update(gb.get_screen_buffer());

        frame_end = SDL_GetPerformanceCounter();

        elapsed_time += double(frame_end - frame_start) / double(SDL_GetPerformanceFrequency());
        frame_counter++;

        if (elapsed_time >= 1.0) {
            elapsed_time /= (double)frame_counter;

            window.set_title("Azayaka | " + gb.get_rom_name() + " | " + StringUtils::ftos(1.0/elapsed_time, 2) + " FPS");

            elapsed_time  = 0.0;
            frame_counter = 0;

            seconds++;
        }
    }

    audio_driver.stop();

    LOG_DEBUG("Shutting down SDL...");

    SDL_Quit();

    return 0;
}

void print_usage(char *arg0, const std::vector <Option*> &options) {
    std::cout << "Usage: " << arg0 << " [Rom or Directory Path] [Options...]" << std::endl;

    for (Option *o : options) {
        if (o->get_short_option())
            std::cout << "\t-" << o->get_short_option() << ", --" << o->get_long_option() << "\t" << o->get_description() << std::endl;
        else
            std::cout << "\t    --" << o->get_long_option() << "\t" << o->get_description() << std::endl;
    }
}

int load_rom_from_path(GameBoy &gb, const std::string &path, bool force_gb, bool force_gbc, bool dump_usage) {
    std::string error;

    if (force_gb) {
        if (gb.load_rom_force_mode(path, error, 0, dump_usage) < 0) {
            LOG_ERROR(error);
            return -1;
        }
    }

    else if (force_gbc) {
        if (gb.load_rom_force_mode(path, error, 1, dump_usage) < 0) {
            LOG_ERROR(error);
            return -1;
        }
    }

    else {
        if (gb.load_rom(path, error, dump_usage) < 0) {
            LOG_ERROR(error);
            return -1;
        }
    }

    return 0;
}

int load_rom_from_dir(GameBoy &gb, std::string &path, RomList &rom_list, bool force_gb, bool force_gbc, bool dump_usage) {
    std::string error;

    std::vector <std::string> files;
    rom_list.get_files_names(files);

    if (!files.size()) {
        LOG_ERROR("No ROMs in given directory!");
        return -1;
    }

    for (unsigned int i = 0; i < files.size(); i++)
        std::cout << i+1 << ":\t" << files.at(i) << std::endl;

    unsigned int index;

    std::cout << "\nWhich ROM would you like(1-" << files.size() << "): ";
    std::cin >> index;

    if (index <= files.size())
        path = rom_list.get_file_path(files.at(index - 1));

    else {
        LOG_ERROR("Index out of range!");
        return -1;
    }

    if (force_gb) {
        if (gb.load_rom_force_mode(path, error, 0, dump_usage) < 0) {
            LOG_ERROR(error);
            return -1;
        }
    }

    else if (force_gbc) {
        if (gb.load_rom_force_mode(path, error, 1, dump_usage) < 0) {
            LOG_ERROR(error);
            return -1;
        }
    }

    else {
        if (gb.load_rom(path, error, dump_usage) < 0) {
            LOG_ERROR(error);
            return -1;
        }
    }

    return 0;
}

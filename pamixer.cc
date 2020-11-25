/*
 * Copyright (C) 2011 Clément Démoulins <clement@archivel.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pulseaudio.hh"
#include "device.hh"

#include <cmath>
#include <list>
#include <string>
#include <iostream>
using namespace std;

#include <pulse/pulseaudio.h>
#include <CLI/CLI.hpp>
//Flags and options variables
std::string opt_sink;
std::string opt_source;
int opt_volume_set;
int opt_volume_inc;
int opt_volume_dec;
double opt_gamma;




Device get_selected_device(Pulseaudio& pulse,CLI::Option *po_sink,CLI::Option* po_default_source,CLI::Option* po_source);
int gammaCorrection(int i, double gamma, int delta);
int main(int argc, char* argv[]);



pa_volume_t gammaCorrection(pa_volume_t i, double gamma, int delta) {
    double j = double(i);
    double relRelta = double(delta) / 100.0;

    j = j / PA_VOLUME_NORM;
    j = pow(j, (1.0/gamma));

    j = j + relRelta;
    if(j < 0.0) {
        j = 0.0;
    }

    j = pow(j, gamma);
    j = j * PA_VOLUME_NORM;

    return (pa_volume_t) round(j);
}

int main(int argc, char* argv[])
{

    CLI::App app("pamixer");

    //PO stands for pointer to option - It is needed to modify options later on
    auto po_sink = app.add_option("--sink", opt_sink, "choose a different sink than the default");
    auto po_source = app.add_option("--source", opt_source, "choose a different source than the default");
    auto po_default_source = app.add_flag("--default-source","select the default source");
    auto po_get_default_sink=app.add_flag("--get-default-sink","show default sink");
    auto po_get_volume = app.add_flag("--get-volume","get the current volume");
    auto po_get_volume_human=app.add_flag("--get-volume-human","get the current volume percentage or the string \"muted\"");
    auto po_set_volume=app.add_option("--set-volume",opt_volume_set,"set the volume");
    auto po_increase = app.add_option("--increase,-i",opt_volume_inc,"increase the volume");
    auto po_decrease= app.add_option("--decrease,-d",opt_volume_dec,"decrease the volume");
    auto po_toggle_mute= app.add_flag("--toggle-mute,-t","switch between mute and unmute");
    auto po_mute=    app.add_flag("--mute,-m","set mute");
    auto po_allow_boost=    app.add_flag("--allow-boost","allow volume to go above 100%");
    auto po_gamma =    app.add_option("--gamma",opt_gamma,"increse/decrease using gamma correction e.g. 2.2");
    auto po_unmute =    app.add_flag("--unmute,-u","unset mute");
    auto po_get_mute = app.add_flag("--get-mute","display true if muted false otherwise");
    auto po_list_sinks = app.add_flag("--list-sinks","list the sinks");
    auto po_list_sources = app.add_flag("--list-sources","list the sources");

    //Conflicting Options and requirements
    po_set_volume->excludes(po_increase);
    po_set_volume->excludes(po_decrease);
    po_decrease->excludes(po_increase);
    po_toggle_mute->excludes(po_mute);
    po_toggle_mute->excludes(po_unmute);
    po_unmute->excludes(po_mute);
    po_sink->excludes(po_source);
    po_sink->excludes(po_default_source);
    po_get_volume->excludes(po_list_sinks);
    po_get_volume->excludes(po_list_sources);
    po_get_volume->excludes(po_get_volume_human);
    po_get_volume_human->excludes(po_list_sinks);
    po_get_volume_human->excludes(po_list_sources);
    po_get_volume_human->excludes(po_mute);
    po_mute->excludes(po_list_sinks);
    po_mute->excludes(po_list_sources);

    po_set_volume->expected(0,2);
    po_increase->expected(0,1);
    po_decrease->expected(0,1);
    po_gamma->default_val(1.0);

    CLI11_PARSE(app, argc, argv);

    Pulseaudio pulse("pamixer");
    Device device = get_selected_device(pulse,po_sink,po_default_source,po_source);

        if (opt_volume_set || opt_volume_inc || opt_volume_dec) {

            pa_volume_t new_value = 0;
            if (opt_volume_set) {
                new_value = round( (double)opt_volume_set * (double)PA_VOLUME_NORM / 100.0);
            } else if (opt_volume_inc) {
                new_value = gammaCorrection(device.volume_avg, opt_gamma,  opt_volume_inc);
            } else if (opt_volume_dec) {
                new_value = gammaCorrection(device.volume_avg, opt_gamma, -opt_volume_dec);
            }

            if (!*po_allow_boost && new_value > PA_VOLUME_NORM) {
                new_value = PA_VOLUME_NORM;
            }

            pulse.set_volume(device, new_value);
            device = get_selected_device(pulse,po_sink,po_default_source,po_source);
        }

        if (*po_toggle_mute || *po_mute || *po_unmute) {
            if (*po_toggle_mute) {
                pulse.set_mute(device, !device.mute);
            } else {
                pulse.set_mute(device, *po_mute || !*po_unmute);
            }
            device = get_selected_device(pulse,po_sink,po_default_source,po_source);
        }

        int ret = 0;
        if (*po_get_volume && *po_mute) {
            cout << boolalpha << device.mute << ' ' << device.volume_percent << '\n';
            ret = !device.mute;
        } else if (*po_get_volume) {
            cout << device.volume_percent << '\n';
            ret = device.volume_percent <= 0;
        } else if (*po_get_volume_human) {
            if (device.mute) {
                cout << "muted\n";
            } else {
                cout << device.volume_percent << "%\n";
            }
            ret = (device.volume_percent <= 0) || device.mute;
        } else if (*po_get_mute) {
            cout << boolalpha << device.mute << '\n';
            ret = !device.mute;
        } else {
            if (*po_get_default_sink) {
                cout << "Default sink:\n";
                auto sink =  pulse.get_default_sink();
                    cout << sink.index << " \""
                         << sink.name << "\" \""
                         << sink.description << "\"\n";
            }
            if (*po_list_sinks) {
                cout << "Sinks:\n";
                for (const Device& sink : pulse.get_sinks()) {
                    cout << sink.index << " \""
                         << sink.name << "\" \""
                         << sink.description << "\"\n";
                }
            }
            if (*po_list_sources) {
                cout << "Sources:\n";
                for (const Device& source : pulse.get_sources()) {
                    cout << source.index << " \""
                         << source.name << "\" \""
                         << source.description << "\"\n";
                }
            }
        }

        return ret;
    
}
Device get_selected_device(Pulseaudio& pulse,CLI::Option *po_sink,CLI::Option* po_default_source,CLI::Option* po_source) {
    Device device = pulse.get_default_sink();
    if (*po_sink) {
        device = pulse.get_sink(opt_sink);
    } else if (*po_default_source) {
        device = pulse.get_default_source();
    } else if (*po_source) {
        device = pulse.get_source(opt_source);
    }
    return device;
}


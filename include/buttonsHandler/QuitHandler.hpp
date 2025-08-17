/*QuitHandler.hpp*/

#pragma once

#include "Utility/SSHManager.hpp"
#include <gtkmm.h>
#include <string>

namespace QuitHandler
{
    void handle(Gtk::Window* parentWindow,
                const std::string& redpitayaHost,
                const std::string& redpitayaPassword,
                const std::string& redpitayaPrivateKeyPath);
}

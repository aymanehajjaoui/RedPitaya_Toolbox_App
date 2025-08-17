/*ShowMetricsHandler.hpp*/

#pragma once

#include <gtkmm.h>
#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <sstream>

#include "Utility/DetailsPanel.hpp"
#include "Utility/ConnectionManager.hpp"
#include "Utility/SSHManager.hpp"

namespace ShowMetricsHandler
{
    void handle(Gtk::Window* parentWindow,
                Gtk::Button& buttonShowMetrics,
                Gtk::Button& buttonConnectRedPitaya,
                const std::string& redpitayaHost,
                const std::string& redpitayaPassword,
                const std::string& redpitayaPrivateKeyPath,
                DetailsPanel& detailsPanel);
}

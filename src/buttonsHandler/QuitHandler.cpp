/*QuitHandler.cpp*/

#include "buttonsHandler/QuitHandler.hpp"
#include <cstdlib>
#include <thread>

namespace QuitHandler
{
    void handle(Gtk::Window *parentWindow,
                const std::string &redpitayaHost,
                const std::string &redpitayaPassword,
                const std::string &redpitayaPrivateKeyPath)
    {
        // Kill local monitoring.py plotter if running
        std::system("pkill -f monitoring.py");

        // Delete /root/monitoring from RedPitaya (if connected)
        if (!redpitayaHost.empty()) {
            SSHManager::execute_remote_command(
                redpitayaHost,
                redpitayaPassword,
                redpitayaPrivateKeyPath,
                "rm -rf /root/monitoring");
        }

        // Exit GUI
        parentWindow->hide();
    }
}



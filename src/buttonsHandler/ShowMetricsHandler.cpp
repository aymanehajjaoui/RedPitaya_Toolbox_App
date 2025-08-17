/* ShowMetricsHandler.cpp */

#include "buttonsHandler/ShowMetricsHandler.hpp"

namespace ShowMetricsHandler
{
    void handle(Gtk::Window *parentWindow,
                Gtk::Button &buttonShowMetrics,
                Gtk::Button &buttonConnectRedPitaya,
                const std::string &redpitayaHost,
                const std::string &redpitayaPassword,
                const std::string &redpitayaPrivateKeyPath,
                DetailsPanel &detailsPanel)
    {
        // Step 0: Ask user for interval
        Gtk::Dialog dialog("Select Refresh Interval", *parentWindow);
        dialog.set_modal(true);
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Start", Gtk::RESPONSE_OK);

        Gtk::Box *content = dialog.get_content_area();
        auto label = Gtk::make_managed<Gtk::Label>("Choose interval in microseconds:");
        auto combo = Gtk::make_managed<Gtk::ComboBoxText>();
        auto customEntry = Gtk::make_managed<Gtk::Entry>();

        combo->append("500000");
        combo->append("1000000");
        combo->append("2000000");
        combo->append("Custom");
        combo->set_active(0);

        customEntry->set_placeholder_text("Custom interval (us)");

        content->pack_start(*label, Gtk::PACK_SHRINK);
        content->pack_start(*combo, Gtk::PACK_SHRINK);
        // Note: customEntry is not added initially

        combo->signal_changed().connect([combo, customEntry, content, &dialog]() {
            if (combo->get_active_text() == "Custom") {
                if (!customEntry->get_parent()) {
                    content->pack_start(*customEntry, Gtk::PACK_SHRINK);
                    dialog.show_all();  // refresh the dialog to show the new entry
                    customEntry->grab_focus();  // optional: auto-focus
                }
            } else {
                if (customEntry->get_parent()) {
                    content->remove(*customEntry);
                    dialog.show_all();
                }
            }
        });

        dialog.show_all();

        if (dialog.run() != Gtk::RESPONSE_OK)
            return;

        std::string interval = combo->get_active_text();
        if (interval == "Custom")
            interval = customEntry->get_text();

        if (interval.empty() || std::stoi(interval) <= 0)
        {
            detailsPanel.append_log("[Error] Invalid interval.");
            return;
        }

        buttonShowMetrics.set_sensitive(false);
        buttonConnectRedPitaya.set_sensitive(false);
        detailsPanel.append_log("Checking RedPitaya connectivity...");

        std::thread([=, &buttonShowMetrics, &buttonConnectRedPitaya, &detailsPanel]()
                    {
            bool connected = ConnectionManager::isSSHConnectionAlive(
                redpitayaHost, redpitayaPassword, redpitayaPrivateKeyPath);

            if (!connected) {
                Glib::signal_idle().connect_once([&]() {
                    detailsPanel.append_log("[Error] RedPitaya is not reachable over SSH.");
                    detailsPanel.set_status("RedPitaya connection failed.");
                    buttonShowMetrics.set_sensitive(true);
                });
                return;
            }

            Glib::signal_idle().connect_once([&]() {
                detailsPanel.append_log("SSH connection to RedPitaya successful.");
                detailsPanel.set_status("Connected to RedPitaya.");
            });

            SSHManager::execute_remote_command(
                redpitayaHost, redpitayaPassword, redpitayaPrivateKeyPath,
                "mkdir -p /root/monitoring");

            Glib::signal_idle().connect_once([&]() {
                detailsPanel.append_log("Uploading monitor_sender to RedPitaya...");
            });

            bool transferred = SSHManager::scp_transfer(
                redpitayaHost,
                redpitayaPassword,
                "build/monitoring/monitor_sender",
                "/root/monitoring/monitor_sender",
                redpitayaPrivateKeyPath);

            if (!transferred) {
                Glib::signal_idle().connect_once([&]() {
                    detailsPanel.append_log("[Error] Failed to upload monitor_sender.");
                    buttonShowMetrics.set_sensitive(true);
                });
                return;
            }

            Glib::signal_idle().connect_once([&]() {
                detailsPanel.append_log("monitor_sender uploaded successfully.");
            });

            std::ostringstream remoteCmd;
            remoteCmd << "chmod +x /root/monitoring/monitor_sender && "
                      << "nohup /root/monitoring/monitor_sender " << interval
                      << " > /dev/null 2>&1 &";

            bool senderStarted = SSHManager::execute_remote_command(
                redpitayaHost, redpitayaPassword, redpitayaPrivateKeyPath, remoteCmd.str());

            if (!senderStarted) {
                Glib::signal_idle().connect_once([&]() {
                    detailsPanel.append_log("[Error] Failed to start monitor_sender.");
                    buttonShowMetrics.set_sensitive(true);
                });
                return;
            }

            Glib::signal_idle().connect_once([&]() {
                detailsPanel.append_log("monitor_sender started on RedPitaya.");
            });

            std::system("pkill -f monitoring.py");

            std::thread([&buttonShowMetrics, &detailsPanel]() {
                int status = std::system("python3 build/monitoring/monitoring.py");

                Glib::signal_idle().connect_once([&]() {
                    if (status != 0) {
                        detailsPanel.append_log("[Error] monitoring.py exited with error.");
                    } else {
                        detailsPanel.append_log("monitoring.py closed by user.");
                    }
                    buttonShowMetrics.set_sensitive(true);
                    detailsPanel.set_status("Monitoring stopped.");
                });
            }).detach(); })
            .detach();
    }
}
